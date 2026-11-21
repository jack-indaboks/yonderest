#include "save.hpp"

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

// Everything a save holds survives writing and reading, including values JSON tools often mangle.
void round_trip(const std::filesystem::path& folder, Settings settings) {
    settings.seed = 1795207967560012382ULL; // Beyond 2^53: a double would round it.
    Player before;
    before.x = 305.21666666685525; before.y = 115;
    before.facing = 0;
    before.inventory = {{"base:soil", 7}, {"workshop:pick", 1}, {"missing:crystal", 3}};
    before.selected = "base:soil"; before.last_selected = "workshop:pick";
    before.ready = {"workshop:pick", "base:soil"};
    Content content;
    {
        Save save(folder);
        save.write_settings(settings);
        World world(save, content, settings);
        world.put(40, 100, Cell{"base:timber", 5, true});
        world.put(41, 100, Cell{"workshop:workbench", 1, true, 10});
        world.put(40, 100, Cell{"missing:wall", 1, true}, Layer::background);
        require(world.save_changes(before), "a first save writes");
        require(!world.save_changes(before), "nothing is written when nothing changed");
    }
    Save save(folder);
    auto s = *save.settings();
    require(s.seed == settings.seed && s.columns == settings.columns && s.rows == settings.rows
                && s.terrain.version == settings.terrain.version && s.terrain.ore == settings.terrain.ore,
            "world settings survive, the seed exactly");
    auto p = *save.player();
    require(p.x == before.x && p.y == before.y && p.facing == 0 && p.inventory == before.inventory
                && p.selected == before.selected && p.last_selected == before.last_selected && p.ready == before.ready,
            "the player survives exactly, missing content's identity included");
    World world(save, content, s);
    require(world.cell(40, 100) == Cell{"base:timber", 5, true} && world.cell(41, 100).batch == 10
                && world.cell(40, 100, Layer::background) == Cell{"missing:wall", 1, true},
            "both layers keep unfinished quantities, placement, batch settings and unknown IDs");
    require(save.chunk(1, 3).has_value() && !save.chunk(9, 3).has_value(),
            "a visited chunk is saved; an unvisited one is not, and stays ungenerated");
}

// What a person editing the files meets: absence, unfamiliar additions, a changed block.
void editing(const std::filesystem::path& folder, const Settings& settings) {
    Content content;
    {
        Save save(folder);
        save.write_settings(settings);
        World world(save, content, settings);
        world.put(40, 40, Cell{"base:timber", 1, true}); // open sky
        Player p; p.x = 40; p.y = 39; p.inventory["base:soil"] = 2;
        world.save_changes(p);
    }
    auto chunk = folder / "chunks" / "x1-y1.json";
    auto text = read(chunk);
    require(text.find("\"background\"") == std::string::npos, "an empty layer is simply absent");
    require(text.find("{\"block\": \"base:timber\", \"placed\": true}") != std::string::npos, "a palette entry names its block and only what differs");
    // Hand edits: the timber becomes stone, and unfamiliar properties appear everywhere.
    write(chunk, replaced(replaced(text, "{\"block\": \"base:timber\", \"placed\": true}",
                                   "{\"block\": \"base:stone\", \"placed\": true, \"moss\": 3}"),
                          "\"generation\"", "\"weather\": {\"rain\": true},\n  \"generation\""));
    write(folder / "world.json", replaced(read(folder / "world.json"), "\"seed\"", "\"lighting\": \"soft\", \"seed\""));
    // A minimal player: everything optional is absent.
    write(folder / "player.json", "{\"x\": 40, \"y\": 39, \"inventory\": {\"base:soil\": 2, \"base:sand\": 0}, \"mood\": \"glad\"}");
    auto world_text = read(folder / "world.json");
    auto edited_chunk = read(chunk);
    {
        Save save(folder);
        World world(save, content, *save.settings());
        auto p = *save.player();
        require(world.cell(40, 40) == Cell{"base:stone", 1, true}, "a hand-edited block loads as edited");
        require(world.block(40, 40, Layer::background).empty(), "absent background means empty background");
        require(p.facing == 1 && p.selected.empty() && p.ready.empty() && p.inventory == std::map<Block, std::int64_t>{{"base:soil", 2}},
                "absent facing, hand and readiness take their defaults; an emptied entry is not carried");
        require(!world.save_changes(p), "loading writes nothing back: no migration, no version to advance");
    }
    require(read(folder / "world.json") == world_text && read(chunk) == edited_chunk, "unchanged files stay byte for byte as edited");
    {
        Save save(folder);
        World world(save, content, *save.settings());
        auto p = *save.player();
        world.set(41, 40, "base:sand");
        world.save_changes(p);
    }
    require(read(chunk).find("base:stone") != std::string::npos, "resaving a changed chunk keeps the hand edit");
    {
        Save save(folder);
        bool refused = false;
        try { Save second(folder); } catch (const std::runtime_error&) { refused = true; }
        require(refused, "a second game cannot open a world that is already open");
    }
    write(chunk, replaced(read(chunk), "\"foreground\": [", "\"foreground\": [[0],"));
    bool rejected = false;
    try { Save save(folder); save.chunk(1, 1); } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("x1-y1.json") != std::string::npos;
    }
    require(rejected, "a damaged chunk is refused by name, never regenerated");
}

// From expedition-v3 a new tree's trunk stands in the background; saved trees keep theirs.
void trunks(const std::filesystem::path& folder, Settings v3) {
    auto v2 = v3;
    v2.terrain.version = "expedition-v2";
    Content content;
    // Trees in two different chunk columns.
    std::vector<int> trees;
    {
        Chunk probe;
        for (int x = 0; x < v2.width() && trees.size() < 2; ++x) {
            int y = surface(v2, x) - 1;
            probe = generate(v2, x / chunk_size, y / chunk_size);
            if (probe.at(x % chunk_size, y % chunk_size).id == v2.terrain.timber
                && (trees.empty() || trees[0] / chunk_size != x / chunk_size)) trees.push_back(x);
        }
    }
    require(trees.size() == 2, "fixture: two trees in different chunks");
    int old_tree = trees[0], new_tree = trees[1];
    {
        Save save(folder);
        save.write_settings(v2);
        World world(save, content, v2);
        world.block(old_tree, surface(v2, old_tree) - 1);
        world.save_changes({});
    }
    Save save(folder);
    save.write_settings(v3);
    World world(save, content, v3);
    int th = surface(v3, old_tree);
    require(world.block(old_tree, th - 1) == v3.terrain.timber && world.block(old_tree, th - 1, Layer::background).empty(),
            "a saved tree keeps its trunk in front");
    th = surface(v3, new_tree);
    for (int y = th - 4; y < th; ++y)
        require(world.block(new_tree, y, Layer::background) == v3.terrain.timber, "a new trunk stands behind");
    require(world.block(new_tree, th - 1).empty() && world.block(new_tree, th - 2).empty(), "the character can walk past a new trunk");
    require(world.block(new_tree, th - 3) == v3.terrain.leaves && world.block(new_tree, th - 4) == v3.terrain.leaves,
            "the canopy stays in front of the trunk's top");
}

// Natural backing in new territory; explored territory keeps its empty background.
void backing(const std::filesystem::path& folder, Settings v1, const Settings& v3) {
    v1.terrain.version = "expedition-v1";
    int x = 40, h = surface(v1, x);
    Content content;
    {
        Save save(folder);
        save.write_settings(v1);
        World world(save, content, v1);
        world.block(x, h + 3);
        world.put(x, h - 1, Cell{"base:timber", 1, true}, Layer::background); // an old rear wall
        world.save_changes({});
    }
    int far = 20 * chunk_size + 7, far_h = surface(v3, far);
    {
        Save save(folder);
        save.write_settings(v3);
        World world(save, content, v3);
        require(world.block(x, h + 3, Layer::background).empty() && world.block(x, h - 1, Layer::background) == "base:timber",
                "explored territory keeps its empty and built background cells");
        require(world.block(far, far_h - 1, Layer::background).empty() || world.block(far, far_h - 1, Layer::background) == v3.terrain.timber,
                "open sky above new ground, unless a trunk stands there");
        require(!world.block(far, far_h + 3, Layer::background).empty(), "new territory has natural backing underground");
        require(world.block(far, far_h + 3, Layer::background) == world.block(far, far_h + 3), "backing matches its foreground host material");
        Player p; p.x = far; p.y = far_h - 1;
        auto host = world.block(far, far_h + 3);
        require(gather(world, p, far, far_h + 3).ok && gather(world, p, far, far_h + 3, Layer::background).ok,
                "foreground, then exposed backing, can be gathered");
        require(p.inventory.at(host) == 2, "both layers are actual recoverable material");
        world.save_changes(p);
    }
    Save save(folder);
    World world(save, content, *save.settings());
    require(world.block(far, far_h + 3).empty() && world.block(far, far_h + 3, Layer::background).empty(),
            "the opening stays empty after restart; no further layer is generated behind it");
}

// Old territory sets boundary conditions only for the new chunks that touch it.
void boundaries(const std::filesystem::path& folder, const Settings& old, const Settings& updated) {
    Content content;
    {
        Save save(folder);
        save.write_settings(old);
        World world(save, content, old);
        for (int cx : {0, 1}) world.block(cx * chunk_size, 3 * chunk_size);
        world.save_changes({});
    }
    std::vector<std::uint8_t> next;
    {
        Save save(folder);
        save.write_settings(updated);
        World world(save, content, updated);
        world.block(2 * chunk_size, 3 * chunk_size);
        world.block(3 * chunk_size, 3 * chunk_size);
        world.block(6 * chunk_size, 3 * chunk_size);
        world.save_changes({});
        auto touching = save.chunk(2, 3)->settling;
        require(touching.size() == chunk_size * chunk_size, "a chunk touching old territory carries a transition");
        require(touching[16 * chunk_size] <= 1 && touching[16 * chunk_size + 31] >= 31 && touching[16 * chunk_size + 31] <= 33,
                "settling grows with distance from the old edge");
        next = save.chunk(3, 3)->settling;
        require(next.size() == chunk_size * chunk_size && next[16 * chunk_size] >= 32 && next[16 * chunk_size] <= 34,
                "the following chunk continues from its neighbor's edge, not from a global scan");
        require(save.chunk(6, 3)->settling.empty(), "a chunk with no transitional neighbors is fully settled");
    }
    Save save(folder);
    require(save.chunk(3, 3)->settling == next, "settling persists with the chunk");
}

}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-saves-" + std::to_string(getpid()));
    try {
        require(argc == 2, "expected content root");
        std::filesystem::create_directories(scratch);
        Settings old, updated;
        old.seed = updated.seed = 42;
        old.terrain = load_terrain(std::filesystem::path(argv[1]) / "base");
        updated.terrain = load_terrain(std::filesystem::path(argv[1]) / "expedition");
        require(updated.terrain.version == "expedition-v3", "the flagship expedition terrain puts trunks behind");
        round_trip(scratch / "round-trip", updated);
        editing(scratch / "editing", updated);
        trunks(scratch / "trunks", updated);
        backing(scratch / "backing", updated, updated);
        boundaries(scratch / "boundaries", old, updated);
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
