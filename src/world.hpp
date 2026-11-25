#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace yonder {

// A chunk is the same in every world, so anything built to it, such as a hand-made chunk,
// fits everywhere. A region of chunks groups their files.
constexpr int chunk_size = 64;
constexpr int chunk_area = chunk_size * chunk_size;
constexpr int region_size = 64;
// World sizes, in chunks.
constexpr int max_columns = 65536, max_rows = region_size;
constexpr int starting_columns = 4096, starting_rows = 16;
constexpr double player_width = 0.7;
constexpr double player_height = 1.0;
constexpr double tick_seconds = 1.0 / 120;
// Within this margin the game reads intent instead of demanding pixel precision: a body in the
// air slips onward past a corner it catches by no more, and a placement goes into a cell the
// body overlaps by no more, moving the body clear. Half the body's width, so the body counts as
// standing in the column that holds its centre.
constexpr double margin = player_width / 2;
// Readied stacks, shown as a ring around the character.
constexpr std::size_t belt_size = 12;

// Empty string is empty space. Every material has a stable string ID.
using Block = std::string;

// Saved properties this build does not know, each as its name and its JSON text exactly
// as read, so that rewriting a file keeps them. Only Save looks inside.
struct Unrecognized {
    std::vector<std::pair<std::string, std::string>> members;
    bool operator==(const Unrecognized&) const = default;
};
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
    bool missing = false; // No pack defines this ID; drawn as unknown content.
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
    std::map<std::string, Pose> poses; // From packs' character.json; later packs override.
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
    Unrecognized unknown, unknown_materials;
};
Terrain load_terrain(const std::filesystem::path& pack);
// The one generation this build makes.
constexpr const char* this_generation = "expedition-1";

struct Manifest {
    int version;
    std::vector<std::pair<std::string, int>> packs;
    std::string terrain;
};
Manifest load_manifest(const std::filesystem::path& file);

// A world's lasting parameters. Its size is whole chunks: columns around, rows tall.
struct Settings {
    std::uint64_t seed = 0;
    int columns = starting_columns;
    int rows = starting_rows;
    Terrain terrain;
    Unrecognized unknown{};
    int width() const { return columns * chunk_size; }
    int height() const { return rows * chunk_size; }
};

int wrap(int x, int width);
double wrap(double x, double width);
double distance_x(double from, double to, int width);
// Where the generator puts the ground in column x, before any settling: the top row of ground.
int ground(const Settings& settings, int x);

struct Cell {
    Block id;
    int quantity = 1;
    bool placed = false;
    int batch = 1; // A workbench's setting, independent of the material quantity.
    Unrecognized unknown{};
    bool operator==(const Cell&) const = default;
};

// How far a cell has settled from what a neighbor carried in toward this generator's ordinary
// result: 0 is the neighbor's own, settle_extent is fully settled. It spans a chunk and a half,
// about as far as a difference in the ground takes to fade.
constexpr int settle_extent = chunk_size * 3 / 2;

struct Chunk {
    std::array<Cell, chunk_area> tiles;
    std::array<Cell, chunk_area> background;
    // What generated it. Empty means nothing did: a chunk built by hand.
    std::string generation;
    // Where the ground lay across each column when it was generated, as the top row of ground.
    // Empty means it is read from the blocks.
    std::vector<int> ground;
    // Empty means fully settled. Otherwise one value per cell, row-major.
    std::vector<std::uint8_t> settling;
    Unrecognized unknown{};
    bool dirty = false; // Changed since it was loaded or saved; a new chunk is changed.
    Cell& at(int x, int y) { return tiles[y * chunk_size + x]; }
    const Cell& at(int x, int y) const { return tiles[y * chunk_size + x]; }
};

// The chunks around one being generated that already exist, by offset; the centre is empty.
struct Neighborhood {
    std::array<std::array<std::shared_ptr<const Chunk>, 3>, 3> chunks;
    const Chunk* at(int dx, int dy) const { return chunks[dy + 1][dx + 1].get(); }
};
// The highest and lowest the top row of ground can be in a chunk's column `lx`: one row where the
// chunk carries its ground line, a range where it is read from natural blocks that may not show it.
std::pair<int, int> ground_span(const Chunk& chunk, const Content& content, int cy, int lx);
// A new chunk takes ground and materials only from immediate neighbors that already exist.
Chunk generate(const Settings& settings, const Content& content, int cx, int cy, const Neighborhood& near = {});

struct Player {
    std::string world; // Which world of the universe the player is in.
    double x = 0, y = 0, vy = 0;
    bool grounded = false;
    int takeoff = -1; // The row the body stood in when this jump began; -1 when not jumping.
    std::map<Block, std::int64_t> inventory;
    Block selected;
    Block last_selected;
    std::vector<Block> ready;
    int facing = 1; // -1 left, 0 background, 1 right.
    bool down = false; // S held: crawling, aiming beneath the feet. The body stays a block tall.
    bool up = false; // W held: facing the wall, aiming above the head.
    Unrecognized unknown{};
};

enum class Layer { foreground, background };
struct Target {
    int x, y;
    Layer layer = Layer::foreground;
};
// The cell beyond the facing edge, beneath the feet with S, above the head with W held, or
// the rear wall behind the body after a tap of W.
Target target(const Player& player);

class Save;
class WorldFolder;

// World owns the loaded chunks, on the game's thread. Loading, generating and writing them
// happen on the save's worker, ahead of the player, so the game never waits for them; a chunk
// that has not arrived yet is solid and empty.
class World {
public:
    World(Save& save, const WorldFolder& folder, const Content& content, Settings settings);
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    const Settings settings;
    const Content& content;

    // References belong to the loaded chunk and expire when retain unloads it.
    const Block& block(int x, int y, Layer layer = Layer::foreground);
    const Cell& cell(int x, int y, Layer layer = Layer::foreground);
    bool ready(int x, int y);
    bool solid(int x, int y);
    // Where the ground lay in column x when its loaded chunk was made, or the generator's
    // ground if no chunk of that column is loaded.
    int ground(int x);
    void set(int x, int y, const Block& block, Layer layer = Layer::foreground);
    void put(int x, int y, const Cell& cell, Layer layer = Layer::foreground);
    // Asks for the chunks around this view, nearest first, takes in those that have arrived,
    // and unloads those far outside it, writing each changed one as it leaves.
    void retain(int left, int top, int right, int bottom);
    // Loads every chunk overlapping this area and waits for them, for starting play.
    void load(int left, int top, int right, int bottom);
    // Writes every changed chunk and a changed player, behind play. Returns whether anything changed.
    bool save_changes(const Player& player);

private:
    using Key = std::pair<int, int>;
    Save& save;
    const WorldFolder& folder;
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

bool intersects(World& world, double x, double y);
Player spawn(World& world);
void move(World& world, Player& player, int direction, bool jump, bool sprint = false);

// A completed recipe: the ingredients it consumed and the result standing at (x, y), in
// wrapped world coordinates, so that presentation can show the change happening.
struct Crafted {
    int x, y;
    Block output;
    std::vector<Ingredient> consumed;
};
struct ActionResult {
    bool ok;
    std::string message;
    bool quiet = false; // Routine outcomes the world already shows.
    std::optional<Crafted> crafted{};
};
// In the air the body straddles two rows. When the target cannot take an action that needs it
// occupied (or empty) and the other row the body overlaps can, that row is meant instead.
Target favor(World& world, const Player& player, Target target, bool occupied);
// What J addresses: anything sharing the body's own cell, before the target.
Target reach(World& world, const Player& player);
// Moves the body clear of a cell it overlaps by no more than the margin, as little as possible.
// Returns false if that cell cannot be cleared.
bool clear_of(World& world, Player& player, int x, int y);
// Taking a block into the inventory, and putting the held one into the world.
ActionResult gather(World& world, Player& player, int x, int y, Layer layer = Layer::foreground);
ActionResult place_at(World& world, Player& player, int x, int y, Layer layer = Layer::foreground);
// Adds to the inventory. A newly carried kind is readied while the belt has room.
void carry(Player& player, const Block& id, std::int64_t count);
ActionResult adjust_workbench(World& world, int x, int y, Layer layer = Layer::foreground);
// Only carried objects exist for the player: an emptied entry leaves the inventory,
// its readiness and the hand.
void drop_empty(Player& player);
std::vector<Block> slots(const Player& player);

// Spatial crafting lives in the world. No inventory-grid or SDL dependency.
int placement_quantity(World& world, const Block& ingredient, int x, int y);
std::optional<ActionResult> craft(World& world, Player& player, int changed_x, int changed_y);

}
