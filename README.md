# Yonderest

An early native 2D fantasy sandbox about exploring, building, and making places your own.

This is a very early prototype. It has a small test world, one character, a handful of materials and recipes, and no goal beyond building what you like. Everything may change between versions, and a world saved now may not open in a later one. Where the game is headed is described in [DESIGN.md](DESIGN.md), and the order of what comes next in [ROADMAP.md](ROADMAP.md).

## What it is

A side-view world of blocks that wraps around east to west. Your character is one block tall and works with their own hands: they gather what is directly in front of them, below them, above them or on the wall behind them, and build there, without a cursor or remote reach. Blocks can hang in midair, and a rear wall behind the foreground lets you build the inside of a home. There is no crafting grid: you craft by building a recipe's shape in the world, and the shape transforms into what it makes. Your world, your position and everything you carry are saved.

## Build and run

Yonderest is built and tested on Linux only for now.

You need a C++20 compiler, CMake 3.20 or newer, pkg-config, SDL3 3.4 or newer and JsonCpp, with their development headers. The build downloads nothing. On Arch Linux: `base-devel cmake sdl3 jsoncpp`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/yonderest
```

That opens your world, creating it the first time. To keep a separate world, give it a folder and, for a new world, a seed:

```sh
./build/yonderest --save saves/another --seed 42
```

Run the same command again to return to it. `--help` lists the other options. `cmake --install build --prefix DIR` makes a self-contained directory that still uses the system SDL3 and JsonCpp, and `ctest --test-dir build` runs the tests, which never touch your worlds.

## Controls

Esc opens the menu: Resume, Controls, Fullscreen and Quit. The recipe book (R) and backpack (I) lie in the lower-right corner, and clicking them opens them too. The mouse never acts on the world.

| Key | Action |
| --- | --- |
| A / D | Walk and turn; a one-block step is climbed by jumping |
| Space | Jump |
| Semicolon, held | Sprint |
| W, tapped | Turn to the rear wall behind you |
| W, held | Reach up to the block above your head; A and D shuffle along the wall without turning |
| S, held | Crawl, and reach down to the block beneath your feet |
| J | Hit or gather; hold to repeat |
| K | Use; a workbench cycles its batch size 1, 5, 10 |
| L | Place what you hold, once per press |
| H | Put away what you hold, then open the ring, then hold its choice |
| J / K, ring open | Previous / next object on the ring; you can keep walking |
| I | Backpack |
| R | Show or hide the recipe book; Left / Right turn its pages |
| P | Pause |
| Esc | Close the ring or an open dialog; otherwise the menu |

Between W and S, the last one pressed decides: W ends a crawl, and S turns you back from the wall to the side you last faced. Letting go of W while A or D is held turns you that way; otherwise you stay facing the wall.

You act on the block directly in front of you, or above, below or behind you as above. J first takes back an object you are standing in, such as a tool you set down. Placing while your body overlaps the next cell by a few pixels puts the block there and steps you back, and a jump that catches a corner by a few pixels slips past it, so you can jump into an opening one block high.

**Scaffolding.** Hold S, then press Space and L: the block goes beneath you, and an early L waits until there is room. Press L near the top of the jump to build two blocks up instead.

**The ring.** Up to twelve objects are readied on the ring that H opens around your character. Anything new you pick up is readied while there is room, and using up the last of something takes it off the ring. In the backpack, arrows or WASD move the highlight, J readies or unreadies the highlighted object, K holds it, and H empties your hand.

**Recipes.** Build the shape shown in the recipe book with blocks you have placed, and it transforms when complete. A dotted cell must stay empty, and a working surface drawn beneath the shape must be on the row directly under it. Counts you cannot yet afford show in red. The book can stay open while you play; it never crafts anything itself.

**Workbench batching.** Face a workbench and press K to choose N. Each ingredient placed over it consumes N, and the result is multiplied by N. A forged batch needs its shape to span both the forge and a workbench beside it. Unfinished ingredients keep their quantities and are refunded in full when gathered.

**Where things are.** Copper needs the stone pick, made from three stone over a centred timber. Clay lies in damp low groves, copper in rocky uplands, and sand in dry reaches.

## Your world

The default world is kept in `~/.local/share/yonderest/world/`. It is saved when the window loses focus, when you quit, and once a minute if anything changed, which shows a small "Saved". A world is a folder of plain JSON files that can be read and edited by hand.

## Limitations

- The world is a small test world, 1,024 blocks around and 256 high.
- The view always shows 40 by 22 blocks, scaled by whole multiples to fit the window.
- A crash can lose up to the last minute of play, and the files written in that minute can disagree about a craft made in it, duplicating or losing its ingredients.
- Saving happens during play and can cause a brief pause.
- Only one running game can open a world at a time.
- Recipes complete by themselves, so new construction can complete one by accident.
- A held tool is drawn as the standing pose holds it, so it crosses the body in the crawling poses.
- Errors are printed only to the terminal that launched the game.

## License

Code, content and art are [MIT licensed](LICENSE). The art was made with AI. SDL3 (zlib license) and JsonCpp (MIT license) are external dependencies; nothing is vendored.
