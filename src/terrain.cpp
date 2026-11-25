#include "world.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

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
std::uint64_t hash(const Settings& s, std::uint64_t salt, int x, int y = 0) {
    return hash(s.seed ^ hash(salt ^ (std::uint64_t(std::uint32_t(x)) << 32) ^ std::uint32_t(y)));
}
double unit(std::uint64_t n) { return (n % 100001) / 50000.0 - 1; }
double fraction(std::uint64_t n) { return (n % 100000) / 100000.0; }
double smooth(double f) { return f * f * f * (f * (f * 6 - 15) + 10); }

// Smooth noise with features about `across` blocks apart horizontally and `down` blocks
// vertically. A whole number of features fits around the world, so the noise wraps exactly
// and a wider world holds more of them; a world narrower than one feature holds one.
double noise(const Settings& s, std::uint64_t salt, double across, double down, double x, double y = 0) {
    int n = std::max(1, int(std::lround(s.width() / across)));
    double u = wrap(x, double(s.width())) * n / s.width(), v = y / down;
    int i = int(u), j = int(std::floor(v));
    auto at = [&](int a, int b) { return unit(hash(s, salt, wrap(a, n), b)); };
    double fx = smooth(u - i), fy = smooth(v - j);
    return std::lerp(std::lerp(at(i, j), at(i + 1, j), fx), std::lerp(at(i, j + 1), at(i + 1, j + 1), fx), fy);
}
double wave(const Settings& s, std::uint64_t salt, double across, double x) { return noise(s, salt, across, 1, x); }

// Relief shrinks in short worlds so the ground keeps room above and below it.
double relief_scale(const Settings& s) { return std::clamp(s.height() / 1024.0, 0.125, 1.0); }

// How far the land rises above the world's ground level, before scaling: rolling country in
// some reaches, rugged hills in others. Most ground lies within about 40 blocks of the level,
// and nothing beyond 120.
double relief(const Settings& s, int x) {
    double rugged = 0.5 + 0.5 * wave(s, 11, 2048, x);
    return rugged * (80 * wave(s, 12, 512, x) + 30 * wave(s, 13, 160, x)) + 8 * wave(s, 14, 48, x) + 2 * wave(s, 15, 12, x);
}

double humidity(const Settings& s, int x) {
    return 0.8 * wave(s, 21, 1536, x) + 0.2 * wave(s, 22, 384, x);
}

// Provisional biomes, which change materials. Elsewhere: moss, or sand where it is dry.
enum class Biome { plain, rocky_uplands, claybank_groves };
Biome biome(const Settings& s, int x) {
    double r = relief(s, x);
    if (r > 32) return Biome::rocky_uplands;
    if (r < -22 && humidity(s, x) > 0.1) return Biome::claybank_groves;
    return Biome::plain;
}

int tree(const Settings& s, int cell) {
    cell = wrap(cell, s.width() / 16);
    int x = cell * 16 + 3 + int(hash(s, 41, cell) % 10);
    if (humidity(s, x) < 0.2 || std::abs(ground(s, x - 1) - ground(s, x + 1)) > 1) return -1;
    return x;
}

// Each cell may differ from a boundary cell by at most their distance. Where the neighbors
// agree, the lower envelope satisfies every one of them; where they cannot all be met, split
// the difference rather than tear one edge. Also says which boundary cell each value came from.
struct Point { int x, y, settled; };
struct Settling { std::vector<std::uint8_t> values; std::vector<int> source; };
Settling settle(const std::vector<Point>& boundary) {
    Settling result;
    if (std::ranges::all_of(boundary, [](const Point& b) { return b.settled >= settle_extent; })) return result;
    result.values.resize(chunk_area);
    result.source.resize(chunk_area);
    for (int y = 0; y < chunk_size; ++y)
        for (int x = 0; x < chunk_size; ++x) {
            double lower = settle_extent, upper = 0;
            int from = -1;
            for (int i = 0; i < int(boundary.size()); ++i) {
                const auto& b = boundary[i];
                double d = std::hypot(x - b.x, y - b.y);
                if (b.settled + d < lower) { lower = b.settled + d; from = i; }
                upper = std::max(upper, b.settled - d);
            }
            result.values[y * chunk_size + x] = std::uint8_t(std::lround(lower >= upper ? lower : (lower + upper) / 2));
            result.source[y * chunk_size + x] = from;
        }
    return result;
}

}

int ground(const Settings& s, int x) {
    int level = s.height() / 4;
    return std::clamp(int(std::lround(level - relief_scale(s) * relief(s, x))), 2, s.height() - 2);
}

std::pair<int, int> ground_span(const Chunk& c, const Content& content, int cy, int lx) {
    if (!c.ground.empty()) return {c.ground[lx], c.ground[lx]};
    int top = cy * chunk_size;
    // Built things stand on the ground; they are not it.
    for (int ly = 0; ly < chunk_size; ++ly)
        if (const auto& cell = c.at(lx, ly); !cell.placed && content.material(cell.id).solid) {
            if (ly == 0) return {INT_MIN / 2, top}; // At its top edge or higher.
            return {top + ly, top + ly};
        }
    return {top + chunk_size, INT_MAX / 2}; // Below its bottom edge.
}

Chunk generate(const Settings& s, const Content& content, int cx, int cy, const Neighborhood& near) {
    const auto& t = s.terrain;
    const int x0 = cx * chunk_size, y0 = cy * chunk_size;
    Chunk chunk;
    chunk.generation = t.version;

    // The ground: the generator's own, bent to meet the ground its neighbors carry. Beside
    // the chunk only the adjacent column counts, and the difference fades with distance as it
    // would across any number of chunks; above and below, the neighbor's own line is kept.
    std::array<int, chunk_size + 2> ordinary; // Columns -1 to chunk_size.
    for (int i = 0; i < chunk_size + 2; ++i) ordinary[i] = ground(s, x0 - 1 + i);
    auto meet = [&](std::initializer_list<std::pair<int, int>> offsets, int lx, int own) -> std::optional<int> {
        long sum = 0;
        int exact = 0, lo = INT_MIN, hi = INT_MAX;
        bool any = false;
        for (auto [dx, dy] : offsets)
            if (const auto* n = near.at(dx, dy)) {
                any = true;
                auto [a, b] = ground_span(*n, content, cy + dy, lx);
                if (a == b) { sum += a; ++exact; }
                else { lo = std::max(lo, a); hi = std::min(hi, b); }
            }
        if (!any) return std::nullopt;
        if (exact) return int(std::lround(double(sum) / exact));
        return lo <= hi ? std::clamp(own, lo, hi) : own;
    };
    auto left = meet({{-1, -1}, {-1, 0}, {-1, 1}}, chunk_size - 1, ordinary[0]);
    auto right = meet({{1, -1}, {1, 0}, {1, 1}}, 0, ordinary[chunk_size + 1]);
    constexpr double reach = 24; // Blocks over which a difference falls to about a third.
    const double span = chunk_size + 1;
    double l = left ? *left - ordinary[0] : 0, r = right ? *right - ordinary[chunk_size + 1] : 0;
    chunk.ground.resize(chunk_size);
    for (int lx = 0; lx < chunk_size; ++lx) {
        double p = lx + 1, offset = 0;
        if (left && right) offset = (l * std::sinh((span - p) / reach) + r * std::sinh(p / reach)) / std::sinh(span / reach);
        else if (left) offset = l * std::exp(-p / reach);
        else if (right) offset = r * std::exp(-(span - p) / reach);
        int h = ordinary[lx + 1] + int(std::lround(offset));
        if (auto vertical = meet({{0, -1}, {0, 1}}, lx, h)) h = *vertical;
        chunk.ground[lx] = std::clamp(h, 2, s.height() - 2);
    }

    // Materials settle the same way: near a neighbor that this generation did not make,
    // cells take that neighbor's material at the same depth below the ground.
    std::vector<Point> boundary;
    std::vector<std::pair<int, int>> sources; // The neighbor offset of each boundary cell.
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const auto* n = near.at(dx, dy);
            if (!n) continue;
            bool ours = n->generation == t.version;
            for (int ly = dy < 0 ? chunk_size - 1 : 0; ly <= (dy > 0 ? 0 : chunk_size - 1); ++ly)
                for (int lx = dx < 0 ? chunk_size - 1 : 0; lx <= (dx > 0 ? 0 : chunk_size - 1); ++lx) {
                    int settled = !ours ? 0 : n->settling.empty() ? settle_extent : n->settling[ly * chunk_size + lx];
                    boundary.push_back({dx * chunk_size + lx, dy * chunk_size + ly, settled});
                    sources.emplace_back(dx, dy);
                }
        }
    auto settling = settle(boundary);
    chunk.settling = settling.values;
    // A neighbor's natural cell at local coordinates outside this chunk, if that neighbor exists.
    auto natural = [&](int lx, int ly, Layer layer) -> const Cell* {
        int dx = int(std::floor(double(lx) / chunk_size)), dy = int(std::floor(double(ly) / chunk_size));
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1 || (!dx && !dy)) return nullptr;
        const auto* n = near.at(dx, dy);
        if (!n) return nullptr;
        int i = (ly - dy * chunk_size) * chunk_size + lx - dx * chunk_size;
        const auto& c = layer == Layer::foreground ? n->tiles[i] : n->background[i];
        return c.id.empty() || c.placed ? nullptr : &c;
    };

    const int bottom = s.height() - 3;
    for (int lx = 0; lx < chunk_size; ++lx) {
        int x = x0 + lx, h = chunk.ground[lx];
        bool dry = humidity(s, x) < -0.25;
        auto here = biome(s, x);
        double rise = relief(s, x);
        for (int ly = 0; ly < chunk_size; ++ly) {
            int y = y0 + ly, depth = y - h;
            Block id, rear;
            if (depth >= 0) {
                Block host = depth == 0 ? (dry ? t.sand : t.grass) : depth < 5 ? (dry ? t.sand : t.soil) : t.stone;
                if (here == Biome::rocky_uplands && depth < 5) host = t.stone;
                if (here == Biome::claybank_groves && depth >= 1 && depth < 5) host = t.clay;
                // Deposits near the surface lie in patches, copper in the uplands and clay in the
                // groves. Beneath the uplands their copper continues in thin veins, in roots that
                // taper with depth, so only the highest uplands reach deep.
                auto deposit = [&](std::uint64_t salt, int rate, double vein) {
                    if (depth < 1) return host;
                    if (depth < 48) {
                        if (hash(s, salt, wrap(x, s.width()) / 5, y / 3) % 7 >= std::uint64_t(rate)) return host;
                        return here == Biome::rocky_uplands ? t.ore : here == Biome::claybank_groves ? t.clay : host;
                    }
                    if (rise > 32 + (depth - 48) / 4.0 && std::abs(noise(s, salt, 48, 12, x, y)) < vein) return t.ore;
                    return host;
                };
                id = deposit(51, 2, 0.05);
                // The rear layer has deposits of its own, a little richer, and no caves.
                if (depth > 0) rear = deposit(52, 3, 0.07);
                // Near land this generation did not make, caves close up rather than end at its edge.
                int i = ly * chunk_size + lx;
                double settled = settling.values.empty() ? 1.0 : settling.values[i] / double(settle_extent);
                settled = settled * settled * (3 - 2 * settled);
                double closing = 1 - settled;
                bool open = depth > 10 && y < bottom
                    && (noise(s, 61, 32, 24, x, y) > 0.35 + closing || (depth > 120 && noise(s, 62, 96, 48, x, y) > 0.55 + closing));
                if (open) id.clear();
                if (!settling.values.empty()) {
                    double threshold = fraction(hash(s, 71, wrap(x, s.width()), y));
                    if (threshold >= settled) {
                        // The same depth below the neighbor's ground, where it is known.
                        const auto& b = boundary[settling.source[i]];
                        int sy = b.y;
                        auto [dx, dy] = sources[settling.source[i]];
                        if (dx != 0) {
                            auto [a, z] = ground_span(*near.at(dx, dy), content, cy + dy, b.x - dx * chunk_size);
                            if (a == z) sy = a - y0 + depth;
                        }
                        for (auto layer : {Layer::foreground, Layer::background}) {
                            auto& cell = layer == Layer::foreground ? id : rear;
                            if (cell.empty()) continue;
                            const Cell* from = natural(b.x, sy, layer);
                            if (!from) from = natural(b.x, b.y, layer);
                            if (from) cell = from->id;
                        }
                    }
                }
            } else if (h == ordinary[lx + 1]) {
                // Trees grow where the ground is the generator's own.
                for (int cell = x / 16 - 1; cell <= x / 16 + 1; ++cell) {
                    int tx = tree(s, cell);
                    if (tx < 0) continue;
                    int th = ground(s, tx);
                    int dx = std::abs(int(distance_x(tx, x, s.width())));
                    bool trunk = dx == 0 && y >= th - 4 && y < th;
                    bool canopy = dx <= 2 && y >= th - 6 && y <= th - 3 && dx + std::abs(y - (th - 4)) <= 3;
                    // The trunk stands behind, where the character can walk past it, and the
                    // canopy stays in front, around its top as well.
                    if (trunk) rear = t.timber;
                    if (canopy) id = t.leaves;
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
