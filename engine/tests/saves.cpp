// What the engine keeps: a save is a universe holding the player and its worlds, chunks by region,
// and every module's properties under its name, whether or not anything here understands them.

#include "yonderest/save.hpp"
#include "yonderest/content.hpp"
#include "yonderest/generation.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

using namespace yonder;
namespace {

void require(bool result, const std::string& what) {
    if (!result) throw std::runtime_error(what);
}

std::string read(const std::filesystem::path& file) {
    std::ifstream in(file);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

void write(const std::filesystem::path& file, const std::string& text) {
    std::ofstream(file) << text;
}

std::string replaced(std::string text, const std::string& from, const std::string& to) {
    auto at = text.find(from);
    require(at != std::string::npos, "fixture text not found: " + from);
    return text.replace(at, from.size(), to);
}

// The engine's own test needs land, not a generator's idea of it: every new chunk is empty.
class Nothing : public Generation {
public:
    void create(Settings&) override {}
    void open(const Settings&) override {}
    int first_row() override { return 0; }
    Chunk generate(int, int, const Neighborhood&) override { return Chunk(2); }
};

const std::vector<std::string> layers{"foreground", "rear"};

Content content() {
    Content c;
    c.layers = layers;
    return c;
}

Value group(Value::Object members) { return Value(std::move(members)); }

Cell cell(const Block& id, const char* module = nullptr, Value value = {}) {
    Cell c{id, {}};
    if (module) c.properties.set(module, std::move(value));
    return c;
}

// Everything a save holds survives writing and reading, including values JSON tools often mangle.
void round_trip(const std::filesystem::path& folder) {
    Nothing nothing;
    auto c = content();
    Settings settings;
    settings.seed = 9007199254740993ULL; // 2^53 + 1: a double would round it.
    settings.columns = 8; settings.rows = 4;
    settings.properties.set("generator", group({{"generation", "test-1"}}));
    Player before;
    before.world = Save::starting_world;
    before.x = 305.21666666685525; before.y = 115;
    before.facing = 0;
    before.held = "base:soil";
    before.properties.set("inventory", group({{"carried", group({{"base:soil", 7}, {"missing:crystal", 3}})}, {"ready", Value::Array{"base:soil"}}}));
    Cell batch = cell("base:timber", "crafting", group({{"quantity", 5}}));
    {
        Save save(folder);
        auto world_folder = save.world(Save::starting_world, layers);
        world_folder.write_settings(settings);
        World world(save, world_folder, c, settings, nothing);
        world.load(40, 100, 41, 100);
        world.put(40, 100, batch);
        world.put(41, 100, cell("workshop:workbench", "crafting", group({{"batch", 10}})));
        world.put(40, 100, cell("missing:wall"), 1);
        require(world.save_changes() | save.write_player(before), "a first save writes");
        require(!world.save_changes() && !save.write_player(before), "nothing is written when nothing changed");
    }
    require(std::filesystem::exists(folder / "player.json") && std::filesystem::exists(folder / "origin" / "world.json")
                && std::filesystem::exists(folder / "origin" / "regions" / "x0-y0" / "x0-y1.json"),
            "the player sits in the universe, the world in its folder, a chunk in its region's folder");
    auto text = read(folder / "origin" / "regions" / "x0-y0" / "x0-y1.json");
    require(text.find("{\"block\": \"base:timber\", \"crafting\": {\"quantity\": 5}}") != std::string::npos,
            "a palette entry names its block, and each module's properties sit under the module");
    require(text.find("\"rear\": [") != std::string::npos, "layers are named as the content declares them");
    Save save(folder);
    auto p = *save.player();
    auto world_folder = save.world(p.world, layers);
    auto s = *world_folder.settings();
    require(s.seed == settings.seed && s.columns == settings.columns && s.rows == settings.rows && s.properties == settings.properties,
            "world settings survive, the seed exactly");
    require(p.world == before.world && p.x == before.x && p.y == before.y && p.facing == 0 && p.held == before.held
                && p.properties == before.properties,
            "the player survives exactly, with what modules keep on it");
    World world(save, world_folder, c, s, nothing);
    world.load(40, 100, 41, 100);
    require(world.cell(40, 100) == batch && world.cell(41, 100).properties["crafting"]["batch"] == Value(10)
                && world.cell(40, 100, 1) == cell("missing:wall"),
            "both layers keep module properties and unknown IDs");
    require(world_folder.chunk(0, 1).has_value() && !world_folder.chunk(5, 1).has_value(),
            "a visited chunk is saved; an unvisited one is not, and stays ungenerated");
    Chunk far(2);
    far.properties.set("generator", group({{"ground", Value::Array(chunk_size, Value(70))}}));
    world_folder.write_chunk(region_size + 1, 2, far);
    require(std::filesystem::exists(folder / "origin" / "regions" / "x1-y0" / "x65-y2.json"), "chunk files gather by region");
    require(world_folder.chunk(region_size + 1, 2)->properties == far.properties, "a chunk keeps what a module records about it");
}

// What a person editing the files meets: absence, unfamiliar additions, a changed block.
void editing(const std::filesystem::path& folder) {
    Nothing nothing;
    auto c = content();
    Settings settings;
    settings.columns = 8; settings.rows = 8;
    {
        Save save(folder);
        auto world_folder = save.world(Save::starting_world, layers);
        world_folder.write_settings(settings);
        World world(save, world_folder, c, settings, nothing);
        world.load(40, 40, 41, 40);
        world.put(40, 40, cell("base:timber"));
        Player p; p.x = 40; p.y = 39;
        world.save_changes();
        save.write_player(p);
    }
    auto chunk = folder / "origin" / "regions" / "x0-y0" / "x0-y0.json";
    auto world_file = folder / "origin" / "world.json";
    auto text = read(chunk);
    require(text.find("\"rear\"") == std::string::npos, "an empty layer is simply absent");
    // Hand edits: the timber becomes stone, and unfamiliar properties appear everywhere.
    write(chunk, replaced(replaced(text, "{\"block\": \"base:timber\"}", "{\"block\": \"base:stone\", \"crafting\": {\"quantity\":2}, \"moss\": 3}"),
                          "\"palette\"", "\"weather\": {\"rain\": true},\n  \"palette\""));
    write(world_file, replaced(read(world_file), "\"seed\"", "\"lighting\": \"soft\", \"seed\""));
    // A minimal player: everything optional is absent.
    write(folder / "player.json", "{\"x\": 40, \"y\": 39, \"mood\": \"glad\"}");
    auto world_text = read(world_file);
    auto edited_chunk = read(chunk);
    {
        Save save(folder);
        auto p = *save.player();
        auto world_folder = save.world(p.world, layers);
        World world(save, world_folder, c, *world_folder.settings(), nothing);
        world.load(40, 40, 41, 40);
        require(world.cell(40, 40).id == "base:stone" && world.cell(40, 40).properties["crafting"]["quantity"] == Value(2),
                "a hand-edited block loads as edited");
        require(world.block(40, 40, 1).empty(), "an absent layer means an empty layer");
        require(p.world == Save::starting_world && p.facing == 1 && p.held.empty(), "absent world, facing and hand take their defaults");
        require(!world.save_changes() && !save.write_player(p), "loading writes nothing back: no migration, no version to advance");
    }
    require(read(world_file) == world_text && read(chunk) == edited_chunk, "unchanged files stay byte for byte as edited");
    // Rewrite every file: a changed chunk, a moved player, the world settings.
    auto rewrite = [&](double step) {
        Save save(folder);
        auto world_folder = save.world(Save::starting_world, layers);
        world_folder.write_settings(*world_folder.settings());
        World world(save, world_folder, c, *world_folder.settings(), nothing);
        world.load(40, 40, 41, 40);
        auto p = *save.player();
        p.x += step;
        world.set(41, 40, step > 0 ? "base:sand" : "base:soil");
        require(world.save_changes() && save.write_player(p), "a change is written");
    };
    rewrite(1);
    auto kept = [&](const std::filesystem::path& file, const std::string& property) {
        require(read(file).find(property) != std::string::npos, file.filename().string() + " keeps " + property + " through a rewrite");
    };
    kept(chunk, "{\"block\": \"base:stone\", \"crafting\": {\"quantity\":2}, \"moss\": 3}");
    kept(chunk, "\"weather\": {\"rain\": true}");
    kept(world_file, "\"lighting\": \"soft\"");
    kept(folder / "player.json", "\"mood\": \"glad\"");
    auto first = read(chunk) + read(world_file) + read(folder / "player.json");
    rewrite(-1);
    rewrite(1);
    require(read(chunk) + read(world_file) + read(folder / "player.json") == first,
            "kept properties settle: rewriting the same state again gives the same files");
    {
        Save save(folder);
        bool refused = false;
        try { Save second(folder); } catch (const std::runtime_error&) { refused = true; }
        require(refused, "a second game cannot open a world that is already open");
    }
    write(chunk, replaced(read(chunk), "\"foreground\": [", "\"foreground\": [[0],"));
    bool rejected = false;
    try { Save save(folder); save.world(Save::starting_world, layers).chunk(0, 0); } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("x0-y0.json") != std::string::npos;
    }
    require(rejected, "a damaged chunk is refused by name, never regenerated");
}

// Earlier worlds are refused by name, never misread.
void retired(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder / "chunks");
    write(folder / "world.json", "{\"seed\": 1, \"columns\": 32, \"rows\": 8, \"chunk_size\": 32}");
    auto refusal = [](const std::filesystem::path& f) {
        try { Save save(f); } catch (const std::runtime_error& error) { return std::string(error.what()); }
        return std::string();
    };
    require(refusal(folder).find("0.0.6 or earlier") != std::string::npos, "a world from 0.0.6 is refused, saying so");
    require(std::filesystem::exists(folder / "world.json") && !std::filesystem::exists(folder / "player.json"), "and left as it was");
    std::filesystem::create_directories(folder.parent_path() / "universe");
    {
        Save save(folder.parent_path() / "universe");
        save.world(Save::starting_world, layers).write_settings(Settings{});
    }
    require(refusal(folder.parent_path() / "universe" / "origin").find("open the save that holds it") != std::string::npos,
            "a world folder inside a save is not taken for a save");
}

}

int main() {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-engine-" + std::to_string(getpid()));
    try {
        std::filesystem::create_directories(scratch);
        round_trip(scratch / "round-trip");
        editing(scratch / "editing");
        retired(scratch / "old" / "world");
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
