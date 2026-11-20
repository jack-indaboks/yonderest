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
    std::string station{};
    std::string tool{};
    std::string requires_tool{};
    // Art from the defining pack: blocks/<name>.png, and held/<name>.png when
    // the object is drawn in the character's frame rather than as a carried block.
    std::filesystem::path texture{}, held{};
};

struct Ingredient {
    int x, y;
    Block id; // Empty means this cell must be empty, not an ingredient.
};

struct Recipe {
    std::string id;
    int order = 0; // Definition order across packs, for teaching; matching uses size.
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

// A character pose: a 16x16 image from images/, drawn so column `anchor` stands over the
// player's centre. Held art is drawn as the idle pose holds it, moved to this pose's hand.
struct Pose {
    std::string image, bare; // `bare`, if given, replaces `image` while the hand is empty.
    int anchor = 0, hand_x = 0, hand_y = 0;
    int block_x = 0, block_y = 0; // Top-left of a carried block; beside the hand unless given.
};

struct Content {
    std::map<Block, Material> blocks;
    std::map<std::string, Pose> poses; // From packs' character.txt; later packs override.
    std::map<std::string, Module> modules;
    std::vector<Recipe> recipes;
    // Named presentation images from packs' images/ directories; later packs override.
    std::map<std::string, std::filesystem::path> images;
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
// Known generation profiles in order: shelter-v1, expedition-v1, expedition-v2.
// Zero means unknown. Expedition profiles add regional materials; v2 adds natural backing.
int profile_rank(const std::string& profile);

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

struct Cell {
    Block id;
    int quantity = 1;
    bool placed = false;
    int batch = 1; // A workbench's setting, independent of the material quantity.
    bool operator==(const Cell&) const = default;
};

// How far a cell has settled from an earlier generation's materials toward
// this generation's ordinary result: 0 is the old material, settle_extent is
// fully settled. It spans roughly two chunks.
constexpr int settle_extent = 2 * chunk_size;

struct Chunk {
    std::array<Cell, chunk_size * chunk_size> tiles;
    std::array<Cell, chunk_size * chunk_size> background;
    std::string profile;
    // Empty means fully settled. Otherwise one value per cell, row-major.
    std::vector<std::uint8_t> settling;
    bool dirty = false;
    Cell& at(int x, int y) { return tiles[y * chunk_size + x]; }
    const Cell& at(int x, int y) const { return tiles[y * chunk_size + x]; }
};
// A neighboring cell's settling, in the new chunk's local coordinates.
struct Boundary {
    int x, y, settled;
};
// Boundary conditions come only from immediate neighbors that already exist.
Chunk generate(const Settings& settings, int cx, int cy, const std::vector<Boundary>& boundary = {});

struct Player {
    double x = 0, y = 0, vy = 0;
    bool grounded = false;
    std::map<Block, std::int64_t> inventory;
    Block selected;
    Block last_selected;
    std::vector<Block> ready;
    int facing = 1; // -1 left, 0 background, 1 right.
    bool down = false; // S held: crawling, aiming beneath the feet. The body stays a block tall.
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
    bool solid(int x, int y);
    void set(int x, int y, const Block& block, Layer layer = Layer::foreground);
    void put(int x, int y, const Cell& cell, Layer layer = Layer::foreground);
    void retain(int left, int top, int right, int bottom);
    void checkpoint(const Player& player);

private:
    Save& save;
    std::map<std::pair<int, int>, Chunk> chunks;
    Chunk& chunk(int x, int y);
    Chunk* existing(int cx, int cy);
    std::vector<Boundary> boundary(int cx, int cy);
};

bool intersects(World& world, double x, double y);
Player spawn(World& world);
void move(World& world, Player& player, int direction, bool jump, bool sprint = false);

struct ActionResult {
    bool ok;
    std::string message;
    bool quiet = false; // Routine outcomes the world already shows.
};
// Taking a block into the inventory, and putting the held one into the world.
ActionResult gather(World& world, Player& player, int x, int y, Layer layer = Layer::foreground);
ActionResult place_at(World& world, Player& player, int x, int y, Layer layer = Layer::foreground);
ActionResult adjust_workbench(World& world, int x, int y, Layer layer = Layer::foreground);
// Only carried objects exist for the player: an emptied entry leaves the inventory,
// its readiness and the hand.
void drop_empty(Player& player);
std::vector<Block> slots(const Player& player);

// Spatial crafting lives in the world. No inventory-grid or SDL dependency.
int placement_quantity(World& world, const Block& ingredient, int x, int y);
std::optional<ActionResult> craft(World& world, Player& player, int changed_x, int changed_y);

}
