#include "app.hpp"
#include "save.hpp"

#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using namespace yonder;
namespace {

std::atomic<SDL_WindowID> window_id = 0;

bool SDLCALL observe_window(void*, SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_SHOWN) window_id = event->window.windowID;
    return true;
}

void require(bool result, const std::string& what) {
    if (!result) throw std::runtime_error(what);
}

void key(SDL_Keycode code, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.key = code;
    event.key.windowID = window_id;
    require(SDL_PushEvent(&event), "push key event");
}

void tap(SDL_Keycode code) {
    key(code, true); key(code, false);
}

}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-desktop-" + std::to_string(getpid()));
    std::filesystem::create_directories(scratch);
    auto save_path = scratch / "world.sqlite3";
    try {
        require(argc >= 2, "expected content root");
        bool building = argc >= 3 && std::string(argv[2]) == "build";
        bool empty = argc >= 3 && std::string(argv[2]) == "empty";
        std::string content = argv[1], save = save_path.string();
        {
            Settings settings;
            settings.seed = 42;
            settings.terrain = load_terrain(std::filesystem::path(content) / "expedition");
            Content blocks;
            for (const auto& [id, version] : load_manifest(std::filesystem::path(content) / "flagship.txt").packs)
                blocks.load(std::filesystem::path(content) / id);
            Save fixture(save_path);
            fixture.initialize(settings);
            World world(fixture, blocks, settings);
            for (int x = 6; x <= 20; ++x) {
                for (int y = 23; y < 30; ++y) world.set(x, y, {});
                for (int y : {30, 31}) world.set(x, y, "base:soil");
                for (int y : {28, 29, 30}) world.set(x, y, "base:stone", Layer::background);
            }
            Player p;
            if (empty) {
                p.x = 10.15; p.y = 29;
                p.inventory["base:soil"] = 2;
                p.inventory["workshop:pick"] = 0;
                p.ready = {"workshop:pick"};
            } else if (building) {
                p.x = 10.15; p.y = 29; p.selected = "base:soil";
                p.inventory["base:soil"] = 12;
                p.ready = {"base:soil"};
                for (int x = 6; x <= 20; ++x)
                    for (int y : {28, 29, 30}) world.set(x, y, {}, Layer::background);
            } else {
                world.set(10, 30, {}, Layer::background);
                p.x = 10.15; p.y = 29;
                p.inventory["base:timber"] = 2;
                p.inventory["workshop:pick"] = 1;
            }
            world.checkpoint(p);
        }
        std::string failure;
        require(SDL_Init(SDL_INIT_EVENTS), "initialize event observer");
        require(SDL_AddEventWatch(observe_window, nullptr), "observe desktop window");
        std::jthread input([&] {
            try {
                for (int i = 0; i < 1000 && window_id == 0; ++i)
                    std::this_thread::sleep_for(5ms);
                require(window_id != 0, "desktop did not show a window");
                std::this_thread::sleep_for(100ms);
                if (empty) {
                    tap(SDLK_I); tap(SDLK_D); tap(SDLK_J);
                    std::this_thread::sleep_for(40ms); // redraw after the last highlighted slot disappears
                    tap(SDLK_I); tap(SDLK_H);
                    std::this_thread::sleep_for(40ms); // the empty wheel icon
                } else if (building) {
                    key(SDLK_S, true);
                    for (int i = 0; i < 2; ++i) {
                        tap(SDLK_SPACE); key(SDLK_L, true);
                        std::this_thread::sleep_for(1100ms);
                        key(SDLK_L, false);
                    }
                    key(SDLK_S, false);
                    tap(SDLK_W); tap(SDLK_L);
                    tap(SDLK_D); tap(SDLK_L); // place a floating foreground block to the right
                    key(SDLK_A, true); // back away with Place released, keeping work-facing
                    std::this_thread::sleep_for(200ms);
                    key(SDLK_A, false);
                } else {
                    // Gather directly below, then manage the ONE inventory.
                    key(SDLK_S, true); tap(SDLK_J);
                    std::this_thread::sleep_for(80ms);
                    key(SDLK_S, false);
                    std::this_thread::sleep_for(300ms);
                    tap(SDLK_I); tap(SDLK_J); tap(SDLK_K);
                    tap(SDLK_W); key(SDLK_L, true);
                    std::this_thread::sleep_for(200ms);
                    key(SDLK_L, false);
                    // The depleted soil is still marked in slot zero.
                    tap(SDLK_I); tap(SDLK_D); tap(SDLK_J);
                    tap(SDLK_D); tap(SDLK_J); tap(SDLK_K); // equip the pick
                    tap(SDLK_H); tap(SDLK_H); // empty, then chooser at pick
                    tap(SDLK_J); // timber
                    key(SDLK_J, true); // pick again; do NOT release before closing
                    key(SDLK_D, true);
                    std::this_thread::sleep_for(80ms); // choosing while walking
                    key(SDLK_D, false);
                    tap(SDLK_W); // aim at the prepared rear wall
                    tap(SDLK_H); // equip pick with J still physically held
                    std::this_thread::sleep_for(220ms);
                    key(SDLK_J, false);
                    // Mouse and number-row input no longer operate on the world.
                    tap(SDLK_1);
                    SDL_Event mouse{};
                    mouse.type = SDL_EVENT_MOUSE_BUTTON_DOWN; mouse.button.button = SDL_BUTTON_LEFT;
                    require(SDL_PushEvent(&mouse), "push ignored mouse input");
                    tap(SDLK_P);
                    tap(SDLK_ESCAPE); // closes pause, does not quit
                    std::this_thread::sleep_for(30ms);
                    tap(SDLK_H); tap(SDLK_H); // leave the radial visible for inspection
                    std::this_thread::sleep_for(30ms);
                }
            } catch (const std::exception& error) { failure = error.what(); }
            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quit);
        });
        std::vector<const char*> args{"desktop-test", "--content", content.c_str(), "--save", save.c_str()};
        if (argc >= 4) { args.push_back("--screenshot"); args.push_back(argv[3]); }
        require(run(int(args.size()), args.data()) == 0, "desktop returned failure");
        input.join();
        require(failure.empty(), failure);
        {
            Save result(save_path);
            auto p = result.player();
            auto settings = *result.settings();
            Content blocks;
            blocks.load(std::filesystem::path(content) / "base");
            World world(result, blocks, settings);
            if (empty) {
                require(p.ready.empty() && p.inventory.at("base:soil") == 2 && p.inventory.at("workshop:pick") == 0,
                        "unreadying the last depleted slot keeps the other inventory slot valid");
            } else if (building) {
                require(world.block(10, 29) == "base:soil" && world.block(10, 28) == "base:soil", "SDL S+Space+L builds two scaffold cells without precise timing");
                require(world.block(10, 27, Layer::background) == "base:soil", "standing on that scaffold reaches the rear wall above");
                require(world.block(11, 27) == "base:soil" && world.block(11, 28).empty() && world.block(10, 27).empty(),
                        "direct forward placement can hang in midair with no adjacent foreground blocks");
                require(p.inventory.at("base:soil") == 8 && p.x < 9.5 && p.y >= 27 && p.facing == 1,
                        "held Place does not repeat; backing off the scaffold keeps work-facing even across a gap with L released");
            } else {
                require(world.block(10, 30).empty() && world.block(10, 30, Layer::background) == "base:soil", "keyboard gathering and facing-based rear placement change the intended layers");
                require(p.x > 10.35 && p.x < 11.2, "walking remains active while choosing");
                require(p.inventory.at("base:soil") == 0 && p.inventory.at("base:timber") == 2 && p.inventory.at("workshop:pick") == 1,
                        "selection does not execute a newly equipped tool");
                require(p.ready == std::vector<Block>{"base:soil", "base:timber", "workshop:pick"}, "readiness marks stay with the original inventory");
                require(p.selected.empty() && p.last_selected == "workshop:pick", "hand transitions persist the last actual held object, not an unconfirmed choice");
            }
        }
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
