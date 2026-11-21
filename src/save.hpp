#pragma once

#include "world.hpp"
#include <optional>

namespace yonder {

// A world is one folder: world.json, player.json and chunks/x<column>-y<row>.json.
// A chunk's file exists exactly when its territory has been generated and saved.
// Files are read when needed and each is replaced whole when written; only one
// running game may hold a world.
class Save {
public:
    explicit Save(const std::filesystem::path& folder);
    ~Save();
    Save(const Save&) = delete;
    Save& operator=(const Save&) = delete;

    std::optional<Settings> settings() const;
    void write_settings(const Settings& settings);
    std::optional<Player> player();
    // Writes only if the player differs from what was last read or written. Returns whether it wrote.
    bool write_player(const Player& player);
    std::optional<Chunk> chunk(int x, int y) const;
    void write_chunk(int x, int y, const Chunk& chunk);

private:
    std::filesystem::path folder;
    int lock = -1;
    std::string player_text;
};

}
