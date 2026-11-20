# Yonderest

A native pixel-art sandbox about finding places and making homes in them. This build is a step toward the first playable release, not the full game in [DESIGN.md](DESIGN.md); the build sequence is in [ROADMAP.md](ROADMAP.md).

## Build and run on Linux

You need a C++20 compiler, CMake 3.20 or newer, SDL3 3.4 or newer and SQLite3, with their development headers and CMake package files. The build downloads nothing. On Arch Linux: `base-devel cmake sdl3 sqlite`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/yonderest
```

The build mirrors `content/` beside the executable, deletions included. For a separate world:

```sh
./build/yonderest --save ./expedition.sqlite3 --seed 42
```

Run the same command again to resume. `--seed` only applies to a new save. `--help` lists the options. `cmake --install build --prefix DIR` makes a self-contained directory that still uses the system SDL3 and SQLite.

## Play

Esc opens the menu: Resume, Controls, Save, Fullscreen, Save and quit. The recipe book (R) and backpack (I) rest in the lower-right corner; clicking them opens them too. The mouse never acts on the world.

| Key | Action |
| --- | --- |
| A / D | Walk; steps up are jumped |
| Space | Jump |
| Semicolon, held | Sprint |
| W | Face the rear wall, ending a crawl; letting go while A or D is held turns that way |
| S, held | Crawl and aim beneath your feet; from the rear wall, turn back to the side you last faced |
| J | Hit or gather; hold to repeat |
| K | Use; a workbench cycles its batch size 1, 5, 10 |
| L | Place the held object, once per press |
| H | Held object, then empty hand, then the chooser, then equip |
| J / K, chooser open | Previous / next readied object; walking continues |
| I | Inventory |
| R | Open or close the recipe book; Left / Right turn its pages |
| P | Pause |
| Esc | Close the chooser or an open dialog; otherwise the menu |

You start with an empty hand and nothing carried. You act on the block directly in front of you, behind you after W, or beneath you while S is held. There is no cursor and no remote reach. Blocks can be placed in midair. For scaffolding, hold S, then press Space and L: an early L waits until there is room beneath you.

**Inventory.** Arrows or WASD move the highlight; Down from the last row reaches the Hold, Ready and Empty hand buttons. Enter holds the highlighted object, or activates the focused button. K, J and H are shortcuts for the same three. Readied objects (gold corner mark, up to twelve) appear in the H chooser. Only carried objects are listed or can be readied: using up the last of something removes it from the inventory, the chooser and your hand.

**Recipes.** There is no crafting grid. Build the shape shown in the recipe book with blocks you placed; completing it transforms it. A dotted cell must stay empty, and a station drawn beneath the shape must be on the row directly under it. Counts you cannot yet afford show in red. The book lies open in its corner while you play and never crafts anything itself; click a page corner to turn the page, or elsewhere on the book to close it.

**Workbench batching.** Face a workbench and press K to choose N. Each ingredient placed over it consumes N and the result is multiplied by N. A forged batch needs its shape to span both the forge and a neighboring workbench. Unfinished ingredients keep their quantities through saving and are refunded in full when gathered.

**Copper** needs the stone pick, made from three stone over a centered timber. Clay lies in damp low groves, copper in rocky uplands, sand in dry reaches.

## Saves

The default save is `world.sqlite3` in SDL's per-user `yonderest` directory, normally `~/.local/share/yonderest/`. Only one running game may write a save. The world, player, inventory and unfinished assemblies are checkpointed together every five seconds, when the window loses focus, from the menu and on exit. A crash can lose the last few seconds, never split a craft from its ingredients.

Opening a save from an earlier build upgrades it to format 4 after writing a backup beside it: `FILE.before-backing.sqlite3` from format 3, `FILE.before-keyboard.sqlite3` from 2, `FILE.before-expedition.sqlite3` from 1. Backups are never overwritten. Explored chunks are never regenerated or rewritten by an upgrade.

New territory uses the `expedition-v2` generator: regional materials, and natural backing underground. Removing a foreground block underground exposes the backing behind it, which is ordinary material you can gather; removing that leaves an opening to the void. Open sky stays above ground. Territory explored before this build keeps what it had, including empty background, so backing appears only where the world is newly generated. `--keep-generation` keeps a save's stored generator.

Where new territory meets territory from the older shelter generator, materials settle toward the new generator over about two chunks. Each new chunk takes that transition only from its immediate neighbors. This covers material changes on unchanged landform; it is not a general mechanism for future changes to heights, caves or sky islands.

## Content packs

`content/flagship.txt` lists the packs and the terrain pack; `--manifest` and `--content` select others, and `--pack DIR` adds one. First-party and added packs use the same loader. A pack has `module.txt` (identity, version, dependencies), `blocks.txt`, and optionally `recipes.txt`, `terrain.txt` and art.

A block line is a stable ID, an RGB colour, a collision flag and a quoted name, then optional station, tool capability and required-tool tags (`-` leaves one empty):

```text
demo:bench 160 109 64 1 "Bench" workbench
demo:pick 151 162 174 0 "Pick" - pick
demo:ore 101 116 124 1 "Ore" - - pick
```

Art is found by convention in the defining pack: `blocks/NAME.png` (16x16) for the block, and optionally `held/NAME.png` (16x16) for a tool, drawn on the character's canvas exactly as the idle pose holds it. The game moves it to each pose's hand, mirrors it, and turns it a quarter for a swing, so a tool needs no other art. A block without art is drawn in its colour; a carried block is drawn as a small version of its tile. `images/*.png` supplies named images such as the character poses, font and icons; later packs override earlier ones. `character.txt` gives each pose its image, the column over the player's centre, where its hand is and where a carried block rests, and optionally a `bare` image for an empty hand.

A recipe:

```text
recipe demo:amber demo:amber 2 0 0
at 0 0 base:stone
at 1 0 base:sand
requires forge
end
```

The header gives ID, output, output count and the output's offset, which must replace an ingredient. `at x y ID` is an ingredient, `empty x y` a cell that must stay empty, `requires` a station capability on the row beneath. Larger shapes take priority when matching; the recipe book lists recipes in definition order. A missing block definition keeps its saved ID and quantity and behaves like earth until its pack returns.

There is no Lua yet, no scripted generation and no dynamic native loading.

## Verify

```sh
ctest --test-dir build --output-on-failure
```

The tests use temporary saves, never your world. They cover keyboard interaction, the recipe book and dialogs not leaking into the world, save upgrades (emptied entries dropped on load), natural backing and generation boundaries. They do not establish how anything looks or feels. To render a frame without a desktop:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/yonderest \
  --save /tmp/yonderest-render.sqlite3 --seed 42 --frames 3 --screenshot /tmp/yonderest.bmp
```

## Limits

- The character has idle, walking, jumping, falling, crawling and back poses, and a single-frame swing. Crawling is also the pose in midair while S is held. The art is drawn tight to its 16x16 canvas, which will constrain customization.
- The view shows 40 by 22 blocks at any window size, scaled by whole multiples.
- Tool progression is one pick. There are no doors, lighting, fluids, creatures, sound, survival systems or other worlds.
- Recipes, yields and batching are experiments awaiting playtest. New construction can still accidentally complete a recipe.
- Loading and saving are synchronous. Errors print to the launching terminal. Only Linux is built and tested.

## License

Code, content and artwork in this repository are [MIT licensed](LICENSE). SDL3 (zlib license) and SQLite (public domain) are external dependencies; nothing is vendored.
