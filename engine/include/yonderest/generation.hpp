#pragma once

// The generation interface: populating the world.

#include "yonderest/world.hpp"

namespace yonder {

// A new world's first chunk, in column zero with ground in it, and the cell on that ground
// where the character starts.
struct Beginning {
    int row;
    Chunk chunk;
    int x, y;
};

class Generation {
public:
    virtual ~Generation() = default;
    // Gives a new world what the generator keeps in its settings.
    virtual void create(Settings& settings) = 0;
    // Takes up the world to generate, before any of its chunks.
    virtual void open(const Settings& settings) = 0;
    // The first chunk of a new world, chosen by the generator's own means.
    virtual Beginning begin() = 0;
    // Makes a new chunk alongside play, from its place, the world and those of its immediate
    // neighbors that already exist.
    virtual Chunk generate(int cx, int cy, const Neighborhood& near) = 0;
};

}
