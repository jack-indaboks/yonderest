#pragma once

// The interaction interface: the player's intent into outcomes. The engine frames each action:
// who acts, the cell in front, the block held, and which action. Modules decide what it means.

#include "yonderest/player.hpp"
#include "yonderest/world.hpp"

#include <optional>
#include <string>

namespace yonder {

// The actions the engine frames. Controls decide what triggers them, modules what they mean.
enum class Act { a, b, c };

// An action, announced before its base meaning is carried out. A layer that gives it another
// meaning, such as stepping through an open ring, claims it.
struct Action {
    Act act;
    Target cell;
    Block held;
    bool claimed = false;
    bool ok = false;
    std::string message{};
    bool repeat = true; // Holding the action's key repeats it, unless a layer says otherwise.
    bool stopped() const { return claimed; }
};

// After the base meaning of an action was carried out.
struct Acted {
    Act act;
};

// Before a block is placed: modules adjust the cell placed and how many blocks it takes,
// move the body clear, or refuse.
struct Placing {
    Player& player;
    int x, y;
    Layer layer;
    Cell cell;
    int count = 1;
    std::optional<std::string> refusal{};
    bool stopped() const { return refusal.has_value(); }
};
// After a block was placed.
struct Placed {
    Player& player;
    Block block;
    int count;
};

// Before a block is taken: how many it gives.
struct Taking {
    Player& player;
    int x, y;
    Layer layer;
    const Cell& cell;
    int count = 1;
};
// As a block is taken, before its cell empties.
struct Taken {
    Player& player;
    Block block;
    int count;
};

// After a cell changed, whoever changed it.
struct Changed {
    int x, y;
    Layer layer;
    const Cell& before;
};

// After the held block was set, even to what it already was.
struct Held {};

// The base meanings of the actions.
class Interaction {
public:
    virtual ~Interaction() = default;
    virtual void act(Action& action) = 0;
};

}
