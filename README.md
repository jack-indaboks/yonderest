# Yonderest

A fantasy sandbox platformer about exploring, building, and making places your own.

This is a very early prototype: one vast world, one character, a handful of materials and recipes, and no goal beyond building what you like. Things will change between versions, and a world saved now may not open in a later one. Where the game is headed is in [DESIGN.md](DESIGN.md) and [ROADMAP.md](ROADMAP.md).

## Build and run

Yonderest is tested on Linux only for now.

You need a C++20 compiler, CMake 3.20 or newer, pkg-config, SDL3 3.4 or newer and JsonCpp, with their development headers. On Arch Linux: `base-devel cmake sdl3 jsoncpp`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/yonderest
```

That opens your save, creating it the first time. `--help` lists the command-line options. If the game does not start, the reason is printed in the terminal.

## Playing

WASD move you and move through menus. A and D walk and S crouches. W turns you toward the background, for example to cut down a tree. Hold W or S to reach above your head or beneath your feet, respectively. Space jumps and semicolon sprints.

Your right hand does the work:

- **H**, hand: open the picker of what you carry, take something in hand, or put it away.
- **J** gathers, or breaks with a tool.
- **K** uses.
- **L** places what you hold.

Recipes are shapes: build one in the world and it becomes what it makes. R opens the recipe book, I your inventory, P pauses, and Esc opens the menu.

## Saving the world

Each save is a folder in `yonderest/saves/` inside your user data folder: `$XDG_DATA_HOME` if it is set, otherwise `~/.local/share`. The game opens the save called `first` unless you name another with `--save NAME`, and a new save's world is over a quarter of a million blocks around unless you choose its size with `--size`. A save holds you and your world as plain JSON files you can read and edit. It is saved when you quit, when the window loses focus, and once a minute while you play.

Worlds from version 0.0.6 and earlier do not open in this version.

## License

Yonderest is [MIT licensed](LICENSE). Current art made with AI.
