#pragma once

#include "world.hpp"

namespace yonder {

// Transient hand/placement state. The inventory and readiness marks belong to Player.
struct Hands {
    bool chooser = false;
    Block choice;
    bool working = false;
    std::optional<Target> pending;

    void equip(Player& player, const Block& id);
    void hand(Player& player);
    void cycle(const Player& player, int direction);
    ActionResult mark(Player& player, const Block& id);
    ActionResult primary(World& world, Player& player);
    ActionResult interact(World& world, Player& player);
    ActionResult place(World& world, Player& player, bool jumping);
    std::optional<ActionResult> step(World& world, Player& player);
    void depleted(Player& player);
    void cancel();
};

}
