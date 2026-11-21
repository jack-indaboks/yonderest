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
    : settings(std::move(settings)), content(content), save(save) {}

Chunk& World::chunk(int x, int y) {
    auto key = std::pair{x / chunk_size, y / chunk_size};
    if (auto found = existing(key.first, key.second)) return *found;
    return chunks.emplace(key, generate(settings, key.first, key.second, boundary(key.first, key.second))).first->second;
}

// A loaded or saved chunk. Never generates one.
Chunk* World::existing(int cx, int cy) {
    auto key = std::pair{cx, cy};
    auto found = chunks.find(key);
    if (found != chunks.end()) return &found->second;
    auto saved = save.chunk(cx, cy);
    return saved ? &chunks.emplace(key, std::move(*saved)).first->second : nullptr;
}

std::vector<Boundary> World::boundary(int cx, int cy) {
    std::vector<Boundary> result;
    bool materials = profile_rank(settings.terrain.version) >= profile_rank("expedition-v1");
    if (!materials) return result;
    for (int oy = -1; oy <= 1; ++oy)
        for (int ox = -1; ox <= 1; ++ox) {
            if ((!ox && !oy) || cy + oy < 0 || cy + oy >= settings.rows) continue;
            const auto* neighbor = existing(wrap(cx + ox, settings.columns), cy + oy);
            if (!neighbor) continue;
            bool old = profile_rank(neighbor->profile) < profile_rank("expedition-v1");
            // Only the neighbor's cells touching this chunk carry forward.
            for (int ly = oy < 0 ? chunk_size - 1 : 0; ly <= (oy > 0 ? 0 : chunk_size - 1); ++ly)
                for (int lx = ox < 0 ? chunk_size - 1 : 0; lx <= (ox > 0 ? 0 : chunk_size - 1); ++lx) {
                    int settled = old ? 0 : neighbor->settling.empty() ? settle_extent
                        : neighbor->settling[ly * chunk_size + lx];
                    result.push_back({ox * chunk_size + lx, oy * chunk_size + ly, settled});
                }
        }
    return result;
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
        if (it->second.dirty) save.write_chunk(it->first.first, it->first.second, it->second);
        it = chunks.erase(it);
    }
}

bool World::save_changes(const Player& player) {
    bool wrote = false;
    for (auto& [key, c] : chunks)
        if (c.dirty) {
            save.write_chunk(key.first, key.second, c);
            c.dirty = false;
            wrote = true;
        }
    return save.write_player(player) || wrote;
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
            return p;
        }
    }
    throw std::runtime_error("No clear starting position.");
}

Target target(const Player& p) {
    int x = int(std::floor(p.x + player_width / 2));
    int y = int(std::floor(p.y + player_height / 2));
    if (p.down) y = int(std::floor(p.y + player_height + contact_epsilon));
    else if (p.facing > 0) x = int(std::ceil(p.x + player_width - contact_epsilon));
    else if (p.facing < 0) x = int(std::floor(p.x + contact_epsilon)) - 1;
    return {x, y, p.facing == 0 ? Layer::background : Layer::foreground};
}

// The jump's height is liked as it is; both halves of the arc are kept brisk around it.
constexpr double jump_height = 2.54, rise_gravity = 44, fall_gravity = 52, terminal_speed = 28;

void move(World& world, Player& p, int direction, bool jump, bool sprint) {
    p.grounded = intersects(world, p.x, p.y + 0.025);
    if (jump && p.grounded) { p.vy = -std::sqrt(2 * rise_gravity * jump_height); p.grounded = false; }
    double speed = p.down ? 2.0 : (sprint ? 9.0 : 6.0);
    double dx = direction * speed * tick_seconds;
    // No automatic climbing: a step up is a jump.
    if (!intersects(world, p.x + dx, p.y)) p.x += dx;
    p.vy = std::min(terminal_speed, p.vy + (p.vy < 0 ? rise_gravity : fall_gravity) * tick_seconds);
    double dy = p.vy * tick_seconds;
    if (!intersects(world, p.x, p.y + dy)) p.y += dy;
    else {
        if (dy > 0) p.y = std::floor(p.y + player_height + dy) - player_height;
        else p.y = std::floor(p.y + dy) + 1;
        p.grounded = dy > 0;
        p.vy = 0;
    }
    p.x = wrap(p.x, double(world.settings.width()));
}

ActionResult gather(World& world, Player& p, int x, int y, Layer layer) {
    if (y < 0 || y >= world.settings.height()) return {false, "World boundary"};
    Cell old = world.cell(x, y, layer);
    if (old.id.empty()) return {false, "Empty space", true};
    const auto& needed = world.content.material(old.id).requires_tool;
    if (!needed.empty() && world.content.material(p.selected).tool != needed) return {false, "Requires a " + needed};
    p.inventory[old.id] += old.quantity;
    world.set(x, y, {}, layer);
    // Taking a piece out can complete a shape that needs that cell empty.
    if (layer == Layer::foreground)
        if (auto result = craft(world, p, x, y)) return *result;
    return {true, "Gathered x" + std::to_string(old.quantity), true};
}

ActionResult place_at(World& world, Player& p, int x, int y, Layer layer) {
    if (y < 0 || y >= world.settings.height()) return {false, "World boundary"};
    if (!world.block(x, y, layer).empty()) return {false, "Already occupied"};
    int quantity = layer == Layer::foreground ? placement_quantity(world, p.selected, x, y) : 1;
    if (quantity == 0) return {false, "Check the workbench and ingredient batch sizes"};
    if (p.inventory.at(p.selected) < quantity) return {false, "Need " + std::to_string(quantity) + " of this material"};
    world.put(x, y, Cell{p.selected, quantity, true}, layer);
    p.inventory[p.selected] -= quantity;
    if (layer == Layer::foreground)
        if (auto result = craft(world, p, x, y)) return *result;
    return {true, "Placed", true};
}

ActionResult adjust_workbench(World& world, int x, int y, Layer layer) {
    auto cell = world.cell(x, y, layer);
    if (world.content.material(cell.id).station != "workbench") return {false, "Nothing to interact with"};
    cell.batch = cell.batch == 1 ? 5 : (cell.batch == 5 ? 10 : 1);
    world.put(x, y, cell, layer);
    return {true, "Workbench batch x" + std::to_string(cell.batch)};
}

void drop_empty(Player& p) {
    std::erase_if(p.inventory, [](const auto& entry) { return entry.second <= 0; });
    std::erase_if(p.ready, [&](const Block& id) { return !p.inventory.contains(id); });
    if (!p.selected.empty() && !p.inventory.contains(p.selected)) {
        p.last_selected = p.selected;
        p.selected.clear();
    }
}

std::vector<Block> slots(const Player& p) {
    std::vector<Block> result;
    for (const auto& [id, count] : p.inventory) result.push_back(id);
    return result;
}

}
