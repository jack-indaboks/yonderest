#include "save.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <sqlite3.h>

namespace yonder {
namespace {

void sql(sqlite3* db, const char* command) {
    if (sqlite3_exec(db, command, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db));
}

// A statement owns exactly one SQLite resource; no database abstraction above it.
struct Statement {
    sqlite3_stmt* value = nullptr;
    explicit Statement(sqlite3* db, const char* query) {
        if (sqlite3_prepare_v2(db, query, -1, &value, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    ~Statement() { sqlite3_finalize(value); }
    void number(int index, int n) { sqlite3_bind_int(value, index, n); }
    void text(int index, const std::string& s) {
        sqlite3_bind_text(value, index, s.c_str(), int(s.size()), SQLITE_TRANSIENT);
    }
    bool step() {
        int result = sqlite3_step(value);
        if (result != SQLITE_ROW && result != SQLITE_DONE)
            throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(value)));
        return result == SQLITE_ROW;
    }
    std::string text(int column) {
        auto p = sqlite3_column_text(value, column);
        return std::string(reinterpret_cast<const char*>(p), sqlite3_column_bytes(value, column));
    }
};

std::optional<std::string> meta(sqlite3* db, const char* key) {
    Statement query(db, "SELECT value FROM meta WHERE key=?");
    query.text(1, key);
    if (query.step()) return query.text(0);
    return std::nullopt;
}

void put_meta(sqlite3* db, const char* key, const std::string& value) {
    Statement query(db, "INSERT OR REPLACE INTO meta VALUES (?,?)");
    query.text(1, key);
    query.text(2, value);
    query.step();
}

[[noreturn]] void invalid() {
    throw std::runtime_error("Invalid or unsupported save; refusing to regenerate saved terrain.");
}

}

Save::Save(const std::filesystem::path& path) : path(path) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        throw std::runtime_error(error);
    }
    try {
        // One writer per save. Evicted chunks remain in this transaction until
        // the matching player/inventory checkpoint commits with them.
        sql(db, "BEGIN IMMEDIATE;"
                "CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);"
                "CREATE TABLE IF NOT EXISTS chunks(x INTEGER, y INTEGER, tiles TEXT NOT NULL, PRIMARY KEY(x,y));"
                "CREATE TABLE IF NOT EXISTS generation(x INTEGER, y INTEGER, profile TEXT NOT NULL, PRIMARY KEY(x,y));");
        // A reader inspecting a checkpoint can briefly hold up its commit.
        // A competing writer still fails at the initial BEGIN above.
        sqlite3_busy_timeout(db, 1000);
    } catch (...) {
        sqlite3_close(db);
        throw;
    }
}

Save::~Save() {
    // Uncheckpointed work rolls back, including chunks staged during eviction.
    sqlite3_close(db);
}

std::optional<Settings> Save::settings() {
    auto value = meta(db, "world");
    if (!value) return std::nullopt;
    std::istringstream in(*value);
    Settings s;
    int version, size;
    auto& t = s.terrain;
    if (!(in >> version >> size >> s.seed >> s.columns >> s.rows
          >> t.version >> t.grass >> t.soil >> t.stone >> t.sand >> t.timber >> t.leaves)) invalid();
    if ((version != 1 && version != 2) || size != chunk_size || s.columns < 4 || s.columns > 1024
        || s.rows < 4 || s.rows > 1024 || (t.version != "shelter-v1" && t.version != "expedition-v1")) invalid();
    if (version == 2 && !(in >> std::quoted(t.clay) >> std::quoted(t.ore))) invalid();
    format = version;
    return s;
}

std::optional<Player> Save::player() {
    auto value = meta(db, "player");
    if (!value) return std::nullopt;
    std::istringstream in(*value);
    Player p;
    int count;
    if (!(in >> p.x >> p.y >> std::quoted(p.selected) >> count) || count < 0 || count > 65536) invalid();
    for (int i = 0; i < count; ++i) {
        Block id;
        int amount;
        if (!(in >> std::quoted(id) >> amount) || id.empty() || amount < 0) invalid();
        p.inventory[id] = amount;
    }
    return p;
}

void Save::initialize(const Settings& s) {
    const auto& t = s.terrain;
    std::ostringstream out;
    out << 2 << ' ' << chunk_size << ' ' << s.seed << ' ' << s.columns << ' ' << s.rows << '\n'
        << t.version << ' ' << t.grass << ' ' << t.soil << ' ' << t.stone << ' '
        << t.sand << ' ' << t.timber << ' ' << t.leaves << '\n'
        << std::quoted(t.clay) << ' ' << std::quoted(t.ore);
    put_meta(db, "world", out.str());
}

void Save::upgrade(const Settings& settings) {
    // The format belongs to the save, not to the caller's read order.
    this->settings();
    if (format == 1) {
        auto backup_path = std::filesystem::path(path.string() + ".before-expedition.sqlite3");
        if (!std::filesystem::exists(backup_path)) {
            auto temporary = std::filesystem::path(backup_path.string() + ".tmp");
            // A separate read connection sees the last committed save, not this
            // writer's uncommitted schema. SQLite copies a consistent snapshot.
            sqlite3 *source = nullptr, *destination = nullptr;
            int result = sqlite3_open_v2(path.c_str(), &source, SQLITE_OPEN_READONLY, nullptr);
            if (result == SQLITE_OK) result = sqlite3_open(temporary.c_str(), &destination);
            sqlite3_backup* copy = nullptr;
            if (result == SQLITE_OK) {
                copy = sqlite3_backup_init(destination, "main", source, "main");
                result = copy ? sqlite3_backup_step(copy, -1) : sqlite3_errcode(destination);
            }
            if (copy) {
                int finished = sqlite3_backup_finish(copy);
                if (finished != SQLITE_OK) result = finished;
            }
            sqlite3_close(source);
            sqlite3_close(destination);
            if (result != SQLITE_DONE) {
                std::filesystem::remove(temporary);
                throw std::runtime_error("Could not back up the old shelter save; upgrade cancelled.");
            }
            std::filesystem::rename(temporary, backup_path);
        }
    }
    initialize(settings);
    format = 2;
}

std::optional<Chunk> Save::chunk(int x, int y) {
    Statement query(db, "SELECT tiles, COALESCE(profile,'shelter-v1') FROM chunks LEFT JOIN generation USING(x,y) WHERE x=? AND y=?");
    query.number(1, x);
    query.number(2, y);
    if (!query.step()) return std::nullopt;
    std::istringstream in(query.text(0));
    Chunk c;
    c.profile = query.text(1);
    bool quantities = in.peek() == 'Y';
    if (quantities) {
        std::string header;
        if (!(in >> header) || header != "Y2") invalid();
    }
    int filled = 0, count;
    // Runs encode the entire chunk, including empty space, never a seed/edit list.
    while (filled < int(c.tiles.size())) {
        Cell cell;
        if (!(in >> count >> std::quoted(cell.id)) || count <= 0 || count > int(c.tiles.size()) - filled) invalid();
        if (quantities && (!(in >> cell.quantity >> cell.placed >> cell.batch)
                           || cell.quantity < 1 || cell.quantity > 1000 || cell.batch < 1 || cell.batch > 1000)) invalid();
        std::fill_n(c.tiles.begin() + filled, count, cell);
        filled += count;
    }
    in >> std::ws;
    if (!in.eof()) invalid();
    return c;
}

void Save::stage(int x, int y, const Chunk& c) {
    std::ostringstream out;
    out << "Y2\n";
    for (std::size_t i = 0; i < c.tiles.size();) {
        std::size_t end = i + 1;
        while (end < c.tiles.size() && c.tiles[end] == c.tiles[i]) ++end;
        const auto& cell = c.tiles[i];
        out << end - i << ' ' << std::quoted(cell.id) << ' ' << cell.quantity << ' ' << cell.placed << ' ' << cell.batch << '\n';
        i = end;
    }
    Statement query(db, "INSERT OR REPLACE INTO chunks VALUES (?,?,?)");
    query.number(1, x);
    query.number(2, y);
    query.text(3, out.str());
    query.step();
    Statement profile(db, "INSERT OR REPLACE INTO generation VALUES (?,?,?)");
    profile.number(1, x);
    profile.number(2, y);
    profile.text(3, c.profile);
    profile.step();
}

std::vector<std::pair<int, int>> Save::legacy_chunks() {
    Statement query(db, "SELECT chunks.x,chunks.y FROM chunks LEFT JOIN generation USING(x,y) "
                        "WHERE profile IS NULL OR profile='shelter-v1' ORDER BY chunks.x,chunks.y");
    std::vector<std::pair<int, int>> result;
    while (query.step()) result.emplace_back(sqlite3_column_int(query.value, 0), sqlite3_column_int(query.value, 1));
    return result;
}

void Save::checkpoint(const Player& p) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << p.x << ' ' << p.y << ' ' << std::quoted(p.selected) << ' ' << p.inventory.size() << '\n';
    for (const auto& [id, count] : p.inventory) out << std::quoted(id) << ' ' << count << '\n';
    put_meta(db, "player", out.str());
    sql(db, "COMMIT; BEGIN IMMEDIATE;");
}

}
