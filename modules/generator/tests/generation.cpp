// The generator, through the generation interface: land is the same whatever order it is
// explored in, a hand-built chunk is met by the land around it, the rear layer holds its own
// deposits, and a new world begins with one chunk holding ground.

#include "yonderest/host.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace yonder;
namespace yonder::generator { void attach(Host&); }
namespace {

void require(bool result, const std::string& what) {
    if (!result) throw std::runtime_error(what);
}

std::string read(const std::filesystem::path& file) {
    std::ifstream in(file);
    return {std::istreambuf_iterator<char>(in), {}};
}

void write(const std::filesystem::path& file, const std::string& text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << text;
}

// The generator's own content: a pack with two layers and a terrain configuration.
std::filesystem::path content(const std::filesystem::path& root) {
    write(root / "flagship.json", R"({"version": 1, "packs": [{"id": "land", "version": 1}], "generator": {"terrain": "land"}})");
    write(root / "land" / "module.json", R"({"id": "land", "version": 1})");
    write(root / "land" / "layers.json", R"(["foreground", "rear"])");
    write(root / "land" / "terrain.json", R"({"generation": "expedition-1", "materials": {"grass": "land:grass", "soil": "land:soil",
        "stone": "land:stone", "sand": "land:sand", "timber": "land:timber", "leaves": "land:leaves", "clay": "land:clay", "ore": "land:ore"}})");
    return root;
}

// Stand-ins for the interfaces the generator and a new world lean on: anything is solid.
class Solid : public Physics {
public:
    bool solid(const Block& id) const override { return !id.empty(); }
    void steer(int, bool) override {}
    void jump() override {}
    void tick(World&, Player&) override {}
    bool grounded() const override { return true; }
    bool rising() const override { return false; }
    bool jumping() const override { return false; }
    int takeoff() const override { return -1; }
    double stride() const override { return 0; }
};
class Nothing : public Interaction {
public:
    void act(Action&) override {}
};

struct Game {
    Solid physics;
    Nothing interaction;
    Host host;
    explicit Game(const std::filesystem::path& root, const std::filesystem::path& save = {}) : host(options(root, save)) {
        host.physics = &physics;
        host.interaction = &interaction;
        generator::attach(host);
    }
    static Options options(const std::filesystem::path& root, const std::filesystem::path& save) {
        Options o;
        o.content = root;
        o.path = save;
        o.seed = 42;
        o.size = {8, 4};
        return o;
    }
    Settings settings(int columns = 8, int rows = 4) {
        Settings s;
        s.seed = 42; s.columns = columns; s.rows = rows;
        host.generation->create(s);
        host.generation->open(s);
        return s;
    }
};

const Value& ground(const Chunk& c) { return c.properties["generator"]["ground"]; }
int ground(const Chunk& c, int lx) { return int(std::get<std::int64_t>((*ground(c).array())[lx].data)); }

// Unchanged generation gives the same land whatever order it is explored in, and each chunk keeps its ground line.
void order(const std::filesystem::path& root, const std::filesystem::path& folder) {
    Game game(root);
    auto settings = game.settings();
    auto explore = [&](const std::string& name, std::vector<int> columns) {
        Save save(folder / name);
        auto world_folder = save.world(Save::starting_world, game.host.content.layers);
        world_folder.write_settings(settings);
        World world(save, world_folder, game.host.content, settings, *game.host.generation);
        for (int cx : columns) world.load(cx * chunk_size, 64, cx * chunk_size, 127);
        world.save_changes();
    };
    explore("east", {6, 7, 0, 1});
    explore("west", {1, 0, 7, 6});
    for (int cx : {6, 7, 0, 1}) {
        auto name = std::filesystem::path("origin") / "regions" / "x0-y0" / ("x" + std::to_string(cx) + "-y1.json");
        require(read(folder / "east" / name) == read(folder / "west" / name), "exploration order does not change the land");
    }
    auto c = *Save(folder / "east").world(Save::starting_world, game.host.content.layers).chunk(0, 1);
    require(ground(c).array() && ground(c).array()->size() == chunk_size, "a chunk keeps the ground line it was generated with");
}

// A chunk nobody generated, with its ground lower than the generator's and made of a material the
// generator never uses, is met by the land generated around it, within a chunk or two, beside it,
// across the wrap point, above and below.
void handmade(const std::filesystem::path& root, const std::filesystem::path& folder) {
    Game game(root);
    auto settings = game.settings();
    constexpr int floor = 110; // The hand-built ground; the generator's lies around 64 in this world.
    Save save(folder);
    auto world_folder = save.world(Save::starting_world, game.host.content.layers);
    world_folder.write_settings(settings);
    {
        Chunk built(2);
        for (int ly = floor - chunk_size; ly < chunk_size; ++ly)
            for (int lx = 0; lx < chunk_size; ++lx) built.at(lx, ly).id = "handmade:marble";
        world_folder.write_chunk(0, 1, built);
    }
    World world(save, world_folder, game.host.content, settings, *game.host.generation);
    auto chunk = [&](int cx, int cy) {
        world.load(cx * chunk_size, cy * chunk_size, cx * chunk_size, cy * chunk_size);
        world.save_changes();
        save.worker.wait();
        return *world_folder.chunk(wrap(cx, settings.columns), cy);
    };
    // Unsettled ground, from a chunk far from any other.
    auto own = [&](int x) {
        int cx = x / chunk_size;
        Neighborhood alone;
        return ground(game.host.generation->generate(cx, 1, alone), x % chunk_size);
    };
    auto east = chunk(1, 1), farther = chunk(2, 1), west = chunk(-1, 1), above = chunk(0, 0), below = chunk(0, 2);
    require(std::abs(ground(east, 0) - floor) <= 2 && std::abs(ground(west, chunk_size - 1) - floor) <= 2,
            "the land beside it meets its ground, across the wrap point too");
    require(ground(east, chunk_size - 1) != own(2 * chunk_size - 1), "fixture: the difference has not gone in one chunk");
    require(ground(farther, chunk_size - 1) == own(3 * chunk_size - 1), "within two chunks it is the generator's own ground");
    for (int lx = 0; lx < chunk_size; ++lx)
        require(ground(above, lx) == floor && ground(below, lx) == floor, "above and below it the ground line is its own");
    auto marble = [](const Chunk& c, int from, int to) {
        int n = 0;
        for (int ly = 0; ly < chunk_size; ++ly)
            for (int lx = from; lx < to; ++lx) n += c.at(lx, ly).id == "handmade:marble";
        return n;
    };
    require(marble(east, 0, 8) > marble(east, 56, 64) && marble(east, 0, 8) > 0, "its material carries into the land beside it, fading");
    require(marble(farther, 32, 64) == 0, "and is gone two chunks away");
    require(marble(below, 0, chunk_size) > 0, "it carries downward too");
    for (int ly = 0; ly < chunk_size; ++ly)
        for (int lx = 0; lx < chunk_size; ++lx)
            require(east.at(lx, ly).id != "handmade:marble" || chunk_size + ly >= ground(east, lx), "it stays below the ground");
}

// Underground, the rear layer holds deposits of its own, a little richer than the foreground's.
void deposits(const std::filesystem::path& root) {
    Game game(root);
    game.settings(starting_columns, starting_rows);
    int front = 0, rear = 0, hidden = 0;
    // Chunks spread around the whole world, so the sample crosses uplands and groves.
    for (int cx = 0; cx < starting_columns; cx += 16) {
        auto c = game.host.generation->generate(cx, 4, {});
        for (int ly = 0; ly < chunk_size; ++ly)
            for (int lx = 0; lx < chunk_size; ++lx) {
                const auto &f = c.at(lx, ly).id, &b = c.at(lx, ly, 1).id;
                auto found = [&](const Block& id) { return id == "land:ore" || id == "land:clay"; };
                front += found(f);
                rear += found(b);
                hidden += found(b) && f == "land:stone";
            }
    }
    require(rear > front && front > 0, "the rear layer is a little richer than the foreground");
    require(hidden > rear / 4, "much of it is hidden behind ordinary stone");
}

// A new world begins with one chunk, in column zero, holding ground; the player stands on it.
void beginning(const std::filesystem::path& root, const std::filesystem::path& folder) {
    Game game(root, folder);
    game.host.start();
    auto& p = game.host.player();
    auto& world = game.host.world();
    int row = game.host.generation->first_row();
    require(int(p.y) / chunk_size == row && p.x > 0 && p.x < 1, "the player stands in column zero of the first chunk");
    require(world.block(0, int(p.y)).empty() && !world.block(0, int(p.y) + 1).empty(), "on the ground, in open space");
    game.host.save_changes();
    game.host.save().worker.wait();
    int files = 0;
    for (const auto& file : std::filesystem::recursive_directory_iterator(folder / "origin" / "regions")) files += file.is_regular_file();
    require(files == 1, "nothing else in the column is generated");
}

}

int main() {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-generator-" + std::to_string(getpid()));
    try {
        auto root = content(scratch / "content");
        order(root, scratch / "order");
        handmade(root, scratch / "handmade");
        deposits(root);
        beginning(root, scratch / "beginning");
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
