#include "world.hpp"
#include "save.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace yonder {
namespace {
constexpr double contact_epsilon = 0.00001;
}

World::World(Save& save, const Content& content, Settings settings)
    : settings(std::move(settings)), content(content), save(save), legacy(save.legacy_chunks()) {}

Chunk& World::chunk(int x, int y) {
    auto key = std::pair{x / chunk_size, y / chunk_size};
    auto found = chunks.find(key);
    if (found != chunks.end()) return found->second;
    auto saved = save.chunk(key.first, key.second);
    return chunks.emplace(key, saved ? std::move(*saved) : generate(settings, key.first, key.second, legacy)).first->second;
}

Block World::block(int x, int y) {
    return cell(x, y).id;
}

Cell World::cell(int x, int y) {
    if (y < 0 || y >= settings.height()) return {};
    x = wrap(x, settings.width());
    return chunk(x, y).at(x % chunk_size, y % chunk_size);
}

std::string World::region(int x, int y) {
    auto local = settings;
    local.terrain.version = chunk(wrap(x, settings.width()), std::clamp(y, 0, settings.height() - 1)).profile;
    return biome(local, x) + (local.terrain.version == settings.terrain.version ? "" : " (old)");
}

bool World::solid(int x, int y) {
    return y < 0 || y >= settings.height() || content.material(block(x, y)).solid;
}

void World::set(int x, int y, const Block& id) {
    put(x, y, Cell{id});
}

void World::put(int x, int y, const Cell& value) {
    x = wrap(x, settings.width());
    auto& c = chunk(x, y);
    c.at(x % chunk_size, y % chunk_size) = value;
    c.dirty = true;
}

void World::retain(int left, int top, int right, int bottom) {
    std::set<std::pair<int, int>> keep;
    top = std::max(0, top);
    bottom = std::min(settings.height() - 1, bottom);
    // Block coordinates can be negative around the horizontal seam.
    int first = int(std::floor(double(left) / chunk_size));
    int last = int(std::floor(double(right) / chunk_size));
    for (int x = first; x <= last; ++x)
        for (int y = top / chunk_size; y <= bottom / chunk_size; ++y) {
            int cx = wrap(x, settings.columns);
            keep.emplace(cx, y);
            chunk(cx * chunk_size, y * chunk_size);
        }
    for (auto it = chunks.begin(); it != chunks.end();) {
        if (keep.contains(it->first)) { ++it; continue; }
        if (it->second.dirty) save.stage(it->first.first, it->first.second, it->second);
        it = chunks.erase(it);
    }
}

void World::checkpoint(const Player& player) {
    for (auto& [key, c] : chunks) if (c.dirty) save.stage(key.first, key.second, c);
    save.checkpoint(player);
    for (auto& [key, c] : chunks) c.dirty = false;
}

bool intersects(World& world, double x, double y) {
    for (int bx = int(std::floor(x)); bx <= int(std::floor(x + player_width - contact_epsilon)); ++bx)
        for (int by = int(std::floor(y)); by <= int(std::floor(y + player_height - contact_epsilon)); ++by)
            if (world.solid(bx, by)) return true;
    return false;
}

Player spawn(World& world) {
    Player p;
    // Find actual clear space, rather than assuming the generator left it clear.
    for (int x = 0; x < world.settings.width(); ++x) {
        int h = surface(world.settings, x);
        p.x = x + 0.15;
        p.y = h - player_height;
        if (!intersects(world, p.x, p.y)) {
            p.selected = world.settings.terrain.soil;
            return p;
        }
    }
    throw std::runtime_error("No clear starting position.");
}

void move(World& world, Player& p, int direction, bool jump) {
    p.grounded = intersects(world, p.x, p.y + 0.025);
    if (jump && p.grounded) p.vy = -11.5;
    double dx = direction * 6.0 * tick_seconds;
    if (!intersects(world, p.x + dx, p.y)) p.x += dx;
    else if (direction && p.grounded && !jump && !intersects(world, p.x + dx, p.y - 1)) {
        // Walk up one-block terrain steps; jumping is for larger obstacles.
        p.x += dx;
        p.y -= 1;
    }
    p.vy = std::min(22.0, p.vy + 26 * tick_seconds);
    double dy = p.vy * tick_seconds;
    if (!intersects(world, p.x, p.y + dy)) p.y += dy;
    else {
        // Find the contacted tile edge, including after a short fall onto a step.
        if (dy > 0) p.y = std::floor(p.y + player_height + dy) - player_height;
        else p.y = std::floor(p.y + dy) + 1;
        p.grounded = dy > 0;
        p.vy = 0;
    }
    p.x = wrap(p.x, double(world.settings.width()));
}

namespace {

std::string reach(World& world, const Player& p, int x, int y) {
    if (y < 0 || y >= world.settings.height()) return "World boundary";
    double px = p.x + player_width / 2, py = p.y + player_height / 2;
    double dx = distance_x(px, x + 0.5, world.settings.width()), dy = y + 0.5 - py;
    double distance = std::hypot(dx, dy);
    if (distance > 5) return "Out of reach";
    int steps = int(std::ceil(distance * 8));
    int target_x = wrap(x, world.settings.width());
    for (int i = 1; i < steps; ++i) {
        int bx = int(std::floor(px + dx * i / steps));
        int by = int(std::floor(py + dy * i / steps));
        if (wrap(bx, world.settings.width()) == target_x && by == y) break;
        if (world.solid(bx, by)) return "Blocked";
    }
    return {};
}

}

ActionResult act(World& world, Player& p, Action action, int x, int y) {
    auto refused = reach(world, p, x, y);
    if (!refused.empty()) return {false, refused};
    Cell old = world.cell(x, y);
    if (action == Action::mine) {
        if (old.id.empty()) return {false, "Empty space"};
        p.inventory[old.id] += old.quantity;
        world.set(x, y, {});
    } else {
        if (!old.id.empty()) return {false, "Already occupied"};
        int quantity = placement_quantity(world, p.selected, x, y);
        if (quantity == 0) return {false, "Check the workbench and ingredient batch sizes"};
        if (p.selected.empty() || p.inventory[p.selected] < quantity) return {false, "Need " + std::to_string(quantity) + " of this material"};
        double px = p.x + player_width / 2, py = p.y + player_height / 2;
        double dx = distance_x(px, x + 0.5, world.settings.width()), dy = y + 0.5 - py;
        if (std::abs(dx) < (player_width + 1) / 2 - contact_epsilon && std::abs(dy) < (player_height + 1) / 2 - contact_epsilon)
            return {false, "You are standing there"};
        if (!(world.solid(x - 1, y) || world.solid(x + 1, y) || world.solid(x, y - 1) || world.solid(x, y + 1)))
            return {false, "Build against an existing block"};
        world.put(x, y, Cell{p.selected, quantity, true});
        p.inventory[p.selected] -= quantity;
    }
    if (auto result = craft(world, p, x, y)) return *result;
    return {true, action == Action::mine ? "Gathered x" + std::to_string(old.quantity) : "Placed"};
}

ActionResult adjust_workbench(World& world, const Player& p, int x, int y) {
    auto refused = reach(world, p, x, y);
    if (!refused.empty()) return {false, refused};
    auto cell = world.cell(x, y);
    if (world.content.material(cell.id).station != "workbench") return {false, "Aim at a workbench to set its batch"};
    cell.batch = cell.batch == 1 ? 5 : (cell.batch == 5 ? 10 : 1);
    world.put(x, y, cell);
    return {true, "Workbench batch x" + std::to_string(cell.batch)};
}

std::vector<Block> slots(const Content& content, const Player& p) {
    std::vector<Block> result;
    for (const auto& [id, m] : content.blocks) result.push_back(id);
    for (const auto& [id, count] : p.inventory) if (!content.blocks.contains(id)) result.push_back(id);
    if (!p.selected.empty() && !content.blocks.contains(p.selected) && !p.inventory.contains(p.selected)) result.push_back(p.selected);
    return result;
}

}
