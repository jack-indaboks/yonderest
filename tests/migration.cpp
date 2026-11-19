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
        require(save.settings()->terrain.version == "expedition-v1", "persist upgraded generation configuration");
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
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
