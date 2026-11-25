#include "world.hpp"
#include "save.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace yonder {
namespace {
constexpr double contact_epsilon = 0.00001;
}

// How far beyond the view chunks are made ready, and how far beyond that they stay loaded.
// The first decides how much unvisited territory counts as explored; it must also stay ahead
// of a sprint and a fall.
constexpr int load_margin = 24, keep_margin = 48;

World::World(Save& save, const WorldFolder& folder, const Content& content, Settings settings)
    : settings(std::move(settings)), content(content), save(save), folder(folder) {}

World::~World() {
    // Jobs still queued refer to this world.
    save.worker.wait();
}

Chunk* World::loaded(int x, int y) {
    if (y < 0 || y >= settings.height()) return nullptr;
    x = wrap(x, settings.width());
    auto found = chunks.find({x / chunk_size, y / chunk_size});
    return found == chunks.end() ? nullptr : &found->second;
}

// The worker's view of a chunk: its own copy while the game holds it, otherwise the saved file.
std::shared_ptr<const Chunk> World::neighbor(int cx, int cy) {
    if (cy < 0 || cy >= settings.rows) return nullptr;
    cx = wrap(cx, settings.columns);
    if (auto found = known.find({cx, cy}); found != known.end()) return found->second;
    auto saved = folder.chunk(cx, cy);
    return saved ? std::make_shared<const Chunk>(std::move(*saved)) : nullptr;
}

void World::request(int cx, int cy) {
    if (!requested.insert({cx, cy}).second) return;
    save.worker.post([this, cx, cy] {
        auto saved = folder.chunk(cx, cy);
        Chunk chunk;
        if (saved) chunk = std::move(*saved);
        else {
            Neighborhood near;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy) near.chunks[dy + 1][dx + 1] = neighbor(cx + dx, cy + dy);
            chunk = generate(settings, content, cx, cy, near);
        }
        known[{cx, cy}] = std::make_shared<const Chunk>(chunk);
        std::lock_guard lock(mutex);
        arrived.emplace_back(Key{cx, cy}, std::move(chunk));
    });
}

void World::receive() {
    save.worker.check();
    std::vector<std::pair<Key, Chunk>> taken;
    {
        std::lock_guard lock(mutex);
        taken.swap(arrived);
    }
    for (auto& [key, chunk] : taken) {
        requested.erase(key);
        chunks.insert_or_assign(key, std::move(chunk));
    }
}

const Block& World::block(int x, int y, Layer layer) {
    return cell(x, y, layer).id;
}

const Cell& World::cell(int x, int y, Layer layer) {
    static const Cell empty;
    auto* c = loaded(x, y);
    if (!c) return empty;
    x = wrap(x, settings.width());
    return (layer == Layer::foreground ? c->tiles : c->background)[(y % chunk_size) * chunk_size + x % chunk_size];
}

bool World::ready(int x, int y) {
    return y < 0 || y >= settings.height() || loaded(x, y);
}

bool World::solid(int x, int y) {
    return y < 0 || y >= settings.height() || !loaded(x, y) || content.material(block(x, y)).solid;
}

int World::ground(int x) {
    x = wrap(x, settings.width());
    for (int cy = 0; cy < settings.rows; ++cy)
        if (auto* c = loaded(x, cy * chunk_size); c && !c->ground.empty()) return c->ground[x % chunk_size];
    return yonder::ground(settings, x);
}

void World::set(int x, int y, const Block& id, Layer layer) {
    put(x, y, Cell{id}, layer);
}

void World::put(int x, int y, const Cell& value, Layer layer) {
    auto* c = loaded(x, y);
    if (!c) throw std::logic_error("Changing a cell of a chunk that is not loaded");
    x = wrap(x, settings.width());
    (layer == Layer::foreground ? c->tiles : c->background)[(y % chunk_size) * chunk_size + x % chunk_size] = value;
    c->dirty = true;
}

void World::retain(int left, int top, int right, int bottom) {
    receive();
    auto chunk_range = [&](int margin) {
        // Block coordinates can be negative around the horizontal seam.
        return std::array{int(std::floor(double(left - margin) / chunk_size)), std::max(0, top - margin) / chunk_size,
                          int(std::floor(double(right + margin) / chunk_size)),
                          std::min(settings.height() - 1, bottom + margin) / chunk_size};
    };
    auto [first, high, last, low] = chunk_range(load_margin);
    // Nearest the view first.
    double mx = (left + right) / 2.0 / chunk_size - 0.5, my = (top + bottom) / 2.0 / chunk_size - 0.5;
    std::vector<std::pair<double, Key>> wanted;
    for (int x = first; x <= last; ++x)
        for (int y = high; y <= low; ++y) {
            Key key{wrap(x, settings.columns), y};
            if (!chunks.contains(key)) wanted.emplace_back(std::hypot(x - mx, y - my), key);
        }
    std::ranges::sort(wanted);
    for (auto [distance, key] : wanted) request(key.first, key.second);
    auto [keep_first, keep_high, keep_last, keep_low] = chunk_range(keep_margin);
    for (auto it = chunks.begin(); it != chunks.end();) {
        auto [x, y] = it->first;
        if (wrap(x - keep_first, settings.columns) <= keep_last - keep_first && y >= keep_high && y <= keep_low) { ++it; continue; }
        auto key = it->first;
        if (it->second.dirty)
            save.worker.post([this, key, chunk = std::move(it->second)] {
                folder.write_chunk(key.first, key.second, chunk);
                known.erase(key);
            });
        else save.worker.post([this, key] { known.erase(key); });
        it = chunks.erase(it);
    }
}

void World::load(int left, int top, int right, int bottom) {
    for (int x = int(std::floor(double(left) / chunk_size)); x <= int(std::floor(double(right) / chunk_size)); ++x)
        for (int y = std::max(0, top) / chunk_size; y <= std::min(settings.height() - 1, bottom) / chunk_size; ++y)
            if (!chunks.contains({wrap(x, settings.columns), y})) request(wrap(x, settings.columns), y);
    save.worker.wait();
    receive();
}

bool World::save_changes(const Player& player) {
    bool wrote = false;
    for (auto& [key, c] : chunks)
        if (c.dirty) {
            c.dirty = false;
            save.worker.post([this, key, chunk = std::make_shared<const Chunk>(c)] {
                folder.write_chunk(key.first, key.second, *chunk);
                known[key] = chunk;
            });
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
        int h = ground(world.settings, x);
        world.load(x - chunk_size / 2, h - chunk_size / 2, x + chunk_size / 2, h + chunk_size / 2);
        p.x = x + 0.15;
        p.y = world.ground(x) - player_height;
        if (!intersects(world, p.x, p.y)) return p;
    }
    throw std::runtime_error("No clear starting position.");
}

Target target(const Player& p) {
    int x = int(std::floor(p.x + player_width / 2));
    int y = int(std::floor(p.y + player_height / 2));
    // A jump aims one up from the moment it leaves the ground, not once the body is halfway there.
    if (p.vy < 0 && y == p.takeoff) --y;
    // A cell the body overlaps by no more than the margin counts as beyond the edge.
    if (p.up) y = int(std::floor(p.y + margin)) - 1;
    else if (p.down) y = int(std::ceil(p.y + player_height - margin));
    else if (p.facing > 0) x = int(std::ceil(p.x + player_width - margin));
    else if (p.facing < 0) x = int(std::floor(p.x + margin)) - 1;
    return {x, y, p.facing == 0 && !p.up ? Layer::background : Layer::foreground};
}

Target favor(World& world, const Player& p, Target t, bool occupied) {
    if (p.grounded || p.up || p.down) return t;
    auto fits = [&](int y) { return world.ready(t.x, y) && world.block(t.x, y, t.layer).empty() != occupied; };
    if (fits(t.y)) return t;
    int top = int(std::floor(p.y));
    for (int y : {top, top + 1}) {
        double overlap = std::min(p.y + player_height, y + 1.0) - std::max(p.y, double(y));
        if (y != t.y && overlap >= 0.25 && fits(y)) return {t.x, y, t.layer};
    }
    return t;
}

Target reach(World& world, const Player& p) {
    Target own{int(std::floor(p.x + player_width / 2)), int(std::floor(p.y + player_height / 2))};
    return world.block(own.x, own.y).empty() ? favor(world, p, target(p), true) : own;
}

bool clear_of(World& world, Player& p, int x, int y) {
    double left = x - (p.x + player_width), right = x + 1 - p.x;
    double up = y - (p.y + player_height), down = y + 1 - p.y;
    if (left >= 0 || right <= 0 || up >= 0 || down <= 0) return true;
    std::optional<std::pair<double, double>> best;
    for (auto [sx, sy] : {std::pair{left, 0.0}, {right, 0.0}, {0.0, up}, {0.0, down}})
        if (std::abs(sx + sy) <= margin + contact_epsilon && !intersects(world, p.x + sx, p.y + sy)
            && (!best || std::abs(sx + sy) < std::abs(best->first + best->second)))
            best = {sx, sy};
    if (!best) return false;
    p.x = wrap(p.x + best->first, double(world.settings.width()));
    p.y += best->second;
    return true;
}

// The jump's height is liked as it is; both halves of the arc are kept brisk around it.
constexpr double jump_height = 2.54, rise_gravity = 44, fall_gravity = 52, terminal_speed = 28;

// The smallest shift of at most the margin, never against `way` (-1, 0 for either, or 1), that
// brings the body's span [low, low + size) into line with a cell boundary where the body fits.
template <class Fits>
std::optional<double> slip(double low, double size, int way, Fits fits) {
    std::optional<double> best;
    for (double shift : {std::ceil(low) - low, std::floor(low + size) - (low + size)})
        if (shift != 0 && std::abs(shift) <= margin && shift * way >= 0 && fits(shift)
            && (!best || std::abs(shift) < std::abs(*best)))
            best = shift;
    return best;
}

void move(World& world, Player& p, int direction, bool jump, bool sprint) {
    p.grounded = intersects(world, p.x, p.y + 0.025);
    if (jump && p.grounded) {
        p.vy = -std::sqrt(2 * rise_gravity * jump_height);
        p.grounded = false;
        p.takeoff = int(std::floor(p.y + player_height / 2));
    }
    double speed = p.down ? 2.0 : (sprint ? 9.0 : 6.0);
    double dx = direction * speed * tick_seconds;
    // No automatic climbing: a step up is a jump. A body in the air slips past a corner it
    // catches by no more than the margin, onward in the way it is already moving and never back,
    // so a jump can enter an opening just its own height and a landing is never refused.
    if (!intersects(world, p.x + dx, p.y)) p.x += dx;
    else if (auto shift = slip(p.y, player_height, p.vy < 0 ? -1 : 1, [&](double s) { return !intersects(world, p.x + dx, p.y + s); })) {
        p.x += dx;
        p.y += *shift;
    }
    p.vy = std::min(terminal_speed, p.vy + (p.vy < 0 ? rise_gravity : fall_gravity) * tick_seconds);
    double dy = p.vy * tick_seconds;
    if (!intersects(world, p.x, p.y + dy)) p.y += dy;
    else if (auto shift = dy < 0 ? slip(p.x, player_width, direction, [&](double s) { return !intersects(world, p.x + s, p.y + dy); })
                                 : std::nullopt) {
        p.x += *shift;
        p.y += dy;
    } else {
        if (dy > 0) p.y = std::floor(p.y + player_height + dy) - player_height;
        else p.y = std::floor(p.y + dy) + 1;
        p.grounded = dy > 0;
        p.vy = 0;
    }
    if (p.grounded) p.takeoff = -1;
    p.x = wrap(p.x, double(world.settings.width()));
}

ActionResult gather(World& world, Player& p, int x, int y, Layer layer) {
    if (y < 0 || y >= world.settings.height()) return {false, "World boundary"};
    Cell old = world.cell(x, y, layer);
    if (old.id.empty()) return {false, "Empty space", true};
    const auto& needed = world.content.material(old.id).requires_tool;
    if (!needed.empty() && world.content.material(p.selected).tool != needed) return {false, "Requires a " + needed};
    carry(p, old.id, old.quantity);
    world.set(x, y, {}, layer);
    // Taking a piece out can complete a shape that needs that cell empty.
    if (layer == Layer::foreground)
        if (auto result = craft(world, p, x, y)) return *result;
    return {true, "Gathered x" + std::to_string(old.quantity), true};
}

ActionResult place_at(World& world, Player& p, int x, int y, Layer layer) {
    if (y < 0 || y >= world.settings.height()) return {false, "World boundary"};
    if (!world.ready(x, y)) return {false, "Not there yet"};
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

void carry(Player& p, const Block& id, std::int64_t count) {
    if (!p.inventory.contains(id) && p.ready.size() < belt_size) p.ready.push_back(id);
    p.inventory[id] += count;
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
