#pragma once

#include "yonderest/space.hpp"
#include "yonderest/value.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace yonder {

class Content;
class Generation;
class Save;
class WorldFolder;

struct Cell {
    Block id;
    Properties properties;
    bool operator==(const Cell&) const = default;
};
using Cells = std::array<Cell, chunk_area>;

struct Chunk {
    explicit Chunk(std::size_t layers = 0) : layers(layers) {}
    std::vector<Cells> layers;
    Properties properties;
    bool dirty = false; // Changed since it was loaded or saved; a new chunk is changed.
    Cell& at(int x, int y, Layer layer = 0) { return layers[layer][y * chunk_size + x]; }
    const Cell& at(int x, int y, Layer layer = 0) const { return layers[layer][y * chunk_size + x]; }
};

// The chunks around one being generated that already exist, by offset; the centre is empty.
struct Neighborhood {
    std::array<std::array<std::shared_ptr<const Chunk>, 3>, 3> chunks;
    const Chunk* at(int dx, int dy) const { return chunks[dy + 1][dx + 1].get(); }
};

// A world's lasting parameters. Its size is whole chunks: columns around, rows tall.
struct Settings {
    std::uint64_t seed = 0;
    int columns = starting_columns;
    int rows = starting_rows;
    Properties properties;
    int width() const { return columns * chunk_size; }
    int height() const { return rows * chunk_size; }
};

// World owns the loaded chunks, on the game's thread. Loading, generating and writing them
// happen on the save's worker, ahead of the player, so the game never waits for them.
class World {
public:
    World(Save& save, const WorldFolder& folder, const Content& content, Settings settings, Generation& generation);
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    const Settings settings;
    const Content& content;

    // References belong to the loaded chunk and expire when retain unloads it. Outside the
    // world, or in a chunk that has not arrived yet, every cell is empty.
    const Block& block(int x, int y, Layer layer = 0);
    const Cell& cell(int x, int y, Layer layer = 0);
    bool ready(int x, int y);
    // The loaded chunk holding this cell, if any.
    const Chunk* chunk(int x, int y);
    void set(int x, int y, const Block& block, Layer layer = 0);
    // Changes a cell, then tells `changed`.
    void put(int x, int y, const Cell& cell, Layer layer = 0);
    std::function<void(int x, int y, Layer layer)> changed;
    // Asks for the chunks around this view, nearest first, takes in those that have arrived,
    // and unloads those far outside it, writing each changed one as it leaves.
    void retain(int left, int top, int right, int bottom);
    // Loads every chunk overlapping this area and waits for them, for starting play.
    void load(int left, int top, int right, int bottom);
    // Takes in a chunk made outside the worker, such as a new world's first.
    void keep(int cx, int cy, Chunk chunk);
    // Writes every changed chunk, behind play. Returns whether anything changed.
    bool save_changes();

private:
    using Key = std::pair<int, int>;
    Save& save;
    const WorldFolder& folder;
    Generation& generation;
    std::map<Key, Chunk> chunks;
    std::set<Key> requested;
    // Arrivals from the worker.
    std::mutex mutex;
    std::vector<std::pair<Key, Chunk>> arrived;
    // The worker's own copies of chunks it has handed over and not yet been told are gone,
    // so a neighbor still in play can shape new land. Only the worker touches it.
    std::map<Key, std::shared_ptr<const Chunk>> known;
    Chunk* loaded(int x, int y);
    void request(int cx, int cy);
    void receive();
    std::shared_ptr<const Chunk> neighbor(int cx, int cy);
};

}
