#include "save.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <sqlite3.h>

using namespace yonder;
namespace {

void require(bool result, const std::string& what) {
    if (!result) throw std::runtime_error(what);
}

struct Scratch {
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("yonderest-test-" + std::to_string(getpid()));
    Scratch() { std::filesystem::create_directories(path); }
    ~Scratch() { std::filesystem::remove_all(path); }
};

Settings settings(const std::filesystem::path& base) {
    Settings s;
    s.seed = 42;
    s.terrain = load_terrain(base);
    return s;
}

void terrain(const Settings& s) {
    for (int x = -100; x < s.width() + 100; ++x) {
        require(surface(s, x) == surface(s, x + s.width()), "surface must be periodic");
        require(std::abs(surface(s, x) - surface(s, x + 1)) <= 1, "ordinary terrain, including the seam, must be walkable");
    }
    for (int y = 0; y < s.rows; ++y) {
        auto a = generate(s, 0, y), b = generate(s, s.columns, y);
        require(a.tiles == b.tiles, "all generated fields must wrap, including trees and caves");
        generate(s, s.columns - 1, y);
        require(a.tiles == generate(s, 0, y).tiles, "generation cannot depend on exploration order");
    }
    auto other = s;
    ++other.seed;
    bool different = false;
    for (int x = 0; x < s.width(); ++x) different |= surface(s, x) != surface(other, x);
    require(different, "seed must affect terrain");
}

void physics_and_actions(const std::filesystem::path& path, const Content& content, const Settings& s) {
    Save save(path);
    save.initialize(s);
    World world(save, content, s);
    int h = 30;
    for (int x = -12; x <= 12; ++x) {
        for (int y = h - 8; y < h; ++y) world.set(x, y, {});
        world.set(x, h, s.terrain.soil);
    }
    Player p;
    p.x = s.width() - 1.2;
    p.y = h - player_height;
    p.selected = s.terrain.soil;
    for (int i = 0; i < 80; ++i) move(world, p, 1, false);
    require(p.x > 2 && p.x < 4, "walking must cross the world seam");
    require(std::abs(p.y - (h - player_height)) < 0.001, "standing collision must not drift");
    require(!intersects(world, p.x, p.y), "walking cannot enter solid terrain");
    double start = p.y;
    move(world, p, 0, true);
    for (int i = 0; i < 30; ++i) move(world, p, 0, false);
    require(p.y < start - 1, "jump must leave the floor");
    for (int i = 0; i < 180; ++i) move(world, p, 0, false);
    require(std::abs(p.y - start) < 0.001, "jump must land on the same floor");

    p.x = s.width() - 0.85;
    require(act(world, p, Action::mine, 0, h).ok, "mining must work through the seam");
    require(world.block(0, h).empty() && p.inventory[p.selected] == 1, "mining transfers exactly one block");
    require(act(world, p, Action::place, 0, h).ok, "placement must work through the seam");
    require(world.block(0, h) == p.selected && p.inventory[p.selected] == 0, "placement consumes exactly one block");
    require(!act(world, p, Action::mine, 40, h).ok, "cannot gather out of reach");
    p.inventory[p.selected] = 2;
    require(!act(world, p, Action::place, -1, h - 1).ok, "cannot place inside the player");
    world.set(1, h - 1, s.terrain.stone);
    world.set(2, h - 1, s.terrain.stone);
    require(!act(world, p, Action::mine, 2, h - 1).ok, "cannot mine through a wall");
    world.set(1, h - 1, {});
    world.set(2, h - 1, {});
    require(!act(world, p, Action::place, 2, h - 4).ok, "unsupported placement must be refused");

    p.x = 5;
    world.set(6, h - 1, s.terrain.stone);
    for (int i = 0; i < 40; ++i) move(world, p, 1, false);
    require(p.x > 6, "walk must climb a one-block step");
    world.set(9, h - 1, s.terrain.stone);
    world.set(9, h - 2, s.terrain.stone);
    for (int i = 0; i < 80; ++i) move(world, p, 1, false);
    require(p.x + player_width <= 9.001, "a two-block wall must stop walking");
    require(!intersects(world, p.x, p.y), "wall collision cannot embed the player");
    world.checkpoint(p);
}

void shelter(const std::filesystem::path& path, const Content& content, const Settings& s) {
    int home = -1, ground = 0;
    Player before;
    std::map<std::pair<int, int>, Block> built;
    {
        Save save(path);
        save.initialize(s);
        World world(save, content, s);
        Player p = spawn(world);
        require(p.inventory.empty(), "new player must gather their own materials");
        // Use a natural clearing, not a hand-prepared building fixture.
        for (int x = 12; x < s.width() - 12 && home < 0; ++x) {
            int h = surface(s, x);
            bool clear = true;
            for (int dx = -5; dx <= 5; ++dx) {
                clear &= surface(s, x + dx) == h;
                for (int y = h - 4; y < h; ++y) clear &= world.block(x + dx, y).empty();
            }
            if (clear) { home = x; ground = h; }
        }
        require(home >= 0, "seed 42 must offer a natural shelter clearing");

        // A small open quarry: move alongside it, mine the top layer, then the
        // exposed layer beneath. Every building block comes through act().
        std::vector<int> quarry;
        for (int x = home - 40; x < home - 6 && quarry.size() < 7; ++x) {
            int h = surface(s, x);
            if (world.block(x, h - 1).empty() && world.block(x, h - 2).empty()) quarry.push_back(x);
        }
        require(quarry.size() == 7, "find an unobstructed quarry");
        for (int depth = 0; depth < 3; ++depth) {
            for (int x : quarry) {
                int h = surface(s, x);
                p.x = x + 0.15;
                p.y = h + depth - player_height;
                Block id = world.block(x, h + depth);
                require(!id.empty(), "quarry must contain a block");
                auto result = act(world, p, Action::mine, x, h + depth);
                require(result.ok, "gather quarry: " + result.message);
            }
        }
        p.x = home + 0.15;
        p.y = ground - player_height;
        // Three-block interior; two-block doorway in the right-hand side.
        auto place = [&](int x, int y) {
            auto found = std::find_if(p.inventory.begin(), p.inventory.end(), [](const auto& entry) { return entry.second > 0; });
            require(found != p.inventory.end(), "quarry must supply the shelter");
            p.selected = found->first;
            auto result = act(world, p, Action::place, x, y);
            require(result.ok, "build shelter: " + result.message + " at " + std::to_string(x) + "," + std::to_string(y));
            built[{x, y}] = p.selected;
        };
        for (int y = ground - 1; y >= ground - 3; --y) place(home - 3, y);
        // Jump to see over the wall when placing the first roof corner.
        move(world, p, 0, true);
        for (int i = 0; i < 30; ++i) move(world, p, 0, false);
        place(home - 3, ground - 4);
        for (int i = 0; i < 180; ++i) move(world, p, 0, false);
        for (int x = home - 2; x <= home + 3; ++x) {
            if (x > home + 1) for (int i = 0; i < 20; ++i) move(world, p, 1, false);
            place(x, ground - 4);
        }
        place(home + 3, ground - 3);
        int remaining = 0;
        for (const auto& [id, count] : p.inventory) remaining += count;
        require(remaining + int(built.size()) == 21, "shelter must conserve gathered blocks");
        before = p;
        // Leave in both axes before checkpointing, forcing dirty chunk eviction.
        world.retain(400, 0, 445, 24);
        require(world.loaded_chunks() <= 6, "loading must be bounded, not full-height columns");
        world.checkpoint(p);
    }
    {
        Save save(path);
        auto loaded = save.settings();
        require(loaded && loaded->seed == s.seed && loaded->terrain.soil == s.terrain.soil, "world configuration must persist");
        auto p = save.player();
        require(p && p->x == before.x && p->y == before.y && p->inventory == before.inventory
                && p->selected == before.selected, "player and inventory must resume exactly");
        auto changed = *loaded;
        ++changed.seed;
        World world(save, content, changed);
        for (const auto& [where, id] : built)
            require(world.block(where.first, where.second) == id, "saved shelter must override even a changed generator seed");
        require(world.block(home + 3, ground - 1).empty(), "doorway must stay open after reload");
        require(world.block(home + 3, ground - 2).empty(), "doorway must be tall enough");
    }
}

void interrupted_save(const std::filesystem::path& path, const Content& content, const Settings& s) {
    Player original;
    original.x = 4;
    original.y = 12;
    original.selected = s.terrain.soil;
    {
        Save save(path);
        save.initialize(s);
        World world(save, content, s);
        world.set(4, 14, s.terrain.soil);
        world.checkpoint(original);
    }
    pid_t child = fork();
    require(child >= 0, "fork failed");
    if (child == 0) {
        Save save(path);
        World world(save, content, s);
        world.set(4, 14, {});
        world.retain(500, 150, 540, 180);
        // Actual process death, without destructors or a final checkpoint.
        _exit(0);
    }
    int status;
    waitpid(child, &status, 0);
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "interrupted writer failed");
    Save save(path);
    World world(save, content, s);
    require(world.block(4, 14) == s.terrain.soil, "interrupted eviction must not commit a mined block");
    require(save.player()->inventory.empty(), "interrupted save must retain matching inventory");
    require(!save.chunk(16, 5), "uncheckpointed generated chunks must roll back too");
}

void missing_content(const std::filesystem::path& path, Content content, const Settings& s, const std::filesystem::path& scratch) {
    auto pack = scratch / "extension";
    std::filesystem::create_directory(pack);
    std::ofstream(pack / "blocks.txt") << "demo:amber 220 146 52 1 \"Amber\"\n";
    content.load(pack);
    require(content.material("demo:amber").name == "Amber", "extension must use the ordinary content path");
    {
        Save save(path);
        save.initialize(s);
        World world(save, content, s);
        world.set(0, 12, "demo:amber");
        world.set(1, 12, "demo:amber");
        Player p;
        p.x = 1.5;
        p.y = 10;
        p.selected = "demo:amber";
        world.checkpoint(p);
    }
    content.blocks.erase("demo:amber");
    {
        Save save(path);
        World world(save, content, s);
        auto p = *save.player();
        require(world.block(0, 12) == "demo:amber" && world.solid(0, 12), "missing block must retain identity and ordinary collision");
        require(act(world, p, Action::mine, 1, 12).ok, "missing block must remain mineable");
        require(p.inventory["demo:amber"] == 1, "mining unknown content must retain its ID");
        world.checkpoint(p);
    }
    content.load(pack);
    {
        Save save(path);
        World world(save, content, s);
        require(world.content.material(world.block(0, 12)).name == "Amber", "restoring pack must restore interpretation");
        require(world.block(1, 12).empty(), "restoring pack cannot resurrect mined blocks");
    }
}

void damaged_chunk(const std::filesystem::path& path, const Content& content, const Settings& s) {
    {
        Save save(path);
        save.initialize(s);
        World world(save, content, s);
        world.block(0, 0);
        world.checkpoint(spawn(world));
    }
    sqlite3* db;
    require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "open corruption fixture");
    require(sqlite3_exec(db, "UPDATE chunks SET tiles='2 \"base:soil\"' WHERE x=0 AND y=0", nullptr, nullptr, nullptr) == SQLITE_OK,
            "damage chunk fixture");
    sqlite3_close(db);
    bool refused = false;
    try {
        Save save(path);
        World world(save, content, *save.settings());
        world.block(0, 0);
    } catch (const std::runtime_error&) { refused = true; }
    require(refused, "invalid saved chunk must fail, never silently regenerate");
}

}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "expected content directory");
        Scratch scratch;
        Content content;
        content.load(argv[1]);
        auto s = settings(argv[1]);
        terrain(s);
        physics_and_actions(scratch.path / "physics.sqlite3", content, s);
        shelter(scratch.path / "shelter.sqlite3", content, s);
        interrupted_save(scratch.path / "interrupted.sqlite3", content, s);
        missing_content(scratch.path / "missing.sqlite3", content, s, scratch.path);
        damaged_chunk(scratch.path / "damaged.sqlite3", content, s);
        std::cout << "Shelter survives eviction and restart; wrapping, gameplay, interruption, and missing content verified.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
