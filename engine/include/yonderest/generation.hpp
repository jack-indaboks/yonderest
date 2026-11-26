#pragma once

// The generation interface: populating the world.

#include "yonderest/world.hpp"

namespace yonder {

class Generation {
public:
    virtual ~Generation() = default;
    // Gives a new world what the generator keeps in its settings.
    virtual void create(Settings& settings) = 0;
    // Takes up the world to generate, before any of its chunks.
    virtual void open(const Settings& settings) = 0;
    // The row of the first chunk of a new world, in column zero, chosen so that it holds ground
    // with open space above.
    virtual int first_row() = 0;
    // Makes a new chunk alongside play, from its place, the world and those of its immediate
    // neighbors that already exist.
    virtual Chunk generate(int cx, int cy, const Neighborhood& near) = 0;
};

}
