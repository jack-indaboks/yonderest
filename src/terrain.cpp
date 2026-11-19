#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace yonder {

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

std::string biome(const Settings& s, int x, const std::string& profile) {
    double wet = humidity(s, x);
    int h = surface(s, x);
    if (profile == "expedition-v1" && h < s.height() * 0.43 - 5) return "Rocky uplands";
    if (wet < -0.25) return "Sandreach";
    if (profile == "expedition-v1" && h > s.height() * 0.43 + 4 && wet > 0.2) return "Claybank groves";
    return "Moss groves";
}

namespace {

double transition(const Settings& s, int x, int y, const std::vector<std::pair<int, int>>& legacy) {
    constexpr double extent = 2 * chunk_size;
    double distance = extent;
    for (auto [cx, cy] : legacy) {
        double dx = std::max(0.0, std::abs(distance_x(cx * chunk_size + (chunk_size - 1) / 2.0, x, s.width())) - (chunk_size - 1) / 2.0);
        double dy = std::max(0.0, std::abs(y - (cy * chunk_size + (chunk_size - 1) / 2.0)) - (chunk_size - 1) / 2.0);
        distance = std::min(distance, std::hypot(dx, dy));
    }
    double t = distance / extent;
    return t * t * (3 - 2 * t);
}

}

Chunk generate(const Settings& s, int cx, int cy, const std::vector<std::pair<int, int>>& legacy) {
    Chunk chunk;
    chunk.profile = s.terrain.version;
    const auto& t = s.terrain;
    for (int lx = 0; lx < chunk_size; ++lx) {
        int x = cx * chunk_size + lx;
        int h = surface(s, x);
        bool dry = humidity(s, x) < -0.25;
        auto region = biome(s, x, t.version);
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
            // The first update changes materials, not landform or cave geometry.
            // A fixed old-territory footprint makes this independent of the
            // order in which new chunks are encountered, in both axes.
            if (t.version == "expedition-v1" && !id.empty() && y >= h) {
                Block newer = id;
                auto patch = hash(s.seed ^ (std::uint64_t(wrap(x, s.width()) / 5) * 971)
                                         ^ (std::uint64_t(y / 3) * 12983));
                if (region == "Rocky uplands" && y < h + 5) newer = t.stone;
                if (region == "Rocky uplands" && y >= h + 1 && y < h + 48 && patch % 7 < 2) newer = t.ore;
                if (region == "Claybank groves" && y >= h + 1 && y < h + 48
                    && (y < h + 5 || patch % 7 < 2)) newer = t.clay;
                double threshold = (hash(s.seed ^ (std::uint64_t(wrap(x, s.width())) * 972111)
                                                ^ (std::uint64_t(y) * 178991)) % 100000) / 100000.0;
                if (threshold < transition(s, x, y, legacy)) id = newer;
            }
            chunk.at(lx, ly) = Cell{id};
        }
    }
    chunk.dirty = true;
    return chunk;
}

}
