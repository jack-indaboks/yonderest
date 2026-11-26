#pragma once

// The display interface: how the player sees everything. The display module owns the window and
// draws; modules that need to show something hand it what to show, in these terms, and never
// depend on the display module itself.

#include "yonderest/space.hpp"
#include "yonderest/world.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace yonder {

// A brief message.
struct Notice {
    std::string text;
    double seconds = 2.0;
};

// A completed arrangement resolving into its result. The world has already changed; this
// shows it happening.
struct Transformation {
    struct Part {
        int x, y;
        Block block;
    };
    Part result;
    std::vector<Part> consumed;
};

// What lies behind each place: open sky that darkens underground, and distant ridges.
struct Backdrop {
    double daylight;                              // 1 in open sky, 0 well underground.
    double level;                                 // The row the ridges stand above.
    std::function<double(int ridge, double x)> rise; // Each ridge's rise in blocks at column x.
};

// Something drawn on a cell beside its block, such as how many it holds.
struct Label {
    std::function<std::string(const Cell&)> text;
};

// The ring of readied objects around the character.
struct Ring {
    bool open = false;
    std::vector<std::pair<Block, std::int64_t>> entries;
    std::size_t selected = 0;
};

// The backpack's place in the corner: open or closed, and opened or closed by the pointer.
struct Bag {
    bool open;
    std::function<void()> toggle;
};

// The recipe book in the corner, and the page it is open at.
struct Book {
    struct Cell {
        int x, y;
        Block block; // Empty: the cell must stay empty.
    };
    struct Need {
        Block block;
        int count;
        bool enough;
    };
    bool open;
    std::function<void()> toggle;
    // The page it is open at.
    Block output{};
    int amount = 1, width = 0, height = 0;
    std::vector<Cell> shape{};
    std::vector<Block> stations{}; // Working surfaces beneath the shape.
    std::vector<Need> needs{};
    int index = 0, count = 0;
    std::function<void(int pages)> browse{};
};

// Screens laid over play, which take the keys while open.
struct Menu {
    std::vector<std::string> items;
    int index;
    std::function<void(int item)> choose, hover;
};
struct Sheet {
    std::string title;
    std::vector<std::pair<std::string, std::string>> lines;
    std::string footer;
};
struct Banner {
    std::string text;
};
struct Backpack {
    struct Entry {
        Block block;
        std::string name;
        std::int64_t count;
        bool ready, held;
    };
    std::string title, readiness;
    std::vector<Entry> entries;
    int columns, rows;
    int index, focus; // Focus is an action, or -1 for the entries.
    std::array<std::string, 3> actions;
    std::function<void(int entry, int clicks)> select;
    std::function<void(int action)> act;
};
using View = std::variant<Menu, Sheet, Banner, Backpack>;

class Screen {
public:
    virtual ~Screen() = default;
    // Controls choose their bindings by it.
    virtual std::string_view name() const = 0;
    // A command named by the screen's owner, such as "up", "confirm" or "back".
    virtual void command(std::string_view command) = 0;
    virtual View view() = 0;
};

// An entry in the menu, between Resume and Quit.
struct MenuItem {
    std::function<std::string()> label;
    std::function<void()> choose;
};

class Display {
public:
    virtual ~Display() = default;
    // Play begins: once the land around the player is in place, show it.
    virtual void begin() = 0;
    // Collects what happened since the last frame, passing keys on to the controls.
    virtual void collect() = 0;
    virtual bool focused() const = 0;
    virtual void draw(double now) = 0;
    virtual void capture(const std::filesystem::path& file) = 0;
    virtual void present() = 0;
};

}
