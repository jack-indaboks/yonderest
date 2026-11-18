#include "save.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

using namespace yonder;
namespace {

void require(bool result, const std::string& what) {
    if (!result) throw std::runtime_error(what);
}

Settings settings(const std::filesystem::path& root) {
    Settings s;
    s.seed = 42;
    s.terrain = load_terrain(root / "expedition");
    return s;
}

void ground(World& world) {
    for (int x = -8; x < 40; ++x) {
        for (int y = 22; y < 30; ++y) world.set(x, y, {});
        world.set(x, 30, world.settings.terrain.stone);
    }
}

ActionResult place(World& world, Player& p, int x, int y, const Block& id) {
    p.selected = id;
    p.x = wrap(x + 0.15, double(world.settings.width()));
    p.y = y - player_height;
    auto result = act(world, p, Action::place, x, y);
    require(result.ok, "place " + id + " at " + std::to_string(x) + "," + std::to_string(y) + ": " + result.message);
    return result;
}

void mine(World& world, Player& p, int x, int y) {
    p.x = wrap(x + 0.15, double(world.settings.width()));
    p.y = y - player_height;
    auto result = act(world, p, Action::mine, x, y);
    require(result.ok, "mine: " + result.message);
}

void basic_workshop(const std::filesystem::path& path, const Content& content, const Settings& s) {
    Save save(path);
    save.initialize(s);
    World world(save, content, s);
    ground(world);
    Player p;
    p.inventory[s.terrain.timber] = 8;
    for (int x : {10, 15}) {
        place(world, p, x, 29, s.terrain.timber);
        place(world, p, x + 1, 29, s.terrain.timber);
        place(world, p, x, 28, s.terrain.timber);
        auto result = place(world, p, x + 1, 28, s.terrain.timber);
        require(result.message == "Made Planks x4", "complete the physical timber pattern automatically");
        mine(world, p, x, 29);
    }
    require(p.inventory["workshop:planks"] == 8 && p.inventory[s.terrain.timber] == 0, "craft outputs conserve the specified recipe amounts");
    for (auto [x, y] : std::vector<std::pair<int, int>>{{10, 29}, {12, 29}, {10, 28}, {11, 28}, {12, 28}})
        place(world, p, x, y, "workshop:planks");
    require(world.block(10, 29) == "workshop:workbench" && p.inventory["workshop:planks"] == 3, "assemble the workbench itself");
    p.inventory[s.terrain.stone] = 7;
    p.inventory[s.terrain.timber] = 1;
    place(world, p, 19, 29, s.terrain.timber);
    for (auto [x, y] : std::vector<std::pair<int, int>>{{18, 29}, {20, 29}, {18, 28}, {20, 28}, {18, 27}, {19, 27}, {20, 27}})
        place(world, p, x, y, s.terrain.stone);
    require(world.block(19, 29) == "workshop:forge", "assemble a forge without needing a forge first");
}

void batching(const std::filesystem::path& path, const Content& content, const Settings& s) {
    {
        Save save(path);
        save.initialize(s);
        World world(save, content, s);
        ground(world);
        world.put(10, 30, Cell{"workshop:forge", 1, true});
        world.put(11, 30, Cell{"workshop:workbench", 1, true, 10});
        Player p;
        p.inventory[s.terrain.timber] = 10;
        p.inventory[s.terrain.sand] = 20;
        place(world, p, 10, 29, s.terrain.timber);
        require(world.cell(10, 29).quantity == 10 && p.inventory[s.terrain.timber] == 0, "each bulk placement consumes N units");
        p.x = 12.15; p.y = 28.3;
        require(adjust_workbench(world, p, 11, 30).ok && world.cell(11, 30).batch == 1, "change bench setting during assembly");
        place(world, p, 10, 28, s.terrain.sand);
        require(world.cell(10, 28).quantity == 10, "unfinished assembly keeps its original N, not the bench's new setting");
        world.retain(400, 150, 440, 180);
        world.checkpoint(p);
    }
    pid_t child = fork();
    require(child >= 0, "fork interrupted craft writer");
    if (child == 0) {
        Save save(path);
        World world(save, content, *save.settings());
        auto p = *save.player();
        place(world, p, 11, 28, s.terrain.sand);
        world.retain(400, 150, 440, 180);
        _exit(0);
    }
    int status;
    waitpid(child, &status, 0);
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "interrupted crafting process");
    {
        Save save(path);
        World world(save, content, *save.settings());
        auto p = *save.player();
        require(world.cell(10, 29).quantity == 10 && world.cell(10, 28).quantity == 10, "unfinished batch survives eviction and restart");
        require(!p.inventory.contains("workshop:glass"), "interrupted craft rolls terrain and output inventory back together");
        auto result = place(world, p, 11, 28, s.terrain.sand);
        require(result.message == "Made Glass x40", "one shared forge recipe works in bulk");
        require(world.block(10, 29) == "workshop:glass" && p.inventory["workshop:glass"] == 39, "place one result and inventory the remaining outputs");
        require(world.block(10, 30) == "workshop:forge" && world.cell(11, 30).batch == 1, "both working surfaces remain untouched");
        mine(world, p, 10, 29);
        require(p.inventory["workshop:glass"] == 40, "all batch output is recoverable");
        world.put(11, 30, Cell{"workshop:workbench", 1, true, 5});
        p.inventory[s.terrain.timber] = 4;
        auto before = p.inventory;
        p.x = 10.15; p.y = 27.3; p.selected = s.terrain.timber;
        auto refused = act(world, p, Action::place, 10, 29);
        require(!refused.ok && world.block(10, 29).empty() && p.inventory == before, "insufficient batch ingredients do not partially debit inventory");
        p.inventory[s.terrain.timber] = 5;
        p.inventory[s.terrain.sand] = 10;
        place(world, p, 10, 29, s.terrain.timber);
        place(world, p, 10, 28, s.terrain.sand);
        mine(world, p, 10, 28);
        mine(world, p, 10, 29);
        require(p.inventory[s.terrain.timber] == 5 && p.inventory[s.terrain.sand] == 10, "dismantling an unfinished batch refunds its full quantities");
        world.checkpoint(p);
    }
}

void missing_workshop(const std::filesystem::path& path, Content content, const Settings& s) {
    content.modules.erase("workshop");
    content.recipes.clear();
    std::erase_if(content.blocks, [](const auto& entry) { return entry.first.starts_with("workshop:"); });
    content.validate();
    {
        Save save(path);
        World world(save, content, s);
        auto p = *save.player();
        require(world.block(11, 30) == "workshop:workbench" && world.cell(11, 30).batch == 5,
                "disabled workshop retains station ID and setting");
        require(world.solid(11, 30) && world.content.material(world.block(11, 30)).station.empty(), "missing station behaves like ordinary fallback terrain");
        mine(world, p, 10, 30);
        require(p.inventory["workshop:forge"] == 1, "missing forge can be deliberately gathered without erasing identity");
        world.checkpoint(p);
    }
}

void support_and_provenance(const std::filesystem::path& path, const Content& content, const Settings& s) {
    Save save(path);
    save.initialize(s);
    World world(save, content, s);
    ground(world);
    Player p;
    p.inventory[s.terrain.timber] = 30;
    p.inventory[s.terrain.sand] = 50;
    world.put(12, 30, Cell{"workshop:workbench", 1, true, 10});
    world.put(10, 30, Cell{"workshop:forge", 1, true});
    place(world, p, 10, 29, s.terrain.timber);
    place(world, p, 10, 28, s.terrain.sand);
    auto result = place(world, p, 11, 28, s.terrain.sand);
    require(result.message == "Made Glass x4", "a nearby bench outside the shape must not grant batching");
    mine(world, p, 10, 29);
    world.set(12, 30, s.terrain.stone);
    world.set(10, 30, s.terrain.stone);
    world.put(11, 30, Cell{"workshop:workbench", 1, true, 5});
    place(world, p, 10, 29, s.terrain.timber);
    place(world, p, 10, 28, s.terrain.sand);
    place(world, p, 11, 28, s.terrain.sand);
    require(world.cell(10, 29).quantity == 5, "without a forge, ingredients remain assembled and unconsumed");
    require(!craft(world, p, 11, 28), "missing required work surface prevents transformation");
    // Replace the ordinary support with a forge, reaching from a dug-out side.
    for (int y = 29; y <= 32; ++y) world.set(9, y, {});
    world.set(9, 33, s.terrain.stone);
    p.x = 9.15; p.y = 31.3;
    require(act(world, p, Action::mine, 10, 30).ok, "remove old support from the side");
    p.inventory["workshop:forge"] = 1; p.selected = "workshop:forge";
    result = act(world, p, Action::place, 10, 30);
    require(result.ok && result.message == "Made Glass x20", "placing the final required surface also completes the pattern");
    require(world.block(10, 30) == "workshop:forge" && world.block(11, 30) == "workshop:workbench", "forge and bench both remain");

    // Natural terrain with the right IDs is not an assembled ingredient.
    world.set(20, 29, s.terrain.timber);
    world.set(21, 29, s.terrain.timber);
    world.set(20, 28, s.terrain.timber);
    result = place(world, p, 21, 28, s.terrain.timber);
    require(world.block(20, 29) == s.terrain.timber && result.message == "Placed", "do not consume natural terrain or unmarked legacy buildings");
}

void seam_and_extension(const std::filesystem::path& path, Content content, const Settings& s, const std::filesystem::path& scratch) {
    auto pack = scratch / "pack";
    std::filesystem::create_directory(pack);
    std::ofstream(pack / "blocks.txt") << "demo:tile 180 190 210 1 \"Tile\"\n";
    std::ofstream(pack / "recipes.txt") << "recipe demo:tile demo:tile 2 0 0\nat 0 0 base:stone\nat 1 0 base:sand\nend\n";
    content.load(pack);
    content.validate();
    Save save(path);
    save.initialize(s);
    World world(save, content, s);
    ground(world);
    Player p;
    p.inventory[s.terrain.stone] = 1; p.inventory[s.terrain.sand] = 1;
    place(world, p, -1, 29, s.terrain.stone);
    auto result = place(world, p, 0, 29, s.terrain.sand);
    require(result.message == "Made Tile x2" && world.block(s.width() - 1, 29) == "demo:tile",
            "extension recipes use the same engine and match across the world seam");
    require(p.inventory["demo:tile"] == 1, "extension recipe output amount");
}

}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-crafting-" + std::to_string(getpid()));
    try {
        require(argc == 2, "expected content root");
        std::filesystem::create_directories(scratch);
        std::filesystem::path root = argv[1];
        Content content;
        for (const auto& [id, version] : load_manifest(root / "flagship.txt").packs) content.load(root / id);
        content.validate();
        auto s = settings(root);
        basic_workshop(scratch / "basic.sqlite3", content, s);
        batching(scratch / "batch.sqlite3", content, s);
        missing_workshop(scratch / "batch.sqlite3", content, s);
        {
            Save save(scratch / "batch.sqlite3");
            World world(save, content, s);
            require(world.content.material(world.block(11, 30)).station == "workbench" && world.cell(11, 30).batch == 5,
                    "restoring workshop restores its unmodified bench setting");
            require(world.block(10, 30).empty(), "restoring workshop does not resurrect mined stations");
        }
        support_and_provenance(scratch / "support.sqlite3", content, s);
        seam_and_extension(scratch / "extension.sqlite3", content, s, scratch);
        std::filesystem::remove_all(scratch);
        std::cout << "Spatial assembly, retained supports, batches, refunds, resume, provenance, and extension recipes work.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
