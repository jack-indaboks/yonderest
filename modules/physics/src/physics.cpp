// Standard physics: walking, jumping and falling, collision, and a block's solidity.

#include "yonderest/host.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>

namespace yonder::physics {
namespace {

constexpr double contact_epsilon = 0.00001;
// The jump's height is liked as it is; both halves of the arc are kept brisk around it.
constexpr double jump_height = 2.54, rise_gravity = 44, fall_gravity = 52, terminal_speed = 28;

// The smallest shift of at most the margin, never against `way` (-1, 0 for either, or 1), that
// brings the body's span [low, low + size) into line with a cell boundary where the body fits.
template <class Fits>
std::optional<double> slip(double low, double size, int way, Fits fits) {
    std::optional<double> best;
    for (double shift : {std::ceil(low) - low, std::floor(low + size) - (low + size)})
        if (shift != 0 && std::abs(shift) <= margin && shift * way >= 0 && fits(shift)
            && (!best || std::abs(shift) < std::abs(*best)))
            best = shift;
    return best;
}

class Standard : public Physics {
public:
    explicit Standard(Host& host) {
        // A block stops movement unless its definition says otherwise; missing content takes that default.
        for (const auto& [id, definition] : host.content.blocks) {
            Field solid(definition.properties["physics"]["solid"], id + ".physics.solid");
            if (!solid.flag_or(true)) passable.insert(id);
        }
    }

    bool solid(const Block& id) const override { return !id.empty() && !passable.contains(id); }

    void steer(int way, bool fast) override {
        direction = way;
        sprint = fast;
    }
    void jump() override { jumping_ = true; }
    bool grounded() const override { return grounded_; }
    bool rising() const override { return vy < 0; }
    bool jumping() const override { return jumping_; }
    int takeoff() const override { return vy < 0 ? takeoff_ : -1; }
    double stride() const override { return travelled; }

    void tick(World& world, Player& p) override {
        double before = p.x;
        move(world, p);
        // A wrap around the world is not a step.
        double moved = std::abs(p.x - before);
        travelled = moved > 0 && moved < 1 ? travelled + moved : 0;
        jumping_ = false;
    }

    void release() {
        direction = 0;
        sprint = jumping_ = false;
    }

    // Moves the body clear of a cell it overlaps by no more than the margin, as little as possible.
    // Returns false if that cell cannot be cleared.
    bool clear_of(World& world, Player& p, int x, int y) const {
        double left = x - (p.x + player_width), right = x + 1 - p.x;
        double up = y - (p.y + player_height), down = y + 1 - p.y;
        if (left >= 0 || right <= 0 || up >= 0 || down <= 0) return true;
        std::optional<std::pair<double, double>> best;
        for (auto [sx, sy] : {std::pair{left, 0.0}, {right, 0.0}, {0.0, up}, {0.0, down}})
            if (std::abs(sx + sy) <= margin + contact_epsilon && !intersects(world, p.x + sx, p.y + sy)
                && (!best || std::abs(sx + sy) < std::abs(best->first + best->second)))
                best = {sx, sy};
        if (!best) return false;
        p.x = wrap(p.x + best->first, double(world.settings.width()));
        p.y += best->second;
        return true;
    }

private:
    std::set<Block> passable;
    double vy = 0, travelled = 0;
    bool grounded_ = false, jumping_ = false, sprint = false;
    int direction = 0, takeoff_ = -1;

    // Outside the world, and in land that has not arrived yet, everything stops the body.
    bool blocked(World& world, int x, int y) const {
        return y < 0 || y >= world.settings.height() || !world.ready(x, y) || solid(world.block(x, y));
    }

    bool intersects(World& world, double x, double y) const {
        for (int bx = int(std::floor(x)); bx <= int(std::floor(x + player_width - contact_epsilon)); ++bx)
            for (int by = int(std::floor(y)); by <= int(std::floor(y + player_height - contact_epsilon)); ++by)
                if (blocked(world, bx, by)) return true;
        return false;
    }

    void move(World& world, Player& p) {
        grounded_ = intersects(world, p.x, p.y + 0.025);
        if (jumping_ && grounded_) {
            vy = -std::sqrt(2 * rise_gravity * jump_height);
            grounded_ = false;
            takeoff_ = int(std::floor(p.y + player_height / 2));
        }
        double speed = p.down ? 2.0 : (sprint ? 9.0 : 6.0);
        double dx = direction * speed * tick_seconds;
        // No automatic climbing: a step up is a jump. A body in the air slips past a corner it
        // catches by no more than the margin, onward in the way it is already moving and never back,
        // so a jump can enter an opening just its own height and a landing is never refused.
        if (!intersects(world, p.x + dx, p.y)) p.x += dx;
        else if (auto shift = slip(p.y, player_height, vy < 0 ? -1 : 1, [&](double s) { return !intersects(world, p.x + dx, p.y + s); })) {
            p.x += dx;
            p.y += *shift;
        }
        vy = std::min(terminal_speed, vy + (vy < 0 ? rise_gravity : fall_gravity) * tick_seconds);
        double dy = vy * tick_seconds;
        if (!intersects(world, p.x, p.y + dy)) p.y += dy;
        else if (auto shift = dy < 0 ? slip(p.x, player_width, direction, [&](double s) { return !intersects(world, p.x + s, p.y + dy); })
                                     : std::nullopt) {
            p.x += *shift;
            p.y += dy;
        } else {
            if (dy > 0) p.y = std::floor(p.y + player_height + dy) - player_height;
            else p.y = std::floor(p.y + dy) + 1;
            grounded_ = dy > 0;
            vy = 0;
        }
        if (grounded_) takeoff_ = -1;
        p.x = wrap(p.x, double(world.settings.width()));
    }
};

}

void attach(Host& host) {
    auto& physics = host.adopt<Standard>(host);
    host.physics = &physics;
    // A solid block placed where the body overlaps by no more than the margin moves the body clear.
    host.events.on<Placing>([&physics, &host](Placing& placing) {
        if (placing.layer == 0 && physics.solid(placing.cell.id) && !physics.clear_of(host.world(), placing.player, placing.x, placing.y))
            placing.refusal = "No room";
    });
    host.events.on<Interrupted>([&physics](Interrupted&) { physics.release(); });
}

}
