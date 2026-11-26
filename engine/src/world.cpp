#include "yonderest/world.hpp"
#include "yonderest/content.hpp"
#include "yonderest/generation.hpp"
#include "yonderest/save.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace yonder {

// How far beyond the view chunks are made ready, and how far beyond that they stay loaded.
// The first decides how much unvisited territory counts as explored; it must also stay ahead
// of a sprint and a fall.
constexpr int load_margin = 24, keep_margin = 48;

World::World(Save& save, const WorldFolder& folder, const Content& content, Settings settings, Generation& generation)
    : settings(std::move(settings)), content(content), save(save), folder(folder), generation(generation) {}

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

const Chunk* World::chunk(int x, int y) {
    return loaded(x, y);
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
            chunk = generation.generate(cx, cy, near);
            chunk.dirty = true;
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
    return c->at(x % chunk_size, y % chunk_size, layer);
}

bool World::ready(int x, int y) {
    return y < 0 || y >= settings.height() || loaded(x, y);
}

void World::set(int x, int y, const Block& id, Layer layer) {
    put(x, y, Cell{id, {}}, layer);
}

void World::put(int x, int y, const Cell& value, Layer layer) {
    auto* c = loaded(x, y);
    x = wrap(x, settings.width());
    c->at(x % chunk_size, y % chunk_size, layer) = value;
    c->dirty = true;
    if (changed) changed(x, y, layer);
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

void World::keep(int cx, int cy, Chunk chunk) {
    chunk.dirty = true;
    save.worker.post([this, key = Key{cx, cy}, copy = std::make_shared<const Chunk>(chunk)] { known[key] = copy; });
    chunks.insert_or_assign({cx, cy}, std::move(chunk));
}

bool World::save_changes() {
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
    return wrote;
}

}
