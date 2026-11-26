// The pixel display: the window and SDL's event queue, and everything drawn in it. Blocks are
// 16 pixels, the view a fixed 640 by 360 scaled by whole numbers. Its art is the display's pack:
// images found by name, later packs overriding earlier ones.

#include "yonderest/host.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace yonder::display {
namespace {

constexpr int screen_width = 640, screen_height = 360, tile = 16;

// A failed SDL call returns false or null and carries on; this makes it stop the game.
void sdl(bool result) {
    if (!result) throw std::runtime_error(SDL_GetError());
}
template <class T>
T* sdl(T* made) {
    if (!made) throw std::runtime_error(SDL_GetError());
    return made;
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

constexpr int slot = 26;
// The backpack's screen rises from it; the book waits beneath it while it is open.
const Rect inventory_panel{screen_width - 6 - 232, backpack_button.y - 4 - 174, 232, 174};
Rect inventory_slot(int i, int columns) {
    return {inventory_panel.x + 12 + i % columns * slot, inventory_panel.y + 24 + i / columns * slot, 24, 24};
}
Rect inventory_action(int i) {
    static constexpr int widths[]{48, 56, 72};
    int x = inventory_panel.x + 12;
    for (int k = 0; k < i; ++k) x += widths[k] + 6;
    return {x, inventory_panel.y + inventory_panel.h - 26, widths[i], 16};
}
Rect menu_panel(int items) { return {(screen_width - 150) / 2, 92, 150, 22 + items * 18 + 8}; }
Rect menu_item(int i) { return {menu_panel(0).x + 8, menu_panel(0).y + 22 + i * 18, 150 - 16, 16}; }

// Animation follows distance travelled, so steps keep pace with the ground at any speed.
constexpr double walk_step = 1.5, crawl_step = 0.6; // Blocks per frame change.
constexpr double swing_seconds = 0.1, craft_seconds = 0.4;

// A character pose: a 16x16 image, drawn so column `anchor` stands over the player's centre.
// Held art is drawn as the idle pose holds it, moved to this pose's hand.
struct Pose {
    std::string image, bare; // `bare`, if given, replaces `image` while the hand is empty.
    int anchor = 0, hand_x = 0, hand_y = 0;
    int block_x = 0, block_y = 0; // Top-left of a carried block; beside the hand unless given.
};

class Pixels : public Display {
public:
    explicit Pixels(Host& host) : host(host) {
        sdl(SDL_Init(SDL_INIT_VIDEO));
        // Shown once the land around the player is in place and play begins.
        sdl(SDL_CreateWindowAndRenderer("Yonderest", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN, &window, &renderer));
        sdl(SDL_SetRenderLogicalPresentation(renderer, screen_width, screen_height, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE));
        sdl(SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND));
        SDL_SetRenderVSync(renderer, 1);
        prepare();
    }
    ~Pixels() override {
        for (auto& [path, art] : images) { SDL_DestroyTexture(art.texture); SDL_DestroySurface(art.surface); }
        for (auto& [path, texture] : carried) SDL_DestroyTexture(texture);
        for (auto& [key, texture] : turns) SDL_DestroyTexture(texture);
        SDL_DestroyTexture(entry);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool fullscreen() const { return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0; }
    void toggle_fullscreen() { sdl(SDL_SetWindowFullscreen(window, !fullscreen())); }
    void notify(const Notice& notice) {
        toast = notice.text;
        toast_until = host.now() + notice.seconds;
    }
    void show(Transformation t) { crafts.push_back({std::move(t), host.now()}); }
    void struck() { swing_until = host.now() + swing_seconds; }

    void begin() override {
        // Play starts with the land around the player in place.
        const auto& p = host.player();
        host.world().load(int(p.x) - screen_width / tile, int(p.y) - screen_height / tile,
                          int(p.x) + screen_width / tile, int(p.y) + screen_height / tile);
        follow();
        sdl(SDL_ShowWindow(window));
    }

    bool focused() const override { return has_focus; }

    void collect() override {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            SDL_ConvertEventToRenderCoordinates(renderer, &event);
            if (event.type == SDL_EVENT_QUIT) host.quit();
            else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                has_focus = false;
                host.interrupt();
                host.save_changes();
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) has_focus = true;
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                mouse_x = event.motion.x; mouse_y = event.motion.y;
                if (auto* screen = host.screen())
                    if (auto view = screen->view(); auto* menu = std::get_if<Menu>(&view))
                        for (int i = 0; i < int(menu->items.size()); ++i) if (menu_item(i).contains(mouse_x, mouse_y)) menu->hover(i);
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                press(event.button.x, event.button.y, event.button.clicks);
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL && !host.screen()) {
                auto book = host.provided<Book>();
                if (book && book->open && book_layout.book.contains(event.wheel.mouse_x, event.wheel.mouse_y))
                    book->browse(event.wheel.y > 0 ? -1 : 1);
            } else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                host.controls->key({int(event.key.key), event.type == SDL_EVENT_KEY_DOWN, event.key.repeat});
            }
        }
    }

    void draw(double now) override {
        follow();
        std::erase_if(crafts, [&](const Craft& c) { return now - c.start >= craft_seconds; });
        scene(now);
        auto book = host.provided<Book>();
        auto bag = host.provided<Bag>();
        std::optional<View> view;
        if (auto* screen = host.screen()) view = screen->view();
        bool backpack = view && std::holds_alternative<Backpack>(*view);
        bool book_shown = book && book->open && !backpack;
        if (book_shown) draw_book(*book);
        hud(book_shown, bag && bag->open);
        if (view) {
            if (auto* b = std::get_if<Backpack>(&*view)) inventory(*b);
            else if (auto* m = std::get_if<Menu>(&*view)) menu(*m);
            else if (auto* s = std::get_if<Sheet>(&*view)) sheet(*s);
            else if (auto* b = std::get_if<Banner>(&*view)) banner(b->text);
        } else if (!has_focus) banner("Paused");
        if (now < toast_until) draw_toast(toast);
    }

    void capture(const std::filesystem::path& path) override {
        SDL_Surface* pixels = sdl(SDL_RenderReadPixels(renderer, nullptr));
        bool saved = SDL_SaveBMP(pixels, path.c_str());
        SDL_DestroySurface(pixels);
        sdl(saved);
    }

    void present() override {
        sdl(SDL_RenderPresent(renderer));
        SDL_Delay(1);
    }

private:
    Host& host;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int camera_x = 0, camera_y = 0; // In art pixels, so the world never shimmers between pixels.
    bool has_focus = true;
    float mouse_x = -1, mouse_y = -1;
    std::string toast;
    double toast_until = 0, swing_until = 0;
    struct Craft { Transformation made; double start; };
    std::vector<Craft> crafts;

    struct Image { SDL_Surface* surface; SDL_Texture* texture; };
    std::map<std::filesystem::path, Image> images;
    std::map<Block, SDL_Texture*> carried;
    std::map<std::pair<std::filesystem::path, int>, SDL_Texture*> turns;
    static constexpr int ring_entry = 20;
    SDL_Texture* entry = nullptr; // One ring entry, drawn apart so it can be subdued as a whole.
    // Art found by name in the packs: named images, and each block's in the world and in hand.
    std::map<std::string, std::filesystem::path> named;
    std::map<Block, std::filesystem::path> textures, held_art;
    std::filesystem::path unknown_path;
    std::map<std::string, Pose> pose_specs;
    struct PoseArt { const Pose* pose; SDL_Texture *holding, *bare; };
    std::map<std::string, PoseArt> poses;
    SDL_Texture *grip = nullptr, *font = nullptr, *unknown = nullptr;
    SDL_Texture *recipes_icon = nullptr, *backpack_icon = nullptr, *hand_icon = nullptr;
    std::array<int, 128> advance{};

    // A block's art is blocks/<namespace>/<name>.png, and held/<namespace>/<name>.png when it is
    // drawn in the character's hand rather than as a carried block.
    static std::filesystem::path art(const Block& id) {
        auto colon = id.find(':');
        if (colon == std::string::npos) return id + ".png";
        return std::filesystem::path(id.substr(0, colon)) / (id.substr(colon + 1) + ".png");
    }

    void prepare() {
        for (const auto& pack : host.content.packs) {
            if (std::filesystem::is_directory(pack / "images"))
                for (const auto& file : std::filesystem::directory_iterator(pack / "images"))
                    if (file.path().extension() == ".png") named[file.path().stem().string()] = file.path();
            for (const auto& [id, definition] : host.content.blocks) {
                if (auto path = pack / "blocks" / art(id); std::filesystem::exists(path)) textures[id] = path;
                if (auto path = pack / "held" / art(id); std::filesystem::exists(path)) held_art[id] = path;
            }
            if (std::filesystem::exists(pack / "character.json")) read_poses(Field(read_json(pack / "character.json"), (pack / "character.json").string()));
        }
        auto image_named = [&](const std::string& name) { return named.at(name); };
        for (const char* name : {"idle", "stride", "jump", "fall", "crawl-1", "crawl-2", "leapfrog", "back", "back-2", "strike-up"}) {
            const auto& pose = pose_specs.at(name);
            poses[name] = {&pose, canvas(image_named(pose.image)), canvas(image_named(pose.bare.empty() ? pose.image : pose.bare))};
        }
        grip = canvas(image_named("character-grip"));
        recipes_icon = image(image_named("recipes"));
        backpack_icon = image(image_named("backpack"));
        hand_icon = image(image_named("hand"));
        unknown_path = image_named("unknown");
        unknown = image(unknown_path);
        auto font_path = image_named("font");
        font = image(font_path);
        auto* glyphs = images.at(font_path).surface;
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
        entry = sdl(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, ring_entry, ring_entry));
        SDL_SetTextureScaleMode(entry, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(entry, SDL_BLENDMODE_BLEND);
    }

    void read_poses(const Field& file) {
        auto list = file["poses"];
        for (const auto& name : list.keys()) {
            auto entry = list[name.c_str()];
            auto pixel = [](const Field& f) { return int(f.integer()); };
            Pose p;
            p.image = entry["image"].text();
            p.bare = entry["bare"].text_or("");
            p.anchor = pixel(entry["anchor"]);
            p.hand_x = pixel(entry["hand"]["x"]);
            p.hand_y = pixel(entry["hand"]["y"]);
            // A carried block rests beside the hand unless the pose places it.
            auto block = entry["block"];
            p.block_x = block.present() ? pixel(block["x"]) : p.hand_x + 1;
            p.block_y = block.present() ? pixel(block["y"]) : p.hand_y - 3;
            pose_specs[name] = p;
        }
    }

    // The pointer operates interface controls only, never the world.
    void press(float x, float y, int clicks) {
        auto bag = host.provided<Bag>();
        auto book = host.provided<Book>();
        if (auto* screen = host.screen()) {
            auto view = screen->view();
            if (auto* menu = std::get_if<Menu>(&view)) {
                for (int i = 0; i < int(menu->items.size()); ++i) if (menu_item(i).contains(x, y)) menu->choose(i);
            } else if (auto* b = std::get_if<Backpack>(&view)) {
                int page = b->columns * b->rows, first_entry = b->index / page * page;
                for (int k = 0; k < page; ++k)
                    if (inventory_slot(k, b->columns).contains(x, y) && first_entry + k < int(b->entries.size())) b->select(first_entry + k, clicks);
                for (int i = 0; i < 3; ++i)
                    if (inventory_action(i).contains(x, y) && !b->entries.empty()) b->act(i);
                if (backpack_button.contains(x, y) && bag) bag->toggle();
            } else screen->command("back");
        } else if (backpack_button.contains(x, y) && bag) bag->toggle();
        else if (book && book->open && book_layout.book.contains(x, y)) {
            // Turn a page at its corner; anywhere else on the book closes it.
            if (book_layout.previous.contains(x, y)) book->browse(-1);
            else if (book_layout.next.contains(x, y)) book->browse(1);
            else book->toggle();
        } else if (book && !book->open && book_button.contains(x, y)) book->toggle();
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
    // A block without art of its own, defined or not, is drawn as unknown.
    void block(const Block& id, int x, int y, Color tint = {255, 255, 255}) {
        auto found = textures.find(id);
        draw(found == textures.end() ? unknown : image(found->second), {x, y, tile, tile}, SDL_FLIP_NONE, tint);
    }

    int center_x(const Player& p) const { return int(std::lround((p.x + player_width / 2) * tile)) - camera_x; }

    void follow() {
        const auto& p = host.player();
        auto& world = host.world();
        camera_x = int(std::lround((p.x + player_width / 2) * tile)) - screen_width / 2;
        int y = int(std::lround((p.y + player_height) * tile)) - screen_height * 3 / 5;
        camera_y = std::clamp(y, -2 * tile, world.settings.height() * tile - screen_height);
        int left = int(std::floor(camera_x / double(tile))), top = int(std::floor(camera_y / double(tile)));
        world.retain(left, top, left + screen_width / tile + 1, top + screen_height / tile + 1);
    }

    const char* pose_for(const Player& p, bool swinging) const {
        double travelled = host.physics->stride();
        bool grounded = host.physics->grounded();
        // In midair with S held, the character leapfrogs over the block it is about to set down.
        if (p.down) return !grounded ? "leapfrog" : int(travelled / crawl_step) % 2 ? "crawl-2" : "crawl-1";
        if (p.facing == 0) {
            if (p.up && swinging) return "strike-up";
            return int(travelled / walk_step) % 2 ? "back-2" : "back";
        }
        if (!grounded) return host.physics->rising() ? "jump" : "fall";
        if (travelled == 0) return "idle";
        return int(travelled / walk_step) % 2 ? "idle" : "stride";
    }

    void scene(double now) {
        auto& world = host.world();
        const auto& p = host.player();
        // Open sky above ground; depth darkens the backdrop rather than inventing scenery.
        auto behind = host.provided<Backdrop>();
        double daylight = behind ? behind->daylight : 1;
        for (int y = 0; y < screen_height; y += 4)
            fill({0, y, screen_width, 4}, {Uint8((96 + y / 6) * daylight + 10), Uint8((150 + y / 6) * daylight + 10), Uint8((196 + y / 10) * daylight + 18)});
        if (daylight > 0.8) {
            fill({476, 50, 20, 16}, {255, 236, 178}); fill({478, 48, 16, 20}, {255, 236, 178});
        }
        if (behind) {
            // The ridges keep their place above the world's ground level, wherever in the height it lies:
            // `level` is the camera standing there.
            int level = int(behind->level) * tile - screen_height * 3 / 5;
            for (int layer = 0; layer < 3; ++layer) {
                Color c{Uint8((92 - layer * 14) * daylight + 8), Uint8((140 - layer * 16) * daylight + 10), Uint8((150 - layer * 14) * daylight + 18)};
                for (int sx = 0; sx < screen_width; sx += 4) {
                    int ridge = int(72 + layer * 30 + tile * behind->rise(layer, camera_x / double(tile) + sx / double(tile))
                                    - (camera_y - level) * 0.05);
                    fill({sx, ridge, 4, screen_height - ridge}, c);
                }
            }
        }
        auto label = host.provided<Label>();
        int left = int(std::floor(camera_x / double(tile))), top = int(std::floor(camera_y / double(tile)));
        // Back to front; layers behind the front are shaded.
        for (Layer layer = Layer(host.content.layers.size()) - 1; layer >= 0; --layer)
            for (int x = left; x <= left + screen_width / tile + 1; ++x)
                for (int y = top; y <= top + screen_height / tile + 1; ++y) {
                    const auto& cell = world.cell(x, y, layer);
                    int sx = x * tile - camera_x, sy = y * tile - camera_y, wx = wrap(x, world.settings.width());
                    // Land that has not arrived yet is shown for what it is, not as open air.
                    if (!world.ready(x, y)) {
                        if (layer == 0) fill({sx, sy, tile, tile}, {18, 16, 26});
                        continue;
                    }
                    if (layer == 0 && transform(now, wx, y, sx, sy)) continue;
                    if (cell.id.empty()) continue;
                    block(cell.id, sx, sy, layer > 0 ? Color{132, 128, 150} : Color{255, 255, 255});
                    if (label)
                        if (auto shown = label->text(cell); !shown.empty())
                            text(sx + tile - measure(shown), sy + tile - 8, shown, gold, true);
                }
        character(p, pose_for(p, now < swing_until), now < swing_until);
        if (auto ring = host.provided<Ring>(); ring && ring->open) chooser(p, *ring);
    }

    // A completed recipe resolves in place: its ingredients dissolve into light, which settles
    // into the result. The world has already changed; this only shows it happening, and a cell
    // the player has since changed shows the world as it is. Returns whether it drew the cell.
    bool transform(double now, int wx, int y, int sx, int sy) {
        auto& world = host.world();
        for (const auto& [made, start] : crafts) {
            double t = (now - start) / craft_seconds;
            bool result = made.result.x == wx && made.result.y == y;
            auto ingredient = std::ranges::find_if(made.consumed, [&](const Transformation::Part& i) { return i.x == wx && i.y == y; });
            if (!result && ingredient == made.consumed.end()) continue;
            if (world.block(wx, y) != (result ? made.result.block : Block{})) return false;
            const Block* from = ingredient == made.consumed.end() ? nullptr : &ingredient->block;
            const Block* to = result ? &made.result.block : nullptr;
            constexpr int bayer[16]{0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
            constexpr Color light{255, 236, 178};
            for (int py = 0; py < tile; ++py)
                for (int px = 0; px < tile; ++px) {
                    double order = (bayer[py % 4 * 4 + px % 4] + 0.5) / 16;
                    auto before = from ? texel(*from, px, py) : std::nullopt;
                    auto after = to ? texel(*to, px, py) : std::nullopt;
                    std::optional<Color> c;
                    if (t < 0.5) { if (before) c = order >= t * 2 ? *before : light; }
                    else if (after && order < t * 2 - 1) c = *after;
                    else if ((before || after) && order >= t * 2 - 1) c = light;
                    if (!c) continue;
                    SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, c->a);
                    SDL_RenderPoint(renderer, float(sx + px), float(sy + py));
                }
            return true;
        }
        return false;
    }

    SDL_Surface* surface(const Block& id) {
        auto found = textures.find(id);
        const auto& path = found == textures.end() ? unknown_path : found->second;
        image(path);
        return images.at(path).surface;
    }

    // One pixel of a block as drawn in the world, or nothing where it is transparent.
    std::optional<Color> texel(const Block& id, int x, int y) {
        Uint8 r, g, b, a;
        SDL_ReadSurfacePixel(surface(id), x, y, &r, &g, &b, &a);
        if (!a) return std::nullopt;
        return Color{r, g, b, a};
    }

    // Body, then the held object, then the grip closing over it, all on the pose's 16x16 canvas
    // and mirrored as one. Held art is drawn as the idle pose holds it, moved to this pose's hand.
    // An empty hand rests at the side.
    void character(const Player& p, const char* name, bool swinging) {
        const auto& [pose, holding, bare] = poses.at(name);
        const auto& idle = pose_specs.at("idle");
        bool flip = p.facing < 0;
        int cx = center_x(p), top = int(std::lround((p.y + player_height) * tile)) - camera_y - tile;
        int left = flip ? cx + pose->anchor - tile : cx - pose->anchor;
        // Art whose column 0 lies at pose column ox.
        auto place = [&](SDL_Texture* art, int size, int ox, int oy) {
            draw(art, {flip ? left + tile - size - ox : left + ox, top + oy, size, size}, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        };
        int dx = pose->hand_x - idle.hand_x, dy = pose->hand_y - idle.hand_y;
        if (p.held.empty()) return place(bare, tile, 0, 0);
        place(holding, tile, 0, 0);
        auto held = held_art.find(p.held);
        if (held == held_art.end()) place(carry(p.held), 8, pose->block_x, pose->block_y);
        // A swing turns the tool toward what it strikes: down in front, or up overhead.
        else if (swinging) place(turned(held->second, idle, p.up ? -1 : 1), 2 * tile, dx - tile / 2, dy - tile / 2);
        else place(canvas(held->second), tile, dx, dy);
        place(grip, tile, dx, dy);
    }

    void chooser(const Player& p, const Ring& ring) {
        int px = center_x(p), py = int(std::lround((p.y + player_height) * tile)) - camera_y - 8;
        if (ring.entries.empty()) {
            // An empty wheel, not a text error.
            Rect box{px - 10, py - 40, 20, 20};
            panel(box);
            SDL_SetRenderDrawColor(renderer, muted.r, muted.g, muted.b, 255);
            SDL_RenderLine(renderer, box.x + 4, box.y + box.h - 5, box.x + box.w - 5, box.y + 4);
            return;
        }
        int selected = int(ring.selected);
        int count = int(ring.entries.size());
        double radius = count > 6 ? 40 : 32;
        for (int i = 0; i < count; ++i) {
            double angle = 2 * std::numbers::pi * (i - selected) / count - std::numbers::pi / 2;
            int x = px + int(std::lround(radius * std::cos(angle))) - ring_entry / 2;
            int y = py + int(std::lround(radius * std::sin(angle))) - ring_entry / 2;
            if (i == selected) {
                ring_item(ring.entries[i], x, y);
                frame({x, y, ring_entry, ring_entry}, focus);
                continue;
            }
            // The others are subdued as a whole, drawn apart and laid over the world at half opacity.
            sdl(SDL_SetRenderTarget(renderer, entry));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);
            ring_item(ring.entries[i], 0, 0);
            sdl(SDL_SetRenderTarget(renderer, nullptr));
            draw(entry, {x, y, ring_entry, ring_entry}, SDL_FLIP_NONE, {255, 255, 255, 128});
        }
    }

    void ring_item(const std::pair<Block, std::int64_t>& item, int x, int y) {
        panel({x, y, ring_entry, ring_entry});
        block(item.first, x + 2, y + 2);
        auto count = std::to_string(item.second);
        text(x + ring_entry - measure(count), y + 13, count, ink, true);
    }

    // Free-standing objects; one lifts a pixel when pointed at or open.
    void hud(bool book_open, bool bag_open) {
        auto object = [&](SDL_Texture* art, const Rect& r, bool lifted) { draw(art, {r.x, r.y - lifted, r.w, r.h}); };
        if (!book_open) object(recipes_icon, book_button, book_button.contains(mouse_x, mouse_y));
        object(backpack_icon, backpack_button, bag_open || backpack_button.contains(mouse_x, mouse_y));
    }

    // The recipe book lies open in the corner: the shape on the left page, what it makes and
    // needs on the right. It never crafts anything itself.
    void draw_book(const Book& r) {
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
            if (button.contains(mouse_x, mouse_y)) fill(button, edge);
            centered(button, glyph, ink_on_paper);
        }

        auto title = host.content.name(r.output);
        text(l.left.x + (l.left.w - measure(title)) / 2, l.left.y + 4, title, ink_on_paper);
        int span = std::max(r.width * 17, int(r.stations.size()) * 17 + 2 + measure("beneath")) - 1;
        int gx = l.left.x + (l.left.w - span) / 2, gy = l.left.y + 18;
        for (const auto& i : r.shape) {
            int x = gx + i.x * 17, y = gy + i.y * 17;
            if (i.block.empty()) {
                // A cell that must stay empty: a dotted outline.
                SDL_SetRenderDrawColor(renderer, faint.r, faint.g, faint.b, 255);
                for (int k = 0; k < 16; k += 3) {
                    SDL_RenderPoint(renderer, x + k, y); SDL_RenderPoint(renderer, x + k, y + 15);
                    SDL_RenderPoint(renderer, x, y + k); SDL_RenderPoint(renderer, x + 15, y + k);
                }
            } else block(i.block, x, y);
        }
        if (!r.stations.empty()) {
            // Required working surfaces sit directly beneath the shape.
            int x = gx, y = gy + r.height * 17;
            for (const auto& station : r.stations) {
                block(station, x, y);
                x += 17;
            }
            text(x + 2, y + 4, "beneath", faint);
        }
        auto page = std::to_string(r.index + 1) + "/" + std::to_string(r.count);
        text(l.left.x + (l.left.w - measure(page)) / 2, l.previous.y + 2, page, faint);

        int x = l.right.x + 8, y = l.right.y + 4;
        text(x, y, "Makes", faint);
        block(r.output, x, y + 12);
        text(x + 20, y + 16, "x" + std::to_string(r.amount), ink_on_paper);
        text(x, y + 36, "Needs", faint);
        int column = 0;
        for (const auto& need : r.needs) {
            int ix = x + column % 2 * 46, iy = y + 48 + column / 2 * 18;
            block(need.block, ix, iy);
            text(ix + 18, iy + 4, "x" + std::to_string(need.count), need.enough ? ink_on_paper : short_on_paper);
            ++column;
        }
    }

    void inventory(const Backpack& b) {
        panel(inventory_panel);
        text(inventory_panel.x + 12, inventory_panel.y + 8, b.title);
        text(inventory_panel.x + inventory_panel.w - 12 - measure(b.readiness), inventory_panel.y + 8, b.readiness, gold);
        int page = b.columns * b.rows, first_entry = b.index / page * page;
        for (int k = 0; k < page; ++k) {
            auto r = inventory_slot(k, b.columns);
            fill(r, {44, 38, 60});
            int i = first_entry + k;
            if (i >= int(b.entries.size())) continue;
            const auto& e = b.entries[i];
            block(e.block, r.x + 4, r.y + 3);
            auto label = std::to_string(e.count);
            text(r.x + r.w - 1 - measure(label), r.y + r.h - 9, label, ink, true);
            if (e.ready) fill({r.x + 1, r.y + 1, 4, 4}, gold);
            if (e.held) frame({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, ink);
            if (i == b.index && b.focus < 0) frame(r, focus);
            else if (r.contains(mouse_x, mouse_y)) frame(r, panel_edge);
        }
        int y = inventory_slot(page - 1, b.columns).y + 28;
        if (b.entries.empty()) {
            draw(hand_icon, {inventory_panel.x + inventory_panel.w / 2 - 8, inventory_slot(0, b.columns).y + 40, 16, 16});
            return;
        }
        text(inventory_panel.x + 12, y, b.entries[std::min(b.index, int(b.entries.size()) - 1)].name);
        for (int i = 0; i < 3; ++i) {
            auto r = inventory_action(i);
            fill(r, r.contains(mouse_x, mouse_y) ? panel_edge : Color{44, 38, 60});
            if (i == b.focus) frame(r, focus);
            centered(r, b.actions[i]);
        }
    }

    void menu(const Menu& m) {
        auto r = menu_panel(int(m.items.size()));
        panel(r);
        centered({r.x, r.y + 4, r.w, 14}, "Paused");
        for (int i = 0; i < int(m.items.size()); ++i) {
            auto item = menu_item(i);
            if (i == m.index) fill(item, {60, 52, 82});
            else if (item.contains(mouse_x, mouse_y)) fill(item, {44, 38, 60});
            centered(item, m.items[i], i == m.index ? gold : ink);
        }
    }

    void sheet(const Sheet& s) {
        Rect r{(screen_width - 300) / 2, 46, 300, 22 + int(s.lines.size()) * 12 + 30};
        panel(r);
        centered({r.x, r.y + 5, r.w, 12}, s.title);
        int y = r.y + 22;
        for (const auto& [key, what] : s.lines) {
            text(r.x + 14, y, key, gold);
            text(r.x + 64, y, what);
            y += 12;
        }
        text(r.x + 14, y + 6, s.footer, muted);
    }

    void banner(const std::string& s) {
        int w = measure(s) + 24;
        Rect r{(screen_width - w) / 2, 140, w, 22};
        panel(r);
        centered(r, s);
    }

    void draw_toast(const std::string& s) {
        int w = measure(s) + 16;
        Rect r{(screen_width - w) / 2, screen_height - 30, w, 17};
        panel(r);
        centered(r, s);
    }

    SDL_Texture* image(const std::filesystem::path& path) {
        auto found = images.find(path);
        if (found != images.end()) return found->second.texture;
        SDL_Surface* loaded = SDL_LoadPNG(path.c_str());
        if (!loaded) throw std::runtime_error(path.string() + ": " + SDL_GetError());
        SDL_Texture* texture = sdl(SDL_CreateTextureFromSurface(renderer, loaded));
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        images.emplace(path, Image{loaded, texture});
        return texture;
    }

    // Character art: a 16x16 canvas.
    SDL_Texture* canvas(const std::filesystem::path& path) { return image(path); }

    // A swing: the held art turned a quarter about the knuckle, one pixel up and forward of the
    // idle hand, clockwise (1) or counterclockwise (-1). Pixel-exact, so any tool swings without
    // art of its own. The result is 32x32 with the held canvas's origin at (8, 8).
    SDL_Texture* turned(const std::filesystem::path& held, const Pose& idle, int way) {
        auto found = turns.find({held, way});
        if (found != turns.end()) return found->second;
        canvas(held);
        SDL_Surface* source = images.at(held).surface;
        SDL_Surface* out = sdl(SDL_CreateSurface(2 * tile, 2 * tile, SDL_PIXELFORMAT_RGBA32));
        SDL_ClearSurface(out, 0, 0, 0, 0);
        int kx = idle.hand_x + 1, ky = idle.hand_y - 1, o = tile / 2;
        for (int y = 0; y < tile; ++y)
            for (int x = 0; x < tile; ++x) {
                Uint8 r, g, b, a;
                SDL_ReadSurfacePixel(source, x, y, &r, &g, &b, &a);
                if (a) SDL_WriteSurfacePixel(out, o + kx - way * (y - ky), o + ky + way * (x - kx), r, g, b, a);
            }
        SDL_Texture* texture = sdl(SDL_CreateTextureFromSurface(renderer, out));
        SDL_DestroySurface(out);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        return turns.emplace(std::pair{held, way}, texture).first->second;
    }

    // A carried block: one fixed 8x8 icon, a 6x6 sampling of the tile inside the character's outline colour.
    // Each block ID has its own, whatever its display name.
    SDL_Texture* carry(const Block& id) {
        constexpr int size = 8, inner = size - 2;
        auto found = carried.find(id);
        if (found != carried.end()) return found->second;
        SDL_Surface* small = sdl(SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA32));
        SDL_Surface* source = surface(id);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                bool edge = x == 0 || y == 0 || x == size - 1 || y == size - 1;
                bool corner = (x == 0 || x == size - 1) && (y == 0 || y == size - 1);
                Uint8 r = outline.r, g = outline.g, b = outline.b, a = corner ? 0 : 255;
                if (!edge) {
                    int sx = (x - 1) * tile / inner + 1, sy = (y - 1) * tile / inner + 1;
                    SDL_ReadSurfacePixel(source, sx, sy, &r, &g, &b, &a);
                    if (!a) { r = outline.r; g = outline.g; b = outline.b; a = 255; }
                }
                SDL_WriteSurfacePixel(small, x, y, r, g, b, a);
            }
        SDL_Texture* texture = sdl(SDL_CreateTextureFromSurface(renderer, small));
        SDL_DestroySurface(small);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        return carried.emplace(id, texture).first->second;
    }
};

}

void attach(Host& host) {
    auto& pixels = host.adopt<Pixels>(host);
    host.display = &pixels;
    host.menu_items.push_back({[&pixels] { return std::string(pixels.fullscreen() ? "Windowed" : "Fullscreen"); },
                               [&pixels] { pixels.toggle_fullscreen(); }});
    host.events.on<Notice>([&pixels](Notice& notice) { pixels.notify(notice); });
    host.events.on<Transformation>([&pixels](Transformation& made) { pixels.show(made); });
    host.events.on<Acted>([&pixels](Acted& acted) {
        if (acted.act == Act::a) pixels.struck();
    });
}

}
