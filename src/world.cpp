#include "world.hpp"
#include "save.hpp"

#include <algorithm>
#include <cmath>
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

const Block& World::block(int x, int y, Layer layer) {
    return cell(x, y, layer).id;
}

const Cell& World::cell(int x, int y, Layer layer) {
    static const Cell empty;
    if (y < 0 || y >= settings.height()) return empty;
    x = wrap(x, settings.width());
    auto& c = chunk(x, y);
    return (layer == Layer::foreground ? c.tiles : c.background)[(y % chunk_size) * chunk_size + x % chunk_size];
}

std::string World::region(int x, int y) {
    const auto& profile = chunk(wrap(x, settings.width()), std::clamp(y, 0, settings.height() - 1)).profile;
    return biome(settings, x, profile) + (profile == settings.terrain.version ? "" : " (old)");
}

bool World::solid(int x, int y) {
    return y < 0 || y >= settings.height() || content.material(block(x, y)).solid;
}

void World::set(int x, int y, const Block& id, Layer layer) {
    put(x, y, Cell{id}, layer);
}

void World::put(int x, int y, const Cell& value, Layer layer) {
    x = wrap(x, settings.width());
    auto& c = chunk(x, y);
    (layer == Layer::foreground ? c.tiles : c.background)[(y % chunk_size) * chunk_size + x % chunk_size] = value;
    c.dirty = true;
}

void World::retain(int left, int top, int right, int bottom) {
    top = std::max(0, top) / chunk_size;
    bottom = std::min(settings.height() - 1, bottom) / chunk_size;
    // Block coordinates can be negative around the horizontal seam.
    int first = int(std::floor(double(left) / chunk_size));
    int last = int(std::floor(double(right) / chunk_size));
    for (int x = first; x <= last; ++x)
        for (int y = top; y <= bottom; ++y) {
            int cx = wrap(x, settings.columns);
            chunk(cx * chunk_size, y * chunk_size);
        }
    for (auto it = chunks.begin(); it != chunks.end();) {
        auto [x, y] = it->first;
        if (wrap(x - first, settings.columns) <= last - first && y >= top && y <= bottom) { ++it; continue; }
        if (it->second.dirty) save.stage(it->first.first, it->first.second, it->second);
        it = chunks.erase(it);
    }
}

void World::checkpoint(const Player& player) {
    for (auto& [key, c] : chunks) if (c.dirty) save.stage(key.first, key.second, c);
    save.checkpoint(player);
    for (auto& [key, c] : chunks) c.dirty = false;
}

bool intersects(World& world, double x, double y, double height) {
    for (int bx = int(std::floor(x)); bx <= int(std::floor(x + player_width - contact_epsilon)); ++bx)
        for (int by = int(std::floor(y)); by <= int(std::floor(y + height - contact_epsilon)); ++by)
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
            return p;
        }
    }
    throw std::runtime_error("No clear starting position.");
}

Target target(const Player& p) {
    int x = int(std::floor(p.x + player_width / 2));
    int y = int(std::floor(p.y + p.height() / 2));
    if (p.down) y = int(std::floor(p.y + p.height() + contact_epsilon));
    else if (p.facing > 0) x = int(std::ceil(p.x + player_width - contact_epsilon));
    else if (p.facing < 0) x = int(std::floor(p.x + contact_epsilon)) - 1;
    return {x, y, p.facing == 0 ? Layer::background : Layer::foreground};
}

void crouch(World& world, Player& p, bool down) {
    p.down = down;
    if (p.crouched != down) {
        double feet = p.y + p.height();
        // Keep the feet still. A low ceiling prevents standing up.
        if (down || !intersects(world, p.x, feet - player_height)) {
            p.crouched = down;
            p.y = feet - p.height();
        }
    }
}

void move(World& world, Player& p, int direction, bool jump, bool down, bool sprint) {
    crouch(world, p, down);
    double height = p.height();
    p.grounded = intersects(world, p.x, p.y + 0.025, height);
    if (jump && p.grounded) { p.vy = -11.5; p.grounded = false; }
    double speed = p.crouched ? 2.0 : (sprint ? 9.0 : 6.0);
    double dx = direction * speed * tick_seconds;
    if (!intersects(world, p.x + dx, p.y, height)) p.x += dx;
    else if (direction && p.grounded && !jump && !intersects(world, p.x + dx, p.y - 1, height)) {
        p.x += dx;
        p.y -= 1;
    }
    p.vy = std::min(22.0, p.vy + 26 * tick_seconds);
    double dy = p.vy * tick_seconds;
    if (!intersects(world, p.x, p.y + dy, height)) p.y += dy;
    else {
        if (dy > 0) p.y = std::floor(p.y + height + dy) - height;
        else p.y = std::floor(p.y + dy) + 1;
        p.grounded = dy > 0;
        p.vy = 0;
    }
    p.x = wrap(p.x, double(world.settings.width()));
}

ActionResult act(World& world, Player& p, Action action, int x, int y, Layer layer) {
    if (y < 0 || y >= world.settings.height()) return {false, "World boundary"};
    Cell old = world.cell(x, y, layer);
    if (action == Action::mine) {
        if (old.id.empty()) return {false, "Empty space"};
        const auto& needed = world.content.material(old.id).requires_tool;
        if (!needed.empty() && world.content.material(p.selected).tool != needed)
            return {false, "Requires a " + needed};
        p.inventory[old.id] += old.quantity;
        world.set(x, y, {}, layer);
    } else {
        if (!old.id.empty()) return {false, "Already occupied"};
        int quantity = layer == Layer::foreground ? placement_quantity(world, p.selected, x, y) : 1;
        if (quantity == 0) return {false, "Check the workbench and ingredient batch sizes"};
        if (p.inventory.at(p.selected) < quantity) return {false, "Need " + std::to_string(quantity) + " of this material"};
        world.put(x, y, Cell{p.selected, quantity, true}, layer);
        p.inventory[p.selected] -= quantity;
    }
    if (layer == Layer::foreground)
        if (auto result = craft(world, p, x, y)) return *result;
    return {true, action == Action::mine ? "Gathered x" + std::to_string(old.quantity) : "Placed"};
}

ActionResult adjust_workbench(World& world, int x, int y, Layer layer) {
    auto cell = world.cell(x, y, layer);
    if (world.content.material(cell.id).station != "workbench") return {false, "Nothing to interact with"};
    cell.batch = cell.batch == 1 ? 5 : (cell.batch == 5 ? 10 : 1);
    world.put(x, y, cell, layer);
    return {true, "Workbench batch x" + std::to_string(cell.batch)};
}

std::vector<Block> slots(const Player& p) {
    std::vector<Block> result;
    for (const auto& [id, count] : p.inventory)
        if (count > 0 || std::ranges::find(p.ready, id) != p.ready.end()) result.push_back(id);
    return result;
}

std::vector<Block> readied(const Player& p) {
    std::vector<Block> result;
    for (const auto& id : p.ready)
        if (p.inventory.at(id) > 0) result.push_back(id);
    return result;
}

}
