#include "save.hpp"
#include "json.hpp"

#include <algorithm>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>

namespace yonder {
namespace {

std::filesystem::path chunk_file(const std::filesystem::path& folder, int x, int y) {
    return folder / "chunks" / ("x" + std::to_string(x) + "-y" + std::to_string(y) + ".json");
}

std::string player_json(const Player& p) {
    JsonWriter out;
    out.object().key("x").number(p.x).key("y").number(p.y)
        .key("facing").text(p.facing < 0 ? "left" : p.facing > 0 ? "right" : "rear");
    if (!p.selected.empty()) out.key("held").text(p.selected);
    if (!p.last_selected.empty()) out.key("last_held").text(p.last_selected);
    out.key("inventory").object();
    for (const auto& [id, count] : p.inventory) out.key(id).integer(count);
    out.close().key("ready").array(true);
    for (const auto& id : p.ready) out.text(id);
    out.close().close();
    return out.finish();
}

using Cells = std::array<Cell, chunk_size * chunk_size>;

bool empty(const Cells& layer) {
    return std::ranges::all_of(layer, [](const Cell& c) { return c.id.empty(); });
}

}

Save::Save(const std::filesystem::path& folder) : folder(folder) {
    if (std::filesystem::exists(folder) && !std::filesystem::is_directory(folder))
        throw std::runtime_error(folder.string() + " is not a world folder");
    std::filesystem::create_directories(folder / "chunks");
    lock = open(folder.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (lock < 0) throw std::runtime_error("Cannot open " + folder.string());
    // Released by the system however the game ends.
    if (flock(lock, LOCK_EX | LOCK_NB) != 0) {
        close(lock);
        throw std::runtime_error(folder.string() + " is already open in another game");
    }
}

Save::~Save() {
    close(lock);
}

std::optional<Settings> Save::settings() const {
    auto path = folder / "world.json";
    if (!std::filesystem::exists(path)) return std::nullopt;
    JsonFile file(path);
    auto root = file.root();
    Settings s;
    s.seed = root["seed"].whole();
    s.columns = int(root["columns"].integer(1, 1 << 16));
    s.rows = int(root["rows"].integer(1, 1 << 16));
    if (root["chunk_size"].integer(1, 1 << 16) != chunk_size)
        root["chunk_size"].fail("must be " + std::to_string(chunk_size) + " in this build");
    s.terrain = read_terrain(root["terrain"]);
    return s;
}

void Save::write_settings(const Settings& s) {
    JsonWriter out;
    out.object().key("seed").whole(s.seed).key("columns").integer(s.columns).key("rows").integer(s.rows)
        .key("chunk_size").integer(chunk_size).key("terrain");
    write_terrain(out, s.terrain);
    out.close();
    replace_file(folder / "world.json", out.finish());
}

std::optional<Player> Save::player() {
    auto path = folder / "player.json";
    if (!std::filesystem::exists(path)) return std::nullopt;
    JsonFile file(path);
    auto root = file.root();
    Player p;
    p.x = root["x"].number();
    p.y = root["y"].number();
    auto facing = root["facing"].text_or("right");
    if (facing == "left") p.facing = -1;
    else if (facing == "rear") p.facing = 0;
    else if (facing == "right") p.facing = 1;
    else root["facing"].fail("must be left, right or rear");
    p.selected = root["held"].text_or("");
    p.last_selected = root["last_held"].text_or("");
    auto inventory = root["inventory"];
    for (const auto& id : inventory.keys())
        p.inventory[id] = inventory[id.c_str()].integer(0, std::numeric_limits<std::int64_t>::max());
    auto ready = root["ready"];
    if (ready.size() > 12) ready.fail("can hold at most twelve objects");
    for (Json::ArrayIndex i = 0; i < ready.size(); ++i) p.ready.push_back(ready[i].text());
    drop_empty(p); // An emptied entry, or readiness without stock, is not carried.
    player_text = player_json(p);
    return p;
}

bool Save::write_player(const Player& p) {
    auto text = player_json(p);
    if (text == player_text) return false;
    replace_file(folder / "player.json", text);
    player_text = std::move(text);
    return true;
}

// A chunk names each distinct cell once in its palette; its layers are rows of palette
// positions, top to bottom. An absent layer is empty; absent settling is fully settled.
std::optional<Chunk> Save::chunk(int x, int y) const {
    auto path = chunk_file(folder, x, y);
    if (!std::filesystem::exists(path)) return std::nullopt;
    JsonFile file(path);
    auto root = file.root();
    Chunk c;
    c.profile = root["generation"].text();
    if (profile_rank(c.profile) == 0) root["generation"].fail("is not a generation this build knows: " + c.profile);
    std::vector<Cell> palette;
    auto entries = root["palette"];
    for (Json::ArrayIndex i = 0; i < entries.size(); ++i) {
        auto entry = entries[i];
        Cell cell;
        if (entry.present()) {
            cell.id = entry["block"].text();
            if (cell.id.empty()) entry["block"].fail("must name a block; empty space is null");
            cell.quantity = int(entry["quantity"].integer_or(1, 1, std::numeric_limits<int>::max()));
            cell.placed = entry["placed"].flag_or(false);
            cell.batch = int(entry["batch"].integer_or(1, 1, std::numeric_limits<int>::max()));
        }
        palette.push_back(cell);
    }
    auto grid = [&](const char* name, auto&& use) {
        auto rows = root[name];
        if (!rows.present()) return;
        if (rows.size() != chunk_size) rows.fail("must have " + std::to_string(chunk_size) + " rows");
        for (Json::ArrayIndex ly = 0; ly < chunk_size; ++ly) {
            auto row = rows[ly];
            if (row.size() != chunk_size) row.fail("must have " + std::to_string(chunk_size) + " cells");
            for (Json::ArrayIndex lx = 0; lx < chunk_size; ++lx) use(row[lx], ly * chunk_size + lx);
        }
    };
    auto layer = [&](Cells& cells) {
        return [&](const Field& f, int i) { cells[i] = palette[f.integer(0, std::int64_t(palette.size()) - 1)]; };
    };
    grid("foreground", layer(c.tiles));
    grid("background", layer(c.background));
    if (root["settling"].present()) c.settling.resize(c.tiles.size());
    grid("settling", [&](const Field& f, int i) { c.settling[i] = std::uint8_t(f.integer(0, settle_extent)); });
    return c;
}

void Save::write_chunk(int x, int y, const Chunk& c) {
    std::vector<const Cell*> palette;
    auto index = [&](const Cell& cell) {
        auto same = [&](const Cell* p) { return p->id.empty() ? cell.id.empty() : *p == cell; };
        auto found = std::ranges::find_if(palette, same);
        if (found == palette.end()) { palette.push_back(&cell); return int(palette.size()) - 1; }
        return int(found - palette.begin());
    };
    std::array<std::array<int, chunk_size * chunk_size>, 2> grids;
    for (int i = 0; i < chunk_size * chunk_size; ++i) {
        grids[0][i] = index(c.tiles[i]);
        grids[1][i] = index(c.background[i]);
    }
    JsonWriter out;
    out.object().key("generation").text(c.profile);
    bool layers[]{!empty(c.tiles), !empty(c.background)};
    if (layers[0] || layers[1]) {
        out.key("palette").array();
        for (const auto* cell : palette) {
            if (cell->id.empty()) { out.null(); continue; }
            out.object(true).key("block").text(cell->id);
            if (cell->quantity != 1) out.key("quantity").integer(cell->quantity);
            if (cell->placed) out.key("placed").flag(true);
            if (cell->batch != 1) out.key("batch").integer(cell->batch);
            out.close();
        }
        out.close();
    }
    auto rows = [&](const char* name, auto&& value) {
        out.key(name).array();
        for (int ly = 0; ly < chunk_size; ++ly) {
            out.array(true);
            for (int lx = 0; lx < chunk_size; ++lx) out.integer(value(ly * chunk_size + lx));
            out.close();
        }
        out.close();
    };
    const char* names[]{"foreground", "background"};
    for (int l = 0; l < 2; ++l)
        if (layers[l]) rows(names[l], [&](int i) { return grids[l][i]; });
    if (!c.settling.empty()) rows("settling", [&](int i) { return int(c.settling[i]); });
    out.close();
    replace_file(chunk_file(folder, x, y), out.finish());
}

}
