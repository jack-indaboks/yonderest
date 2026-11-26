#pragma once

// Persistence: modules hand the engine their data; the engine writes it as JSON and gives it a
// home in the save.

#include "yonderest/player.hpp"
#include "yonderest/world.hpp"

#include <condition_variable>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>

namespace yonder {

// One thread beside play that does everything that touches the disk, in the order asked.
class Worker {
public:
    Worker();
    // Finishes what was asked, then stops.
    ~Worker();
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    void post(std::function<void()> job);
    // Waits until everything asked so far is done.
    void wait();
    // Rethrows, on the calling thread, what made a job fail. After that the worker does no more.
    void check();

private:
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::function<void()>> jobs;
    bool busy = false, stopping = false;
    std::exception_ptr failure;
    std::thread thread;
    void run();
};

// A world folder: world.json, and one file per chunk in a folder per region,
// regions/x<column>-y<row>/x<column>-y<row>.json, named by region and by chunk.
// A chunk's file exists exactly when its territory has been made and saved. Its layers are
// named as the content declares them. Reading and writing different chunks is safe from any thread.
class WorldFolder {
public:
    WorldFolder(std::filesystem::path folder, std::vector<std::string> layers)
        : folder(std::move(folder)), layers(std::move(layers)) {}
    const std::filesystem::path folder;
    const std::vector<std::string> layers;
    std::optional<Settings> settings() const;
    void write_settings(const Settings& settings) const;
    std::optional<Chunk> chunk(int x, int y) const;
    void write_chunk(int x, int y, const Chunk& chunk) const;
};

// A save is a universe: one folder holding the player and a folder for each world.
// Files are read when needed and each is replaced whole when written, on the worker;
// only one running game may hold a save.
class Save {
public:
    // The world a new player begins in.
    static constexpr const char* starting_world = "origin";
    explicit Save(const std::filesystem::path& folder);
    ~Save();
    Save(const Save&) = delete;
    Save& operator=(const Save&) = delete;

    const std::filesystem::path folder;
    WorldFolder world(const std::string& name, std::vector<std::string> layers) const;
    std::optional<Player> player();
    // Writes only if the player differs from what was last read or written. Returns whether it will.
    bool write_player(const Player& player);
    Worker worker;

private:
    int lock = -1;
    std::string player_text;
};

}
