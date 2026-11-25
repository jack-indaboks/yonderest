#include "save.hpp"

#include <algorithm>
#include <climits>
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

// A save is a universe: the player at the top, each world in its own folder, chunks by region.
// Everything it holds survives writing and reading, including values JSON tools often mangle.
void round_trip(const std::filesystem::path& folder, Settings settings) {
    settings.seed = 9007199254740993ULL; // 2^53 + 1: a double would round it.
    Player before;
    before.world = Save::starting_world;
    before.x = 305.21666666685525; before.y = 115;
    before.facing = 0;
    before.inventory = {{"base:soil", 7}, {"workshop:pick", 1}, {"missing:crystal", 3}};
    before.selected = "base:soil"; before.last_selected = "workshop:pick";
    before.ready = {"workshop:pick", "base:soil"};
    Content content;
    {
        Save save(folder);
        auto world_folder = save.world(Save::starting_world);
        world_folder.write_settings(settings);
        World world(save, world_folder, content, settings);
        world.load(40, 100, 41, 100);
        world.put(40, 100, Cell{"base:timber", 5, true});
        world.put(41, 100, Cell{"workshop:workbench", 1, true, 10});
        world.put(40, 100, Cell{"missing:wall", 1, true}, Layer::background);
        require(world.save_changes(before), "a first save writes");
        require(!world.save_changes(before), "nothing is written when nothing changed");
    }
    require(std::filesystem::exists(folder / "player.json") && std::filesystem::exists(folder / "origin" / "world.json")
                && std::filesystem::exists(folder / "origin" / "regions" / "x0-y0" / "x0-y1.json"),
            "the player sits in the universe, the world in its folder, a chunk in its region's folder");
    Save save(folder);
    auto p = *save.player();
    auto world_folder = save.world(p.world);
    auto s = *world_folder.settings();
    require(s.seed == settings.seed && s.columns == settings.columns && s.rows == settings.rows
                && s.terrain.version == settings.terrain.version && s.terrain.ore == settings.terrain.ore,
            "world settings survive, the seed exactly");
    require(p.world == before.world && p.x == before.x && p.y == before.y && p.facing == 0 && p.inventory == before.inventory
                && p.selected == before.selected && p.last_selected == before.last_selected && p.ready == before.ready,
            "the player survives exactly, their world and missing content's identity included");
    World world(save, world_folder, content, s);
    world.load(40, 100, 41, 100);
    require(world.cell(40, 100) == Cell{"base:timber", 5, true} && world.cell(41, 100).batch == 10
                && world.cell(40, 100, Layer::background) == Cell{"missing:wall", 1, true},
            "both layers keep unfinished quantities, placement, batch settings and unknown IDs");
    require(world_folder.chunk(0, 1).has_value() && !world_folder.chunk(5, 1).has_value(),
            "a visited chunk is saved; an unvisited one is not, and stays ungenerated");
    auto far = world_folder;
    Chunk c;
    c.ground.assign(chunk_size, 70);
    far.write_chunk(region_size + 1, 2, c);
    require(std::filesystem::exists(folder / "origin" / "regions" / "x1-y0" / "x65-y2.json"), "chunk files gather by region");
}

// What a person editing the files meets: absence, unfamiliar additions, a changed block.
void editing(const std::filesystem::path& folder, const Settings& settings) {
    Content content;
    {
        Save save(folder);
        save.world(Save::starting_world).write_settings(settings);
        auto world_folder = save.world(Save::starting_world);
        World world(save, world_folder, content, settings);
        world.load(40, 40, 41, 40);
        world.put(40, 40, Cell{"base:timber", 1, true}); // open sky
        Player p; p.x = 40; p.y = 39; p.inventory["base:soil"] = 2;
        world.save_changes(p);
    }
    auto chunk = folder / "origin" / "regions" / "x0-y0" / "x0-y0.json";
    auto world_file = folder / "origin" / "world.json";
    auto text = read(chunk);
    require(text.find("\"background\"") == std::string::npos, "an empty layer is simply absent");
    require(text.find("{\"block\": \"base:timber\", \"placed\": true}") != std::string::npos, "a palette entry names its block and only what differs");
    // Hand edits: the timber becomes stone, and unfamiliar properties appear everywhere.
    write(chunk, replaced(replaced(text, "{\"block\": \"base:timber\", \"placed\": true}",
                                   "{\"block\": \"base:stone\", \"placed\": true, \"moss\": 3}"),
                          "\"generation\"", "\"weather\": {\"rain\": true},\n  \"generation\""));
    write(world_file, replaced(replaced(read(world_file), "\"seed\"", "\"lighting\": \"soft\", \"seed\""),
                                          "\"grass\"", "\"ice\": \"base:stone\", \"grass\""));
    // A minimal player: everything optional is absent.
    write(folder / "player.json", "{\"x\": 40, \"y\": 39, \"inventory\": {\"base:soil\": 2, \"base:sand\": 0}, \"mood\": \"glad\"}");
    auto world_text = read(world_file);
    auto edited_chunk = read(chunk);
    {
        Save save(folder);
        auto p = *save.player();
        auto world_folder = save.world(p.world);
        World world(save, world_folder, content, *world_folder.settings());
        world.load(40, 40, 41, 40);
        require(world.cell(40, 40).id == "base:stone" && world.cell(40, 40).placed, "a hand-edited block loads as edited");
        require(world.block(40, 40, Layer::background).empty(), "absent background means empty background");
        require(p.facing == 1 && p.selected.empty() && p.ready.empty() && p.inventory == std::map<Block, std::int64_t>{{"base:soil", 2}},
                "absent facing, hand and readiness take their defaults; an emptied entry is not carried");
        require(!world.save_changes(p), "loading writes nothing back: no migration, no version to advance");
    }
    require(read(world_file) == world_text && read(chunk) == edited_chunk, "unchanged files stay byte for byte as edited");
    // Rewrite every file: a changed chunk, a moved player, the world settings.
    auto rewrite = [&](double step) {
        Save save(folder);
        auto world_folder = save.world(Save::starting_world);
        world_folder.write_settings(*world_folder.settings());
        World world(save, world_folder, content, *world_folder.settings());
        world.load(40, 40, 41, 40);
        auto p = *save.player();
        p.x += step;
        world.set(41, 40, step > 0 ? "base:sand" : "base:soil");
        require(world.save_changes(p), "a change is written");
    };
    rewrite(1);
    auto kept = [&](const std::filesystem::path& file, const std::string& property) {
        require(read(file).find(property) != std::string::npos, file.filename().string() + " keeps " + property + " through a rewrite");
    };
    kept(chunk, "{\"block\": \"base:stone\", \"placed\": true, \"moss\": 3}");
    kept(chunk, "\"weather\": {\"rain\": true}");
    kept(world_file, "\"lighting\": \"soft\"");
    kept(world_file, "\"ice\": \"base:stone\"");
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
    try { Save save(folder); save.world(Save::starting_world).chunk(0, 0); } catch (const std::runtime_error& error) {
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
        save.world(Save::starting_world).write_settings(Settings{});
    }
    require(refusal(folder.parent_path() / "universe" / "origin").find("open the save that holds it") != std::string::npos,
            "a world folder inside a save is not taken for a save");
}

// Unchanged generation gives the same land whatever order it is explored in, and each chunk keeps its ground line.
void order(const std::filesystem::path& folder, const Settings& settings) {
    Content content;
    auto explore = [&](const std::string& name, std::vector<int> columns) {
        Save save(folder / name);
        auto world_folder = save.world(Save::starting_world);
        world_folder.write_settings(settings);
        World world(save, world_folder, content, settings);
        for (int cx : columns) world.load(cx * chunk_size, 64, cx * chunk_size, 127);
        world.save_changes({});
    };
    explore("east", {6, 7, 0, 1});
    explore("west", {1, 0, 7, 6});
    for (int cx : {6, 7, 0, 1})
        for (int cy : {1}) {
            auto name = std::filesystem::path("origin") / "regions" / "x0-y0" / ("x" + std::to_string(cx) + "-y" + std::to_string(cy) + ".json");
            require(read(folder / "east" / name) == read(folder / "west" / name), "exploration order does not change the land");
        }
    auto c = *Save(folder / "east").world(Save::starting_world).chunk(0, 1);
    for (int lx = 0; lx < chunk_size; ++lx) require(c.ground[lx] == ground(settings, lx), "a chunk keeps the ground line it was generated with");
}

// A chunk nobody generated, with its ground lower than the generator's and made of a material the
// generator never uses, is met by the land generated around it, within a chunk or two, beside it,
// across the wrap point, above and below.
void handmade(const std::filesystem::path& folder, const Settings& settings) {
    Content content;
    constexpr int floor = 110; // The hand-built ground; the generator's lies around 64 in this world.
    Save save(folder);
    auto world_folder = save.world(Save::starting_world);
    world_folder.write_settings(settings);
    {
        Chunk built;
        for (int ly = floor - chunk_size; ly < chunk_size; ++ly)
            for (int lx = 0; lx < chunk_size; ++lx) built.at(lx, ly) = Cell{"handmade:marble"};
        world_folder.write_chunk(0, 1, built);
    }
    World world(save, world_folder, content, settings);
    auto chunk = [&](int cx, int cy) {
        world.load(cx * chunk_size, cy * chunk_size, cx * chunk_size, cy * chunk_size);
        world.save_changes({});
        save.worker.wait();
        return *world_folder.chunk(wrap(cx, settings.columns), cy);
    };
    auto east = chunk(1, 1), farther = chunk(2, 1), west = chunk(-1, 1), above = chunk(0, 0), below = chunk(0, 2);
    require(std::abs(east.ground[0] - floor) <= 2 && std::abs(west.ground[chunk_size - 1] - floor) <= 2,
            "the land beside it meets its ground, across the wrap point too");
    require(east.ground[chunk_size - 1] != ground(settings, 2 * chunk_size - 1) || floor == ground(settings, 2 * chunk_size - 1),
            "fixture: the difference has not gone in one chunk");
    require(farther.ground[chunk_size - 1] == ground(settings, 3 * chunk_size - 1), "within two chunks it is the generator's own ground");
    require(std::ranges::all_of(above.ground, [](int g) { return g == floor; }) && std::ranges::all_of(below.ground, [](int g) { return g == floor; }),
            "above and below it the ground line is its own");
    auto marble = [](const Chunk& c, int from, int to) {
        int n = 0;
        for (int ly = 0; ly < chunk_size; ++ly)
            for (int lx = from; lx < to; ++lx) n += c.at(lx, ly).id == "handmade:marble";
        return n;
    };
    require(marble(east, 0, 8) > marble(east, 56, 64) && marble(east, 0, 8) > 0, "its material carries into the land beside it, fading");
    require(marble(farther, 32, 64) == 0, "and is gone two chunks away");
    require(marble(below, 0, chunk_size) > 0, "it carries downward too");
    for (int ly = 0; ly < chunk_size; ++ly)
        for (int lx = 0; lx < chunk_size; ++lx)
            require(east.at(lx, ly).id != "handmade:marble" || chunk_size + ly >= east.ground[lx], "it stays below the ground");
}

// Underground, the rear layer holds deposits of its own, a little richer than the foreground's.
void deposits(const Settings& settings) {
    Content content;
    int front = 0, rear = 0, hidden = 0;
    // Chunks spread around the whole world, so the sample crosses uplands and groves.
    for (int cx = 0; cx < settings.columns; cx += 16) {
        auto c = generate(settings, content, cx, 4);
        for (int i = 0; i < chunk_area; ++i) {
            const auto &f = c.tiles[i].id, &b = c.background[i].id;
            auto found = [&](const Block& id) { return id == settings.terrain.ore || id == settings.terrain.clay; };
            front += found(f);
            rear += found(b);
            hidden += found(b) && f == settings.terrain.stone;
        }
    }
    require(rear > front && front > 0, "the rear layer is a little richer than the foreground");
    require(hidden > rear / 4, "much of it is hidden behind ordinary stone");
}

}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-saves-" + std::to_string(getpid()));
    try {
        require(argc == 2, "expected content root");
        std::filesystem::create_directories(scratch);
        // A small world: eight chunks around and four tall.
        Settings small;
        small.seed = 42; small.columns = 8; small.rows = 4;
        small.terrain = load_terrain(std::filesystem::path(argv[1]) / "expedition");
        round_trip(scratch / "round-trip", small);
        auto tall = small;
        tall.rows = 8; // Its first row of chunks is open sky.
        editing(scratch / "editing", tall);
        retired(scratch / "old" / "world");
        order(scratch / "order", small);
        handmade(scratch / "handmade", small);
        auto starting = small;
        starting.columns = starting_columns; starting.rows = starting_rows;
        deposits(starting);
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
