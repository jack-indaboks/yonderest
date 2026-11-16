#include "app.hpp"

#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <sqlite3.h>

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

void button(int x, int y, Uint8 which, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = window_id;
    event.button.button = which;
    event.button.x = float(x);
    event.button.y = float(y);
    require(SDL_PushEvent(&event), "push mouse event");
}

std::string snapshot(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    std::string result;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
        sqlite3_stmt* query = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT value FROM meta WHERE key='player'", -1, &query, nullptr) == SQLITE_OK
            && sqlite3_step(query) == SQLITE_ROW) result = reinterpret_cast<const char*>(sqlite3_column_text(query, 0));
        sqlite3_finalize(query);
    }
    sqlite3_close(db);
    return result;
}

int inventory_count(const std::string& snapshot) {
    std::istringstream in(snapshot);
    double x, y;
    std::string selected, id;
    int slots, amount, total = 0;
    if (!(in >> x >> y >> std::quoted(selected) >> slots)) return -1;
    for (int i = 0; i < slots; ++i) {
        if (!(in >> std::quoted(id) >> amount)) return -1;
        total += amount;
    }
    return total;
}

}

int main(int argc, char** argv) {
    auto scratch = std::filesystem::temp_directory_path() / ("yonderest-desktop-" + std::to_string(getpid()));
    std::filesystem::create_directories(scratch);
    auto save_path = scratch / "world.sqlite3";
    auto picture = scratch / "frame.bmp";
    try {
        require(argc == 2, "expected content root");
        std::string content = argv[1], save = save_path.string(), screenshot = picture.string();
        std::string failure;
        require(SDL_Init(SDL_INIT_EVENTS), "initialize event observer");
        require(SDL_AddEventWatch(observe_window, nullptr), "observe desktop window");
        std::jthread input([&] {
            try {
                // Wait for the app's first real checkpoint, not a mocked ready flag.
                for (int i = 0; i < 1000 && snapshot(save_path).empty(); ++i) std::this_thread::sleep_for(5ms);
                require(!snapshot(save_path).empty(), "desktop did not initialize its save");
                require(window_id != 0, "desktop did not show a window");
                key(SDLK_D, true);
                std::this_thread::sleep_for(200ms);
                key(SDLK_D, false);
                button(640, 450, SDL_BUTTON_LEFT, true);
                std::this_thread::sleep_for(40ms);
                button(640, 450, SDL_BUTTON_LEFT, false);
                key(SDLK_F5, true);
                key(SDLK_F5, false);
                for (int i = 0; i < 100 && inventory_count(snapshot(save_path)) != 1; ++i) std::this_thread::sleep_for(5ms);
                require(inventory_count(snapshot(save_path)) == 1, "mouse mining did not gather exactly one block");
                double walked_x = 0;
                std::istringstream(snapshot(save_path)) >> walked_x;
                require(walked_x > 0.6 && walked_x < 2, "keyboard did not walk right");
                key(SDLK_A, true);
                std::this_thread::sleep_for(350ms);
                key(SDLK_A, false);
                // Select the material just gathered: Moss is the first hotbar slot.
                key(SDLK_1, true);
                key(SDLK_1, false);
                button(704, 420, SDL_BUTTON_RIGHT, true);
                std::this_thread::sleep_for(40ms);
                button(704, 420, SDL_BUTTON_RIGHT, false);
            } catch (const std::exception& error) { failure = error.what(); }
            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quit);
        });
        const char* args[]{"desktop-test", "--content", content.c_str(), "--save", save.c_str(),
                           "--seed", "42", "--screenshot", screenshot.c_str()};
        require(run(int(std::size(args)), args) == 0, "desktop returned failure");
        input.join();
        require(failure.empty(), failure);
        require(std::filesystem::file_size(picture) > 54, "SDL did not render a screenshot");
        auto final = snapshot(save_path);
        double x = 0;
        std::istringstream(final) >> x;
        require(x < 1.2 || x > 1000, "keyboard did not walk back toward the start");
        require(inventory_count(final) == 0, "mouse placement did not consume the gathered block");
        std::cout << "Desktop event loop walked, mined, selected, placed, rendered, and saved through SDL.\n";
        std::filesystem::remove_all(scratch);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nArtifacts: " << scratch << '\n';
        return 1;
    }
}
