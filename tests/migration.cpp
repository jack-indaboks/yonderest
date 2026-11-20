#include "save.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <sqlite3.h>

using namespace yonder;
namespace {

void require(bool result, const std::string& what) {
    if (!result) throw std::runtime_error(what);
}

std::string query(const std::filesystem::path& path, const char* sql) {
    sqlite3* db = nullptr;
    require(sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "open fixture for reading");
    sqlite3_stmt* statement = nullptr;
    require(sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK, sqlite3_errmsg(db));
    require(sqlite3_step(statement) == SQLITE_ROW, "fixture query returned no row");
    std::string result = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
    sqlite3_finalize(statement);
    sqlite3_close(db);
    return result;
}

void legacy_save(const std::filesystem::path& path, const Settings& old, int format) {
    sqlite3* db = nullptr;
    require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "create legacy fixture");
    require(sqlite3_exec(db, "CREATE TABLE meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
                            "CREATE TABLE chunks(x INTEGER,y INTEGER,tiles TEXT NOT NULL,PRIMARY KEY(x,y));"
                            "INSERT INTO meta VALUES('player','6.15 94.3 \"base:timber\" 1\n\"base:timber\" 7\n');",
                            nullptr, nullptr, nullptr) == SQLITE_OK, "legacy schema");
    std::string text = std::to_string(format) + " 32 42 32 8\nshelter-v1 base:grass base:soil base:stone base:sand base:timber base:leaves";
    if (format == 2) text += "\n\"expedition:clay\" \"expedition:copper_ore\"";
    sqlite3_stmt* statement;
    require(sqlite3_prepare_v2(db, "INSERT INTO meta VALUES('world',?)", -1, &statement, nullptr) == SQLITE_OK, "legacy world statement");
    sqlite3_bind_text(statement, 1, text.c_str(), -1, SQLITE_TRANSIENT);
    require(sqlite3_step(statement) == SQLITE_DONE, "legacy world data");
    sqlite3_finalize(statement);
    for (int cy : {2, 3}) {
        auto chunk = generate(old, 0, cy);
        // A little timber roof and wall crossing the vertical chunk boundary.
        for (int x = 3; x <= 9; ++x) if (cy == 2) chunk.at(x, 29).id = old.terrain.timber;
        chunk.at(3, cy == 2 ? 31 : 0).id = old.terrain.timber;
        std::ostringstream tiles;
        if (format == 2) {
            tiles << "Y2\n";
            chunk.at(8, 31) = Cell{"base:timber", 5, true};
            chunk.at(9, 31) = Cell{"workshop:workbench", 1, true, 10};
        }
        for (const auto& cell : chunk.tiles) {
            tiles << 1 << ' ' << std::quoted(cell.id);
            if (format == 2) tiles << ' ' << cell.quantity << ' ' << cell.placed << ' ' << cell.batch;
            tiles << '\n';
        }
        text = tiles.str();
        require(sqlite3_prepare_v2(db, "INSERT INTO chunks VALUES(0,?,?)", -1, &statement, nullptr) == SQLITE_OK, "legacy chunk statement");
        sqlite3_bind_int(statement, 1, cy);
        sqlite3_bind_text(statement, 2, text.c_str(), -1, SQLITE_TRANSIENT);
        require(sqlite3_step(statement) == SQLITE_DONE, "legacy chunk data");
        sqlite3_finalize(statement);
    }
    sqlite3_close(db);
}

void migration(const std::filesystem::path& path, const Settings& old, const Settings& updated, int format) {
    legacy_save(path, old, format);
    auto old_tiles = query(path, "SELECT tiles FROM chunks WHERE x=0 AND y=2");
    {
        Save save(path);
        auto settings = *save.settings();
        auto p = save.player();
        require(settings.terrain.version == "shelter-v1", "read the earlier generation configuration");
        require(save.chunk(0, 2)->at(3, 29).id == old.terrain.timber, "read the old bunker");
        save.upgrade(updated);
        save.checkpoint(p);
    }
    auto backup = std::filesystem::path(path.string() + (format == 1 ? ".before-expedition.sqlite3" : ".before-keyboard.sqlite3"));
    require(query(backup, "SELECT value FROM meta WHERE key='world'").starts_with(std::to_string(format) + " 32"), "backup remains in the previous format");
    require(query(backup, "SELECT tiles FROM chunks WHERE x=0 AND y=2") == old_tiles, "backup preserves exact chunk payload");
    require(query(path, "SELECT tiles FROM chunks WHERE x=0 AND y=2") == old_tiles, "upgrade never rewrites an old chunk");
    {
        Save save(path);
        require(save.settings()->terrain.version == updated.terrain.version, "persist upgraded generation configuration");
        auto p = save.player();
        require(p.x == 6.15 && p.y == 95 && p.inventory.at("base:timber") == 7, "upgrade preserves feet, position and inventory");
        require(p.ready == std::vector<Block>{"base:timber"} && p.selected == "base:timber", "previously held stock enters the one inventory's ready subset");
        Content content;
        World world(save, content, updated);
        world.put(3, 93, Cell{"base:timber", 1, true}, Layer::background);
        world.checkpoint(p);
    }
    {
        Save save(path);
        Content content;
        World world(save, content, *save.settings());
        require(world.block(3, 93) == old.terrain.timber && world.block(3, 93, Layer::background) == "base:timber",
                "an edited legacy chunk keeps its foreground alongside the new rear wall");
        if (format == 2)
            require(world.cell(8, 95) == Cell{"base:timber", 5, true} && world.cell(9, 95).batch == 10,
                    "old quantities and station settings survive rewriting the chunk in the new format");
    }
}

void interrupted_upgrade(const std::filesystem::path& path, const Settings& old, const Settings& updated, int format) {
    legacy_save(path, old, format);
    pid_t child = fork();
    require(child >= 0, "fork upgrade writer");
    if (child == 0) {
        Save save(path);
        save.upgrade(updated);
        _exit(0);
    }
    int status;
    waitpid(child, &status, 0);
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "interrupted upgrade writer");
    require(query(path, "SELECT value FROM meta WHERE key='world'").starts_with(std::to_string(format) + " 32"), "interrupted upgrade leaves the old save intact");
    require(std::filesystem::exists(path.string() + (format == 1 ? ".before-expedition.sqlite3" : ".before-keyboard.sqlite3")), "interrupted upgrade already has a complete backup");
}

// Format 3 had expedition-v1 terrain and no natural backing.
void backing(const std::filesystem::path& path, Settings v1, const Settings& v2) {
    v1.terrain.version = "expedition-v1";
    int x = 40, h = surface(v1, x);
    {
        Save save(path);
        save.initialize(v1);
        Content content;
        World world(save, content, v1);
        world.block(x, h + 3);
        world.put(x, h - 1, Cell{"base:timber", 1, true}, Layer::background); // an old rear wall
        Player p; p.x = x; p.y = h - 1;
        world.checkpoint(p);
    }
    {
        sqlite3* db = nullptr;
        require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "open format 3 fixture");
        require(sqlite3_exec(db, "UPDATE meta SET value='3'||substr(value,2) WHERE key='world'", nullptr, nullptr, nullptr) == SQLITE_OK, "mark format 3");
        sqlite3_close(db);
    }
    auto chunk_query = "SELECT tiles FROM chunks WHERE x=1 AND y=" + std::to_string((h + 3) / chunk_size);
    auto old_tiles = query(path, chunk_query.c_str());
    {
        Save save(path);
        auto p = save.player();
        save.upgrade(v2);
        save.checkpoint(p);
    }
    require(std::filesystem::exists(path.string() + ".before-backing.sqlite3"), "format 3 is backed up before the backing upgrade");
    require(query(path, chunk_query.c_str()) == old_tiles, "the upgrade never rewrites explored chunks");
    int far = 20 * chunk_size + 7, far_h = surface(v2, far);
    {
        Save save(path);
        Content content;
        World world(save, content, *save.settings());
        require(world.block(x, h + 3, Layer::background).empty() && world.block(x, h - 1, Layer::background) == "base:timber",
                "explored territory keeps its empty and built background cells");
        require(world.block(far, far_h - 1, Layer::background).empty(), "open sky above new ground");
        require(!world.block(far, far_h + 3, Layer::background).empty(), "new territory has natural backing underground");
        require(world.block(far, far_h + 3, Layer::background) == world.block(far, far_h + 3), "backing matches its foreground host material");
        Player p; p.x = far; p.y = far_h - 1;
        auto host = world.block(far, far_h + 3);
        require(gather(world, p, far, far_h + 3).ok && gather(world, p, far, far_h + 3, Layer::background).ok,
                "foreground, then exposed backing, can be gathered");
        require(p.inventory.at(host) == 2, "both layers are actual recoverable material");
        world.checkpoint(p);
    }
    {
        Save save(path);
        Content content;
        World world(save, content, *save.settings());
        require(world.block(far, far_h + 3).empty() && world.block(far, far_h + 3, Layer::background).empty(),
                "the opening stays empty after restart; no further layer is generated behind it");
    }
}

// Old territory sets boundary conditions only for the new chunks that touch it.
void boundaries(const std::filesystem::path& path, const Settings& old, const Settings& updated) {
    {
        Save save(path);
        save.initialize(old);
        Content content;
        World world(save, content, old);
        for (int cx : {0, 1}) world.block(cx * chunk_size, 3 * chunk_size);
        world.checkpoint({});
    }
    std::vector<std::uint8_t> next;
    {
        Save save(path);
        save.upgrade(updated);
        Content content;
        World world(save, content, updated);
        world.block(2 * chunk_size, 3 * chunk_size);
        world.block(3 * chunk_size, 3 * chunk_size);
        world.block(6 * chunk_size, 3 * chunk_size);
        world.checkpoint({});
        auto touching = save.chunk(2, 3)->settling;
        require(touching.size() == chunk_size * chunk_size, "a chunk touching old territory carries a transition");
        require(touching[16 * chunk_size] <= 1 && touching[16 * chunk_size + 31] >= 31 && touching[16 * chunk_size + 31] <= 33,
                "settling grows with distance from the old edge");
        next = save.chunk(3, 3)->settling;
        require(next.size() == chunk_size * chunk_size && next[16 * chunk_size] >= 32 && next[16 * chunk_size] <= 34,
                "the following chunk continues from its neighbor's edge, not from a global scan");
        require(save.chunk(6, 3)->settling.empty(), "a chunk with no transitional neighbors is fully settled");
    }
    {
        Save save(path);
        require(save.chunk(3, 3)->settling == next, "settling persists with the chunk");
    }
}

}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-migration-" + std::to_string(getpid()));
    try {
        require(argc == 2, "expected content root");
        std::filesystem::create_directories(scratch);
        Settings old, updated;
        old.seed = updated.seed = 42;
        old.terrain = load_terrain(std::filesystem::path(argv[1]) / "base");
        updated.terrain = load_terrain(std::filesystem::path(argv[1]) / "expedition");
        for (int format : {1, 2}) {
            migration(scratch / ("upgrade" + std::to_string(format) + ".sqlite3"), old, updated, format);
            interrupted_upgrade(scratch / ("interrupted" + std::to_string(format) + ".sqlite3"), old, updated, format);
        }
        require(updated.terrain.version == "expedition-v2", "the flagship expedition terrain adds natural backing");
        backing(scratch / "backing.sqlite3", updated, updated);
        boundaries(scratch / "boundaries.sqlite3", old, updated);
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
