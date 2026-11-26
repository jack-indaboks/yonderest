#include "yonderest/space.hpp"

#include <cmath>

namespace yonder {

int wrap(int x, int width) { return ((x % width) + width) % width; }
double wrap(double x, double width) { return x - std::floor(x / width) * width; }
double distance_x(double from, double to, int width) {
    return wrap(to - from + width / 2.0, double(width)) - width / 2.0;
}

}
