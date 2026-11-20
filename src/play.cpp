#include "play.hpp"

#include <algorithm>
#include <cmath>

namespace yonder {

void Hands::cancel() {
    chooser = false;
    pending.reset();
    working = false;
}

void Hands::equip(Player& p, const Block& id) {
    cancel();
    p.selected = id;
    if (!id.empty()) p.last_selected = id;
}

void Hands::hand(Player& p) {
    pending.reset();
    working = false;
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
    if (p.ready.size() == 12) return {false, "Twelve objects are already readied"};
    p.ready.push_back(id);
    return {true, "Readied"};
}

void Hands::depleted(Player& p) {
    bool holding = !p.selected.empty();
    drop_empty(p);
    if (holding && p.selected.empty()) {
        pending.reset();
        working = false;
    }
}

ActionResult Hands::primary(World& world, Player& p) {
    working = false;
    pending.reset();
    if (!p.selected.empty() && world.content.material(p.selected).tool.empty())
        return {false, "Use empty hand or a tool"};
    auto t = target(p);
    return gather(world, p, t.x, t.y, t.layer);
}

ActionResult Hands::interact(World& world, Player& p) {
    working = false;
    pending.reset();
    auto t = target(p);
    return adjust_workbench(world, t.x, t.y, t.layer);
}

ActionResult Hands::place(World& world, Player& p, bool jumping) {
    pending.reset();
    auto t = target(p);
    if (p.selected.empty()) return {false, "Empty hand"};
    if (p.down && t.layer == Layer::foreground) {
        double feet = p.y + player_height;
        t.y = int(std::ceil(feet - 0.00001));
        bool rising = p.vy < 0 || (jumping && p.grounded);
        if (rising && !world.block(t.x, t.y).empty()) {
            --t.y;
            if (world.block(t.x, t.y).empty() && feet > t.y + 0.00001) {
                pending = t;
                return {true, "Placement waits for clearance", true};
            }
        }
    }
    auto result = place_at(world, p, t.x, t.y, t.layer);
    if (result.ok && !p.down && p.facing != 0) working = true;
    depleted(p);
    return result;
}

std::optional<ActionResult> Hands::step(World& world, Player& p) {
    if (!pending) return std::nullopt;
    auto t = *pending;
    if (!p.down || p.grounded
        || wrap(t.x, world.settings.width()) != wrap(int(std::floor(p.x + player_width / 2)), world.settings.width())) {
        pending.reset();
        return ActionResult{false, "Placement cancelled", true};
    }
    if (p.y + player_height > t.y + 0.00001) {
        if (p.vy >= 0) {
            pending.reset();
            return ActionResult{false, "No clearance beneath you"};
        }
        return std::nullopt;
    }
    pending.reset();
    auto result = place_at(world, p, t.x, t.y, t.layer);
    depleted(p);
    return result;
}

}
