#include "world.hpp"
#include "save.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numbers>
#include <set>
#include <sstream>
#include <stdexcept>

namespace yonder {

void Content::load(const std::filesystem::path& pack) {
    std::ifstream file(pack / "blocks.txt");
    if (!file) throw std::runtime_error("Cannot read " + (pack / "blocks.txt").string());
    std::string line;
    int number = 0;
    while (std::getline(file, line)) {
        ++number;
        if (line.find_first_not_of(" \t\r") == std::string::npos || line[line.find_first_not_of(" \t")] == '#') continue;
        std::istringstream in(line);
        Block id;
        Material m;
        int solid;
        if (!(in >> id >> m.color[0] >> m.color[1] >> m.color[2] >> solid >> std::quoted(m.name))
            || id.find(':') == std::string::npos || (solid != 0 && solid != 1)
            || std::ranges::any_of(m.color, [](int c) { return c < 0 || c > 255; }))
            throw std::runtime_error("Invalid block in " + pack.string() + ":" + std::to_string(number));
        m.solid = solid;
        if (!blocks.emplace(id, m).second) throw std::runtime_error("Duplicate block ID: " + id);
    }
}

const Material& Content::material(const Block& id) const {
    static const Material air{{0, 0, 0}, false, "Air"};
    static const Material missing{{151, 104, 88}, true, "Unknown"};
    if (id.empty()) return air;
    auto found = blocks.find(id);
    return found == blocks.end() ? missing : found->second;
}

Terrain load_terrain(const std::filesystem::path& pack) {
    std::ifstream file(pack / "terrain.txt");
    if (!file) throw std::runtime_error("Cannot read " + (pack / "terrain.txt").string());
    std::string line, text;
    while (std::getline(file, line)) if (!line.starts_with('#')) text += line + '\n';
    std::istringstream in(text);
    Terrain t;
    if (!(in >> t.version >> t.grass >> t.soil >> t.stone >> t.sand >> t.timber >> t.leaves)
        || t.version != "shelter-v1") throw std::runtime_error("Unsupported terrain definition in " + pack.string());
    return t;
}

int wrap(int x, int width) { return ((x % width) + width) % width; }
double wrap(double x, double width) { return x - std::floor(x / width) * width; }
double distance_x(double from, double to, int width) {
    return wrap(to - from + width / 2.0, double(width)) - width / 2.0;
}

namespace {

std::uint64_t hash(std::uint64_t n) {
    n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ULL;
    n = (n ^ (n >> 27)) * 0x94d049bb133111ebULL;
    return n ^ (n >> 31);
}

double phase(const Settings& s) {
    return (hash(s.seed) % 10000) * (2 * std::numbers::pi / 10000);
}

double humidity(const Settings& s, int x) {
    double angle = wrap(x, s.width()) * (2 * std::numbers::pi / s.width());
    return std::sin(angle * 2 + phase(s) + 1.7);
}

double underground(const Settings& s, int x, int y) {
    // Smooth lattice field: its horizontal lattice wraps, not its output.
    x = wrap(x, s.width());
    int cx = x / 32, cy = y / 24;
    double fx = (x % 32) / 32.0, fy = (y % 24) / 24.0;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    auto sample = [&](int dx, int dy) {
        auto n = hash(s.seed ^ (std::uint64_t(wrap(cx + dx, s.columns)) * 92837111)
                           ^ (std::uint64_t(cy + dy) * 689287499));
        return (n % 10000) / 5000.0 - 1;
    };
    return std::lerp(std::lerp(sample(0, 0), sample(1, 0), fx),
                     std::lerp(sample(0, 1), sample(1, 1), fx), fy);
}

int tree(const Settings& s, int cell) {
    cell = wrap(cell, s.width() / 16);
    int x = cell * 16 + 3 + int(hash(s.seed + cell * 1777) % 10);
    if (humidity(s, x) < 0.2 || std::abs(surface(s, x - 1) - surface(s, x + 1)) > 1) return -1;
    return x;
}

}

int surface(const Settings& s, int x) {
    double angle = wrap(x, s.width()) * (2 * std::numbers::pi / s.width());
    double p = phase(s);
    return int(s.height() * 0.43 + 13 * std::sin(angle + p)
               + 7 * std::sin(3 * angle + p * 2) + 3 * std::sin(9 * angle - p));
}

Chunk generate(const Settings& s, int cx, int cy) {
    Chunk chunk;
    const auto& t = s.terrain;
    for (int lx = 0; lx < chunk_size; ++lx) {
        int x = cx * chunk_size + lx;
        int h = surface(s, x);
        bool dry = humidity(s, x) < -0.25;
        for (int ly = 0; ly < chunk_size; ++ly) {
            int y = cy * chunk_size + ly;
            Block id;
            if (y >= h) {
                id = y == h ? (dry ? t.sand : t.grass) : (y < h + 5 ? (dry ? t.sand : t.soil) : t.stone);
                if (y > h + 10 && y < s.height() - 3 && underground(s, x, y) > 0.35) id.clear();
            } else {
                for (int cell = x / 16 - 1; cell <= x / 16 + 1; ++cell) {
                    int tx = tree(s, cell);
                    if (tx < 0) continue;
                    int th = surface(s, tx);
                    int dx = std::abs(int(distance_x(tx, x, s.width())));
                    if (dx == 0 && y >= th - 4 && y < th) id = t.timber;
                    else if (dx <= 2 && y >= th - 6 && y <= th - 3 && dx + std::abs(y - (th - 4)) <= 3)
                        id = t.leaves;
                }
            }
            chunk.at(lx, ly) = id;
        }
    }
    chunk.dirty = true;
    return chunk;
}

World::World(Save& save, const Content& content, Settings settings)
    : settings(std::move(settings)), content(content), save(save) {}

Chunk& World::chunk(int x, int y) {
    auto key = std::pair{x / chunk_size, y / chunk_size};
    auto found = chunks.find(key);
    if (found != chunks.end()) return found->second;
    auto saved = save.chunk(key.first, key.second);
    return chunks.emplace(key, saved ? std::move(*saved) : generate(settings, key.first, key.second)).first->second;
}

Block World::block(int x, int y) {
    if (y < 0 || y >= settings.height()) return {};
    x = wrap(x, settings.width());
    return chunk(x, y).at(x % chunk_size, y % chunk_size);
}

bool World::solid(int x, int y) {
    return y < 0 || y >= settings.height() || content.material(block(x, y)).solid;
}

void World::set(int x, int y, const Block& id) {
    x = wrap(x, settings.width());
    auto& c = chunk(x, y);
    c.at(x % chunk_size, y % chunk_size) = id;
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
    constexpr double epsilon = 0.00001;
    for (int bx = int(std::floor(x)); bx <= int(std::floor(x + player_width - epsilon)); ++bx)
        for (int by = int(std::floor(y)); by <= int(std::floor(y + player_height - epsilon)); ++by)
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

std::string act(World& world, Player& p, Action action, int x, int y) {
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
    Block id = world.block(x, y);
    if (action == Action::mine) {
        if (id.empty()) return "Empty space";
        ++p.inventory[id];
        world.set(x, y, {});
    } else {
        if (!id.empty()) return "Already occupied";
        if (p.selected.empty() || p.inventory[p.selected] == 0) return "Gather this material first";
        if (std::abs(dx) < (player_width + 1) / 2 && std::abs(dy) < (player_height + 1) / 2)
            return "You are standing there";
        if (!(world.solid(x - 1, y) || world.solid(x + 1, y) || world.solid(x, y - 1) || world.solid(x, y + 1)))
            return "Build against an existing block";
        world.set(x, y, p.selected);
        --p.inventory[p.selected];
    }
    return {};
}

std::vector<Block> slots(const Content& content, const Player& p) {
    std::vector<Block> result;
    for (const auto& [id, m] : content.blocks) result.push_back(id);
    for (const auto& [id, count] : p.inventory) if (!content.blocks.contains(id)) result.push_back(id);
    if (!p.selected.empty() && !content.blocks.contains(p.selected) && !p.inventory.contains(p.selected)) result.push_back(p.selected);
    return result;
}

}
