#include "play.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace yonder {

void Hands::cancel() {
    chooser = false;
    pending.reset();
}

void Hands::equip(Player& p, const Block& id) {
    cancel();
    p.selected = id;
    if (!id.empty()) p.last_selected = id;
}

void Hands::hand(Player& p) {
    pending.reset();
    if (!p.selected.empty()) {
        p.last_selected = p.selected;
        p.selected.clear();
    } else if (!chooser) {
        chooser = true;
        choice = std::ranges::find(p.ready, p.last_selected) != p.ready.end() ? p.last_selected
            : (p.ready.empty() ? Block{} : p.ready.front());
    } else equip(p, choice);
}

void Hands::cycle(const Player& p, int direction) {
    if (p.ready.empty()) return;
    int index = int(std::ranges::find(p.ready, choice) - p.ready.begin());
    choice = p.ready[wrap(index + direction, int(p.ready.size()))];
}

ActionResult Hands::mark(Player& p, const Block& id) {
    auto found = std::ranges::find(p.ready, id);
    if (found != p.ready.end()) {
        p.ready.erase(found);
        return {true, "Unreadied"};
    }
    if (!p.inventory.contains(id)) return {false, "Not carried"};
    if (p.ready.size() == belt_size) return {false, "Twelve objects are already readied"};
    p.ready.push_back(id);
    return {true, "Readied"};
}

void Hands::depleted(Player& p) {
    bool holding = !p.selected.empty();
    drop_empty(p);
    if (holding && p.selected.empty()) pending.reset();
}

ActionResult Hands::primary(World& world, Player& p) {
    pending.reset();
    if (!p.selected.empty() && world.content.material(p.selected).tool.empty())
        return {false, "Use empty hand or a tool"};
    auto t = reach(world, p);
    return gather(world, p, t.x, t.y, t.layer);
}

ActionResult Hands::interact(World& world, Player& p) {
    pending.reset();
    auto t = target(p);
    return adjust_workbench(world, t.x, t.y, t.layer);
}

// A solid block placed where the body overlaps by no more than the margin moves the body clear.
ActionResult Hands::put(World& world, Player& p, const Target& t) {
    auto [x, y] = std::pair{p.x, p.y};
    if (t.layer == Layer::foreground && world.content.material(p.selected).solid
        && world.block(t.x, t.y).empty() && !clear_of(world, p, t.x, t.y))
        return {false, "No room"};
    auto result = place_at(world, p, t.x, t.y, t.layer);
    if (!result.ok) std::tie(p.x, p.y) = std::pair{x, y};
    depleted(p);
    return result;
}

ActionResult Hands::place(World& world, Player& p, bool jumping) {
    pending.reset();
    auto t = target(p);
    if (p.selected.empty()) return {false, "Empty hand"};
    if (p.down && t.layer == Layer::foreground) {
        // Rising out of an occupied cell, the request waits for the feet to clear the one above.
        bool rising = p.vy < 0 || (jumping && p.grounded);
        if (rising && !world.block(t.x, t.y).empty() && world.block(t.x, t.y - 1).empty()) {
            pending = Target{t.x, t.y - 1};
            return {true, "Placement waits for clearance", true};
        }
    }
    return put(world, p, favor(world, p, t, false));
}

std::optional<ActionResult> Hands::step(World& world, Player& p) {
    if (!pending) return std::nullopt;
    auto t = *pending;
    if (!p.down || p.grounded
        || wrap(t.x, world.settings.width()) != wrap(int(std::floor(p.x + player_width / 2)), world.settings.width())) {
        pending.reset();
        return ActionResult{false, "Placement cancelled", true};
    }
    double reaching = p.y + player_height - t.y; // How far the feet still reach into the cell.
    if (reaching > 0.00001) {
        if (p.vy < 0) return std::nullopt;
        if (reaching > margin) {
            pending.reset();
            return ActionResult{false, "No clearance beneath you"};
        }
    }
    pending.reset();
    return put(world, p, t);
}

}
