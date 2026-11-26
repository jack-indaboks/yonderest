// Keyboard controls: keys into actions and intents. J, K and L trigger actions A, B and C; WASD
// walk, turn and aim; the open screen takes the keys it binds. The Controls page lists the bindings.

#include "yonderest/host.hpp"

#include <SDL3/SDL_keycode.h>

namespace yonder::controls {
namespace {

// How often a held J repeats.
constexpr double repeat_seconds = 0.16;

class Page : public Screen {
public:
    explicit Page(Host& host) : host(host) {}
    std::string_view name() const override { return "controls"; }
    void command(std::string_view command) override {
        if (command == "confirm" || command == "back") host.show_menu();
    }
    View view() override {
        return Sheet{"Controls",
                     {{"A  D", "walk"}, {"Space", "jump"}, {";", "sprint while held"},
                      {"W", "tap: face the background; hold: reach up"}, {"S", "crawl, aim beneath your feet"},
                      {"J", "hit or gather (hold to repeat)"}, {"K", "use: a workbench cycles batch 1, 5, 10"},
                      {"L", "place; S + Space + L builds underfoot"}, {"H", "hand: empty, choose (J/K), equip"},
                      {"I", "inventory: ready objects for H"}, {"R", "recipe book; Left/Right turn pages"}, {"P", "pause"}, {"Esc", "menu"}},
                     "Recipes complete themselves when the shape is built."};
    }

private:
    Host& host;
};

class Keyboard : public Controls {
public:
    explicit Keyboard(Host& host) : page(host), host(host) {}
    Page page;
    int side = 1; // The side last faced, for S from the wall.

    void release() { left = right = sprint = mining = false; }

    void key(const Key& k) override {
        auto& p = host.player();
        auto key = SDL_Keycode(k.code);
        if (!k.pressed) {
            if (key == SDLK_A) left = false;
            if (key == SDLK_D) right = false;
            if (key == SDLK_S) p.down = false;
            // Letting go of W ends the reach up; a held A or D then takes over facing.
            if (key == SDLK_W && p.up) {
                p.up = false;
                if (left != right) p.facing = right ? 1 : -1;
            }
            if (key == SDLK_SEMICOLON) sprint = false;
            if (key == SDLK_J) mining = false;
            steer();
            return;
        }
        bool repeat = k.repeat;
        bool enter = key == SDLK_RETURN || key == SDLK_KP_ENTER;
        // Escape closes the innermost thing that is actually open, then offers the menu.
        if (key == SDLK_ESCAPE) {
            if (!repeat) host.back();
            return;
        }
        if (key == SDLK_R) {
            if (!repeat) host.intent("recipes");
            return;
        }
        if (auto* screen = host.screen()) {
            auto name = screen->name();
            if (name == "menu") {
                if (key == SDLK_UP || key == SDLK_W) screen->command("up");
                if (key == SDLK_DOWN || key == SDLK_S) screen->command("down");
                if (enter && !repeat) screen->command("confirm");
            } else if (name == "controls" || name == "pause") {
                if ((enter || (name == "pause" && key == SDLK_P)) && !repeat) screen->command("confirm");
            } else if (name == "backpack") {
                bool arrow = key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN;
                if (repeat && !arrow) return;
                if (key == SDLK_I) return screen->command("close");
                if (key == SDLK_H) return screen->command("empty hand");
                if (key == SDLK_RIGHT || key == SDLK_D) screen->command("right");
                else if (key == SDLK_LEFT || key == SDLK_A) screen->command("left");
                else if (key == SDLK_DOWN || key == SDLK_S) screen->command("down");
                else if (key == SDLK_UP || key == SDLK_W) screen->command("up");
                else if (enter) screen->command("confirm");
                else if (key == SDLK_J) screen->command("ready");
                else if (key == SDLK_K) screen->command("hold");
                else screen->command("stay");
            }
            return;
        }
        if (repeat) return;
        if (key == SDLK_I) { host.intent("backpack"); return; }
        if (key == SDLK_P) { host.intent("pause"); return; }
        if (key == SDLK_LEFT || key == SDLK_RIGHT) {
            host.intent(key == SDLK_LEFT ? "previous page" : "next page");
            return;
        }
        if (!host.display || !host.display->focused()) return;
        if (key == SDLK_A || key == SDLK_D) {
            if (key == SDLK_A) left = true;
            else right = true;
            // While reaching up, the character shuffles along the wall without turning.
            if (!p.up) p.facing = key == SDLK_A ? -1 : 1;
        }
        // Reaching up and crawling exclude each other; the last key pressed wins.
        if (key == SDLK_W) {
            if (p.facing) side = p.facing;
            p.facing = 0;
            p.up = true;
            p.down = false;
        }
        if (key == SDLK_S) {
            if (p.facing == 0) p.facing = side;
            p.up = false;
            p.down = true;
        }
        if (key == SDLK_SPACE) host.physics->jump();
        if (key == SDLK_SEMICOLON) sprint = true;
        if (key == SDLK_H) {
            mining = false;
            host.intent("hand");
        }
        if (key == SDLK_J) strike(host.now());
        if (key == SDLK_K) host.act(Act::b);
        if (key == SDLK_L) host.act(Act::c);
        steer();
    }

    void frame(double now) override {
        bool playing = host.display && host.display->focused() && !host.screen();
        if (playing && mining && now >= next) strike(now);
    }

private:
    Host& host;
    bool left = false, right = false, sprint = false, mining = false;
    double next = 0;

    void steer() { host.physics->steer(int(right) - int(left), sprint); }

    void strike(double now) {
        mining = host.act(Act::a).repeat;
        if (mining) next = now + repeat_seconds;
    }
};

}

void attach(Host& host) {
    auto& keyboard = host.adopt<Keyboard>(host);
    host.controls = &keyboard;
    host.menu_items.push_back({[] { return std::string("Controls"); }, [&host, &keyboard] { host.open(keyboard.page, false); }});
    host.events.on<Interrupted>([&keyboard](Interrupted&) { keyboard.release(); });
    host.events.on<Started>([&host, &keyboard](Started& started) {
        if (host.player().facing) keyboard.side = host.player().facing;
        if (started.fresh) host.events.announce(Notice{"Esc: menu and controls", 6});
    });
}

}
