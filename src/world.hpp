#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace yonder {

constexpr int chunk_size = 32;
constexpr double player_width = 0.7;
constexpr double player_height = 1.7;
constexpr double tick_seconds = 1.0 / 120;

// Empty string is empty space. Every material has a namespaced, stable ID.
using Block = std::string;
struct Material {
    std::array<int, 3> color;
    bool solid;
    std::string name;
};

struct Content {
    std::map<Block, Material> blocks;
    void load(const std::filesystem::path& pack);
    const Material& material(const Block& id) const;
};

struct Terrain {
    std::string version;
    Block grass, soil, stone, sand, timber, leaves;
};
Terrain load_terrain(const std::filesystem::path& pack);

struct Settings {
    std::uint64_t seed = 0;
    int columns = 32;
    int rows = 8;
    Terrain terrain;
    int width() const { return columns * chunk_size; }
    int height() const { return rows * chunk_size; }
};

int wrap(int x, int width);
double wrap(double x, double width);
double distance_x(double from, double to, int width);
int surface(const Settings& settings, int x);

struct Chunk {
    std::array<Block, chunk_size * chunk_size> tiles;
    bool dirty = false;
    Block& at(int x, int y) { return tiles[y * chunk_size + x]; }
    const Block& at(int x, int y) const { return tiles[y * chunk_size + x]; }
};
Chunk generate(const Settings& settings, int cx, int cy);

struct Player {
    double x = 0, y = 0, vy = 0;
    bool grounded = false;
    std::map<Block, int> inventory;
    Block selected;
};

class Save;

// World owns the loaded chunks. Save owns their durable snapshots.
class World {
public:
    World(Save& save, const Content& content, Settings settings);
    const Settings settings;
    const Content& content;

    Block block(int x, int y);
    bool solid(int x, int y);
    void set(int x, int y, const Block& block);
    void retain(int left, int top, int right, int bottom);
    void checkpoint(const Player& player);
    std::size_t loaded_chunks() const { return chunks.size(); }

private:
    Save& save;
    std::map<std::pair<int, int>, Chunk> chunks;
    Chunk& chunk(int x, int y);
};

bool intersects(World& world, double x, double y);
Player spawn(World& world);
void move(World& world, Player& player, int direction, bool jump);

enum class Action { mine, place };
// Returns a message on refusal, empty on success. One block per action.
std::string act(World& world, Player& player, Action action, int x, int y);
std::vector<Block> slots(const Content& content, const Player& player);

}
