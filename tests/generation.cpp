#include "save.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
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

void legacy_save(const std::filesystem::path& path, const Settings& old) {
    sqlite3* db = nullptr;
    require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "create legacy fixture");
    require(sqlite3_exec(db, "CREATE TABLE meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
                            "CREATE TABLE chunks(x INTEGER,y INTEGER,tiles TEXT NOT NULL,PRIMARY KEY(x,y));"
                            "INSERT INTO meta VALUES('player','6.15 94.3 \"base:soil\" 1\n\"base:timber\" 7\n');",
                            nullptr, nullptr, nullptr) == SQLITE_OK, "legacy schema");
    std::string text = "1 32 42 32 8\nshelter-v1 base:grass base:soil base:stone base:sand base:timber base:leaves";
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
        for (const auto& cell : chunk.tiles) tiles << 1 << ' ' << std::quoted(cell.id) << '\n';
        text = tiles.str();
        require(sqlite3_prepare_v2(db, "INSERT INTO chunks VALUES(0,?,?)", -1, &statement, nullptr) == SQLITE_OK, "legacy chunk statement");
        sqlite3_bind_int(statement, 1, cy);
        sqlite3_bind_text(statement, 2, text.c_str(), -1, SQLITE_TRANSIENT);
        require(sqlite3_step(statement) == SQLITE_DONE, "legacy chunk data");
        sqlite3_finalize(statement);
    }
    sqlite3_close(db);
}

void migration(const std::filesystem::path& path, const Settings& old, const Settings& updated) {
    legacy_save(path, old);
    auto old_tiles = query(path, "SELECT tiles FROM chunks WHERE x=0 AND y=2");
    {
        Save save(path);
        auto settings = *save.settings();
        auto p = *save.player();
        require(settings.terrain.version == "shelter-v1", "read format-1 configuration");
        require(save.chunk(0, 2)->at(3, 29).id == old.terrain.timber, "read the old bunker");
        save.upgrade(updated);
        auto c = generate(updated, 4, 3);
        c.at(2, 3) = Cell{"base:timber", 10, true};
        c.at(3, 3) = Cell{"workshop:workbench", 1, true, 5};
        save.stage(4, 3, c);
        save.checkpoint(p);
    }
    auto backup = std::filesystem::path(path.string() + ".before-expedition.sqlite3");
    require(std::filesystem::exists(backup), "create the pre-upgrade backup");
    require(query(backup, "SELECT value FROM meta WHERE key='world'").starts_with("1 32"), "backup remains format 1");
    require(query(backup, "SELECT tiles FROM chunks WHERE x=0 AND y=2") == old_tiles, "backup preserves exact chunk payload");
    require(query(path, "SELECT tiles FROM chunks WHERE x=0 AND y=2") == old_tiles, "upgrade never rewrites an old chunk");
    {
        Save save(path);
        require(save.settings()->terrain.version == "expedition-v1", "persist upgraded generation configuration");
        auto c = *save.chunk(4, 3);
        require(c.at(2, 3) == Cell{"base:timber", 10, true}, "persist unfinished batch quantities and provenance");
        require(c.at(3, 3).batch == 5, "persist workbench setting");
        require(save.legacy_chunks() == std::vector<std::pair<int, int>>{{0, 2}, {0, 3}}, "old-territory footprint excludes new chunks");
        auto p = *save.player();
        require(p.x == 6.15 && p.inventory.at("base:timber") == 7, "upgrade preserves player and inventory");
    }
}

void interrupted_upgrade(const std::filesystem::path& path, const Settings& old, const Settings& updated) {
    legacy_save(path, old);
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
    require(query(path, "SELECT value FROM meta WHERE key='world'").starts_with("1 32"), "interrupted upgrade leaves the old save intact");
    require(std::filesystem::exists(path.string() + ".before-expedition.sqlite3"), "interrupted upgrade already has a complete backup");
}

void geography(const Settings& old, const Settings& updated) {
    std::map<Block, int> resources;
    std::set<std::string> regions;
    for (int cx = 0; cx < updated.columns; ++cx) {
        regions.insert(biome(updated, cx * chunk_size));
        for (int cy = 0; cy < updated.rows; ++cy) {
            auto before = generate(old, cx, cy), after = generate(updated, cx, cy);
            for (std::size_t i = 0; i < before.tiles.size(); ++i) {
                require(before.tiles[i].id.empty() == after.tiles[i].id.empty(), "update must not alter landform, trees, or caves");
                ++resources[after.tiles[i].id];
            }
            require(after.tiles == generate(updated, cx + updated.columns, cy).tiles, "every resource field must wrap");
        }
    }
    require(regions.size() >= 3 && resources[updated.terrain.clay] > 100 && resources[updated.terrain.ore] > 100,
            "world must contain distinct resource-bearing regions");
    const std::vector<std::pair<int, int>> legacy{{0, 2}, {0, 3}};
    for (auto [cx, cy] : std::vector<std::pair<int, int>>{{1, 3}, {31, 3}, {0, 4}}) {
        auto a = generate(updated, cx, cy, legacy);
        generate(updated, 5, 5, legacy);
        require(a.tiles == generate(updated, cx, cy, legacy).tiles, "transition must not depend on new exploration order");
    }
    for (auto [cx, cy] : std::vector<std::pair<int, int>>{{3, 3}, {29, 3}, {0, 6}})
        require(generate(updated, cx, cy, legacy).tiles == generate(updated, cx, cy).tiles,
                "transition must settle to ordinary generation within two chunks, including vertically and at seam");
    auto edge = generate(updated, 31, 3, legacy), original = generate(old, 31, 3);
    int different = 0;
    for (int y = 0; y < chunk_size; ++y) different += edge.at(31, y).id != original.at(31, y).id;
    require(different <= 1, "material transition should meet the old seam gently");
}

}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-generation-" + std::to_string(getpid()));
    try {
        require(argc == 2, "expected content root");
        std::filesystem::create_directories(scratch);
        Settings old, updated;
        old.seed = updated.seed = 42;
        old.terrain = load_terrain(std::filesystem::path(argv[1]) / "base");
        updated.terrain = load_terrain(std::filesystem::path(argv[1]) / "expedition");
        geography(old, updated);
        migration(scratch / "upgrade.sqlite3", old, updated);
        interrupted_upgrade(scratch / "interrupted.sqlite3", old, updated);
        std::filesystem::remove_all(scratch);
        std::cout << "Old bunker and backup survive upgrade; regional resources, transitions, wrapping, and quantities persist.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
