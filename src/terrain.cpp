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

int profile_rank(const std::string& profile) {
    if (profile == "shelter-v1") return 1;
    if (profile == "expedition-v1") return 2;
    if (profile == "expedition-v2") return 3;
    if (profile == "expedition-v3") return 4;
    return 0;
}

namespace {

// Regions that change materials. Elsewhere: moss groves, or sand where it is dry.
enum class Region { plain, rocky_uplands, claybank_groves };
Region region(const Settings& s, int x) {
    int h = surface(s, x);
    if (h < s.height() * 0.43 - 5) return Region::rocky_uplands;
    if (h > s.height() * 0.43 + 4 && humidity(s, x) > 0.2) return Region::claybank_groves;
    return Region::plain;
}

// Each cell may differ from a boundary cell by at most their distance. Where
// the neighbors agree, the lower envelope satisfies every one of them; where
// they cannot all be met, split the difference rather than tear one edge.
std::vector<std::uint8_t> settling(const std::vector<Boundary>& boundary) {
    if (std::ranges::all_of(boundary, [](const Boundary& b) { return b.settled >= settle_extent; })) return {};
    std::vector<std::uint8_t> result(chunk_size * chunk_size);
    for (int y = 0; y < chunk_size; ++y)
        for (int x = 0; x < chunk_size; ++x) {
            double lower = settle_extent, upper = 0;
            for (const auto& b : boundary) {
                double d = std::hypot(x - b.x, y - b.y);
                lower = std::min(lower, b.settled + d);
                upper = std::max(upper, b.settled - d);
            }
            result[y * chunk_size + x] = std::uint8_t(std::lround(lower >= upper ? lower : (lower + upper) / 2));
        }
    return result;
}

}

Chunk generate(const Settings& s, int cx, int cy, const std::vector<Boundary>& boundary) {
    Chunk chunk;
    chunk.profile = s.terrain.version;
    const auto& t = s.terrain;
    int rank = profile_rank(t.version);
    if (rank >= 2) chunk.settling = settling(boundary);
    for (int lx = 0; lx < chunk_size; ++lx) {
        int x = cx * chunk_size + lx;
        int h = surface(s, x);
        bool dry = humidity(s, x) < -0.25;
        auto here = region(s, x);
        for (int ly = 0; ly < chunk_size; ++ly) {
            int y = cy * chunk_size + ly;
            Block id, rear;
            if (y >= h) {
                Block ground = y == h ? (dry ? t.sand : t.grass) : (y < h + 5 ? (dry ? t.sand : t.soil) : t.stone);
                // Expedition generations change materials, not landform or cave geometry.
                if (rank >= 2) {
                    Block newer = ground;
                    auto patch = hash(s.seed ^ (std::uint64_t(wrap(x, s.width()) / 5) * 971)
                                             ^ (std::uint64_t(y / 3) * 12983));
                    if (here == Region::rocky_uplands && y < h + 5) newer = t.stone;
                    if (here == Region::rocky_uplands && y >= h + 1 && y < h + 48 && patch % 7 < 2) newer = t.ore;
                    if (here == Region::claybank_groves && y >= h + 1 && y < h + 48
                        && (y < h + 5 || patch % 7 < 2)) newer = t.clay;
                    double threshold = (hash(s.seed ^ (std::uint64_t(wrap(x, s.width())) * 972111)
                                                    ^ (std::uint64_t(y) * 178991)) % 100000) / 100000.0;
                    double settled = chunk.settling.empty() ? 1.0
                        : chunk.settling[ly * chunk_size + lx] / double(settle_extent);
                    if (threshold < settled * settled * (3 - 2 * settled)) ground = newer;
                }
                id = ground;
                if (y > h + 10 && y < s.height() - 3 && underground(s, x, y) > 0.35) id.clear();
                // Natural backing is the host material, not a second ore deposit.
                if (rank >= 3 && y > h) rear = ground == t.ore ? t.stone : ground;
            } else {
                for (int cell = x / 16 - 1; cell <= x / 16 + 1; ++cell) {
                    int tx = tree(s, cell);
                    if (tx < 0) continue;
                    int th = surface(s, tx);
                    int dx = std::abs(int(distance_x(tx, x, s.width())));
                    bool trunk = dx == 0 && y >= th - 4 && y < th;
                    bool canopy = dx <= 2 && y >= th - 6 && y <= th - 3 && dx + std::abs(y - (th - 4)) <= 3;
                    // From v3 the trunk stands behind, where the character can walk past it,
                    // and the canopy stays in front, around its top as well.
                    if (trunk && rank >= 4) rear = t.timber;
                    if (trunk && rank < 4) id = t.timber;
                    else if (canopy) id = t.leaves;
                }
            }
            chunk.at(lx, ly) = Cell{id};
            chunk.background[ly * chunk_size + lx] = Cell{rear};
        }
    }
    chunk.dirty = true;
    return chunk;
}

}
