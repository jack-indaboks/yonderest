#include "app.hpp"
#include "save.hpp"
#include "play.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <numbers>
#include <optional>
#include <stdexcept>

using namespace yonder;
namespace {

constexpr int screen_width = 640, screen_height = 360, tile = 16;

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
            std::cout << "Yonderest\n"
                         "  --save DIR        World folder, created if new (default: SDL user data/yonderest/world)\n"
                         "  --seed NUMBER     Seed for a NEW save only\n"
                         "  --content DIR     Content root (manifest and packs)\n"
                         "  --manifest FILE   Pack and terrain configuration\n"
                         "  --keep-generation Keep this save's existing terrain profile\n"
                         "  --pack DIR        Additional content pack (repeatable)\n"
                         "  --frames NUMBER   Exit after this many frames (render checks)\n"
                         "  --screenshot FILE Save the last frame as BMP\n"
                         "In game, Esc opens the menu and its Controls page.\n";
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

struct Rect {
    int x, y, w, h;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    SDL_FRect f() const { return {float(x), float(y), float(w), float(h)}; }
};

struct Color { Uint8 r, g, b, a = 255; };
constexpr Color ink{240, 230, 205}, muted{160, 168, 182}, gold{231, 167, 38},
    panel_fill{26, 22, 38, 250}, panel_edge{84, 74, 104}, outline{36, 27, 48}, focus{231, 167, 38};

// One layout per interface, shared by drawing and pointer hit-testing.
// The backpack and the closed book rest in the lower-right corner; the book opens where it lies.
const Rect backpack_button{screen_width - 24, screen_height - 28, 18, 22};
const Rect book_button{backpack_button.x - 24, backpack_button.y, 18, 22};
struct BookLayout { Rect book, left, right, previous, next; };
const BookLayout book_layout = [] {
    BookLayout l;
    l.book = {book_button.x + book_button.w - 220, book_button.y + book_button.h - 120, 220, 120};
    l.left = {l.book.x + 4, l.book.y + 4, l.book.w / 2 - 5, l.book.h - 10};
    l.right = {l.book.x + l.book.w / 2 + 1, l.left.y, l.left.w, l.left.h};
    l.previous = {l.left.x + 2, l.left.y + l.left.h - 13, 12, 11};
    l.next = {l.right.x + l.right.w - 14, l.previous.y, 12, 11};
    return l;
}();

enum class Dialog { none, inventory, menu, controls, pause };
constexpr int inventory_columns = 8, inventory_rows = 4, slot = 26;
// The inventory rises from the backpack; the book waits beneath it while it is open.
const Rect inventory_panel{screen_width - 6 - 232, backpack_button.y - 4 - 174, 232, 174};
Rect inventory_slot(int i) {
    return {inventory_panel.x + 12 + i % inventory_columns * slot, inventory_panel.y + 24 + i / inventory_columns * slot, 24, 24};
}
Rect inventory_action(int i) {
    static constexpr int widths[]{48, 56, 72};
    int x = inventory_panel.x + 12;
    for (int k = 0; k < i; ++k) x += widths[k] + 6;
    return {x, inventory_panel.y + inventory_panel.h - 26, widths[i], 16};
}
constexpr int menu_items = 4;
const Rect menu_panel{(screen_width - 150) / 2, 92, 150, 22 + menu_items * 18 + 8};
Rect menu_item(int i) { return {menu_panel.x + 8, menu_panel.y + 22 + i * 18, menu_panel.w - 16, 16}; }

// Animation follows distance travelled, so steps keep pace with the ground at any speed.
constexpr double walk_step = 1.5, crawl_step = 0.6; // Blocks per frame change.
constexpr double swing_seconds = 0.1;
const char* pose_for(const Player& p, double travelled) {
    if (p.down) return int(travelled / crawl_step) % 2 ? "crawl-2" : "crawl-1";
    if (p.facing == 0) return "back";
    if (!p.grounded) return p.vy < 0 ? "jump" : "fall";
    if (travelled == 0) return "idle";
    return int(travelled / walk_step) % 2 ? "idle" : "stride";
}

class View {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int camera_x = 0, camera_y = 0; // In art pixels, so the world never shimmers between pixels.

    View() {
        sdl(SDL_Init(SDL_INIT_VIDEO));
        sdl(SDL_CreateWindowAndRenderer("Yonderest", 1280, 720, SDL_WINDOW_RESIZABLE, &window, &renderer));
        sdl(SDL_SetRenderLogicalPresentation(renderer, screen_width, screen_height, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE));
        sdl(SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND));
        SDL_SetRenderVSync(renderer, 1);
    }
    ~View() {
        for (auto& [path, art] : images) { SDL_DestroyTexture(art.texture); SDL_DestroySurface(art.surface); }
        for (auto& [path, texture] : carried) SDL_DestroyTexture(texture);
        for (auto& [path, texture] : turns) SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void prepare(const Content& content) {
        auto named = [&](const char* name) {
            auto found = content.images.find(name);
            if (found == content.images.end()) throw std::runtime_error(std::string("Missing image: ") + name);
            return image(found->second);
        };
        for (const char* name : {"idle", "stride", "jump", "fall", "crawl-1", "crawl-2", "back"}) {
            auto found = content.poses.find(name);
            if (found == content.poses.end()) throw std::runtime_error(std::string("Missing pose: ") + name);
            const auto& pose = found->second;
            auto art = [&](const std::string& image) {
                auto path = content.images.find(image);
                if (path == content.images.end()) throw std::runtime_error("Missing image: " + image);
                return canvas(path->second);
            };
            poses[name] = {&pose, art(pose.image), pose.bare.empty() ? art(pose.image) : art(pose.bare)};
        }
        grip = canvas(content.images.at("character-grip"));
        recipes_icon = named("recipes"); backpack_icon = named("backpack"); hand_icon = named("hand");
        unknown = named("unknown");
        font = named("font");
        auto* glyphs = images.at(content.images.at("font")).surface;
        for (int code = 33; code < 127; ++code) {
            int ox = (code - 32) % 16 * 6, oy = (code - 32) / 16 * 9, last = 0;
            for (int x = 0; x < 6; ++x)
                for (int y = 0; y < 9; ++y) {
                    Uint8 r, g, b, a;
                    SDL_ReadSurfacePixel(glyphs, ox + x, oy + y, &r, &g, &b, &a);
                    if (a) last = x + 1;
                }
            advance[code] = last + 1;
        }
        advance[' '] = 3;
    }

    // Primitives.
    void fill(const Rect& r, Color c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        auto f = r.f();
        SDL_RenderFillRect(renderer, &f);
    }
    void frame(const Rect& r, Color c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        auto f = r.f();
        SDL_RenderRect(renderer, &f);
    }
    void panel(const Rect& r) {
        fill({r.x + 1, r.y, r.w - 2, r.h}, panel_fill);
        fill({r.x, r.y + 1, 1, r.h - 2}, panel_fill);
        fill({r.x + r.w - 1, r.y + 1, 1, r.h - 2}, panel_fill);
        fill({r.x + 1, r.y, r.w - 2, 1}, panel_edge);
        fill({r.x + 1, r.y + r.h - 1, r.w - 2, 1}, panel_edge);
        fill({r.x, r.y + 1, 1, r.h - 2}, panel_edge);
        fill({r.x + r.w - 1, r.y + 1, 1, r.h - 2}, panel_edge);
    }
    int measure(const std::string& s) {
        int w = 0;
        for (unsigned char c : s) w += c < 127 && c >= 32 ? advance[c] : 0;
        return std::max(0, w - 1);
    }
    int text(int x, int y, const std::string& s, Color c = ink, bool shadow = false) {
        if (shadow) text(x + 1, y + 1, s, outline);
        SDL_SetTextureColorMod(font, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(font, c.a);
        int start = x;
        for (unsigned char ch : s) {
            if (ch < 32 || ch >= 127) continue;
            SDL_FRect src{float((ch - 32) % 16 * 6), float((ch - 32) / 16 * 9), 6, 9};
            SDL_FRect dst{float(x), float(y), 6, 9};
            if (ch != ' ') SDL_RenderTexture(renderer, font, &src, &dst);
            x += advance[ch];
        }
        return x - start;
    }
    void centered(const Rect& r, const std::string& s, Color c = ink) {
        text(r.x + (r.w - measure(s)) / 2, r.y + (r.h - 7) / 2, s, c);
    }
    void draw(SDL_Texture* texture, const Rect& at, SDL_FlipMode flip = SDL_FLIP_NONE, Color tint = {255, 255, 255}) {
        SDL_SetTextureColorMod(texture, tint.r, tint.g, tint.b);
        SDL_SetTextureAlphaMod(texture, tint.a);
        auto dst = at.f();
        SDL_RenderTextureRotated(renderer, texture, nullptr, &dst, 0, nullptr, flip);
    }
    void block(const Material& m, int x, int y, SDL_FlipMode flip = SDL_FLIP_NONE, Color tint = {255, 255, 255}) {
        if (!m.texture.empty()) return draw(image(m.texture), {x, y, tile, tile}, flip, tint);
        if (m.missing) return draw(unknown, {x, y, tile, tile}, flip, tint);
        // A pack without art still shows its declared colour.
        auto [r, g, b] = m.color;
        fill({x, y, tile, tile}, {Uint8(r * tint.r / 255), Uint8(g * tint.g / 255), Uint8(b * tint.b / 255), tint.a});
    }

    void follow(const Player& p, World& world) {
        camera_x = int(std::lround((p.x + player_width / 2) * tile)) - screen_width / 2;
        int y = int(std::lround((p.y + player_height) * tile)) - screen_height * 3 / 5;
        camera_y = std::clamp(y, -2 * tile, world.settings.height() * tile - screen_height);
        // A small view margin, not full-height columns.
        int left = int(std::floor(camera_x / double(tile))), top = int(std::floor(camera_y / double(tile)));
        world.retain(left - 6, top - 1, left + screen_width / tile + 6, top + screen_height / tile + 2);
    }

    void scene(World& world, const Player& p, const Hands& hands, const char* pose, bool swinging) {
        // Open sky above ground; depth darkens the backdrop rather than inventing scenery.
        double daylight = 1 - std::clamp((p.y - surface(world.settings, int(p.x))) / 12.0, 0.0, 1.0);
        for (int y = 0; y < screen_height; y += 4)
            fill({0, y, screen_width, 4}, {Uint8((96 + y / 6) * daylight + 10), Uint8((150 + y / 6) * daylight + 10), Uint8((196 + y / 10) * daylight + 18)});
        if (daylight > 0.8) {
            fill({476, 50, 20, 16}, {255, 236, 178}); fill({478, 48, 16, 20}, {255, 236, 178});
        }
        for (int layer = 0; layer < 3; ++layer) {
            Color c{Uint8((92 - layer * 14) * daylight + 8), Uint8((140 - layer * 16) * daylight + 10), Uint8((150 - layer * 14) * daylight + 18)};
            for (int sx = 0; sx < screen_width; sx += 4) {
                double angle = (camera_x / double(tile) + sx / double(tile)) * (2 * std::numbers::pi / world.settings.width());
                int ridge = int(150 + layer * 30 + 20 * std::sin(angle * (5 + layer * 2) + layer)
                                + 9 * std::sin(angle * 17) - camera_y * 0.05);
                fill({sx, ridge, 4, screen_height - ridge}, c);
            }
        }
        int left = int(std::floor(camera_x / double(tile))), top = int(std::floor(camera_y / double(tile)));
        for (auto layer : {Layer::background, Layer::foreground})
            for (int x = left; x <= left + screen_width / tile + 1; ++x)
                for (int y = top; y <= top + screen_height / tile + 1; ++y) {
                    const auto& cell = world.cell(x, y, layer);
                    if (cell.id.empty()) continue;
                    const auto& m = world.content.material(cell.id);
                    int sx = x * tile - camera_x, sy = y * tile - camera_y;
                    // Mirror unoriented natural blocks by position so repeats read less like a grid.
                    bool oriented = !m.station.empty() || !m.tool.empty();
                    auto flip = !oriented && (wrap(x, world.settings.width()) * 7 + y * 13) % 3 == 0 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
                    block(m, sx, sy, flip, layer == Layer::background ? Color{132, 128, 150} : Color{255, 255, 255});
                    if (cell.quantity > 1) {
                        auto label = std::to_string(cell.quantity);
                        text(sx + tile - measure(label), sy + tile - 8, label, gold, true);
                    }
                }
        character(world.content, p, pose, swinging);
        if (hands.chooser) chooser(world.content, p, hands);
    }

    // Body, then the held object, then the grip closing over it, all on the pose's 16x16 canvas
    // and mirrored as one. Held art is drawn as the idle pose holds it, moved to this pose's hand.
    // An empty hand rests at the side.
    void character(const Content& content, const Player& p, const char* name, bool swinging) {
        const auto& [pose, holding, bare] = poses.at(name);
        const auto& idle = content.poses.at("idle");
        bool flip = p.facing < 0;
        int cx = center_x(p), top = int(std::lround((p.y + player_height) * tile)) - camera_y - tile;
        int left = flip ? cx + pose->anchor - tile : cx - pose->anchor;
        // Art whose column 0 lies at pose column ox.
        auto place = [&](SDL_Texture* art, int size, int ox, int oy) {
            draw(art, {flip ? left + tile - size - ox : left + ox, top + oy, size, size}, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        };
        int dx = pose->hand_x - idle.hand_x, dy = pose->hand_y - idle.hand_y;
        if (p.selected.empty()) return place(bare, tile, 0, 0);
        place(holding, tile, 0, 0);
        const auto& m = content.material(p.selected);
        if (m.held.empty()) place(carry(p.selected, m), 8, pose->block_x, pose->block_y);
        else if (swinging) place(turned(m, idle), 2 * tile, dx - tile / 2, dy - tile / 2);
        else place(canvas(m.held), tile, dx, dy);
        place(grip, tile, dx, dy);
    }

    int center_x(const Player& p) const { return int(std::lround((p.x + player_width / 2) * tile)) - camera_x; }

    void chooser(const Content& content, const Player& p, const Hands& hands) {
        int px = center_x(p), py = int(std::lround((p.y + player_height) * tile)) - camera_y - 8;
        const auto& ready = p.ready;
        if (ready.empty()) {
            // An empty wheel, not a text error.
            Rect box{px - 10, py - 40, 20, 20};
            panel(box);
            SDL_SetRenderDrawColor(renderer, muted.r, muted.g, muted.b, 255);
            SDL_RenderLine(renderer, box.x + 4, box.y + box.h - 5, box.x + box.w - 5, box.y + 4);
            return;
        }
        int selected = int(std::ranges::find(ready, hands.choice) - ready.begin());
        double radius = ready.size() > 6 ? 40 : 32;
        for (int i = 0; i < int(ready.size()); ++i) {
            double angle = 2 * std::numbers::pi * (i - selected) / ready.size() - std::numbers::pi / 2;
            int x = px + int(std::lround(radius * std::cos(angle))) - 10;
            int y = py + int(std::lround(radius * std::sin(angle))) - 10;
            Rect box{x, y, 20, 20};
            panel(box);
            if (i == selected) frame(box, focus);
            block(content.material(ready[i]), x + 2, y + 2);
            auto count = std::to_string(p.inventory.at(ready[i]));
            text(x + 20 - measure(count), y + 13, count, ink, true);
        }
    }

    // Free-standing objects; one lifts a pixel when pointed at or open.
    void hud(bool book_open, bool bag_open, float mx, float my) {
        auto object = [&](SDL_Texture* art, const Rect& r, bool lifted) { draw(art, {r.x, r.y - lifted, r.w, r.h}); };
        if (!book_open) object(recipes_icon, book_button, book_button.contains(mx, my));
        object(backpack_icon, backpack_button, bag_open || backpack_button.contains(mx, my));
    }

    // The recipe book lies open in the corner: the shape on the left page, what it makes and
    // needs on the right. It never crafts anything itself.
    void book(const Content& content, const Player& p, const Recipe& r, int index, int count, float mx, float my) {
        constexpr Color cover{155, 58, 69}, spine{110, 39, 55}, paper{239, 226, 194}, edge{203, 185, 148},
            ink_on_paper{64, 42, 40}, faint{140, 116, 92}, short_on_paper{178, 46, 40};
        const auto& l = book_layout;
        fill({l.book.x + 1, l.book.y, l.book.w - 2, l.book.h}, outline);
        fill({l.book.x, l.book.y + 1, l.book.w, l.book.h - 2}, outline);
        fill({l.book.x + 1, l.book.y + 1, l.book.w - 2, l.book.h - 2}, cover);
        for (const auto& page : {l.left, l.right}) {
            fill({page.x, page.y + 2, page.w, page.h}, edge);   // the page block beneath
            fill(page, paper);
        }
        fill({l.book.x + l.book.w / 2 - 1, l.book.y + 2, 2, l.book.h - 4}, spine);
        for (auto [button, glyph] : {std::pair{l.previous, "<"}, std::pair{l.next, ">"}}) {
            if (button.contains(mx, my)) fill(button, edge);
            centered(button, glyph, ink_on_paper);
        }

        auto title = content.material(r.output).name;
        text(l.left.x + (l.left.w - measure(title)) / 2, l.left.y + 4, title, ink_on_paper);
        int span = std::max(r.width * 17, int(r.stations.size()) * 17 + 2 + measure("beneath")) - 1;
        int gx = l.left.x + (l.left.w - span) / 2, gy = l.left.y + 18;
        std::map<Block, int> needed;
        for (const auto& i : r.shape) {
            int x = gx + i.x * 17, y = gy + i.y * 17;
            if (i.id.empty()) {
                // A cell that must stay empty: a dotted outline.
                SDL_SetRenderDrawColor(renderer, faint.r, faint.g, faint.b, 255);
                for (int k = 0; k < 16; k += 3) {
                    SDL_RenderPoint(renderer, x + k, y); SDL_RenderPoint(renderer, x + k, y + 15);
                    SDL_RenderPoint(renderer, x, y + k); SDL_RenderPoint(renderer, x + 15, y + k);
                }
            } else {
                block(content.material(i.id), x, y);
                ++needed[i.id];
            }
        }
        if (!r.stations.empty()) {
            // Required working surfaces sit directly beneath the shape.
            int x = gx, y = gy + r.height * 17;
            for (const auto& station : r.stations) {
                for (const auto& [id, m] : content.blocks)
                    if (m.station == station) { block(m, x, y); break; }
                x += 17;
            }
            text(x + 2, y + 4, "beneath", faint);
        }
        auto page = std::to_string(index + 1) + "/" + std::to_string(count);
        text(l.left.x + (l.left.w - measure(page)) / 2, l.previous.y + 2, page, faint);

        int x = l.right.x + 8, y = l.right.y + 4;
        text(x, y, "Makes", faint);
        block(content.material(r.output), x, y + 12);
        text(x + 20, y + 16, "x" + std::to_string(r.amount), ink_on_paper);
        text(x, y + 36, "Needs", faint);
        int column = 0;
        for (const auto& [id, n] : needed) {
            int ix = x + column % 2 * 46, iy = y + 48 + column / 2 * 18;
            block(content.material(id), ix, iy);
            auto have = p.inventory.find(id);
            bool enough = have != p.inventory.end() && have->second >= n;
            text(ix + 18, iy + 4, "x" + std::to_string(n), enough ? ink_on_paper : short_on_paper);
            ++column;
        }
    }

    void inventory(const Content& content, const Player& p, int index, int action, float mx, float my) {
        panel(inventory_panel);
        text(inventory_panel.x + 12, inventory_panel.y + 8, "Inventory");
        auto items = slots(p);
        auto ready_text = "Ready " + std::to_string(p.ready.size()) + "/12";
        text(inventory_panel.x + inventory_panel.w - 12 - measure(ready_text), inventory_panel.y + 8, ready_text, gold);
        int page = inventory_columns * inventory_rows, first = index / page * page;
        for (int k = 0; k < page; ++k) {
            auto r = inventory_slot(k);
            fill(r, {44, 38, 60});
            int i = first + k;
            if (i >= int(items.size())) continue;
            const auto& id = items[i];
            block(content.material(id), r.x + 4, r.y + 3);
            auto label = std::to_string(p.inventory.at(id));
            text(r.x + r.w - 1 - measure(label), r.y + r.h - 9, label, ink, true);
            if (std::ranges::find(p.ready, id) != p.ready.end()) fill({r.x + 1, r.y + 1, 4, 4}, gold);
            if (id == p.selected) frame({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, ink);
            if (i == index && action < 0) frame(r, focus);
            else if (r.contains(mx, my)) frame(r, panel_edge);
        }
        int y = inventory_slot(page - 1).y + 28;
        if (items.empty()) {
            draw(hand_icon, {inventory_panel.x + inventory_panel.w / 2 - 8, inventory_slot(0).y + 40, 16, 16});
            return;
        }
        const auto& id = items[std::min(index, int(items.size()) - 1)];
        text(inventory_panel.x + 12, y, content.material(id).name);
        bool ready = std::ranges::find(p.ready, id) != p.ready.end();
        const char* labels[]{"Hold  K", ready ? "Unready J" : "Ready  J", "Empty hand  H"};
        for (int i = 0; i < 3; ++i) {
            auto r = inventory_action(i);
            fill(r, r.contains(mx, my) ? panel_edge : Color{44, 38, 60});
            if (i == action) frame(r, focus);
            centered(r, labels[i]);
        }
    }

    void menu(int index, bool fullscreen, float mx, float my) {
        panel(menu_panel);
        centered({menu_panel.x, menu_panel.y + 4, menu_panel.w, 14}, "Paused");
        const char* items[]{"Resume", "Controls", fullscreen ? "Windowed" : "Fullscreen", "Quit"};
        for (int i = 0; i < menu_items; ++i) {
            auto r = menu_item(i);
            if (i == index) fill(r, {60, 52, 82});
            else if (r.contains(mx, my)) fill(r, {44, 38, 60});
            centered(r, items[i], i == index ? gold : ink);
        }
    }

    void controls() {
        static const std::pair<const char*, const char*> lines[]{
            {"A  D", "walk"}, {"Space", "jump"}, {";", "sprint while held"},
            {"W", "face the rear wall; a held A or D takes over"}, {"S", "crawl, aim beneath your feet"},
            {"J", "hit or gather (hold to repeat)"}, {"K", "use: a workbench cycles batch 1, 5, 10"},
            {"L", "place; S + Space + L builds underfoot"}, {"H", "hand: empty, choose (J/K), equip"},
            {"I", "backpack: ready objects for H"}, {"R", "recipe book; Left/Right turn pages"}, {"P", "pause"}, {"Esc", "menu"}};
        Rect r{(screen_width - 300) / 2, 46, 300, 22 + int(std::size(lines)) * 12 + 30};
        panel(r);
        centered({r.x, r.y + 5, r.w, 12}, "Controls");
        int y = r.y + 22;
        for (auto [key, what] : lines) {
            text(r.x + 14, y, key, gold);
            text(r.x + 64, y, what);
            y += 12;
        }
        text(r.x + 14, y + 6, "Recipes complete themselves when the shape is built.", muted);
    }

    void banner(const std::string& s) {
        int w = measure(s) + 24;
        Rect r{(screen_width - w) / 2, 140, w, 22};
        panel(r);
        centered(r, s);
    }

    void toast(const std::string& s) {
        int w = measure(s) + 16;
        Rect r{(screen_width - w) / 2, screen_height - 30, w, 17};
        panel(r);
        centered(r, s);
    }

    void screenshot(const std::filesystem::path& path) {
        SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
        if (!pixels) throw std::runtime_error(SDL_GetError());
        bool result = SDL_SaveBMP(pixels, path.c_str());
        SDL_DestroySurface(pixels);
        sdl(result);
    }

private:
    struct Image { SDL_Surface* surface; SDL_Texture* texture; };
    std::map<std::filesystem::path, Image> images;
    std::map<Block, SDL_Texture*> carried;
    std::map<std::filesystem::path, SDL_Texture*> turns;
    struct PoseArt { const Pose* pose; SDL_Texture *holding, *bare; };
    std::map<std::string, PoseArt> poses;
    SDL_Texture *grip = nullptr, *font = nullptr, *unknown = nullptr;
    SDL_Texture *recipes_icon = nullptr, *backpack_icon = nullptr, *hand_icon = nullptr;
    std::array<int, 128> advance{};

    SDL_Texture* image(const std::filesystem::path& path) {
        auto found = images.find(path);
        if (found != images.end()) return found->second.texture;
        SDL_Surface* surface = SDL_LoadPNG(path.c_str());
        if (!surface) throw std::runtime_error(path.string() + ": " + SDL_GetError());
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        images.emplace(path, Image{surface, texture});
        return texture;
    }

    // Character art: a 16x16 canvas.
    SDL_Texture* canvas(const std::filesystem::path& path) {
        auto* texture = image(path);
        const auto* surface = images.at(path).surface;
        if (surface->w != tile || surface->h != tile) throw std::runtime_error(path.string() + ": character art must be 16x16");
        return texture;
    }

    // A swing: the held art turned a quarter clockwise about the knuckle, one pixel up and
    // forward of the idle hand. Pixel-exact, so any tool swings without art of its own.
    // The result is 32x32 with the held canvas's origin at (8, 8).
    SDL_Texture* turned(const Material& m, const Pose& idle) {
        auto found = turns.find(m.held);
        if (found != turns.end()) return found->second;
        canvas(m.held);
        SDL_Surface* source = images.at(m.held).surface;
        SDL_Surface* out = SDL_CreateSurface(2 * tile, 2 * tile, SDL_PIXELFORMAT_RGBA32);
        if (!out) throw std::runtime_error(SDL_GetError());
        SDL_ClearSurface(out, 0, 0, 0, 0);
        int kx = idle.hand_x + 1, ky = idle.hand_y - 1, o = tile / 2;
        for (int y = 0; y < tile; ++y)
            for (int x = 0; x < tile; ++x) {
                Uint8 r, g, b, a;
                SDL_ReadSurfacePixel(source, x, y, &r, &g, &b, &a);
                if (a) SDL_WriteSurfacePixel(out, o + kx - (y - ky), o + ky + (x - kx), r, g, b, a);
            }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, out);
        SDL_DestroySurface(out);
        if (!texture) throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        return turns.emplace(m.held, texture).first->second;
    }

    // A carried block: one fixed 8x8 icon, a 6x6 sampling of the tile inside the character's outline colour.
    // Each block ID has its own, whatever its display name.
    SDL_Texture* carry(const Block& id, const Material& m) {
        constexpr int size = 8, inner = size - 2;
        auto found = carried.find(id);
        if (found != carried.end()) return found->second;
        SDL_Surface* small = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA32);
        if (!small) throw std::runtime_error(SDL_GetError());
        SDL_Surface* source = m.texture.empty() ? nullptr : (image(m.texture), images.at(m.texture).surface);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                bool edge = x == 0 || y == 0 || x == size - 1 || y == size - 1;
                bool corner = (x == 0 || x == size - 1) && (y == 0 || y == size - 1);
                Uint8 r = outline.r, g = outline.g, b = outline.b, a = corner ? 0 : 255;
                if (!edge) {
                    if (source) {
                        int sx = (x - 1) * tile / inner + 1, sy = (y - 1) * tile / inner + 1;
                        SDL_ReadSurfacePixel(source, sx, sy, &r, &g, &b, &a);
                        if (!a) { r = outline.r; g = outline.g; b = outline.b; a = 255; }
                    } else { r = Uint8(m.color[0]); g = Uint8(m.color[1]); b = Uint8(m.color[2]); }
                }
                SDL_WriteSurfacePixel(small, x, y, r, g, b, a);
            }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, small);
        SDL_DestroySurface(small);
        if (!texture) throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        return carried.emplace(id, texture).first->second;
    }
};

int run_game(Options o) {
    View view;
    if (o.content.empty()) o.content = std::filesystem::path(SDL_GetBasePath()) / "content";
    if (o.save.empty()) {
        char* path = SDL_GetPrefPath(nullptr, "yonderest");
        if (!path) throw std::runtime_error(SDL_GetError());
        o.save = std::filesystem::path(path) / "world";
        SDL_free(path);
    }
    Content content;
    auto manifest = load_manifest(o.manifest.empty() ? o.content / "flagship.json" : o.manifest);
    for (const auto& [id, version] : manifest.packs) {
        content.load(o.content / id);
        auto found = content.modules.find(id);
        if (found == content.modules.end() || found->second.version != version) throw std::runtime_error("Manifest version mismatch for " + id);
    }
    for (const auto& pack : o.packs) content.load(pack);
    content.validate();
    view.prepare(content);
    std::vector<const Recipe*> book;
    for (const auto& r : content.recipes) book.push_back(&r);
    std::ranges::sort(book, {}, &Recipe::order);

    auto terrain = load_terrain(o.content / manifest.terrain);
    Save save(o.save);
    auto saved_settings = save.settings();
    Settings settings;
    if (saved_settings) {
        settings = *saved_settings;
        if (!o.keep_generation && profile_rank(terrain.version) > profile_rank(settings.terrain.version)) {
            settings.terrain.version = terrain.version;
            settings.terrain.clay = terrain.clay;
            settings.terrain.ore = terrain.ore;
            std::cout << "New territory: " << terrain.version << "; saved chunks stay unchanged.\n";
            save.write_settings(settings);
        }
        if (o.seed && *o.seed != settings.seed) std::cout << "Existing save: using its seed, not --seed.\n";
    } else {
        settings.seed = o.seed.value_or(std::uint64_t(std::chrono::system_clock::now().time_since_epoch().count()));
        settings.terrain = terrain;
        if (o.keep_generation) settings.terrain.version = "shelter-v1";
        save.write_settings(settings);
    }
    World world(save, content, settings);
    auto saved_player = save.player();
    Player player = saved_player ? *saved_player : spawn(world);
    std::cout << "World: " << o.save << "\nSeed: " << settings.seed << '\n';

    Hands hands;
    int side = player.facing ? player.facing : 1; // The side last faced, for S from the rear wall.
    bool quit = false, focused = true, recipes_open = false;
    Dialog dialog = Dialog::none;
    bool left = false, right = false, sprint = false, mining = false, jump = false;
    int recipe_index = 0, inventory_index = 0, inventory_focus = -1, menu_index = 0;
    float mouse_x = -1, mouse_y = -1;
    double previous = SDL_GetTicksNS() / 1e9, accumulated = 0, last_save = previous, next_action = 0;
    double travelled = 0, swing_until = 0;
    std::string toast;
    double toast_until = 0;
    auto notify = [&](const std::string& s, double seconds = 2.0) {
        toast = s;
        toast_until = SDL_GetTicksNS() / 1e9 + seconds;
    };
    auto report = [&](const ActionResult& r) { if (!r.quiet) notify(r.message); };
    auto swing = [&](double now) {
        report(hands.primary(world, player));
        next_action = now + 0.16;
        swing_until = now + swing_seconds;
    };
    if (!saved_settings) notify("Esc: menu and controls", 6);
    int frames = 0;
    auto release = [&] {
        left = right = sprint = mining = jump = player.down = false;
        hands.cancel();
        accumulated = 0;
    };
    auto open = [&](Dialog next) {
        dialog = next;
        release();
        if (next == Dialog::menu) menu_index = 0;
        if (next == Dialog::inventory) { inventory_index = 0; inventory_focus = -1; }
    };
    auto fullscreen = [&] { return (SDL_GetWindowFlags(view.window) & SDL_WINDOW_FULLSCREEN) != 0; };
    auto choose = [&](int item) {
        switch (item) {
        case 0: dialog = Dialog::none; break;
        case 1: dialog = Dialog::controls; break;
        case 2: sdl(SDL_SetWindowFullscreen(view.window, !fullscreen())); break;
        case 3: quit = true; break; // Quitting always saves.
        }
    };
    auto inventory_act = [&](int action) {
        auto items = slots(player);
        if (action == 2) { hands.equip(player, {}); dialog = Dialog::none; return; }
        if (items.empty()) return;
        const auto& id = items[inventory_index];
        if (action == 0) { hands.equip(player, id); dialog = Dialog::none; }
        else report(hands.mark(player, id));
    };
    auto browse = [&](int change) {
        if (!book.empty()) recipe_index = wrap(recipe_index + change, int(book.size()));
    };

    view.follow(player, world);
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            SDL_ConvertEventToRenderCoordinates(view.renderer, &event);
            if (event.type == SDL_EVENT_QUIT) quit = true;
            else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                focused = false;
                release();
                world.save_changes(player);
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) focused = true;
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                mouse_x = event.motion.x; mouse_y = event.motion.y;
                if (dialog == Dialog::menu)
                    for (int i = 0; i < menu_items; ++i) if (menu_item(i).contains(mouse_x, mouse_y)) menu_index = i;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                // The pointer operates interface controls only, never the world.
                float x = event.button.x, y = event.button.y;
                if (dialog == Dialog::menu) {
                    for (int i = 0; i < menu_items; ++i) if (menu_item(i).contains(x, y)) choose(i);
                } else if (dialog == Dialog::controls) dialog = Dialog::menu;
                else if (dialog == Dialog::pause) dialog = Dialog::none;
                else if (dialog == Dialog::inventory) {
                    auto items = slots(player);
                    int first = inventory_index / (inventory_columns * inventory_rows) * (inventory_columns * inventory_rows);
                    for (int k = 0; k < inventory_columns * inventory_rows; ++k)
                        if (inventory_slot(k).contains(x, y) && first + k < int(items.size())) {
                            inventory_index = first + k;
                            inventory_focus = -1;
                            if (event.button.clicks == 2) inventory_act(0);
                        }
                    for (int i = 0; i < 3; ++i)
                        if (inventory_action(i).contains(x, y) && !items.empty()) inventory_act(i);
                    if (backpack_button.contains(x, y)) dialog = Dialog::none;
                } else if (backpack_button.contains(x, y)) open(Dialog::inventory);
                else if (recipes_open && !book.empty() && book_layout.book.contains(x, y)) {
                    // Turn a page at its corner; anywhere else on the book closes it.
                    if (book_layout.previous.contains(x, y)) browse(-1);
                    else if (book_layout.next.contains(x, y)) browse(1);
                    else recipes_open = false;
                } else if (!recipes_open && book_button.contains(x, y)) recipes_open = true;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL && recipes_open && dialog == Dialog::none && !book.empty()) {
                if (book_layout.book.contains(event.wheel.mouse_x, event.wheel.mouse_y)) browse(event.wheel.y > 0 ? -1 : 1);
            } else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                bool pressed = event.type == SDL_EVENT_KEY_DOWN;
                auto key = event.key.key;
                if (!pressed) {
                    if (key == SDLK_A) left = false;
                    if (key == SDLK_D) right = false;
                    if (key == SDLK_S) player.down = false;
                    // Releasing W only matters if A or D is still held: that key takes over.
                    if (key == SDLK_W && left != right) player.facing = right ? 1 : -1;
                    if (key == SDLK_SEMICOLON) sprint = false;
                    if (key == SDLK_J) mining = false;
                    continue;
                }
                bool repeat = event.key.repeat;
                // Escape closes the innermost thing that is actually open, then offers the menu.
                if (key == SDLK_ESCAPE) {
                    if (repeat) continue;
                    if (hands.chooser) { hands.cancel(); mining = false; }
                    else if (dialog == Dialog::controls) dialog = Dialog::menu;
                    else if (dialog != Dialog::none) dialog = Dialog::none;
                    else open(Dialog::menu);
                    continue;
                }
                if (key == SDLK_R && !repeat) {
                    if (dialog == Dialog::inventory) { dialog = Dialog::none; recipes_open = true; }
                    else recipes_open = !recipes_open;
                    continue;
                }
                if (dialog == Dialog::menu) {
                    if (key == SDLK_UP || key == SDLK_W) menu_index = wrap(menu_index - 1, menu_items);
                    if (key == SDLK_DOWN || key == SDLK_S) menu_index = wrap(menu_index + 1, menu_items);
                    if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && !repeat) choose(menu_index);
                    continue;
                }
                if (dialog == Dialog::controls) {
                    if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && !repeat) dialog = Dialog::menu;
                    continue;
                }
                if (dialog == Dialog::pause) {
                    if ((key == SDLK_P || key == SDLK_RETURN || key == SDLK_KP_ENTER) && !repeat) dialog = Dialog::none;
                    continue;
                }
                if (dialog == Dialog::inventory) {
                    if (repeat && !(key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN)) continue;
                    if (key == SDLK_I) { dialog = Dialog::none; continue; }
                    auto items = slots(player);
                    if (key == SDLK_H) { inventory_act(2); continue; }
                    if (items.empty()) continue;
                    int dx = (key == SDLK_RIGHT || key == SDLK_D) - (key == SDLK_LEFT || key == SDLK_A);
                    int dy = (key == SDLK_DOWN || key == SDLK_S) - (key == SDLK_UP || key == SDLK_W);
                    if (inventory_focus < 0) {
                        int next = inventory_index + dx + dy * inventory_columns;
                        // Down from the last row reaches the action buttons.
                        if (dy > 0 && next >= int(items.size())) inventory_focus = 0;
                        else inventory_index = std::clamp(next, 0, int(items.size()) - 1);
                    } else {
                        if (dy < 0) inventory_focus = -1;
                        else inventory_focus = std::clamp(inventory_focus + dx, 0, 2);
                    }
                    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) inventory_act(inventory_focus < 0 ? 0 : inventory_focus);
                    if (key == SDLK_J) inventory_act(1);
                    if (key == SDLK_K) inventory_act(0);
                    continue;
                }
                if (repeat) continue;
                if (key == SDLK_I) { open(Dialog::inventory); continue; }
                if (key == SDLK_P) { open(Dialog::pause); continue; }
                if (key == SDLK_LEFT || key == SDLK_RIGHT) {
                    if (recipes_open) browse(key == SDLK_LEFT ? -1 : 1);
                    continue;
                }
                if (!focused) continue;
                if (key == SDLK_A || key == SDLK_D) {
                    if (key == SDLK_A) left = true;
                    else right = true;
                    player.facing = key == SDLK_A ? -1 : 1;
                }
                // Facing the rear wall and crawling exclude each other; the last key pressed wins.
                if (key == SDLK_W) {
                    if (player.facing) side = player.facing;
                    player.facing = 0;
                    player.down = false;
                    hands.pending.reset();
                }
                if (key == SDLK_S) {
                    if (player.facing == 0) player.facing = side;
                    player.down = true;
                }
                if (key == SDLK_SPACE) jump = true;
                if (key == SDLK_SEMICOLON) sprint = true;
                if (key == SDLK_H) { mining = false; hands.hand(player); }
                if (key == SDLK_J || key == SDLK_K) {
                    if (hands.chooser) {
                        mining = false;
                        hands.cycle(player, key == SDLK_J ? -1 : 1);
                    } else if (key == SDLK_J) {
                        mining = true;
                        swing(SDL_GetTicksNS() / 1e9);
                    } else report(hands.interact(world, player));
                }
                if (key == SDLK_L) report(hands.place(world, player, jump));
            }
        }
        double now = SDL_GetTicksNS() / 1e9;
        bool playing = focused && dialog == Dialog::none;
        accumulated += playing ? std::min(0.1, now - previous) : 0;
        previous = now;
        while (accumulated >= tick_seconds) {
            double before = player.x;
            move(world, player, int(right) - int(left), jump, sprint);
            // A wrap around the world is not a step.
            double moved = std::abs(player.x - before);
            travelled = moved > 0 && moved < 1 ? travelled + moved : 0;
            jump = false;
            if (auto result = hands.step(world, player)) report(*result);
            accumulated -= tick_seconds;
        }
        view.follow(player, world);
        if (playing && mining && now >= next_action) swing(now);
        // Once a minute, and only if something changed. Taking and placing only change memory.
        if (now - last_save >= 60) {
            if (world.save_changes(player)) notify("Saved");
            last_save = now;
        }
        view.scene(world, player, hands, pose_for(player, travelled), now < swing_until);
        bool book_shown = recipes_open && !book.empty() && dialog != Dialog::inventory;
        if (book_shown)
            view.book(content, player, *book[recipe_index], recipe_index, int(book.size()), mouse_x, mouse_y);
        view.hud(book_shown, dialog == Dialog::inventory, mouse_x, mouse_y);
        if (dialog == Dialog::inventory) view.inventory(content, player, inventory_index, inventory_focus, mouse_x, mouse_y);
        if (dialog == Dialog::menu) view.menu(menu_index, fullscreen(), mouse_x, mouse_y);
        if (dialog == Dialog::controls) view.controls();
        if (dialog == Dialog::pause || (!focused && dialog == Dialog::none)) view.banner("Paused");
        if (now < toast_until) view.toast(toast);
        ++frames;
        if (o.frames && frames >= o.frames) quit = true;
        if (quit && !o.screenshot.empty()) view.screenshot(o.screenshot);
        sdl(SDL_RenderPresent(view.renderer));
        SDL_Delay(1);
    }
    world.save_changes(player);
    return 0;
}

}

int yonder::run(int argc, const char* const* argv) {
    return run_game(options(argc, argv));
}
