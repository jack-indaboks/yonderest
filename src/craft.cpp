#include "world.hpp"

#include <algorithm>
#include <set>

namespace yonder {
namespace {

struct Assembly {
    int existing = 0;
    int quantity = 0;
    bool bench = false;
    int setting = 1;
    bool conflicting_benches = false;
    std::set<std::string> stations;
};

// Partial matching is also how a placement finds the workshop it belongs to.
// Nothing is consumed or reserved until the complete shape matches.
std::optional<Assembly> assembly(World& world, const Recipe& r, int x, int y, bool complete) {
    if (r.width > world.settings.width() || y < 0 || y + r.height > world.settings.height()) return {};
    Assembly a;
    for (const auto& i : r.shape) {
        const auto& cell = world.cell(x + i.x, y + i.y);
        if (i.id.empty()) {
            if (!cell.id.empty()) return {};
        } else if (cell.id.empty()) {
            if (complete) return {};
        } else {
            if (cell.id != i.id || !cell.placed) return {};
            if (a.quantity && a.quantity != cell.quantity) return {};
            a.quantity = cell.quantity;
            ++a.existing;
        }
    }
    // All supports are on the row immediately under the shape. A workbench
    // only contributes when the shape actually spans its column.
    for (int column = 0; column < r.width; ++column) {
        const auto& cell = world.cell(x + column, y + r.height);
        const auto& station = world.content.material(cell.id).station;
        if (!station.empty()) a.stations.insert(station);
        if (station == "workbench") {
            if (a.bench && a.setting != cell.batch) a.conflicting_benches = true;
            a.bench = true;
            a.setting = cell.batch;
        }
    }
    if (complete) {
        for (const auto& station : r.stations) if (!a.stations.contains(station)) return {};
        if (a.quantity > 1 && !a.bench) return {};
    }
    return a;
}

std::vector<std::pair<int, int>> origins(World& world, const Recipe& r, int x, int y, const Block* placing) {
    std::vector<std::pair<int, int>> result;
    for (const auto& i : r.shape) {
        if (placing && i.id != *placing) continue;
        result.emplace_back(wrap(x - i.x, world.settings.width()), y - i.y);
    }
    if (!placing)
        for (int column = 0; column < r.width; ++column)
            result.emplace_back(wrap(x - column, world.settings.width()), y - r.height);
    std::ranges::sort(result);
    return result;
}

}

int placement_quantity(World& world, const Block& ingredient, int x, int y) {
    int progress = -1;
    std::set<int> quantities;
    for (const auto& r : world.content.recipes)
        for (auto [ox, oy] : origins(world, r, x, y, &ingredient)) {
            auto a = assembly(world, r, ox, oy, false);
            if (!a || (!a->bench && a->existing == 0)) continue;
            int quantity = a->quantity ? a->quantity : (a->conflicting_benches ? 0 : a->setting);
            if (quantity > 1 && !a->bench) quantity = 0;
            if (a->existing > progress) { progress = a->existing; quantities.clear(); }
            if (a->existing == progress) quantities.insert(quantity);
        }
    if (quantities.empty()) return 1;
    return quantities.size() == 1 ? *quantities.begin() : 0;
}

std::optional<ActionResult> craft(World& world, Player& p, int changed_x, int changed_y) {
    for (const auto& r : world.content.recipes)
        for (auto [x, y] : origins(world, r, changed_x, changed_y, nullptr)) {
            auto a = assembly(world, r, x, y, true);
            if (!a) continue;
            for (const auto& i : r.shape) if (!i.id.empty()) world.set(x + i.x, y + i.y, {});
            auto output = std::int64_t(r.amount) * a->quantity;
            world.put(x + r.output_x, y + r.output_y, Cell{r.output, 1, true});
            p.inventory[r.output] += output - 1;
            return ActionResult{true, "Made " + world.content.material(r.output).name + " x" + std::to_string(output)};
        }
    return {};
}

}
