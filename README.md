# Yonderest

A 2D fantasy sandbox about exploring, building, and making places your own.

This is a very early prototype: a small test world, one character, a handful of materials and recipes, and no goal beyond building what you like. Things will change between versions, and a world saved now may not open in a later one. Where the game is headed is in [DESIGN.md](DESIGN.md) and [ROADMAP.md](ROADMAP.md).

## Build and run

Yonderest is tested on Linux only for now.

You need a C++20 compiler, CMake 3.20 or newer, pkg-config, SDL3 3.4 or newer and JsonCpp, with their development headers. On Arch Linux: `base-devel cmake sdl3 jsoncpp`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/yonderest
```

That opens your world, creating it the first time. `--help` lists the command-line options. If the game does not start, the reason is printed in the terminal.

## Playing

WASD move you and move through menus. A and D walk and S crouches. W turns you toward the background, for example to cut down a tree. Hold W or S to reach above your head or beneath your feet, respectively. Space jumps and semicolon sprints.

Your right hand does the work:

- **H**, hand: open the picker of what you carry, take something in hand, or put it away.
- **J** gathers, or breaks with a tool.
- **K** uses.
- **L** places what you hold.

Recipes are shapes: build one in the world and it becomes what it makes. R opens the recipe book, I your inventory, P pauses, and Esc opens the menu.

## Saving the world

Your world is kept in `yonderest/world/` inside your user data folder: `$XDG_DATA_HOME` if it is set, otherwise `~/.local/share`. It is saved when you quit, when the window loses focus, and once a minute while you play.

## License

Yonderest is [MIT licensed](LICENSE). Current art made with AI.
