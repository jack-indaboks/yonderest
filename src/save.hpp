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

    std::optional<Settings> settings();
    std::optional<Player> player();
    void initialize(const Settings& settings);
    std::optional<Chunk> chunk(int x, int y);
    void stage(int x, int y, const Chunk& chunk);
    void checkpoint(const Player& player);

private:
    sqlite3* db = nullptr;
};

}
