#pragma once

// What the inventory offers other modules: what the player carries.

#include "yonderest/player.hpp"

#include <cstdint>

namespace yonder::inventory {

// How many of a block the player carries.
std::int64_t count(const Player& player, const Block& block);
// Adds to what the player carries. A newly carried kind is readied while fewer than twelve are.
void carry(Player& player, const Block& block, std::int64_t count);

}
