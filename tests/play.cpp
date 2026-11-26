// Play through the engine and the flagship's modules, without the window: where actions land,
// taking and placing, the ring, crafting, the forgiveness at the edges, and what survives a restart.

#include "yonderest/host.hpp"
#include "yonderest/inventory.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <unistd.h>

using namespace yonder;
namespace yonder {
namespace physics { void attach(Host&); }
namespace interaction { void attach(Host&); }
namespace inventory { void attach(Host&); }
namespace crafting { void attach(Host&); }
namespace generator { void attach(Host&); }
}
namespace {

void require(bool yes, const std::string& what) {
    if (!yes) throw std::runtime_error(what);
}

// The game without a window: a small world, eight chunks around and four tall.
struct Game {
    Host host;
    std::string notice;
    Game(const std::filesystem::path& content, const std::filesystem::path& save) : host(options(content, save)) {
        physics::attach(host);
        interaction::attach(host);
        inventory::attach(host);
        crafting::attach(host);
        generator::attach(host);
        host.events.on<Notice>([this](Notice& n) { notice = n.text; });
        host.start();
    }
    static Options options(const std::filesystem::path& content, const std::filesystem::path& save) {
        Options o;
        o.content = content;
        o.path = save;
        o.seed = 42;
        o.size = {8, 4};
        return o;
    }
    World& w() { return host.world(); }
    Player& p() { return host.player(); }
    Physics& physics() { return *host.physics; }
    int width() { return w().settings.width(); }

    void clear() {
        w().load(-10, 20, 50, 31);
        for (int x = -10; x < 50; ++x) {
            for (int y = 20; y < 30; ++y) {
                w().set(x, y, {});
                w().set(x, y, {}, 1);
            }
            w().set(x, 30, "base:soil");
        }
    }
    // A fresh player standing on the ground: nothing carried, nothing held, facing right.
    void standing(double x = 10.15) {
        host.interrupt();
        host.hold({});
        p().properties = {};
        p().x = x; p().y = 29;
        p().facing = 1;
        // Whatever it was doing, the body comes to rest on the ground.
        for (int i = 0; i < 600 && !(physics().grounded() && !physics().rising()); ++i) host.tick();
        host.tick();
    }
    void carry(const Block& id, std::int64_t count) { inventory::carry(p(), id, count); }
    std::int64_t count(const Block& id) { return inventory::count(p(), id); }
    // Holds an object, the way the backpack does.
    void equip(const Block& id) { host.hold(id); }
    void move(int direction, bool jump = false, bool sprint = false) {
        physics().steer(direction, sprint);
        if (jump) physics().jump();
        host.tick();
    }
    void tick(int n, int direction = 0, bool down = true) {
        for (int i = 0; i < n; ++i) {
            p().down = down;
            move(direction);
        }
    }
    // Whether the body at this position overlaps a cell whose block stops it.
    bool intersects(double x, double y) {
        for (int bx = int(std::floor(x)); bx <= int(std::floor(x + player_width - 0.00001)); ++bx)
            for (int by = int(std::floor(y)); by <= int(std::floor(y + player_height - 0.00001)); ++by)
                if (physics().solid(w().block(bx, by))) return true;
        return false;
    }
    Action act(Act a) { return host.act(a); }
    Ring ring() { return host.provided<Ring>().front(); }
    Block choice() {
        auto r = ring();
        return r.selected < r.entries.size() ? r.entries[r.selected].first : Block{};
    }
    std::vector<Block> ready() {
        std::vector<Block> result;
        const auto* list = p().properties["inventory"]["ready"].array();
        if (list) for (const auto& v : *list) result.push_back(std::get<std::string>(v.data));
        return result;
    }
    Block last_held() { return Field(p().properties["inventory"]["last_held"], "last_held").text_or(""); }
    // Readies or unreadies a carried object through the backpack.
    void mark(const Block& id) {
        host.intent("backpack");
        auto view = std::get<Backpack>(host.screen()->view());
        auto at = std::ranges::find(view.entries, id, &Backpack::Entry::block);
        require(at != view.entries.end(), "fixture: " + id + " is in the backpack");
        view.select(int(at - view.entries.begin()), 1);
        std::get<Backpack>(host.screen()->view()).act(1);
        host.close();
    }
};

void targets(Game& g) {
    g.clear();
    g.standing();
    auto& p = g.p();
    require(g.host.target().x == 11 && g.host.target().y == 29, "one-block character addresses exactly the forward cell");
    p.facing = -1;
    require(g.host.target().x == 9, "turning left changes the direct target");
    p.facing = 0;
    require(g.host.target().x == 10 && g.host.target().y == 29 && g.host.target().layer == 1, "turning away addresses the rear wall behind the character");
    g.move(1);
    require(p.facing == 0, "walking does not itself undo an explicit facing");
    p.down = true;
    require(g.host.target().y == 30 && g.host.target().layer == 1, "S aims beneath the feet without facing the camera");
    p.facing = 1;
    require(g.host.target().x == 10 && g.host.target().layer == 0, "S overrides lateral reach, not facing");
    double start = p.x;
    for (int i = 0; i < 60; ++i) g.move(1, false, true);
    require(std::abs(p.x - start - 1) < 0.001, "crawling is slower than walking even with sprint held");
    p.down = false;
    start = p.x;
    for (int i = 0; i < 60; ++i) g.move(1);
    require(std::abs(p.x - start - 3) < 0.001, "walking retains the six-block speed");
    start = p.x;
    for (int i = 0; i < 60; ++i) g.move(1, false, true);
    require(std::abs(p.x - start - 4.5) < 0.001, "sprint uses nine blocks per second");
    g.standing(20.15);
    g.move(1, true);
    auto walk = std::tuple{p.x, p.y};
    g.standing(20.15);
    g.move(1, true, true);
    require(std::get<1>(walk) == p.y && p.x > std::get<0>(walk), "running jumps change travel, not the jump impulse");
    g.standing(g.width() - 0.85);
    require(wrap(g.host.target().x, g.width()) == 0, "direct forward reach crosses the world seam");
    g.standing();
    p.facing = 0; p.up = true;
    require(g.host.target().x == 10 && g.host.target().y == 28 && g.host.target().layer == 0, "W held reaches the cell above the head");
}

void boundary_placement(Game& g) {
    for (auto [x, facing, expected] : std::vector<std::tuple<double, int, int>>{
             {10.3, 1, 11}, {10.6, 1, 11}, {10.7, 1, 12}, {10.8, -1, 10}, {11.0, -1, 10},
             {g.width() - 0.4, 1, 0}, {0.1, -1, g.width() - 1}}) {
        g.clear();
        g.standing(x);
        g.p().facing = facing;
        g.carry("base:soil", 2);
        g.equip("base:soil");
        auto result = g.act(Act::c);
        require(result.ok, "placement at a tile boundary: " + result.message);
        require(g.w().block(expected, 29) == "base:soil" && g.count("base:soil") == 1,
                "place beyond the facing edge, in a cell the body overlaps by no more than the margin, including at the seam");
        require(!g.intersects(g.p().x, g.p().y), "placement stays outside the character's body");
        g.host.intent("hand");
        require(g.act(Act::a).ok && g.w().block(expected, 29).empty(), "gathering addresses that same cell in front of the body");
    }
    g.clear();
    g.standing();
    auto& p = g.p();
    g.w().set(10, 27, "base:stone"); // Head contact stops upward velocity before landing.
    g.carry("base:soil", 2);
    g.equip("base:soil");
    p.y = 28;
    p.down = true;
    require(g.act(Act::c).ok && g.w().block(10, 29) == "base:soil" && !g.intersects(p.x, p.y),
            "downward placement uses the first clear cell below the feet even when upward motion has stopped");
}

void selection(Game& g) {
    g.clear();
    g.standing();
    auto& p = g.p();
    g.host.intent("hand");
    require(g.ring().open && g.ring().entries.empty(), "H opens a zero-entry ring");
    g.act(Act::b);
    g.host.intent("hand");
    require(!g.ring().open && p.held.empty(), "an empty ring closes to the empty hand");
    g.carry("base:soil", 4);
    g.carry("base:stone", 3);
    g.carry("base:timber", 2);
    g.mark("base:stone");
    require(g.ready() == std::vector<Block>{"base:soil", "base:timber"} && g.notice == "Unreadied", "the backpack unreadies a carried object");
    auto counts = std::tuple{g.count("base:soil"), g.count("base:stone"), g.count("base:timber")};
    g.equip("base:timber");
    g.host.intent("hand");
    require(p.held.empty() && !g.ring().open && g.last_held() == "base:timber", "one press puts the held object away");
    g.host.intent("hand");
    require(g.ring().open && g.choice() == "base:timber", "next press opens at last-used available object");
    auto cycled = g.act(Act::b);
    require(cycled.claimed && g.choice() == "base:soil", "K wraps forward through the ready subset only");
    g.act(Act::a);
    require(g.choice() == "base:timber", "J wraps backward");
    g.host.intent("hand");
    require(p.held == "base:timber" && !g.ring().open && std::tuple{g.count("base:soil"), g.count("base:stone"), g.count("base:timber")} == counts,
            "equipping neither transfers nor duplicates items");
    // Use up the held timber.
    g.clear();
    g.standing();
    g.carry("base:soil", 4);
    g.carry("base:timber", 1);
    g.equip("base:timber");
    g.act(Act::c);
    require(p.held.empty() && g.last_held() == "base:timber", "depletion returns to empty hand");
    auto ready = g.ready();
    require(g.count("base:timber") == 0 && std::ranges::find(ready, "base:timber") == ready.end(),
            "an emptied object leaves the inventory and its readiness with it");
    g.host.intent("hand");
    require(g.choice() == "base:soil", "unavailable last object falls back to the first available ready entry");
    g.host.intent("hand");
    g.mark("base:soil");
    require(g.ready().empty() && g.count("base:soil") == 4, "unreadying keeps the object in its inventory position");
    g.standing();
    for (int i = 0; i < 13; ++i) {
        auto id = "test:" + std::to_string(i);
        g.carry(id, 1);
        if (i < 12) continue;
        g.mark(id);
        require(g.notice == "Twelve objects are already readied" && g.ready().size() == 12, "the ring has twelve readiness marks, not a second container");
    }
}

void actions(Game& g) {
    g.clear();
    g.standing();
    auto& w = g.w();
    w.set(11, 29, "base:timber");
    require(g.act(Act::a).ok && g.count("base:timber") == 1, "empty-hand J gathers a hand-gatherable block");
    w.set(11, 29, "workshop:workbench");
    require(g.act(Act::b).ok && w.cell(11, 29).properties["crafting"]["batch"] == Value(5) && g.count("base:timber") == 1,
            "K interacts without gathering");
    require(g.act(Act::a).ok && g.count("workshop:workbench") == 1, "J gathers the same station rather than interacting");
    w.set(11, 29, "expedition:copper_ore");
    require(!g.act(Act::a).ok && w.block(11, 29) == "expedition:copper_ore", "ore requires a tool");
    g.equip("base:timber");
    require(!g.act(Act::a).ok, "a held building material does not inherit breaking");
    g.carry("workshop:pick", 1);
    g.equip("workshop:pick");
    require(g.act(Act::a).ok && g.count("expedition:copper_ore") == 1, "breaking is the pick's primary action");
    require(g.act(Act::c).ok && g.p().held.empty(), "L sets down that same tool and empties a depleted hand");
    require(!g.physics().solid(w.block(11, 29)) && w.block(11, 29) == "workshop:pick", "placed tool has its own shape and no collision");
    for (int i = 0; i < 25; ++i) g.move(1);
    require(g.p().x > 11 && !g.intersects(g.p().x, g.p().y), "the character can walk past a set-down tool");
    g.standing(11.15);
    w.set(12, 29, "base:timber");
    require(g.act(Act::a).ok && g.count("workshop:pick") == 1 && w.block(11, 29).empty() && w.block(12, 29) == "base:timber",
            "J takes back what shares the character's own cell before the block in front");
}

void forgiveness(Game& g) {
    auto& w = g.w();
    for (int opening : {28, 27}) {
        g.clear();
        for (int y = 20; y < 30; ++y) if (y != opening) w.set(12, y, "base:stone");
        g.standing(10.15);
        g.move(1, true);
        bool entered = false;
        for (int i = 0; i < 120; ++i) {
            g.move(1);
            entered |= g.p().x > 11.5 && g.p().x < 12 && g.p().y == opening;
        }
        require(entered, "a running jump enters a one-block opening " + std::to_string(29 - opening) + " up");
    }
    g.clear();
    g.standing();
    auto& p = g.p();
    g.carry("base:soil", 3);
    g.equip("base:soil");
    p.down = true;
    g.move(0, true);
    g.tick(17);
    require(p.y + player_height > 28.1, "fixture: the feet are still a few pixels into the second row up");
    require(g.act(Act::c).ok && w.block(10, 28) == "base:soil" && w.block(10, 29).empty() && !g.intersects(p.x, p.y),
            "a press a few pixels short of clearing two rows still builds two up, lifting the body clear");
    g.clear();
    g.standing();
    p.facing = 0; p.up = true;
    g.carry("base:soil", 3);
    g.equip("base:soil");
    require(g.act(Act::c).ok && w.block(10, 28) == "base:soil", "W held roofs over the head");
    require(!g.act(Act::a).ok, "a held block does not dig");
    g.host.intent("hand");
    require(g.act(Act::a).ok && w.block(10, 28).empty(), "W held digs straight up");

    g.clear();
    g.standing();
    g.move(0, true);
    require(g.host.target().y == 28, "a jump aims one up from the moment it leaves the ground");
    // Just past the switch to two up, the body still overlaps the row below by half a block.
    auto rising = [&] {
        g.standing();
        g.move(0, true);
        while (g.host.target().y != 27) g.move(0);
    };
    g.clear(); w.set(11, 28, "base:stone");
    rising();
    require(g.act(Act::a).ok && w.block(11, 28).empty(), "J in a jump takes the block in the other row the body overlaps when the aimed cell is empty");
    g.clear(); w.set(11, 28, "base:stone"); w.set(11, 27, "base:stone");
    rising();
    require(g.act(Act::a).ok && w.block(11, 27).empty() && !w.block(11, 28).empty(), "deliberate aim still wins when the aimed cell can be hit");
}

void belt(Game& g) {
    g.clear();
    g.standing();
    auto& w = g.w();
    w.set(11, 29, "base:timber");
    g.act(Act::a);
    require(g.ready() == std::vector<Block>{"base:timber"}, "a newly carried kind is readied");
    g.equip("base:timber");
    g.act(Act::c);
    require(g.ready().empty() && g.count("base:timber") == 0, "using up a stack ends its readiness");
    g.act(Act::a);
    require(g.ready() == std::vector<Block>{"base:timber"}, "gathered again, it is readied again");
    g.mark("base:timber");
    w.set(11, 29, "base:timber");
    g.act(Act::a);
    require(g.ready().empty() && g.count("base:timber") == 2, "more of an unreadied kind stays unreadied");
    for (int i = 0; i < 12; ++i) g.carry("test:" + std::to_string(i), 1);
    require(g.ready().size() == 12 && g.ready().back() == "test:11", "new kinds fill the ring");
    g.carry("test:12", 1);
    require(g.ready().size() == 12 && g.count("test:12") == 1, "a full ring readies nothing more");
}

// One result stands in the world; only a surplus is carried. Whatever blocks stand in the
// arrangement count, natural or placed.
void craft(Game& g) {
    auto& w = g.w();
    auto& p = g.p();
    g.clear();
    g.standing();
    for (int x : {10, 11, 12}) w.set(x, 27, "base:stone");
    g.carry("base:timber", 1);
    g.equip("base:timber");
    p.y = 28; // Facing the cell beneath the stones.
    require(g.act(Act::c).ok && w.block(11, 28) == "workshop:pick", "the pick's shape completes in the world, from natural stone");
    require(g.count("workshop:pick") == 0 && g.notice == "Made Stone pick x1", "a single result is not also carried");
    g.clear();
    g.standing();
    for (auto [x, y] : {std::pair{10, 27}, {11, 27}, {10, 28}}) w.set(x, y, "base:timber");
    g.carry("base:timber", 1);
    g.equip("base:timber");
    p.y = 28;
    require(g.act(Act::c).ok && w.block(10, 28) == "workshop:planks" && g.count("workshop:planks") == 3, "four planks: one in the world, three carried");
    // A batch at a workbench: each placement takes five, and taking one back returns all five.
    g.clear();
    g.standing();
    w.set(11, 29, "workshop:workbench");
    g.act(Act::b);
    g.carry("base:timber", 12);
    g.equip("base:timber");
    p.y = 28;
    require(g.act(Act::c).ok && w.cell(11, 28).properties["crafting"]["quantity"] == Value(5) && g.count("base:timber") == 7,
            "a placement over a workbench set to five takes five");
    g.host.intent("hand");
    require(g.act(Act::a).ok && g.count("base:timber") == 12, "taking an unfinished ingredient gives back all it stands for");
}

void jump_building(Game& g) {
    auto& w = g.w();
    auto& p = g.p();
    g.clear();
    g.standing();
    g.carry("base:soil", 10);
    g.equip("base:soil");
    p.down = true;
    g.physics().jump();
    require(g.act(Act::c).message == "Placement waits for clearance", "S held before Space+L buffers an early placement");
    g.move(0);
    require(w.block(10, 29).empty() && g.count("base:soil") == 10, "an overlapping request waits, without spending a block");
    g.tick(150);
    require(w.block(10, 29) == "base:soil" && g.count("base:soil") == 9, "one press places once when clear, never on every pending check");
    require(std::abs(p.y + player_height - 29) < 0.001, "jump-building lands on the new scaffold");
    g.physics().jump();
    require(g.act(Act::c).ok, "the next distinct press can build the next scaffold block");
    g.move(0);
    g.tick(150);
    require(w.block(10, 28) == "base:soil" && g.count("base:soil") == 8, "successive jumps make a usable pillar");

    g.clear(); g.standing(); g.carry("base:soil", 3); g.equip("base:soil");
    p.down = true;
    g.move(0, true);
    g.tick(57); // Descending, but with a whole clear cell beneath the feet.
    require(!g.physics().rising() && !g.physics().grounded(), "descent fixture");
    auto t = int(std::ceil(p.y + player_height - margin));
    require(g.act(Act::c).ok && w.block(10, t) == "base:soil", "descent places in the first cell beneath the body, give or take the margin");
    require(!g.intersects(p.x, p.y), "late placement never embeds the character");

    g.clear(); g.standing(); g.carry("base:soil", 3); g.equip("base:soil");
    w.set(10, 28, "base:stone");
    p.down = true; g.physics().jump(); g.act(Act::c);
    g.move(0); g.tick(150);
    require(w.block(10, 29).empty() && g.count("base:soil") == 3, "a low ceiling cancels an impossible request without consuming blocks");

    g.clear(); g.standing(); g.carry("base:soil", 3); g.equip("base:soil");
    p.down = true; g.physics().jump(); g.act(Act::c);
    g.move(0);
    p.down = false;
    g.tick(150, 0, false);
    require(w.block(10, 29).empty() && g.count("base:soil") == 3, "releasing downward aim cancels the buffered underfoot request, costing nothing");

    g.clear(); g.standing(); g.carry("base:soil", 3); g.equip("base:soil");
    p.down = true; g.move(0, true); g.tick(40);
    int apex = int(std::ceil(p.y + player_height - 0.00001));
    require(g.act(Act::c).ok && w.block(10, apex) == "base:soil",
            "underfoot placement works near the apex without attachment or waiting for an unreachable higher row");

    g.clear(); g.standing(); g.carry("base:soil", 3); g.equip("base:soil");
    p.down = true; g.move(0, true); g.tick(73);
    require(!g.physics().rising() && p.y + player_height > 29, "near-landing fixture has less than a full empty cell beneath");
    require(!g.act(Act::c).ok && g.count("base:soil") == 3,
            "a late press with less than one block of clearance cannot manufacture space or lift the player");

    // Ordinary airborne placement is immediate, never deferred to a later height.
    g.clear(); g.standing(); g.carry("base:soil", 3); g.equip("base:soil");
    g.move(0, true);
    for (int i = 0; i < 15; ++i) g.move(0);
    auto forward = g.host.target();
    require(g.act(Act::c).ok && w.block(forward.x, forward.y) == "base:soil", "forward placement targets the row at the instant of the press");
    p.facing = 0;
    auto rear = g.host.target();
    require(g.act(Act::c).ok && w.block(rear.x, rear.y, 1) == "base:soil", "airborne rear-wall placement also uses its immediate target");
}

void rear_wall(Game& g) {
    auto& w = g.w();
    auto& p = g.p();
    g.clear();
    g.standing();
    g.carry("base:timber", 8);
    g.equip("base:timber");
    // A rear 2x2 timber wall must not transform into the foreground planks recipe.
    for (int y : {29, 28}) for (int x : {10, 11}) {
        p.x = x + 0.15; p.y = y; p.facing = 0;
        require(g.act(Act::c).ok, "place a rear wall through the character's own column");
    }
    require(g.count("base:timber") == 4 && g.count("workshop:planks") == 0, "rear-wall geometry does not craft");
    require(w.block(10, 29).empty() && w.block(10, 29, 1) == "base:timber", "layers have separate occupancy");
    g.standing(9.15);
    for (int i = 0; i < 80; ++i) g.move(1);
    require(p.x > 12 && p.y == 29, "rear walls never impede walking");
    g.standing();
    p.facing = 0;
    require(g.act(Act::a).ok && g.count("base:timber") == 1 && w.block(10, 29, 1).empty(), "J gathers the addressed rear wall without touching foreground");
    Cell bench{"workshop:workbench", {}};
    bench.properties.set("crafting", Value(Value::Object{{"batch", 5}}));
    w.put(10, 30, bench);
    g.equip("base:timber");
    require(g.act(Act::c).ok && w.cell(10, 29, 1).properties.empty() && g.count("base:timber") == 0,
            "foreground workbench batching does not charge N for a single rear-wall block");
}

void persist(const std::filesystem::path& content, const std::filesystem::path& file) {
    // Cells on both sides of a vertical and a horizontal chunk boundary.
    int edge = chunk_size - 1;
    std::vector<Block> ready;
    {
        Game g(content, file);
        auto& w = g.w();
        g.clear();
        w.load(edge, edge, edge + 1, edge + 1);
        Cell batch{"base:timber", {}}, bench{"workshop:workbench", {}};
        batch.properties.set("crafting", Value(Value::Object{{"quantity", 5}}));
        bench.properties.set("crafting", Value(Value::Object{{"batch", 10}}));
        w.put(edge, edge, batch);
        w.put(edge, edge, Cell{"missing:wall", {}}, 1);
        w.put(edge + 1, edge + 1, bench, 1);
        g.standing();
        g.carry("base:soil", 7);
        g.carry("workshop:pick", 1);
        g.equip("workshop:pick");
        g.equip("base:soil");
        g.p().facing = 0;
        ready = g.ready();
        w.retain(300, 200, 340, 220);
        g.host.save_changes();
    }
    auto reopen = [&](auto&& check) {
        Game g(content, file);
        g.w().load(edge, edge, edge + 1, edge + 1);
        check(g);
    };
    reopen([&](Game& g) {
        require(g.count("base:soil") == 7 && g.count("workshop:pick") == 1 && g.ready() == ready && g.p().held == "base:soil"
                    && g.p().facing == 0 && g.p().y == 29 && g.p().world == Save::starting_world,
                "the world, inventory, readiness, held object, facing and position survive restart");
        require(g.w().cell(edge, edge).properties["crafting"]["quantity"] == Value(5), "foreground batch state survives");
        require(g.w().block(edge, edge, 1) == "missing:wall", "missing background content retains its stable ID");
        require(g.w().cell(edge + 1, edge + 1, 1).properties["crafting"]["batch"] == Value(10),
                "both layers survive eviction across vertical and horizontal chunk boundaries");
        g.w().set(edge, edge, {}, 1);
        g.host.save_changes();
    });
    reopen([&](Game& g) {
        require(g.w().block(edge, edge, 1).empty() && g.w().cell(edge, edge).properties["crafting"]["quantity"] == Value(5),
                "background removal persists without changing foreground state");
    });
}

}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-play-" + std::to_string(getpid()));
    try {
        require(argc == 2, "expected content root");
        std::filesystem::path content = argv[1];
        std::filesystem::create_directories(scratch);
        {
            Game g(content, scratch / "play");
            targets(g); boundary_placement(g); selection(g); actions(g); forgiveness(g); belt(g); craft(g); jump_building(g); rear_wall(g);
        }
        persist(content, scratch / "persist");
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
