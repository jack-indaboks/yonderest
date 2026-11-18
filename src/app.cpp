#include "app.hpp"
#include "save.hpp"
#include "world.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>

using namespace yonder;
namespace {

constexpr int screen_width = 640, screen_height = 360, tile = 16;
constexpr int world_top = 32, world_bottom = 307;

void sdl(bool result) {
    if (!result) throw std::runtime_error(SDL_GetError());
}

struct Options {
    std::filesystem::path save, content, manifest, screenshot;
    std::vector<std::filesystem::path> packs;
    std::optional<std::uint64_t> seed;
    int frames = 0;
    bool keep_generation = false;
};

Options options(int argc, const char* const* argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "Yonderest - the first expedition\n"
                         "  --save FILE       Separate SQLite world (default: SDL user data/yonderest)\n"
                         "  --seed NUMBER     Seed for a NEW save only\n"
                         "  --content DIR     Content root (manifest and packs)\n"
                         "  --manifest FILE   Pack and terrain configuration\n"
                         "  --keep-generation Keep this save's existing terrain profile\n"
                         "  --pack DIR        Additional content pack (repeatable)\n"
                         "  --frames NUMBER   Exit after this many frames (render checks)\n"
                         "  --screenshot FILE Save the last frame as BMP\n"
                         "A/D or arrows: walk; Space/W/Up: jump; mouse: aim\n"
                         "Left mouse: gather; right: place; 1-9/wheel: select\n"
                         "E over workbench: batch 1/5/10; F2: recipe plans\n"
                         "F1: help; F5: save; F11: fullscreen; Esc: save and quit\n";
            std::exit(0);
        }
        if (arg == "--keep-generation") { o.keep_generation = true; continue; }
        if (i + 1 == argc) throw std::runtime_error("Missing value for " + arg);
        std::string value = argv[++i];
        if (arg == "--save") o.save = value;
        else if (arg == "--content") o.content = value;
        else if (arg == "--manifest") o.manifest = value;
        else if (arg == "--pack") o.packs.emplace_back(value);
        else if (arg == "--screenshot") o.screenshot = value;
        else if (arg == "--seed") {
            std::size_t end;
            o.seed = std::stoull(value, &end);
            if (end != value.size() || value.starts_with('-')) throw std::runtime_error("Invalid seed");
        } else if (arg == "--frames") {
            std::size_t end;
            o.frames = std::stoi(value, &end);
            if (end != value.size() || o.frames <= 0) throw std::runtime_error("Invalid frame count");
        } else throw std::runtime_error("Unknown option: " + arg);
    }
    return o;
}

class View {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    double camera_x = 0, camera_y = 0;
    int facing = 1;

    View() {
        sdl(SDL_Init(SDL_INIT_VIDEO));
        sdl(SDL_CreateWindowAndRenderer("Yonderest - first expedition", 1280, 720,
                                       SDL_WINDOW_RESIZABLE, &window, &renderer));
        sdl(SDL_SetRenderLogicalPresentation(renderer, screen_width, screen_height, SDL_LOGICAL_PRESENTATION_LETTERBOX));
        SDL_SetRenderVSync(renderer, 1);
    }
    ~View() {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void color(int r, int g, int b, int a = 255) {
        SDL_SetRenderDrawColor(renderer, Uint8(std::clamp(r, 0, 255)), Uint8(std::clamp(g, 0, 255)),
                               Uint8(std::clamp(b, 0, 255)), Uint8(a));
    }
    void rectangle(float x, float y, float w, float h) {
        SDL_FRect rect{x, y, w, h};
        SDL_RenderFillRect(renderer, &rect);
    }
    void text(int x, int y, const std::string& value) {
        SDL_RenderDebugText(renderer, float(x), float(y), value.c_str());
    }
    void follow(const Player& p, World& world) {
        camera_x = p.x + player_width / 2 - screen_width / (2.0 * tile);
        camera_y = std::clamp(p.y - 12.0, -world_top / double(tile), world.settings.height() - world_bottom / double(tile));
        // Six blocks cover interaction reach, not a ring of full-height columns.
        world.retain(int(std::floor(camera_x)) - 6, int(std::floor(camera_y)) - 1,
                     int(std::ceil(camera_x + screen_width / double(tile))) + 6,
                     int(std::ceil(camera_y + screen_height / double(tile))) + 1);
    }
    std::pair<int, int> target(float mx, float my) const {
        return {int(std::floor(camera_x + mx / tile)), int(std::floor(camera_y + my / tile))};
    }
    void block_icon(const Material& m, int x, int y, int detail) {
        auto [r, g, b] = m.color;
        if (m.appearance == "glass") {
            color(r, g, b);
            SDL_FRect frame{float(x), float(y), tile, tile};
            SDL_RenderRect(renderer, &frame);
            rectangle(float(x + 2), float(y + 2), 2, 7);
            color(194, 224, 226);
            rectangle(float(x + 4), float(y + 2), 5, 1);
            return;
        }
        if (m.appearance == "bench") {
            color(r, g, b);
            rectangle(float(x), float(y + 2), 16, 5);
            rectangle(float(x + 2), float(y + 7), 3, 9);
            rectangle(float(x + 11), float(y + 7), 3, 9);
            color(r + 28, g + 28, b + 28);
            rectangle(float(x), float(y + 2), 16, 1);
            return;
        }
        color(r, g, b);
        rectangle(float(x), float(y), tile, tile);
        color(r - 18, g - 18, b - 18);
        rectangle(float(x), float(y + 14), tile, 2);
        if (m.appearance == "bricks" || m.appearance == "planks" || m.appearance == "metal") {
            for (int row = 4; row < tile; row += 5) {
                rectangle(float(x), float(y + row), tile, 1);
                if (m.appearance == "bricks") rectangle(float(x + (row % 2 ? 4 : 11)), float(y + row - 4), 1, 4);
            }
        }
        if (m.appearance == "forge") {
            color(30, 32, 39); rectangle(float(x + 3), float(y + 5), 10, 8);
            color(227, 112, 54); rectangle(float(x + 5), float(y + 8), 6, 4);
            color(255, 201, 103); rectangle(float(x + 7), float(y + 8), 2, 3);
            return;
        }
        if (m.appearance == "ore") {
            color(203, 127, 73);
            rectangle(float(x + 3), float(y + 5), 4, 3);
            rectangle(float(x + 9), float(y + 9), 3, 3);
        }
        for (int i = 0; i < 3; ++i) {
            int px = (detail + i * 7) % 12 + 2, py = (detail / 13 + i * 5) % 11 + 2;
            color(r + (i == 0 ? 18 : -12), g + (i == 0 ? 18 : -12), b + (i == 0 ? 18 : -12));
            rectangle(float(x + px), float(y + py), 2, 1);
        }
    }
    void plan(const Content& content, int index) {
        color(20, 28, 37); rectangle(30, 47, 580, 251);
        color(234, 220, 186);
        text(47, 61, "RECIPE PLANS / LEFT-RIGHT TO BROWSE / F2 CLOSES");
        if (content.recipes.empty()) { text(47, 90, "No recipes loaded."); return; }
        const auto& r = content.recipes[wrap(index, int(content.recipes.size()))];
        text(47, 84, content.material(r.output).name + " x" + std::to_string(r.amount) + " per assembly");
        text(47, 101, "Place this shape. Completion transforms it.");
        text(47, 116, "One result here; surplus goes to inventory.");
        std::set<Block> ingredients;
        for (const auto& i : r.shape) {
            int x = 70 + i.x * 24, y = 133 + i.y * 24;
            if (i.id.empty()) {
                color(70, 85, 94);
                SDL_FRect empty{float(x), float(y), 16, 16};
                SDL_RenderRect(renderer, &empty);
            } else {
                block_icon(content.material(i.id), x, y, 0);
                ingredients.insert(i.id);
            }
        }
        int y = 133;
        for (const auto& id : ingredients) {
            block_icon(content.material(id), 293, y - 3, 0);
            color(234, 220, 186);
            text(321, y, content.material(id).name);
            y += 23;
        }
        color(165, 187, 190);
        text(47, 240, r.stations.empty() ? "Build on solid ground or your workshop." : "Required underneath: " + r.stations.front());
        text(47, 257, "A workbench underneath batches the whole shape.");
        text(47, 274, "Forge + bench: side by side beneath the shape.");
    }
    void draw(World& world, const Player& p, float mx, float my, bool help, const std::string& message) {
        for (int y = 0; y < screen_height; y += 4) {
            color(25 + y / 7, 47 + y / 5, 73 + y / 6);
            rectangle(0, float(y), screen_width, 4);
        }
        color(247, 218, 153);
        rectangle(473, 52, 22, 18);
        rectangle(476, 49, 16, 24);
        for (int layer = 0; layer < 3; ++layer) {
            color(51 - layer * 8, 84 - layer * 9, 100 - layer * 9);
            for (int sx = 0; sx < screen_width; sx += 4) {
                double angle = (camera_x + sx / double(tile)) * (2 * std::numbers::pi / world.settings.width());
                int ridge = int(142 + layer * 28 + 20 * std::sin(angle * (5 + layer * 2) + layer)
                                + 9 * std::sin(angle * 17) - camera_y * 0.3);
                rectangle(float(sx), float(ridge), 4, float(screen_height - ridge));
            }
        }
        int left = int(std::floor(camera_x)), top = int(std::floor(camera_y));
        for (int x = left; x <= left + screen_width / tile + 1; ++x)
            for (int y = top; y <= top + screen_height / tile + 1; ++y) {
                int sx = int(std::floor((x - camera_x) * tile));
                int sy = int(std::floor((y - camera_y) * tile));
                auto cell = world.cell(x, y);
                Block id = cell.id;
                if (id.empty()) {
                    if (y >= surface(world.settings, x)) {
                        color(24, 33, 43);
                        rectangle(float(sx), float(sy), tile, tile);
                    }
                    continue;
                }
                int detail = wrap(x, world.settings.width()) * 31 + y * 177;
                block_icon(world.content.material(id), sx, sy, detail);
                if (cell.quantity > 1) {
                    color(255, 226, 139);
                    text(sx, sy + 7, std::to_string(cell.quantity));
                }
                if (world.block(x, y - 1).empty()) {
                    auto [r, g, b] = world.content.material(id).color;
                    color(r + 28, g + 28, b + 28);
                    rectangle(float(sx), float(sy), tile, 2);
                }
            }
        int px = int((p.x - camera_x) * tile), py = int((p.y - camera_y) * tile);
        color(31, 33, 42); rectangle(float(px + 1), float(py + 22), 4, 5); rectangle(float(px + 7), float(py + 22), 4, 5);
        color(162, 72, 56); rectangle(float(px + 1), float(py + 9), 10, 14);
        color(209, 116, 67); rectangle(float(px + 2), float(py + 9), 8, 3);
        color(84, 65, 51); rectangle(float(px + (facing > 0 ? 0 : 8)), float(py + 12), 4, 8);
        color(230, 187, 137); rectangle(float(px + 3), float(py + 3), 7, 7);
        color(48, 40, 42); rectangle(float(px + 2), float(py + 1), 8, 4);
        rectangle(float(px + (facing > 0 ? 8 : 3)), float(py + 6), 1, 1);

        auto [tx, ty] = target(mx, my);
        double reach = std::hypot(distance_x(p.x + player_width / 2, tx + 0.5, world.settings.width()), ty + 0.5 - (p.y + player_height / 2));
        if (my >= world_top && my < world_bottom) {
            color(reach <= 5 ? 240 : 147, reach <= 5 ? 214 : 113, 137);
            SDL_FRect rect{float((tx - camera_x) * tile), float((ty - camera_y) * tile), tile, tile};
            SDL_RenderRect(renderer, &rect);
            if (reach <= 5) {
                auto cell = world.cell(tx, ty);
                std::string label;
                if (!cell.id.empty()) {
                    label = world.content.material(cell.id).name;
                    if (cell.quantity > 1) label += " x" + std::to_string(cell.quantity);
                    if (world.content.material(cell.id).station == "workbench") label += " batch x" + std::to_string(cell.batch) + " [E]";
                } else {
                    int q = placement_quantity(world, p.selected, tx, ty);
                    if (q > 1) label = "Place x" + std::to_string(q);
                }
                color(234, 220, 186);
                text(std::clamp(int(mx) - int(label.size()) * 4, 4, std::max(4, screen_width - int(label.size()) * 8 - 4)),
                     std::clamp(int(my) - 13, 34, 290), label);
            }
        }
        color(20, 28, 37); rectangle(0, 0, screen_width, 30);
        color(234, 220, 186); text(8, 6, "YONDEREST / EXPEDITION");
        color(165, 187, 190);
        text(8, 18, "A/D walk SPACE jump LMB gather RMB build F1 help F2 plans");
        text(216, 6, world.region(int(p.x), int(p.y + player_height)));
        text(440, 6, "x " + std::to_string(int(p.x)) + " / " + std::to_string(world.settings.width()));
        color(20, 28, 37); rectangle(0, world_bottom, screen_width, screen_height - world_bottom);
        auto bar = slots(world.content, p);
        int selected_index = int(std::find(bar.begin(), bar.end(), p.selected) - bar.begin());
        int first = selected_index / 9 * 9;
        for (int i = first; i < std::min(first + 9, int(bar.size())); ++i) {
            int x = 8 + (i - first) * 48;
            if (bar[i] == p.selected) { color(139, 118, 78); rectangle(float(x - 2), 310, 46, 27); }
            block_icon(world.content.material(bar[i]), x + 2, 315, i * 31);
            auto count = p.inventory.find(bar[i]);
            color(238, 226, 199);
            text(x + 22, 314, std::to_string(i - first + 1));
            int amount = count == p.inventory.end() ? 0 : count->second;
            text(x + 22, 325, amount < 1000 ? std::to_string(amount) : std::to_string(amount / 1000) + "k");
        }
        color(234, 220, 186);
        text(8, 344, world.content.material(p.selected).name + " | " + message.substr(0, 60));
        color(165, 187, 190); text(456, 317, "WHEEL selects"); text(456, 329, "F5 save / ESC exit");
        if (help) {
            color(20, 28, 37); rectangle(55, 62, 530, 188);
            color(234, 220, 186);
            const std::vector<std::string> lines{
                "FIND A PLACE. MAKE IT YOURS.", "",
                "A/D or arrows walk. Space, W or Up jumps.",
                "Small steps are climbed automatically.",
                "Aim with the mouse, within five blocks.",
                "Hold left mouse to gather, right mouse to build.",
                "Build beside a block; leave a two-block doorway.",
                "Wheel or 1-9 selects a material. Gather it first.",
                "F2 shows recipes. E over a bench sets batch size.",
                "F5 saves. Esc saves and quits. Autosave: 5 seconds.",
                "F11 fullscreen. F1 closes this help."};
            int y = 75;
            for (const auto& line : lines) { text(70, y, line); y += 14; }
        }
    }
    void screenshot(const std::filesystem::path& path) {
        SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
        if (!pixels) throw std::runtime_error(SDL_GetError());
        bool result = SDL_SaveBMP(pixels, path.c_str());
        SDL_DestroySurface(pixels);
        sdl(result);
    }
};

int run_game(Options o) {
    View view;
    if (o.content.empty()) o.content = std::filesystem::path(SDL_GetBasePath()) / "content";
    if (o.save.empty()) {
        char* path = SDL_GetPrefPath(nullptr, "yonderest");
        if (!path) throw std::runtime_error(SDL_GetError());
        o.save = std::filesystem::path(path) / "world.sqlite3";
        SDL_free(path);
    }
    Content content;
    auto manifest = load_manifest(o.manifest.empty() ? o.content / "flagship.txt" : o.manifest);
    for (const auto& [id, version] : manifest.packs) {
        content.load(o.content / id);
        auto found = content.modules.find(id);
        if (found == content.modules.end() || found->second.version != version) throw std::runtime_error("Manifest version mismatch for " + id);
    }
    for (const auto& pack : o.packs) content.load(pack);
    content.validate();
    auto terrain = load_terrain(o.content / manifest.terrain);
    Save save(o.save);
    auto saved_settings = save.settings();
    Settings settings;
    if (saved_settings) {
        settings = *saved_settings;
        if (!o.keep_generation && settings.terrain.version == "shelter-v1" && terrain.version == "expedition-v1") {
            settings.terrain.version = terrain.version;
            settings.terrain.clay = terrain.clay;
            settings.terrain.ore = terrain.ore;
            std::cout << "New territory: expedition-v1; saved chunks stay unchanged.\n";
        }
        save.upgrade(settings);
        if (o.seed && *o.seed != settings.seed) std::cout << "Existing save: using its seed, not --seed.\n";
    } else {
        settings.seed = o.seed.value_or(std::uint64_t(std::chrono::system_clock::now().time_since_epoch().count()));
        settings.terrain = terrain;
        if (o.keep_generation) settings.terrain.version = "shelter-v1";
        save.initialize(settings);
    }
    World world(save, content, settings);
    auto saved_player = save.player();
    if (saved_settings && !saved_player) throw std::runtime_error("Save has no player state.");
    Player player = saved_player ? *saved_player : spawn(world);
    if (!std::isfinite(player.x) || !std::isfinite(player.y) || player.x < 0 || player.x >= settings.width()
        || player.y < 0 || player.y + player_height > settings.height()) throw std::runtime_error("Invalid saved player position.");
    std::cout << "Save: " << o.save << "\nSeed: " << settings.seed << '\n';
    world.checkpoint(player);

    bool quit = false, help = false, plans = false, focused = true, jump = false;
    int plan_index = 0;
    bool left = false, right = false, mining = false, placing = false;
    float mx = 320, my = 210;
    double previous = SDL_GetTicksNS() / 1e9, accumulated = 0, autosave = previous, next_action = 0;
    std::string message = "F2: assembly plans. Bring something home.";
    int frames = 0;
    view.follow(player, world);
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            sdl(SDL_ConvertEventToRenderCoordinates(view.renderer, &event));
            if (event.type == SDL_EVENT_QUIT) quit = true;
            else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                focused = false;
                left = right = mining = placing = jump = false;
                world.checkpoint(player);
                message = "Saved (paused)";
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) focused = true;
            else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                bool down = event.type == SDL_EVENT_KEY_DOWN;
                auto key = event.key.key;
                if (plans && down && !event.key.repeat && (key == SDLK_LEFT || key == SDLK_RIGHT)) {
                    plan_index += key == SDLK_RIGHT ? 1 : -1;
                    continue;
                }
                if (key == SDLK_A || key == SDLK_LEFT) left = down;
                if (key == SDLK_D || key == SDLK_RIGHT) right = down;
                if (down && !event.key.repeat) {
                    if (key == SDLK_SPACE || key == SDLK_W || key == SDLK_UP) jump = true;
                    if (key == SDLK_ESCAPE) quit = true;
                    if (key == SDLK_F1) { help = !help; plans = false; mining = placing = false; }
                    if (key == SDLK_F2) { plans = !plans; help = false; left = right = mining = placing = jump = false; }
                    if (key == SDLK_E && !plans && !help) {
                        auto [x, y] = view.target(mx, my);
                        message = adjust_workbench(world, player, x, y).message;
                    }
                    if (key == SDLK_F5) { world.checkpoint(player); message = "Saved"; }
                    if (key == SDLK_F11) sdl(SDL_SetWindowFullscreen(view.window, !(SDL_GetWindowFlags(view.window) & SDL_WINDOW_FULLSCREEN)));
                    if (key >= SDLK_1 && key <= SDLK_9) {
                        auto bar = slots(content, player);
                        int selected = int(std::find(bar.begin(), bar.end(), player.selected) - bar.begin());
                        int index = (selected / 9) * 9 + int(key - SDLK_1);
                        if (index < int(bar.size())) player.selected = bar[index];
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) { mx = event.motion.x; my = event.motion.y; }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                mx = event.button.x; my = event.button.y;
                if (event.button.button == SDL_BUTTON_LEFT) mining = down;
                if (event.button.button == SDL_BUTTON_RIGHT) placing = down;
                next_action = 0;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                auto bar = slots(content, player);
                int index = int(std::find(bar.begin(), bar.end(), player.selected) - bar.begin());
                int direction = event.wheel.y > 0 ? -1 : (event.wheel.y < 0 ? 1 : 0);
                if (!bar.empty()) player.selected = bar[wrap(index + direction, int(bar.size()))];
            }
        }
        double now = SDL_GetTicksNS() / 1e9;
        accumulated += focused && !help && !plans ? std::min(0.1, now - previous) : 0;
        previous = now;
        while (accumulated >= tick_seconds) {
            int direction = int(right) - int(left);
            if (direction) view.facing = direction;
            move(world, player, direction, jump);
            jump = false;
            accumulated -= tick_seconds;
        }
        view.follow(player, world);
        if (focused && !help && !plans && my >= world_top && my < world_bottom && (mining || placing) && now >= next_action) {
            auto [x, y] = view.target(mx, my);
            auto result = act(world, player, mining ? Action::mine : Action::place, x, y);
            message = result.message;
            next_action = now + 0.16;
        }
        if (now - autosave >= 5) {
            world.checkpoint(player);
            autosave = now;
            message = "Autosaved";
        }
        view.draw(world, player, mx, my, help, message);
        if (plans) view.plan(content, plan_index);
        ++frames;
        if (o.frames && frames >= o.frames) quit = true;
        if (quit && !o.screenshot.empty()) view.screenshot(o.screenshot);
        sdl(SDL_RenderPresent(view.renderer));
        SDL_Delay(1);
    }
    world.checkpoint(player);
    return 0;
}

}

int yonder::run(int argc, const char* const* argv) {
    return run_game(options(argc, argv));
}
