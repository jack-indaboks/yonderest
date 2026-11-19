#pragma once

#include "world.hpp"
#include <optional>

struct sqlite3;

namespace yonder {

class Save {
public:
    explicit Save(const std::filesystem::path& path);
    ~Save();
    Save(const Save&) = delete;
    Save& operator=(const Save&) = delete;

    std::optional<Settings> settings() const;
    Player player();
    void initialize(const Settings& settings);
    // Back up earlier formats before committing the keyboard prototype upgrade.
    void upgrade(const Settings& settings);
    std::optional<Chunk> chunk(int x, int y);
    void stage(int x, int y, const Chunk& chunk);
    void checkpoint(const Player& player);
    std::vector<std::pair<int, int>> legacy_chunks();

private:
    sqlite3* db = nullptr;
    std::filesystem::path path;
};

}
