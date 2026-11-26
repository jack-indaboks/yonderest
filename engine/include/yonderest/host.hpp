#pragma once

// The engine: the keeper of the world and the place where modules meet.

#include "yonderest/content.hpp"
#include "yonderest/controls.hpp"
#include "yonderest/display.hpp"
#include "yonderest/events.hpp"
#include "yonderest/generation.hpp"
#include "yonderest/interaction.hpp"
#include "yonderest/physics.hpp"
#include "yonderest/player.hpp"
#include "yonderest/save.hpp"
#include "yonderest/world.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <vector>

namespace yonder {

struct Options {
    std::string name;
    std::filesystem::path path, content, screenshot;
    std::optional<std::uint64_t> seed;
    std::optional<std::pair<int, int>> size;
    int frames = 0;
};
// Exits after printing help for -h and --help.
Options parse_options(int argc, const char* const* argv);

// After every module attached and the player is in the world, before play.
struct Started {
    bool fresh; // The world was just made.
};
// Every tick, after the player moved.
struct Tick {};
// Play stops taking held input: the window lost focus or a screen opened.
struct Interrupted {};

class Host {
public:
    // Loads the content. Modules attach next, then `start` opens the save.
    explicit Host(Options options);
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    Options options;
    Content content;
    Events events;

    // The modules serving each interface.
    Generation* generation = nullptr;
    Physics* physics = nullptr;
    Interaction* interaction = nullptr;
    Controls* controls = nullptr;
    Display* display = nullptr;

    // Keeps a module's own state for as long as the game runs.
    template <class T, class... Args>
    T& adopt(Args&&... args) {
        auto made = std::make_shared<T>(std::forward<Args>(args)...);
        owned.push_back(made);
        return *made;
    }

    // What a module hands the display to show, asked for each time it draws.
    template <class T>
    void provide(std::function<T()> provider) {
        providers[std::type_index(typeid(T))] = [provider = std::move(provider)](void* out) {
            *static_cast<std::optional<T>*>(out) = provider();
        };
    }
    template <class T>
    std::optional<T> provided() const {
        std::optional<T> result;
        if (auto found = providers.find(std::type_index(typeid(T))); found != providers.end()) found->second(&result);
        return result;
    }
    // Entries modules add to the menu, between Resume and Quit, in the order they attach.
    std::vector<MenuItem> menu_items;

    // Opens the save, its world and the player. A new player stands on the ground of the world's
    // first chunk.
    void start();
    Save& save() { return *saved; }
    World& world() { return *opened; }
    Player& player() { return you; }

    // The cell in front of the player.
    Target target() const;
    // Frames an action and has it carried out.
    Action act(Act act);
    void intent(std::string_view name);
    // Sets what the player holds.
    void hold(const Block& block);
    void back();

    void open(Screen& screen, bool interrupting = true);
    // Returns to the menu where it was left, as from a page it opened.
    void show_menu();
    void close();
    Screen* screen() const { return shown; }

    void interrupt();
    void tick();
    // Writes everything changed, behind play. Returns whether anything changed.
    bool save_changes();
    double now() const;
    void quit() { quitting = true; }

    // Runs frames until quitting, then saves.
    int run();

private:
    // Modules outlast the world, whose worker may still be generating for them.
    std::vector<std::shared_ptr<void>> owned;
    std::map<std::type_index, std::function<void(void*)>> providers;
    std::unique_ptr<Save> saved;
    std::unique_ptr<WorldFolder> folder;
    std::unique_ptr<World> opened;
    Player you;
    bool fresh = false, quitting = false;
    Screen* shown = nullptr;
    double accumulated = 0;
    std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
    std::unique_ptr<Screen> menu, pause;
};

// The modules this game is built with, attached in order. The game's build provides it.
void attach_modules(Host& host);

// The program: options, content, the modules `attach` attaches, then play.
int run(int argc, const char* const* argv, void (*attach)(Host&));

}
