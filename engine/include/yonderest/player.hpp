#pragma once

// The player: the point where every module meets.

#include "yonderest/space.hpp"
#include "yonderest/value.hpp"

namespace yonder {

// The body, until the physics build makes it a whole block. Within the margin the game reads
// intent instead of demanding pixel precision: half the body's width, so the body counts as
// standing in the column that holds its centre.
constexpr double player_width = 0.7;
constexpr double player_height = 1.0;
constexpr double margin = player_width / 2;

struct Player {
    std::string world; // Which world of the universe the player is in.
    double x = 0, y = 0;
    int facing = 1;     // -1 left, 0 the wall behind, 1 right.
    bool up = false;    // Aiming above the head.
    bool down = false;  // Aiming beneath the feet.
    Block held;         // Empty is the empty hand.
    Properties properties;
};

struct Target {
    int x, y;
    Layer layer = 0;
};

// The cell in front: beyond the facing edge, beneath the feet when aiming down, above the head
// when aiming up, or the next layer back when facing the wall. A jump aims one up from the
// moment it leaves the ground: `takeoff` is the row the body stood in while it is rising, or -1.
Target target(const Player& player, int takeoff);

}
