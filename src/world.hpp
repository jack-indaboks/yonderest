#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace yonder {

constexpr int chunk_size = 32;
constexpr double player_width = 0.7;
constexpr double player_height = 1.0;
constexpr double tick_seconds = 1.0 / 120;

// Empty string is empty space. Every material has a stable string ID.
using Block = std::string;
struct Material {
    std::array<int, 3> color;
    bool solid;
    std::string name;
    std::string appearance = "grain";
    std::string station{};
    std::string tool{};
    std::string requires_tool{};
};

struct Ingredient {
    int x, y;
    Block id; // Empty means this cell must be empty, not an ingredient.
};

struct Recipe {
    std::string id;
    Block output;
    int amount = 1, output_x = 0, output_y = 0;
    int width = 0, height = 0;
    std::vector<Ingredient> shape;
    std::vector<std::string> stations;
};

struct Module {
    int version;
    std::map<std::string, int> requires_modules;
};

struct Content {
    std::map<Block, Material> blocks;
    std::map<std::string, Module> modules;
    std::vector<Recipe> recipes;
    void load(const std::filesystem::path& pack);
    void validate() const;
    const Material& material(const Block& id) const;
};

struct Terrain {
    std::string version;
    Block grass, soil, stone, sand, timber, leaves;
    Block clay, ore;
};
Terrain load_terrain(const std::filesystem::path& pack);

struct Manifest {
    int version;
    std::vector<std::pair<std::string, int>> packs;
    std::string terrain;
};
Manifest load_manifest(const std::filesystem::path& file);

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
std::string biome(const Settings& settings, int x, const std::string& profile);

struct Cell {
    Block id;
    int quantity = 1;
    bool placed = false;
    int batch = 1; // A workbench's setting, independent of the material quantity.
    bool operator==(const Cell&) const = default;
};

struct Chunk {
    std::array<Cell, chunk_size * chunk_size> tiles;
    std::array<Cell, chunk_size * chunk_size> background;
    std::string profile;
    bool dirty = false;
    Cell& at(int x, int y) { return tiles[y * chunk_size + x]; }
    const Cell& at(int x, int y) const { return tiles[y * chunk_size + x]; }
};
Chunk generate(const Settings& settings, int cx, int cy,
               const std::vector<std::pair<int, int>>& legacy = {});

struct Player {
    double x = 0, y = 0, vy = 0;
    bool grounded = false;
    std::map<Block, std::int64_t> inventory;
    Block selected;
    Block last_selected;
    std::vector<Block> ready;
    int facing = 1; // -1 left, 0 background, 1 right.
    bool crouched = false, down = false;
    double height() const { return crouched ? 0.5 : player_height; }
};

enum class Layer { foreground, background };
struct Target {
    int x, y;
    Layer layer = Layer::foreground;
};
Target target(const Player& player);

class Save;

// World owns the loaded chunks. Save owns their durable snapshots.
class World {
public:
    World(Save& save, const Content& content, Settings settings);
    const Settings settings;
    const Content& content;

    // References belong to the loaded chunk and expire when retain unloads it.
    const Block& block(int x, int y, Layer layer = Layer::foreground);
    const Cell& cell(int x, int y, Layer layer = Layer::foreground);
    std::string region(int x, int y);
    bool solid(int x, int y);
    void set(int x, int y, const Block& block, Layer layer = Layer::foreground);
    void put(int x, int y, const Cell& cell, Layer layer = Layer::foreground);
    void retain(int left, int top, int right, int bottom);
    void checkpoint(const Player& player);

private:
    Save& save;
    std::map<std::pair<int, int>, Chunk> chunks;
    std::vector<std::pair<int, int>> legacy;
    Chunk& chunk(int x, int y);
};

bool intersects(World& world, double x, double y, double height = player_height);
Player spawn(World& world);
void crouch(World& world, Player& player, bool down);
void move(World& world, Player& player, int direction, bool jump, bool crouch = false, bool sprint = false);

enum class Action { mine, place };
struct ActionResult {
    bool ok;
    std::string message;
};
// Coordinate operations; keyboard play supplies the character's direct target.
ActionResult act(World& world, Player& player, Action action, int x, int y, Layer layer = Layer::foreground);
ActionResult adjust_workbench(World& world, int x, int y, Layer layer = Layer::foreground);
std::vector<Block> slots(const Player& player);
std::vector<Block> readied(const Player& player);

// Spatial crafting lives in the world. No inventory-grid or SDL dependency.
int placement_quantity(World& world, const Block& ingredient, int x, int y);
std::optional<ActionResult> craft(World& world, Player& player, int changed_x, int changed_y);

}
