#include "yonderest/save.hpp"
#include "json.hpp"
#include "platform.hpp"
#include "yonderest/content.hpp"

#include <algorithm>

namespace yonder {

Worker::Worker() : thread([this] { run(); }) {}

Worker::~Worker() {
    {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    changed.notify_all();
    thread.join();
}

void Worker::post(std::function<void()> job) {
    {
        std::lock_guard lock(mutex);
        jobs.push_back(std::move(job));
    }
    changed.notify_all();
}

void Worker::wait() {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return (jobs.empty() && !busy) || failure; });
}

void Worker::check() {
    std::lock_guard lock(mutex);
    if (failure) std::rethrow_exception(failure);
}

void Worker::run() {
    std::unique_lock lock(mutex);
    while (true) {
        changed.wait(lock, [&] { return !jobs.empty() || stopping; });
        if (jobs.empty() || failure) {
            if (stopping) return;
            jobs.clear();
            continue;
        }
        auto job = std::move(jobs.front());
        jobs.pop_front();
        busy = true;
        lock.unlock();
        std::exception_ptr error;
        try { job(); } catch (...) { error = std::current_exception(); }
        lock.lock();
        busy = false;
        if (error && !failure) failure = error;
        changed.notify_all();
    }
}

namespace {

std::string place(const char* x, int column, const char* y, int row) {
    return std::string(x) + std::to_string(column) + y + std::to_string(row);
}

std::filesystem::path chunk_file(const std::filesystem::path& folder, int x, int y) {
    return folder / "regions" / place("x", x / region_size, "-y", y / region_size) / (place("x", x, "-y", y) + ".json");
}

std::string player_json(const Player& p) {
    JsonWriter out;
    out.object().key("world").text(p.world).key("x").number(p.x).key("y").number(p.y)
        .key("facing").text(p.facing < 0 ? "left" : p.facing > 0 ? "right" : "rear");
    if (!p.held.empty()) out.key("held").text(p.held);
    out.properties(p.properties).close();
    return out.finish();
}

bool empty(const Cells& layer) {
    return std::ranges::all_of(layer, [](const Cell& c) { return c.id.empty(); });
}

}

std::optional<Settings> WorldFolder::settings() const {
    auto path = folder / "world.json";
    if (!std::filesystem::exists(path)) return std::nullopt;
    auto file = read_json(path);
    Field root(file, path.string());
    Settings s;
    s.seed = root["seed"].whole();
    s.columns = int(root["columns"].integer());
    s.rows = int(root["rows"].integer());
    s.properties = rest(file, {"seed", "columns", "rows"});
    return s;
}

void WorldFolder::write_settings(const Settings& s) const {
    std::filesystem::create_directories(folder);
    JsonWriter out;
    out.object().key("seed").whole(s.seed).key("columns").integer(s.columns).key("rows").integer(s.rows);
    out.properties(s.properties).close();
    replace_file(folder / "world.json", out.finish());
}

// A chunk names each distinct cell once in its palette; its layers are rows of palette
// positions, top to bottom, under the names the content gives them. An absent layer is empty.
std::optional<Chunk> WorldFolder::chunk(int x, int y) const {
    auto path = chunk_file(folder, x, y);
    if (!std::filesystem::exists(path)) return std::nullopt;
    auto file = read_json(path);
    Field root(file, path.string());
    Chunk c(layers.size());
    std::vector<Cell> palette;
    auto entries = root["palette"];
    for (std::size_t i = 0; i < entries.size(); ++i) {
        auto entry = entries[i];
        Cell cell;
        if (entry.present()) {
            cell.id = entry["block"].text();
            cell.properties = rest(entry.value(), {"block"});
        }
        palette.push_back(cell);
    }
    std::vector<std::string_view> known{"palette"};
    for (std::size_t l = 0; l < layers.size(); ++l) {
        known.push_back(layers[l]);
        auto rows = root[layers[l].c_str()];
        if (!rows.present()) continue;
        for (std::size_t ly = 0; ly < chunk_size; ++ly)
            for (std::size_t lx = 0; lx < chunk_size; ++lx) {
                auto index = rows[ly][lx];
                auto i = std::size_t(index.integer());
                if (i >= palette.size()) index.fail("is not in the palette");
                c.layers[l][ly * chunk_size + lx] = palette[i];
            }
    }
    // Everything else belongs to modules, such as the generator's own record of the chunk.
    c.properties = rest(file, {});
    std::erase_if(c.properties.groups, [&](const auto& group) { return std::ranges::find(known, group.first) != known.end(); });
    return c;
}

void WorldFolder::write_chunk(int x, int y, const Chunk& c) const {
    std::vector<const Cell*> palette;
    auto index = [&](const Cell& cell) {
        auto same = [&](const Cell* p) { return p->id.empty() ? cell.id.empty() : *p == cell; };
        auto found = std::ranges::find_if(palette, same);
        if (found == palette.end()) { palette.push_back(&cell); return int(palette.size()) - 1; }
        return int(found - palette.begin());
    };
    std::vector<std::array<int, chunk_area>> grids(c.layers.size());
    for (int i = 0; i < chunk_area; ++i)
        for (std::size_t l = 0; l < c.layers.size(); ++l) grids[l][i] = index(c.layers[l][i]);
    std::vector<bool> filled;
    for (const auto& layer : c.layers) filled.push_back(!empty(layer));
    JsonWriter out;
    out.object();
    if (std::ranges::any_of(filled, std::identity{})) {
        out.key("palette").array();
        for (const auto* cell : palette) {
            if (cell->id.empty()) { out.null(); continue; }
            out.object(true).key("block").text(cell->id).properties(cell->properties).close();
        }
        out.close();
    }
    for (std::size_t l = 0; l < c.layers.size(); ++l) {
        if (!filled[l]) continue;
        out.key(layers[l]).array();
        for (int ly = 0; ly < chunk_size; ++ly) {
            out.array(true);
            for (int lx = 0; lx < chunk_size; ++lx) out.integer(grids[l][ly * chunk_size + lx]);
            out.close();
        }
        out.close();
    }
    out.properties(c.properties).close();
    auto file = chunk_file(folder, x, y);
    std::filesystem::create_directories(file.parent_path());
    replace_file(file, out.finish());
}

Save::Save(const std::filesystem::path& folder) : folder(folder) {
    std::filesystem::create_directories(folder);
    lock = lock_folder(folder);
}

Save::~Save() {
    worker.wait();
    unlock_folder(lock);
}

WorldFolder Save::world(const std::string& name, std::vector<std::string> layers) const {
    return WorldFolder(folder / name, std::move(layers));
}

// The player belongs to the universe; `world` says where in it they are, the starting world if absent.
std::optional<Player> Save::player() {
    auto path = folder / "player.json";
    if (!std::filesystem::exists(path)) return std::nullopt;
    auto file = read_json(path);
    Field root(file, path.string());
    Player p;
    p.world = root["world"].text_or(starting_world);
    p.x = root["x"].number();
    p.y = root["y"].number();
    auto facing = root["facing"].text_or("right");
    p.facing = facing == "left" ? -1 : facing == "rear" ? 0 : 1;
    p.held = root["held"].text_or("");
    p.properties = rest(file, {"world", "x", "y", "facing", "held"});
    player_text = player_json(p);
    return p;
}

bool Save::write_player(const Player& p) {
    auto text = player_json(p);
    if (text == player_text) return false;
    player_text = text;
    worker.post([path = folder / "player.json", text = std::move(text)] { replace_file(path, text); });
    return true;
}

}
