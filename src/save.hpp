#pragma once

#include "world.hpp"
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <optional>
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
// A chunk's file exists exactly when its territory has been made and saved.
// Reading and writing different chunks is safe from any thread.
class WorldFolder {
public:
    explicit WorldFolder(std::filesystem::path folder) : folder(std::move(folder)) {}
    const std::filesystem::path folder;
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
    WorldFolder world(const std::string& name) const;
    std::optional<Player> player();
    // Writes only if the player differs from what was last read or written. Returns whether it will.
    bool write_player(const Player& player);
    Worker worker;

private:
    int lock = -1;
    std::string player_text;
};

}
