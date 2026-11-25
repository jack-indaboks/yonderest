#include "app.hpp"
#include "save.hpp"

#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cmath>
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
    auto save_path = scratch / "world";
    try {
        require(argc >= 2, "expected content root");
        bool building = argc >= 3 && std::string(argv[2]) == "build";
        bool empty = argc >= 3 && std::string(argv[2]) == "empty";
        bool reference = argc >= 3 && std::string(argv[2]) == "reference";
        bool facing = argc >= 3 && std::string(argv[2]) == "facing";
        bool crawl = argc >= 3 && std::string(argv[2]) == "crawl";
        bool reach = argc >= 3 && std::string(argv[2]) == "reach";
        std::string content = argv[1], save = save_path.string();
        {
            Settings settings;
            settings.seed = 42;
            settings.columns = 8; settings.rows = 4;
            settings.terrain = load_terrain(std::filesystem::path(content) / "expedition");
            Content blocks;
            for (const auto& [id, version] : load_manifest(std::filesystem::path(content) / "flagship.json").packs)
                blocks.load(std::filesystem::path(content) / id);
            Save fixture(save_path);
            auto folder = fixture.world(Save::starting_world);
            folder.write_settings(settings);
            World world(fixture, folder, blocks, settings);
            world.load(6, 23, 20, 31);
            for (int x = 6; x <= 20; ++x) {
                for (int y = 23; y < 30; ++y) world.set(x, y, {});
                for (int y : {30, 31}) world.set(x, y, "base:soil");
                for (int y : {28, 29, 30}) world.set(x, y, "base:stone", Layer::background);
            }
            Player p;
            p.world = Save::starting_world;
            if (reach) {
                p.x = 10.15; p.y = 29; p.selected = "workshop:pick";
                p.inventory["workshop:pick"] = 1;
                for (int x = 9; x <= 13; ++x) world.set(x, 28, "base:stone");
            } else if (facing || crawl) {
                p.x = 10.15; p.y = 29; p.selected = "base:soil";
                p.inventory["base:soil"] = 3;
                for (int x = 9; x <= 14; ++x) for (int y : {28, 29}) world.set(x, y, {}, Layer::background);
            } else if (reference) {
                world.set(10, 29, {}, Layer::background);
                p.x = 10.15; p.y = 29; p.selected = "base:soil";
                p.inventory["base:soil"] = 3;
                p.ready = {"base:soil"};
            } else if (empty) {
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
            world.save_changes(p);
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
                if (reach) {
                    key(SDLK_W, true); tap(SDLK_J);       // held W digs straight up
                    std::this_thread::sleep_for(40ms);
                    key(SDLK_D, true);                    // and shuffles along the wall without turning
                    std::this_thread::sleep_for(160ms);
                    tap(SDLK_J);
                    std::this_thread::sleep_for(40ms);
                    key(SDLK_W, false);                   // the held D then takes over facing
                    key(SDLK_D, false);
                } else if (facing) {
                    key(SDLK_D, true); key(SDLK_W, true); // walking right, turned to the wall
                    std::this_thread::sleep_for(40ms);
                    key(SDLK_W, false);                   // D is still held: it takes over
                    key(SDLK_D, false);
                    tap(SDLK_L);                          // so this places in front
                    tap(SDLK_W); tap(SDLK_L);             // letting go of W alone changes nothing: a rear wall
                    tap(SDLK_H);                          // empty hand
                    key(SDLK_S, true); tap(SDLK_J);       // S, pressed last, turns sideways to crawl: J digs the floor
                    std::this_thread::sleep_for(40ms);
                } else if (crawl) {
                    key(SDLK_S, true);
                    std::this_thread::sleep_for(40ms);
                    key(SDLK_W, true); tap(SDLK_L);       // W, pressed last, ends the crawl: L reaches above the head
                    std::this_thread::sleep_for(40ms);
                } else if (reference) {
                    // The recipe reference stays up while building and never acts on the world.
                    tap(SDLK_R); tap(SDLK_RIGHT); tap(SDLK_RIGHT); tap(SDLK_LEFT);
                    std::this_thread::sleep_for(60ms);
                    tap(SDLK_ESCAPE);  // with only the reference visible: the menu, not "close reference"
                    tap(SDLK_L);       // swallowed by the open menu, or it would place in front
                    tap(SDLK_RETURN);  // Resume; Enter must not reach the world
                    tap(SDLK_I); tap(SDLK_ESCAPE); // the inventory closes; it does not open the menu
                    tap(SDLK_W); tap(SDLK_L);      // so this rear placement reaches the world
                    tap(SDLK_ESCAPE); tap(SDLK_DOWN); tap(SDLK_RETURN); // Controls page
                    tap(SDLK_ESCAPE); tap(SDLK_L); // back to the menu, which swallows L
                    tap(SDLK_ESCAPE);              // play again
                    tap(SDLK_F5); tap(SDLK_F1); tap(SDLK_F2); // no function-key bindings remain
                    std::this_thread::sleep_for(60ms);
                } else if (empty) {
                    tap(SDLK_I); tap(SDLK_D); tap(SDLK_J); // the only slot left is soil: ready it
                    std::this_thread::sleep_for(40ms);
                    tap(SDLK_I); tap(SDLK_H);              // a chooser holding soil alone
                    std::this_thread::sleep_for(40ms);
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
                    key(SDLK_A, true); // walk away: A turns the character, placing or not
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
                    // The emptied soil has left; timber and the pick remain.
                    tap(SDLK_I); tap(SDLK_J);
                    tap(SDLK_D); tap(SDLK_J); tap(SDLK_K); // equip the pick
                    tap(SDLK_H); tap(SDLK_H); // empty, then chooser at pick
                    tap(SDLK_J); // timber
                    key(SDLK_J, true); // pick again; do NOT release before closing
                    tap(SDLK_SPACE);   // out of the hole just dug: steps are jumped, never climbed
                    key(SDLK_D, true);
                    std::this_thread::sleep_for(160ms); // choosing while walking, once clear of the hole
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
        std::vector<const char*> args{"desktop-test", "--content", content.c_str(), "--save-path", save.c_str()};
        if (argc >= 4) { args.push_back("--screenshot"); args.push_back(argv[3]); }
        require(run(int(args.size()), args.data()) == 0, "desktop returned failure");
        input.join();
        require(failure.empty(), failure);
        {
            Save result(save_path);
            auto p = *result.player();
            auto folder = result.world(p.world);
            Content blocks;
            blocks.load(std::filesystem::path(content) / "base");
            World world(result, folder, blocks, *folder.settings());
            world.load(6, 23, 20, 31);
            if (reach) {
                require(world.block(10, 28).empty() && world.block(11, 28).empty() && world.block(12, 28) == "base:stone",
                        "W held digs the cell above the head, standing and while shuffling");
                require(p.inventory.at("base:stone") == 2 && p.facing == 1, "letting go of W hands facing to the held D");
            } else if (facing) {
                bool front = false, rear = false, dug = false;
                for (int x = 10; x <= 14; ++x) {
                    front |= world.block(x, 29) == "base:soil";
                    rear |= world.block(x, 29, Layer::background) == "base:soil";
                    dug |= world.block(x, 30).empty() && world.block(x, 30, Layer::background) == "base:stone";
                }
                require(front, "letting go of W hands facing to a held D");
                require(rear, "letting go of W alone leaves the character facing the rear wall");
                require(dug && p.inventory.at("base:soil") == 2, "S pressed while facing the wall turns sideways to crawl and aim beneath");
            } else if (crawl) {
                require(world.block(10, 28) == "base:soil" && p.facing == 0,
                        "W pressed while crawling ends the crawl and reaches up");
            } else if (reference) {
                require(world.block(11, 29).empty() && world.block(10, 29, Layer::background) == "base:soil",
                        "only the rear placement in play reached the world; dialogs swallowed the others");
                require(p.inventory.at("base:soil") == 2 && std::abs(p.x - 10.15) < 0.001, "browsing recipes neither moves nor spends");
            } else if (empty) {
                require(!p.inventory.contains("workshop:pick") && p.inventory.at("base:soil") == 2
                            && p.ready == std::vector<Block>{"base:soil"} && p.selected.empty(),
                        "a save's emptied entry and its readiness are dropped on load; only carried objects ready");
            } else if (building) {
                require(world.block(10, 29) == "base:soil" && world.block(10, 28) == "base:soil", "SDL S+Space+L builds two scaffold cells without precise timing");
                require(world.block(10, 27, Layer::background) == "base:soil", "standing on that scaffold reaches the rear wall above");
                require(world.block(11, 27) == "base:soil" && world.block(11, 28).empty() && world.block(10, 27).empty(),
                        "direct forward placement can hang in midair with no adjacent foreground blocks");
                require(p.inventory.at("base:soil") == 8 && p.x < 9.5 && p.y >= 27 && p.facing == -1,
                        "held Place does not repeat; walking off after placing faces the way walked, with no remembered work-facing");
            } else {
                require(world.block(10, 30).empty() && world.block(10, 30, Layer::background) == "base:soil", "keyboard gathering and facing-based rear placement change the intended layers");
                require(p.x > 10.35 && p.x < 11.2, "walking remains active while choosing");
                require(!p.inventory.contains("base:soil") && p.inventory.at("base:timber") == 2 && p.inventory.at("workshop:pick") == 1,
                        "selection does not execute a newly equipped tool");
                require(p.ready == std::vector<Block>{"base:timber", "workshop:pick"}, "the emptied soil left the ready marks; the others stay");
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
