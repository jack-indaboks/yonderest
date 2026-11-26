#pragma once

// The shape of space: a horizontally wrapping, finite-height grid of cells in chunks and regions.

#include <string>

namespace yonder {

// A chunk is the same in every world, so anything built to it, such as a hand-made chunk,
// fits everywhere. A region of chunks groups their files.
constexpr int chunk_size = 64;
constexpr int chunk_area = chunk_size * chunk_size;
constexpr int region_size = 64;
// World sizes, in chunks.
constexpr int max_columns = 65536, max_rows = region_size;
constexpr int starting_columns = 4096, starting_rows = 16;
constexpr double tick_seconds = 1.0 / 120;

// A block's stable identity, such as "base:stone". Empty is empty space.
using Block = std::string;

// A layer of the cell, by its place in the content's declaration: 0 is the front, 1 the next back.
using Layer = int;

int wrap(int x, int width);
double wrap(double x, double width);
// The shortest signed distance from one column to another around the world.
double distance_x(double from, double to, int width);

}
