#include "play.hpp"
#include "save.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <unistd.h>

using namespace yonder;
namespace {
void require(bool yes, const std::string& what) {
    if (!yes) throw std::runtime_error(what);
}

void clear(World& world) {
    for (int x = -10; x < 50; ++x) {
        for (int y = 20; y < 30; ++y) {
            world.set(x, y, {});
            world.set(x, y, {}, Layer::background);
        }
        world.set(x, 30, "base:soil");
    }
}

Player standing(double x = 10.15) {
    Player p;
    p.x = x; p.y = 29; p.grounded = true;
    return p;
}

void tick(World& w, Player& p, Hands& h, int n, int direction = 0, bool down = true) {
    for (int i = 0; i < n; ++i) {
        p.down = down;
        move(w, p, direction, false);
        h.step(w, p);
    }
}

void targets(World& w) {
    clear(w);
    auto p = standing();
    require(target(p).x == 11 && target(p).y == 29, "one-block character addresses exactly the forward cell");
    p.facing = -1;
    require(target(p).x == 9, "turning left changes the direct target");
    p.facing = 0;
    require(target(p).x == 10 && target(p).y == 29 && target(p).layer == Layer::background, "turning away addresses the rear wall behind the character");
    move(w, p, 1, false);
    require(p.facing == 0, "walking does not itself undo an explicit facing");
    p.down = true;
    require(target(p).y == 30 && target(p).layer == Layer::background, "S aims beneath the feet without facing the camera");
    p.facing = 1;
    require(target(p).x == 10 && target(p).layer == Layer::foreground, "S overrides lateral reach, not facing");
    double start = p.x;
    for (int i = 0; i < 60; ++i) move(w, p, 1, false, true);
    require(std::abs(p.x - start - 1) < 0.001, "crawling is slower than walking even with sprint held");
    p.down = false;
    start = p.x;
    for (int i = 0; i < 60; ++i) move(w, p, 1, false);
    require(std::abs(p.x - start - 3) < 0.001, "walking retains the six-block speed");
    start = p.x;
    for (int i = 0; i < 60; ++i) move(w, p, 1, false, true);
    require(std::abs(p.x - start - 4.5) < 0.001, "sprint uses nine blocks per second");
    Player walk = standing(20.15), run = walk;
    move(w, walk, 1, true);
    move(w, run, 1, true, true);
    require(walk.vy == run.vy && walk.y == run.y && run.x > walk.x, "running jumps change travel, not the jump impulse");
    p = standing(w.settings.width() - 0.85);
    require(wrap(target(p).x, w.settings.width()) == 0, "direct forward reach crosses the world seam");
}

void boundary_placement(World& w) {
    for (auto [x, facing, expected] : std::vector<std::tuple<double, int, int>>{
             {10.3, 1, 11}, {10.6, 1, 12}, {10.8, -1, 9}, {11.0, -1, 10},
             {w.settings.width() - 0.4, 1, 1}, {0.1, -1, w.settings.width() - 1}}) {
        clear(w);
        auto p = standing(x);
        p.facing = facing;
        p.inventory["base:soil"] = 2;
        Hands h;
        h.equip(p, "base:soil");
        auto result = h.place(w, p, false);
        require(result.ok, "placement at a tile boundary: " + result.message);
        require(w.block(expected, 29) == "base:soil" && p.inventory.at("base:soil") == 1,
                "place in the first whole cell beyond the facing edge, including at the seam");
        require(!intersects(w, p.x, p.y), "placement stays outside the character's body");
        h.hand(p);
        require(h.primary(w, p).ok && w.block(expected, 29).empty(), "gathering addresses that same cell in front of the body");
    }
    clear(w);
    auto p = standing();
    p.down = true;
    p.y = 28; p.vy = 0; p.grounded = false;
    w.set(10, 27, "base:stone"); // Head contact stops upward velocity before landing.
    p.inventory["base:soil"] = 2;
    Hands h;
    h.equip(p, "base:soil");
    require(h.place(w, p, false).ok && w.block(10, 29) == "base:soil" && !intersects(w, p.x, p.y),
            "downward placement uses the first clear cell below the feet even when upward motion has stopped");
}

void selection() {
    Player p;
    Hands h;
    h.hand(p);
    require(h.chooser && h.choice.empty(), "H opens a zero-entry chooser");
    h.cycle(p, 1);
    h.hand(p);
    require(!h.chooser && p.selected.empty(), "empty chooser closes to empty hand");
    p.inventory["base:soil"] = 4;
    p.inventory["base:stone"] = 3;
    p.inventory["base:timber"] = 2;
    require(h.mark(p, "base:soil").ok && h.mark(p, "base:timber").ok, "readiness marks existing inventory entries");
    auto original = p.inventory;
    h.equip(p, "base:timber");
    h.hand(p);
    require(p.selected.empty() && !h.chooser, "one press puts the held object away");
    h.hand(p);
    require(h.chooser && h.choice == "base:timber", "next press opens at last-used available object");
    h.cycle(p, 1);
    require(h.choice == "base:soil", "K wraps forward through the ready subset only");
    h.cycle(p, -1);
    require(h.choice == "base:timber", "J wraps backward");
    h.hand(p);
    require(p.selected == "base:timber" && !h.chooser && p.inventory == original, "equipping neither transfers nor duplicates items");
    p.inventory["base:timber"] = 0;
    h.depleted(p);
    require(p.selected.empty() && p.last_selected == "base:timber", "depletion returns to empty hand");
    require(!p.inventory.contains("base:timber") && std::ranges::find(p.ready, "base:timber") == p.ready.end(),
            "an emptied object leaves the inventory and its readiness with it");
    require(!h.mark(p, "base:timber").ok && p.ready == std::vector<Block>{"base:soil"}, "only carried objects can be readied");
    h.hand(p);
    require(h.choice == "base:soil", "unavailable last object falls back to the first available ready entry");
    h.hand(p);
    h.mark(p, "base:soil");
    require(p.ready.empty() && p.inventory["base:soil"] == 4, "unreadying keeps the object in its inventory position");
    p = {};
    for (int i = 0; i < 13; ++i) {
        auto id = "test:" + std::to_string(i);
        p.inventory[id] = 1;
        require(h.mark(p, id).ok == (i < 12), "the radial selector has twelve readiness marks, not a second container");
    }
}

void actions(World& w) {
    clear(w);
    auto p = standing();
    Hands h;
    w.set(11, 29, "base:timber");
    require(h.primary(w, p).ok && p.inventory["base:timber"] == 1, "empty-hand J gathers a hand-gatherable block");
    w.set(11, 29, "workshop:workbench");
    require(h.interact(w, p).ok && w.cell(11, 29).batch == 5 && p.inventory["base:timber"] == 1, "K interacts without gathering");
    require(h.primary(w, p).ok && p.inventory["workshop:workbench"] == 1, "J gathers the same station rather than interacting");
    w.set(11, 29, "expedition:copper_ore");
    require(!h.primary(w, p).ok && w.block(11, 29) == "expedition:copper_ore", "ore requires a tool");
    h.equip(p, "base:timber");
    require(!h.primary(w, p).ok, "a held building material does not inherit breaking");
    p.inventory["workshop:pick"] = 1;
    h.equip(p, "workshop:pick");
    require(h.primary(w, p).ok && p.inventory["expedition:copper_ore"] == 1, "breaking is the pick's primary action");
    require(h.place(w, p, false).ok && p.selected.empty(), "L sets down that same tool and empties a depleted hand");
    require(!w.solid(11, 29) && w.block(11, 29) == "workshop:pick", "placed tool has its own shape and no collision");
    for (int i = 0; i < 25; ++i) move(w, p, 1, false);
    require(p.x > 11 && !intersects(w, p.x, p.y), "the character can walk past a set-down tool");
}

void jump_building(World& w) {
    clear(w);
    Hands h;
    auto p = standing();
    p.inventory["base:soil"] = 10;
    h.equip(p, "base:soil");
    p.down = true;
    require(h.place(w, p, true).ok && h.pending.has_value(), "S held before Space+L buffers an early placement");
    move(w, p, 0, true);
    h.step(w, p);
    require(h.pending.has_value() && w.block(10, 29).empty(), "an overlapping request waits, without spending a block");
    tick(w, p, h, 150);
    require(w.block(10, 29) == "base:soil" && p.inventory.at("base:soil") == 9 && !h.pending,
            "one press places once when clear, never on every pending check");
    require(std::abs(p.y + player_height - 29) < 0.001, "jump-building lands on the new scaffold");
    require(h.place(w, p, true).ok, "the next distinct press can build the next scaffold block");
    move(w, p, 0, true);
    h.step(w, p);
    tick(w, p, h, 150);
    require(w.block(10, 28) == "base:soil" && p.inventory.at("base:soil") == 8, "successive jumps make a usable pillar");

    clear(w); p = standing(); p.inventory["base:soil"] = 3; h.equip(p, "base:soil");
    p.down = true;
    move(w, p, 0, true);
    tick(w, p, h, 57); // Descending, but with a whole clear cell beneath the feet.
    require(p.vy > 0, "descent fixture");
    auto t = int(std::ceil(p.y + player_height - 0.00001));
    require(h.place(w, p, false).ok && w.block(10, t) == "base:soil", "descent places in the first fully clear cell beneath the body");
    require(!intersects(w, p.x, p.y), "late placement never embeds or lifts the character");

    clear(w); p = standing(); p.inventory["base:soil"] = 3; h.equip(p, "base:soil");
    w.set(10, 28, "base:stone");
    p.down = true; h.place(w, p, true);
    move(w, p, 0, true); h.step(w, p); tick(w, p, h, 150);
    require(w.block(10, 29).empty() && p.inventory.at("base:soil") == 3 && !h.pending, "a low ceiling cancels an impossible request without consuming blocks");

    clear(w); p = standing(); p.inventory["base:soil"] = 3; h.equip(p, "base:soil");
    p.down = true; h.place(w, p, true);
    move(w, p, 0, true); h.step(w, p);
    p.down = false; h.step(w, p);
    require(!h.pending, "releasing downward aim cancels the buffered underfoot request");
    require(p.inventory.at("base:soil") == 3, "cancelled request costs nothing");

    clear(w); p = standing(); p.inventory["base:soil"] = 3; h.equip(p, "base:soil");
    p.down = true; move(w, p, 0, true); tick(w, p, h, 40);
    require(std::abs(p.vy) < 0.5, "apex fixture");
    int apex = int(std::ceil(p.y + player_height - 0.00001));
    require(h.place(w, p, false).ok && w.block(10, apex) == "base:soil" && !h.pending,
            "underfoot placement works near the apex without attachment or waiting for an unreachable higher row");

    clear(w); p = standing(); p.inventory["base:soil"] = 3; h.equip(p, "base:soil");
    p.down = true; move(w, p, 0, true); tick(w, p, h, 73);
    require(p.vy > 0 && p.y + player_height > 29, "near-landing fixture has less than a full empty cell beneath");
    require(!h.place(w, p, false).ok && p.inventory.at("base:soil") == 3 && !h.pending,
            "a late press with less than one block of clearance cannot manufacture space or lift the player");

    // Ordinary airborne placement is immediate, never deferred to a later height.
    clear(w); p = standing(); p.inventory["base:soil"] = 3; h.equip(p, "base:soil");
    move(w, p, 0, true);
    for (int i = 0; i < 15; ++i) move(w, p, 0, false);
    auto forward = target(p);
    require(h.place(w, p, false).ok && w.block(forward.x, forward.y) == "base:soil" && !h.pending, "forward placement targets the row at the instant of the press");
    p.facing = 0;
    auto rear = target(p);
    require(h.place(w, p, false).ok && w.block(rear.x, rear.y, Layer::background) == "base:soil" && !h.pending,
            "airborne rear-wall placement also uses its immediate target");
}

void rear_wall(World& w) {
    clear(w);
    Hands h;
    auto p = standing();
    p.inventory["base:timber"] = 8;
    h.equip(p, "base:timber");
    // A rear 2x2 timber wall must NOT transform into the foreground planks recipe.
    for (int y : {29, 28}) for (int x : {10, 11}) {
        p.x = x + 0.15; p.y = y; p.facing = 0;
        require(h.place(w, p, false).ok, "place a rear wall through the character's own column");
    }
    require(p.inventory.at("base:timber") == 4 && !p.inventory.contains("workshop:planks"), "rear-wall geometry does not craft");
    require(w.block(10, 29).empty() && w.block(10, 29, Layer::background) == "base:timber", "layers have separate occupancy");
    p = standing(9.15);
    for (int i = 0; i < 80; ++i) move(w, p, 1, false);
    require(p.x > 12 && p.y == 29, "rear walls never impede walking");
    p = standing(); p.facing = 0;
    require(h.primary(w, p).ok && p.inventory["base:timber"] == 1 && w.block(10, 29, Layer::background).empty(), "J gathers the addressed rear wall without touching foreground");
    w.put(10, 30, Cell{"workshop:workbench", 1, true, 5});
    h.equip(p, "base:timber");
    require(h.place(w, p, false).ok && w.cell(10, 29, Layer::background).quantity == 1 && !p.inventory.contains("base:timber"),
            "foreground workbench batching does not charge N for a single rear-wall block");
}

void persist(const std::filesystem::path& file, const Content& content, const Settings& settings) {
    Player before;
    {
        Save save(file); save.initialize(settings);
        World w(save, content, settings); clear(w);
        w.put(31, 31, Cell{"base:timber", 5, true});
        w.put(31, 31, Cell{"missing:wall", 1, true}, Layer::background);
        w.put(0, 32, Cell{"workshop:workbench", 1, true, 10}, Layer::background);
        before = standing(); before.inventory["base:soil"] = 7;
        before.inventory["workshop:pick"] = 1;
        before.ready = {"workshop:pick", "base:soil"};
        before.selected = "base:soil"; before.last_selected = "workshop:pick";
        before.facing = 0;
        w.retain(400, 150, 440, 180); w.checkpoint(before);
    }
    {
        Save save(file); World w(save, content, *save.settings());
        auto p = save.player();
        require(p.inventory == before.inventory && p.ready == before.ready && p.selected == before.selected
            && p.last_selected == before.last_selected && p.facing == 0 && p.y == before.y,
            "inventory, readiness, last selection, facing and position survive restart");
        require(w.cell(31, 31) == Cell{"base:timber", 5, true}, "foreground batch state survives the new format");
        require(w.block(31, 31, Layer::background) == "missing:wall", "missing background content retains its stable ID");
        require(w.cell(0, 32, Layer::background).batch == 10, "both layers survive eviction and independent vertical/horizontal chunk boundaries");
        w.set(31, 31, {}, Layer::background); w.checkpoint(p);
    }
    {
        Save save(file); World w(save, content, *save.settings());
        require(w.block(31, 31, Layer::background).empty() && w.cell(31, 31).quantity == 5, "background removal persists without changing foreground state");
    }
}
}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-keyboard-" + std::to_string(getpid()));
    try {
        require(argc == 2, "expected content root");
        std::filesystem::create_directories(scratch);
        Content content;
        auto root = std::filesystem::path(argv[1]);
        for (const auto& [id, version] : load_manifest(root / "flagship.txt").packs) content.load(root / id);
        content.validate();
        Settings settings; settings.seed = 42; settings.terrain = load_terrain(root / "expedition");
        {
            Save save(scratch / "play.sqlite3"); save.initialize(settings);
            World w(save, content, settings);
            targets(w); boundary_placement(w); selection(); actions(w); jump_building(w); rear_wall(w);
        }
        persist(scratch / "persist.sqlite3", content, settings);
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
