#include "yonderest/host.hpp"
#include "platform.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <tuple>

namespace yonder {
namespace {

constexpr const char* default_save = "first";

std::optional<int> power_of_two(const std::string& text, int most) {
    std::size_t end = 0;
    int n = 0;
    try { n = std::stoi(text, &end); } catch (const std::exception&) { return std::nullopt; }
    if (end != text.size() || n < 1 || n > most || (n & (n - 1))) return std::nullopt;
    return n;
}

// The menu: Resume, what modules add, and Quit.
class MainMenu : public Screen {
public:
    explicit MainMenu(Host& host) : host(host) {}
    int index = 0;
    std::string_view name() const override { return "menu"; }
    int items() const { return int(host.menu_items.size()) + 2; }
    void choose(int item) {
        if (item == 0) host.close();
        else if (item == items() - 1) host.quit(); // Quitting always saves.
        else host.menu_items[item - 1].choose();
    }
    void command(std::string_view command) override {
        if (command == "up") index = wrap(index - 1, items());
        else if (command == "down") index = wrap(index + 1, items());
        else if (command == "confirm") choose(index);
        else if (command == "back") host.close();
    }
    View view() override {
        Menu menu{{"Resume"}, index, [this](int item) { choose(item); }, [this](int item) { index = item; }};
        for (const auto& item : host.menu_items) menu.items.push_back(item.label());
        menu.items.push_back("Quit");
        return menu;
    }

private:
    Host& host;
};

class Pause : public Screen {
public:
    explicit Pause(Host& host) : host(host) {}
    std::string_view name() const override { return "pause"; }
    void command(std::string_view command) override {
        if (command == "confirm" || command == "back") host.close();
    }
    View view() override { return Banner{"Paused"}; }

private:
    Host& host;
};

}

Options parse_options(int argc, const char* const* argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: yonderest [options]\n"
                         "  --save NAME        Open the save called NAME, or make it (default: " << default_save << ").\n"
                         "                     Saves are kept in yonderest/saves in your user data folder.\n"
                         "  --save-path DIR    Open or make a save in DIR instead\n"
                         "  --size AROUNDxTALL A new save's world size, in chunks of 64 blocks (default: "
                      << starting_columns << 'x' << starting_rows << ").\n"
                         "                     Each is a power of two, up to " << max_columns << " around and " << max_rows << " tall.\n"
                         "  --seed NUMBER      A new save's seed\n"
                         "  --content DIR      Content root (manifest and packs)\n"
                         "  --manifest FILE    Pack and terrain configuration\n"
                         "  --pack DIR         Additional content pack (repeatable)\n"
                         "  --frames NUMBER    Exit after this many frames (render checks)\n"
                         "  --screenshot FILE  Save the last frame as BMP\n"
                         "  -h, --help         Show this help\n"
                         "In game, Esc opens the menu and its Controls page.\n";
            std::exit(0);
        }
        if (i + 1 == argc) throw std::runtime_error("Missing value for " + arg);
        std::string value = argv[++i];
        if (arg == "--save") {
            if (value.empty() || value == "." || value == ".." || value.find('/') != std::string::npos)
                throw std::runtime_error("--save takes a name; use --save-path for a folder: " + value);
            o.name = value;
        } else if (arg == "--save-path") o.path = value;
        else if (arg == "--content") o.content = value;
        else if (arg == "--manifest") o.manifest = value;
        else if (arg == "--pack") o.packs.emplace_back(value);
        else if (arg == "--screenshot") o.screenshot = value;
        else if (arg == "--seed") {
            std::size_t end;
            o.seed = std::stoull(value, &end);
            if (end != value.size() || value.starts_with('-')) throw std::runtime_error("Invalid seed");
        } else if (arg == "--size") {
            auto x = value.find('x');
            auto around = x == std::string::npos ? std::nullopt : power_of_two(value.substr(0, x), max_columns);
            auto tall = x == std::string::npos ? std::nullopt : power_of_two(value.substr(x + 1), max_rows);
            if (!around || !tall)
                throw std::runtime_error("--size takes AROUNDxTALL in chunks, each a power of two up to "
                                         + std::to_string(max_columns) + "x" + std::to_string(max_rows) + ": " + value);
            o.size = {*around, *tall};
        } else if (arg == "--frames") {
            std::size_t end;
            o.frames = std::stoi(value, &end);
            if (end != value.size() || o.frames <= 0) throw std::runtime_error("Invalid frame count");
        } else throw std::runtime_error("Unknown option: " + arg);
    }
    if (!o.name.empty() && !o.path.empty()) throw std::runtime_error("Give --save or --save-path, not both");
    return o;
}

Host::Host(Options o) : options(std::move(o)), menu(std::make_unique<MainMenu>(*this)), pause(std::make_unique<Pause>(*this)) {
    if (options.content.empty()) options.content = program_folder() / "content";
    content.load_flagship(options.content, options.manifest.empty() ? options.content / "flagship.json" : options.manifest);
    for (const auto& pack : options.packs) content.load(pack);
    content.validate();
    events.on<Intent>([this](Intent& intent) {
        if (intent.name == "pause") open(*pause);
    });
}

Host::~Host() = default;

void Host::start() {
    if (!generation || !physics || !interaction) throw std::runtime_error("The game needs modules for generation, physics and interaction");
    if (options.path.empty()) {
        auto data = data_folder();
        options.path = data / "saves" / (options.name.empty() ? default_save : options.name);
        // Until 0.0.7 the one world lived beside the saves, where 0.0.6 can still open it.
        if (!std::filesystem::exists(options.path) && std::filesystem::exists(data / "world" / "world.json"))
            std::cout << "The world in " << (data / "world").string() << " is from Yonderest 0.0.6 or earlier"
                         " and stays there; this version starts a new save.\n";
    }
    saved = std::make_unique<Save>(options.path);
    auto saved_player = saved->player();
    folder = std::make_unique<WorldFolder>(saved->world(saved_player ? saved_player->world : Save::starting_world, content.layers));
    auto saved_settings = folder->settings();
    Settings settings;
    if (saved_settings) {
        settings = *saved_settings;
        if (options.seed && *options.seed != settings.seed) std::cout << "Existing save: using its seed, not --seed.\n";
        if (options.size) std::cout << "Existing save: keeping its size, not --size.\n";
    } else {
        if (saved_player) throw std::runtime_error("The player is in " + folder->folder.string() + ", which has no world.json");
        settings.seed = options.seed.value_or(std::uint64_t(std::chrono::system_clock::now().time_since_epoch().count()));
        if (options.size) std::tie(settings.columns, settings.rows) = *options.size;
        generation->create(settings);
        folder->write_settings(settings);
        fresh = true;
    }
    generation->open(settings);
    opened = std::make_unique<World>(*saved, *folder, content, settings, *generation);
    opened->changed = [this](int x, int y, Layer layer, const Cell& before) { events.announce(Changed{x, y, layer, before}); };
    if (saved_player) you = std::move(*saved_player);
    else {
        // A new player stands on the ground of the world's first chunk, in column zero.
        int top = generation->first_row() * chunk_size;
        opened->load(0, top, 0, top);
        bool placed = false;
        for (int x = 0; x < chunk_size && !placed; ++x)
            for (int y = top; y < top + chunk_size - 1 && !placed; ++y)
                if (opened->block(x, y).empty() && physics->solid(opened->block(x, y + 1))) {
                    you.x = x + 0.15; // The body's centre over the cell's.
                    you.y = y;
                    placed = true;
                }
        if (!placed) throw std::runtime_error("No place to stand in the world's first chunk");
    }
    you.world = folder->folder.filename().string();
    std::cout << "Save: " << options.path.string() << "\nWorld: " << you.world << ", " << settings.columns << " chunks around and "
              << settings.rows << " tall\nSeed: " << settings.seed << '\n';
    events.announce(Started{fresh});
    // What modules settled on loading, such as an emptied entry, is not a change to write back.
    if (saved_player) saved->settle_player(you);
}

Target Host::target() const {
    return yonder::target(you, physics->takeoff());
}

Action Host::act(Act which) {
    Action action{.act = which, .cell = target(), .held = you.held};
    events.announce(action);
    if (!action.claimed) {
        interaction->act(action);
        events.announce(Acted{which});
    }
    return action;
}

Intent Host::intent(std::string_view name) {
    return events.announce(Intent{name});
}

void Host::hold(const Block& block) {
    you.held = block;
    events.announce(Held{});
}

void Host::back() {
    if (events.announce(Back{}).claimed) return;
    if (shown) shown->command("back");
    else {
        static_cast<MainMenu&>(*menu).index = 0;
        open(*menu);
    }
}

void Host::open(Screen& screen, bool interrupting) {
    shown = &screen;
    if (interrupting) interrupt();
}

void Host::show_menu() {
    open(*menu, false);
}

void Host::close() {
    shown = nullptr;
}

void Host::interrupt() {
    you.up = you.down = false;
    accumulated = 0;
    events.announce(Interrupted{});
}

void Host::tick() {
    physics->tick(*opened, you);
    events.announce(Tick{});
}

bool Host::save_changes() {
    bool wrote = opened->save_changes();
    return saved->write_player(you) || wrote;
}

double Host::now() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
}

int Host::run() {
    if (!display || !controls) throw std::runtime_error("The game needs modules for the display and controls");
    double previous = now(), last_save = previous;
    display->begin();
    int frames = 0;
    while (!quitting) {
        display->collect();
        double t = now();
        bool playing = display->focused() && !shown;
        accumulated += playing ? std::min(0.1, t - previous) : 0;
        previous = t;
        while (accumulated >= tick_seconds) {
            tick();
            accumulated -= tick_seconds;
        }
        controls->frame(t);
        // Once a minute, and only if something changed. Taking and placing only change memory.
        if (t - last_save >= 60) {
            if (save_changes()) events.announce(Notice{"Saved"});
            last_save = t;
        }
        display->draw(t);
        ++frames;
        if (options.frames && frames >= options.frames) quitting = true;
        if (quitting && !options.screenshot.empty()) display->capture(options.screenshot);
        display->present();
    }
    save_changes();
    saved->worker.wait();
    saved->worker.check();
    return 0;
}

int run(int argc, const char* const* argv, void (*attach)(Host&)) {
    Host host(parse_options(argc, argv));
    attach(host);
    host.start();
    return host.run();
}

}
