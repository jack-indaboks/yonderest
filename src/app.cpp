#include "app.hpp"
#include "save.hpp"
#include "play.hpp"

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

enum class Panel { none, help, plans, inventory, pause };

Options options(int argc, const char* const* argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "Yonderest - keyboard homebuilding\n"
                         "  --save FILE       Separate SQLite world (default: SDL user data/yonderest)\n"
                         "  --seed NUMBER     Seed for a NEW save only\n"
                         "  --content DIR     Content root (manifest and packs)\n"
                         "  --manifest FILE   Pack and terrain configuration\n"
                         "  --keep-generation Keep this save's existing terrain profile\n"
                         "  --pack DIR        Additional content pack (repeatable)\n"
                         "  --frames NUMBER   Exit after this many frames (render checks)\n"
                         "  --screenshot FILE Save the last frame as BMP\n"
                         "A/D walk; Space jump; W face background; S crouch/crawl and aim down\n"
                         "J hit/gather; K interact; L place; semicolon sprint\n"
                         "H hand/chooser/equip; J/K cycle while choosing; I inventory\n"
                         "P pause; F1 help; F2 plans; F5 save; F11 fullscreen\n";
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

    View() {
        sdl(SDL_Init(SDL_INIT_VIDEO));
        sdl(SDL_CreateWindowAndRenderer("Yonderest - keyboard homebuilding", 1280, 720,
                                       SDL_WINDOW_RESIZABLE, &window, &renderer));
        sdl(SDL_SetRenderLogicalPresentation(renderer, screen_width, screen_height, SDL_LOGICAL_PRESENTATION_LETTERBOX));
        SDL_SetRenderVSync(renderer, 1);
    }
    ~View() {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void color(int r, int g, int b) {
        SDL_SetRenderDrawColor(renderer, Uint8(std::clamp(r, 0, 255)), Uint8(std::clamp(g, 0, 255)),
                               Uint8(std::clamp(b, 0, 255)), 255);
    }
    void rectangle(float x, float y, float w, float h) {
        SDL_FRect rect{x, y, w, h};
        SDL_RenderFillRect(renderer, &rect);
    }
    void text(int x, int y, const std::string& value) {
        SDL_RenderDebugText(renderer, x, y, value.c_str());
    }
    void follow(const Player& p, World& world) {
        camera_x = p.x + player_width / 2 - screen_width / (2.0 * tile);
        camera_y = std::clamp(p.y - 12.0, -world_top / double(tile), world.settings.height() - world_bottom / double(tile));
        // A small view margin, not full-height columns.
        world.retain(int(std::floor(camera_x)) - 6, int(std::floor(camera_y)) - 1,
                     int(std::ceil(camera_x + screen_width / double(tile))) + 6,
                     int(std::ceil(camera_y + screen_height / double(tile))) + 1);
    }
    void block_icon(const Material& m, int x, int y, int detail, bool rear = false) {
        auto [r, g, b] = m.color;
        if (rear) { r = r * 3 / 5; g = g * 3 / 5; b = b * 3 / 5; }
        if (m.appearance == "pick") {
            color(146, 104, 62); rectangle(x + 7, y + 4, 3, 12);
            color(r, g, b); rectangle(x + 2, y + 2, 12, 3);
            rectangle(x + 1, y + 4, 3, 3);
            return;
        }
        if (m.appearance == "glass") {
            color(r, g, b);
            SDL_FRect frame{float(x), float(y), tile, tile};
            SDL_RenderRect(renderer, &frame);
            rectangle(x + 2, y + 2, 2, 7);
            color(194, 224, 226);
            rectangle(x + 4, y + 2, 5, 1);
            return;
        }
        if (m.appearance == "bench") {
            color(r, g, b);
            rectangle(x, y + 2, 16, 5);
            rectangle(x + 2, y + 7, 3, 9);
            rectangle(x + 11, y + 7, 3, 9);
            color(r + 28, g + 28, b + 28);
            rectangle(x, y + 2, 16, 1);
            return;
        }
        color(r, g, b);
        rectangle(x, y, tile, tile);
        color(r - 18, g - 18, b - 18);
        rectangle(x, y + 14, tile, 2);
        if (m.appearance == "bricks" || m.appearance == "planks" || m.appearance == "metal") {
            for (int row = 4; row < tile; row += 5) {
                rectangle(x, y + row, tile, 1);
                if (m.appearance == "bricks") rectangle(x + (row % 2 ? 4 : 11), y + row - 4, 1, 4);
            }
        }
        if (m.appearance == "forge") {
            color(30, 32, 39); rectangle(x + 3, y + 5, 10, 8);
            color(227, 112, 54); rectangle(x + 5, y + 8, 6, 4);
            color(255, 201, 103); rectangle(x + 7, y + 8, 2, 3);
            return;
        }
        if (m.appearance == "ore") {
            color(203, 127, 73);
            rectangle(x + 3, y + 5, 4, 3);
            rectangle(x + 9, y + 9, 3, 3);
        }
        for (int i = 0; i < 3; ++i) {
            int px = (detail + i * 7) % 12 + 2, py = (detail / 13 + i * 5) % 11 + 2;
            color(r + (i == 0 ? 18 : -12), g + (i == 0 ? 18 : -12), b + (i == 0 ? 18 : -12));
            rectangle(x + px, y + py, 2, 1);
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
        text(47, 240, r.stations.empty() ? "No station needed. Ingredients can hang in midair." : "Required underneath: " + r.stations.front());
        text(47, 257, "A workbench underneath batches the whole shape.");
        text(47, 274, "Forge + bench: side by side beneath the shape.");
    }
    void hand_icon(int x, int y) {
        color(230, 187, 137);
        rectangle(x + 3, y + 7, 10, 7);
        for (int i = 0; i < 4; ++i) rectangle(x + 3 + i * 3, y + 2 + i % 2, 2, 8);
        rectangle(x, y + 8, 4, 3);
    }
    void ring(const Content& content, const Player& p, const Hands& hands, int px, int py) {
        auto ready = readied(p);
        if (ready.empty()) {
            // An empty, crossed-out wheel, not a text error.
            color(20, 28, 37); rectangle(px - 14, py - 48, 28, 28);
            color(165, 187, 190);
            SDL_FRect box{float(px - 10), float(py - 44), 20, 20};
            SDL_RenderRect(renderer, &box);
            SDL_RenderLine(renderer, px - 10, py - 24, px + 10, py - 44);
            return;
        }
        int selected = int(std::ranges::find(ready, hands.choice) - ready.begin());
        for (int i = 0; i < int(ready.size()); ++i) {
            double angle = 2 * std::numbers::pi * (i - selected) / ready.size() - std::numbers::pi / 2;
            int x = px + int(48 * std::cos(angle)) - 8;
            int y = py + int(48 * std::sin(angle)) - 8;
            color(i == selected ? 139 : 20, i == selected ? 118 : 28, i == selected ? 78 : 37);
            rectangle(x - 3, y - 3, 22, 22);
            block_icon(content.material(ready[i]), x, y, 0);
        }
    }
    void inventory(const Content& content, const Player& p, int index) {
        color(20, 28, 37); rectangle(48, 45, 544, 253);
        color(234, 220, 186);
        text(64, 57, "INVENTORY / WASD OR ARROWS SELECT / I CLOSES");
        text(64, 74, "J ready/unready (12 max) / K equip / H empty hand");
        auto items = slots(p);
        if (items.empty()) { hand_icon(310, 156); return; }
        int first = index / 24 * 24;
        for (int i = first; i < std::min(first + 24, int(items.size())); ++i) {
            int x = 64 + (i - first) % 6 * 84, y = 100 + (i - first) / 6 * 38;
            bool ready = std::ranges::find(p.ready, items[i]) != p.ready.end();
            color(i == index ? 80 : 30, i == index ? 87 : 40, i == index ? 86 : 49);
            rectangle(x, y, 76, 32);
            block_icon(content.material(items[i]), x + 6, y + 4, 0);
            color(234, 220, 186); text(x + 28, y + 8, std::to_string(p.inventory.at(items[i])));
            if (ready) {
                color(255, 212, 120); rectangle(x + 2, y + 28, 72, 2);
            }
            if (items[i] == p.selected) { color(230, 187, 137); rectangle(x + 65, y + 5, 6, 6); }
        }
        color(234, 220, 186); text(64, 266, content.material(items[index]).name);
        color(255, 212, 120); text(360, 266, "READY " + std::to_string(p.ready.size()) + "/12");
    }
    void pause() {
        color(20, 28, 37); rectangle(123, 124, 394, 90);
        color(234, 220, 186); text(144, 142, "PAUSED / P, ENTER OR ESC TO RESUME");
        text(144, 168, "Q SAVES AND QUITS");
    }
    void draw(World& world, const Player& p, const Hands& hands, bool help, const std::string& message) {
        // Excavation near the surface opens onto the scenery. Depth darkens the
        // whole backdrop, rather than preserving a silhouette of removed terrain.
        double daylight = 1 - std::clamp((p.y - surface(world.settings, int(p.x))) / 12.0, 0.0, 1.0);
        for (int y = 0; y < screen_height; y += 4) {
            color(int((25 + y / 7) * daylight) + 8, int((47 + y / 5) * daylight) + 10, int((73 + y / 6) * daylight) + 16);
            rectangle(0, y, screen_width, 4);
        }
        if (daylight > 0.8) {
            color(247, 218, 153); rectangle(473, 52, 22, 18); rectangle(476, 49, 16, 24);
        }
        for (int layer = 0; layer < 3; ++layer) {
            color(int((51 - layer * 8) * daylight), int((84 - layer * 9) * daylight), int((100 - layer * 9) * daylight));
            for (int sx = 0; sx < screen_width; sx += 4) {
                double angle = (camera_x + sx / double(tile)) * (2 * std::numbers::pi / world.settings.width());
                int ridge = int(142 + layer * 28 + 20 * std::sin(angle * (5 + layer * 2) + layer)
                                + 9 * std::sin(angle * 17) - camera_y * 0.3);
                rectangle(sx, ridge, 4, screen_height - ridge);
            }
        }
        int left = int(std::floor(camera_x)), top = int(std::floor(camera_y));
        for (auto layer : {Layer::background, Layer::foreground})
            for (int x = left; x <= left + screen_width / tile + 1; ++x)
                for (int y = top; y <= top + screen_height / tile + 1; ++y) {
                    const auto& cell = world.cell(x, y, layer);
                    if (cell.id.empty()) continue;
                    int sx = int(std::floor((x - camera_x) * tile));
                    int sy = int(std::floor((y - camera_y) * tile));
                    int detail = wrap(x, world.settings.width()) * 31 + y * 177;
                    block_icon(world.content.material(cell.id), sx, sy, detail, layer == Layer::background);
                    if (cell.quantity > 1) {
                        color(255, 226, 139); text(sx, sy + 7, std::to_string(cell.quantity));
                    }
                }
        int px = int((p.x - camera_x) * tile), py = int((p.y - camera_y) * tile);
        int feet = py + int(p.height() * tile);
        color(31, 33, 42); rectangle(px + 1, feet - 3, 4, 3); rectangle(px + 7, feet - 3, 4, 3);
        color(162, 72, 56); rectangle(px + 1, py + (p.crouched ? 3 : 7), 10, p.crouched ? 3 : 6);
        color(230, 187, 137); rectangle(px + 3, py + 2, 7, p.crouched ? 3 : 5);
        color(48, 40, 42); rectangle(px + 2, py, 8, 3);
        if (p.facing == 0) {
            color(84, 65, 51); rectangle(px + 2, py + 5, 8, p.crouched ? 2 : 6);
        } else {
            color(48, 40, 42); rectangle(px + (p.facing > 0 ? 8 : 3), py + 4, 1, 1);
            color(230, 187, 137); rectangle(px + (p.facing > 0 ? 10 : 0), feet - 6, 2, 2);
        }
        if (hands.chooser) ring(world.content, p, hands, px + 6, py + 4);

        color(20, 28, 37); rectangle(0, 0, screen_width, 30);
        color(234, 220, 186); text(8, 6, "YONDEREST / HOME");
        color(165, 187, 190);
        text(8, 18, "A/D walk SPACE jump W turn S crouch J hit K use L place");
        text(200, 6, world.region(int(p.x), int(p.y + p.height())));
        text(440, 6, "x " + std::to_string(int(p.x)) + " / " + std::to_string(world.settings.width()));
        color(20, 28, 37); rectangle(0, world_bottom, screen_width, screen_height - world_bottom);
        if (p.selected.empty()) hand_icon(8, 314);
        else block_icon(world.content.material(p.selected), 8, 314, 0);
        color(234, 220, 186);
        text(32, 314, p.selected.empty() ? "Hand" : world.content.material(p.selected).name + " x" + std::to_string(p.inventory.at(p.selected)));
        auto t = yonder::target(p);
        const auto& addressed = world.cell(t.x, t.y, t.layer);
        text(300, 314, (t.layer == Layer::background ? "REAR " : "FRONT ") + world.content.material(addressed.id).name);
        color(165, 187, 190); text(8, 331, "H hand/choose/equip  I inventory  ; sprint  P pause");
        color(234, 220, 186); text(8, 346, message.substr(0, 77));
        if (help) {
            color(20, 28, 37); rectangle(47, 51, 546, 232);
            color(234, 220, 186);
            static const std::string lines[]{
                "FIND A PLACE. MAKE IT YOURS.", "",
                "A/D walk. Space jumps. Semicolon sprints.",
                "W faces the rear wall; A/D turns sideways again.",
                "Hold S to crouch/crawl and aim beneath your feet.",
                "J hits/gathers; empty hand or a held tool.",
                "K interacts (a workbench cycles batch 1/5/10).",
                "L places once. S + Space + L builds underfoot.",
                "An early underfoot press waits for clearance.",
                "H: object -> empty -> chooser -> equip.",
                "J/K cycle the open chooser; keep walking.",
                "I manages the one inventory and readiness marks.",
                "F2 plans. F5 saves. P pauses and offers quit.",
                "F11 fullscreen. F1 closes this help."};
            int y = 64;
            for (const auto& line : lines) { text(62, y, line); y += 15; }
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
    Player player = saved_settings ? save.player() : spawn(world);
    std::cout << "Save: " << o.save << "\nSeed: " << settings.seed << '\n';
    world.checkpoint(player);

    Hands hands;
    bool quit = false, focused = true;
    Panel panel = Panel::none;
    bool left = false, right = false, down = false, sprint = false, mining = false, jump = false;
    int plan_index = 0, inventory_index = 0;
    double previous = SDL_GetTicksNS() / 1e9, accumulated = 0, autosave = previous, next_action = 0;
    std::string message = "F1: controls. I: inventory. F2: assembly plans.";
    int frames = 0;
    auto release = [&] {
        left = right = down = sprint = mining = jump = false;
        hands.cancel();
        accumulated = 0;
    };
    auto toggle = [&](Panel next) {
        panel = panel == next ? Panel::none : next;
        release();
    };
    view.follow(player, world);
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) quit = true;
            else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                focused = false;
                release();
                world.checkpoint(player);
                message = "Saved (paused)";
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) focused = true;
            else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                bool pressed = event.type == SDL_EVENT_KEY_DOWN;
                auto key = event.key.key;
                if (!pressed) {
                    if (key == SDLK_A) left = false;
                    if (key == SDLK_D) right = false;
                    if (key == SDLK_S) { down = false; crouch(world, player, false); }
                    if (key == SDLK_SEMICOLON) sprint = false;
                    if (key == SDLK_J) mining = false;
                    continue;
                }
                if (event.key.repeat) continue;
                if (key == SDLK_F5) { world.checkpoint(player); message = "Saved"; continue; }
                if (key == SDLK_F11) {
                    sdl(SDL_SetWindowFullscreen(view.window, !(SDL_GetWindowFlags(view.window) & SDL_WINDOW_FULLSCREEN)));
                    continue;
                }
                if (key == SDLK_ESCAPE) {
                    if (hands.chooser) { hands.cancel(); mining = false; continue; }
                    panel = panel == Panel::none ? Panel::pause : Panel::none;
                    release();
                    continue;
                }
                if (key == SDLK_F1) { toggle(Panel::help); continue; }
                if (key == SDLK_F2) { toggle(Panel::plans); continue; }
                if (key == SDLK_I) {
                    toggle(Panel::inventory);
                    inventory_index = 0;
                    continue;
                }
                if (key == SDLK_P) { toggle(Panel::pause); continue; }
                if (panel == Panel::pause) {
                    if (key == SDLK_Q) quit = true;
                    if (key == SDLK_RETURN) panel = Panel::none;
                    continue;
                }
                if (panel == Panel::help) continue;
                if (panel == Panel::plans) {
                    if (key == SDLK_LEFT || key == SDLK_A || key == SDLK_J) --plan_index;
                    if (key == SDLK_RIGHT || key == SDLK_D || key == SDLK_K) ++plan_index;
                    continue;
                }
                if (panel == Panel::inventory) {
                    auto items = slots(player);
                    if (key == SDLK_H) { hands.equip(player, {}); panel = Panel::none; continue; }
                    if (items.empty()) continue;
                    int change = 0;
                    if (key == SDLK_A || key == SDLK_LEFT) change = -1;
                    if (key == SDLK_D || key == SDLK_RIGHT) change = 1;
                    if (key == SDLK_W || key == SDLK_UP) change = -6;
                    if (key == SDLK_S || key == SDLK_DOWN) change = 6;
                    inventory_index = std::clamp(inventory_index + change, 0, int(items.size()) - 1);
                    auto id = items[inventory_index];
                    if (key == SDLK_J) {
                        message = hands.mark(player, id).message;
                        // Unreadying a depleted entry removes that slot.
                        inventory_index = std::max(0, std::min(inventory_index, int(slots(player).size()) - 1));
                    }
                    if (key == SDLK_K && player.inventory.at(id) > 0) {
                        hands.equip(player, id);
                        panel = Panel::none;
                    }
                    continue;
                }
                if (!focused) continue;
                if (key == SDLK_A || key == SDLK_D) {
                    if (key == SDLK_A) left = true;
                    else right = true;
                    if (!hands.working) player.facing = key == SDLK_A ? -1 : 1;
                }
                if (key == SDLK_W) { player.facing = 0; hands.working = false; hands.pending.reset(); }
                if (key == SDLK_S) { down = true; crouch(world, player, true); }
                if (key == SDLK_SPACE) jump = true;
                if (key == SDLK_SEMICOLON) sprint = true;
                if (key == SDLK_H) { mining = false; hands.hand(player); }
                if (key == SDLK_J || key == SDLK_K) {
                    if (hands.chooser) {
                        mining = false;
                        hands.cycle(player, key == SDLK_J ? -1 : 1);
                    } else if (key == SDLK_J) {
                        mining = true;
                        message = hands.primary(world, player).message;
                        next_action = SDL_GetTicksNS() / 1e9 + 0.16;
                    } else message = hands.interact(world, player).message;
                }
                if (key == SDLK_L) message = hands.place(world, player, jump).message;
            }
        }
        double now = SDL_GetTicksNS() / 1e9;
        accumulated += focused && panel == Panel::none ? std::min(0.1, now - previous) : 0;
        previous = now;
        while (accumulated >= tick_seconds) {
            move(world, player, int(right) - int(left), jump, down, sprint);
            jump = false;
            if (auto result = hands.step(world, player)) message = result->message;
            accumulated -= tick_seconds;
        }
        view.follow(player, world);
        if (focused && panel == Panel::none && mining && now >= next_action) {
            message = hands.primary(world, player).message;
            next_action = now + 0.16;
        }
        if (now - autosave >= 5) {
            world.checkpoint(player);
            autosave = now;
        }
        view.draw(world, player, hands, panel == Panel::help, message);
        if (panel == Panel::plans) view.plan(content, plan_index);
        if (panel == Panel::inventory) view.inventory(content, player, inventory_index);
        if (panel == Panel::pause) view.pause();
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
