#include "yonderest/player.hpp"

#include <cmath>

namespace yonder {

Target target(const Player& p, int takeoff) {
    int x = int(std::floor(p.x + player_width / 2));
    int y = int(std::floor(p.y + player_height / 2));
    if (takeoff >= 0 && y == takeoff) --y;
    // A cell the body overlaps by no more than the margin counts as beyond the edge.
    if (p.up) y = int(std::floor(p.y + margin)) - 1;
    else if (p.down) y = int(std::ceil(p.y + player_height - margin));
    else if (p.facing > 0) x = int(std::ceil(p.x + player_width - margin));
    else if (p.facing < 0) x = int(std::floor(p.x + margin)) - 1;
    return {x, y, p.facing == 0 && !p.up ? 1 : 0};
}

}
