// Basic interaction: the base meanings of the actions. Action A takes the block the player
// addresses, action B uses it, action C leaves the held block in the world. A tool requirement,
// such as copper needing a pick, is a rule of taking in which the held block matters.

#include "yonderest/host.hpp"

#include <cmath>
#include <map>
#include <optional>

namespace yonder::interaction {
namespace {

class Basic : public Interaction {
public:
    explicit Basic(Host& host) : host(host) {
        for (const auto& [id, definition] : host.content.blocks) {
            const auto& own = definition.properties["interaction"];
            auto tool = Field(own["tool"], id + ".interaction.tool").text_or("");
            auto needed = Field(own["requires_tool"], id + ".interaction.requires_tool").text_or("");
            if (!tool.empty()) tools[id] = tool;
            if (!needed.empty()) requirements[id] = needed;
        }
    }

    void act(Action& action) override {
        switch (action.act) {
        case Act::a: take(action); break;
        case Act::b: notify("Nothing to interact with"); break;
        case Act::c: place(action); break;
        }
    }

    // A placement that waits for the feet to clear the cell above the one they were in.
    std::optional<Target> pending;

    void step() {
        if (!pending) return;
        auto& p = host.player();
        auto& world = host.world();
        auto t = *pending;
        // Cancelled quietly when the aim or the column changes or the feet land.
        if (!p.down || host.physics->grounded()
            || wrap(t.x, world.settings.width()) != wrap(int(std::floor(p.x + player_width / 2)), world.settings.width())) {
            pending.reset();
            return;
        }
        double reaching = p.y + player_height - t.y; // How far the feet still reach into the cell.
        if (reaching > 0.00001) {
            if (host.physics->rising()) return;
            if (reaching > margin) {
                pending.reset();
                notify("No clearance beneath you");
                return;
            }
        }
        pending.reset();
        put(p.held, t);
    }

private:
    Host& host;
    std::map<Block, std::string> tools, requirements;

    const std::string& tool(const Block& id) const {
        static const std::string none;
        auto found = tools.find(id);
        return found == tools.end() ? none : found->second;
    }

    // Outcomes the world shows for itself, such as a block taken or placed, say nothing.
    void notify(const std::string& text) { host.events.announce(Notice{text}); }

    // In the air the body straddles two rows. When the target cannot take an action that needs it
    // occupied (or empty) and the other row the body overlaps can, that row is meant instead.
    Target favor(Target t, bool occupied) {
        auto& p = host.player();
        auto& world = host.world();
        if (host.physics->grounded() || p.up || p.down) return t;
        auto fits = [&](int y) { return world.ready(t.x, y) && world.block(t.x, y, t.layer).empty() != occupied; };
        if (fits(t.y)) return t;
        int top = int(std::floor(p.y));
        for (int y : {top, top + 1}) {
            double overlap = std::min(p.y + player_height, y + 1.0) - std::max(p.y, double(y));
            if (y != t.y && overlap >= 0.25 && fits(y)) return {t.x, y, t.layer};
        }
        return t;
    }

    void take(const Action& action) {
        auto& p = host.player();
        auto& world = host.world();
        if (!action.held.empty() && tool(action.held).empty()) return notify("Use empty hand or a tool");
        // Anything sharing the body's own cell comes before the cell in front.
        Target own{int(std::floor(p.x + player_width / 2)), int(std::floor(p.y + player_height / 2))};
        auto t = world.block(own.x, own.y).empty() ? favor(action.cell, true) : own;
        if (t.y < 0 || t.y >= world.settings.height()) return notify("World boundary");
        Cell old = world.cell(t.x, t.y, t.layer);
        if (old.id.empty()) return;
        if (auto needed = requirements.find(old.id); needed != requirements.end() && tool(action.held) != needed->second)
            return notify("Requires a " + needed->second);
        auto count = host.events.announce(Taking{old}).count;
        host.events.announce(Taken{p, old.id, count});
        world.put(t.x, t.y, {}, t.layer);
    }

    void place(const Action& action) {
        auto& p = host.player();
        auto& world = host.world();
        auto t = action.cell;
        if (action.held.empty()) return notify("Empty hand");
        if (p.down && t.layer == 0) {
            // Rising out of an occupied cell, the request waits for the feet to clear the one above.
            bool rising = host.physics->rising() || (host.physics->jumping() && host.physics->grounded());
            if (rising && !world.block(t.x, t.y).empty() && world.block(t.x, t.y - 1).empty()) {
                pending = Target{t.x, t.y - 1};
                return;
            }
        }
        put(action.held, favor(t, false));
    }

    void put(const Block& held, const Target& t) {
        auto& p = host.player();
        auto& world = host.world();
        if (t.y < 0 || t.y >= world.settings.height()) return notify("World boundary");
        if (!world.ready(t.x, t.y)) return notify("Not there yet");
        if (!world.block(t.x, t.y, t.layer).empty()) return notify("Already occupied");
        auto [x, y] = std::pair{p.x, p.y};
        auto placing = host.events.announce(Placing{.player = p, .x = t.x, .y = t.y, .layer = t.layer, .cell = Cell{held, {}}});
        if (placing.refusal) {
            std::tie(p.x, p.y) = std::pair{x, y};
            return notify(*placing.refusal);
        }
        world.put(t.x, t.y, placing.cell, t.layer);
        host.events.announce(Placed{p, held, placing.count});
    }
};

}

void attach(Host& host) {
    auto& interaction = host.adopt<Basic>(host);
    host.interaction = &interaction;
    // Every action, whatever gives it its meaning, ends a placement still waiting.
    host.events.on<Action>([&interaction](Action&) { interaction.pending.reset(); });
    host.events.on<Held>([&interaction](Held&) { interaction.pending.reset(); });
    host.events.on<Interrupted>([&interaction](Interrupted&) { interaction.pending.reset(); });
    host.events.on<Tick>([&interaction](Tick&) { interaction.step(); });
}

}
