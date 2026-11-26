#pragma once

// The physics interface: movement within the world. It moves the player now; other entities
// arrive with the physics build.

#include "yonderest/player.hpp"
#include "yonderest/world.hpp"

namespace yonder {

class Physics {
public:
    virtual ~Physics() = default;
    // Whether a block stops movement. Undefined blocks keep their identity and take the default.
    virtual bool solid(const Block& id) const = 0;
    // Intent: which way to walk (-1, 0, 1), and whether to sprint, until changed.
    virtual void steer(int direction, bool sprint) = 0;
    // Intent: jump when next able, on the next tick.
    virtual void jump() = 0;
    // One tick of movement.
    virtual void tick(World& world, Player& player) = 0;
    virtual bool grounded() const = 0;
    virtual bool rising() const = 0;
    // A jump asked for and not yet taken.
    virtual bool jumping() const = 0;
    // The row the body stood in when the jump it is rising in began, or -1.
    virtual int takeoff() const = 0;
    // How far the body has walked without stopping, for showing its steps.
    virtual double stride() const = 0;
};

}
