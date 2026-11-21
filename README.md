# Yonderest

A native pixel-art sandbox about finding places and making homes in them. This build is a step toward the first playable release, not the full game in [DESIGN.md](DESIGN.md); the build sequence is in [ROADMAP.md](ROADMAP.md).

## Build and run on Linux

You need a C++20 compiler, CMake 3.20 or newer, pkg-config, SDL3 3.4 or newer and JsonCpp, with their development headers. The build downloads nothing. On Arch Linux: `base-devel cmake sdl3 jsoncpp`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/yonderest
```

The build mirrors `content/` beside the executable, deletions included. For a separate world:

```sh
./build/yonderest --save saves/expedition --seed 42
```

Run the same command again to resume. `--save` takes a folder and creates it for a new world; `--seed` only applies to a new world. `--help` lists the options. `cmake --install build --prefix DIR` makes a self-contained directory that still uses the system SDL3 and JsonCpp.

## Play

Esc opens the menu: Resume, Controls, Fullscreen and Quit. Quitting always saves. The recipe book (R) and backpack (I) rest in the lower-right corner; clicking them opens them too. The mouse never acts on the world.

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

A world is a folder of plain JSON files: `world.json` (seed, size and terrain generation), `player.json` (position, facing, hand, inventory and readied objects) and `chunks/x5-y3.json` for each 32x32 region that has been generated. The default world is `world` in SDL's per-user `yonderest` directory, normally `~/.local/share/yonderest/world/`. Only one running game may open a world.

Taking and placing change only memory. A changed chunk is written when it leaves memory, and everything changed is written when the window loses focus, on quitting, and once a minute if anything changed, which shows a small "Saved". Each file is replaced whole, so a crash can lose up to the last minute, and the player's file and a chunk's can disagree about a craft made in that minute.

The files can be read and edited by hand. A chunk names each distinct cell once in its `palette`, then gives its `foreground` and `background` as 32 rows of palette positions, top to bottom; `null` in the palette is empty space. A palette entry has a `block` ID, and `quantity`, `placed` and `batch` only where they differ from 1, false and 1. An absent layer is empty, an absent `settling` is fully settled, and in `player.json` an absent `facing` is right and an absent `held` is an empty hand. Properties the game does not know are ignored on reading, but not kept when it rewrites that file. A chunk file that exists is saved territory, even if it is all sky; deleting one lets that territory generate again.

Earlier builds saved to a single SQLite file. This build cannot open those.

New territory uses the `expedition-v3` generator: regional materials, natural backing underground, and tree trunks in the background behind their canopies, so the character can walk past them. Removing a foreground block underground exposes the backing behind it, which is ordinary material you can gather; removing that leaves an opening to the void. Open sky stays above ground. Opening a world saved by an earlier generator changes only its new territory: saved chunks keep what they had, including empty background and trunks in front. `--keep-generation` keeps a world's stored generator.

Where new territory meets territory from the older shelter generator, materials settle toward the new generator over about two chunks. Each new chunk takes that transition only from its immediate neighbors. This covers material changes on unchanged landform; it is not a general mechanism for future changes to heights, caves or sky islands.

## Content packs

`content/flagship.json` lists the packs, in load order, and the terrain pack; `--manifest` and `--content` select others, and `--pack DIR` adds one. First-party and added packs use the same loader. A pack has `module.json` (identity, version, dependencies), `blocks.json`, and optionally `recipes.json`, `terrain.json`, `character.json` and art. Files are plain JSON: no comments or trailing commas.

```json
{"id": "demo", "version": 1, "requires": {"base": 1}}
```

`blocks.json` lists blocks by stable ID. `solid` defaults to true; `station`, `tool` (a capability the block gives when held) and `requires_tool` are optional:

```json
[
  {"id": "demo:bench", "name": "Bench", "color": [160, 109, 64], "station": "workbench"},
  {"id": "demo:pick", "name": "Pick", "color": [151, 162, 174], "solid": false, "tool": "pick"},
  {"id": "demo:ore", "name": "Ore", "color": [101, 116, 124], "requires_tool": "pick"}
]
```

Art is found by convention in the defining pack: `blocks/NAME.png` (16x16) for the block, and optionally `held/NAME.png` (16x16) for a tool, drawn on the character's canvas exactly as the idle pose holds it. The game moves it to each pose's hand, mirrors it, and turns it a quarter for a swing, so a tool needs no other art. A block without art is drawn in its colour; a carried block is drawn as a small version of its tile. `images/*.png` supplies named images such as the character poses, font and icons; later packs override earlier ones. `character.json` gives each pose its image, the column over the player's centre (`anchor`), where its `hand` is, optionally where a carried `block` rests, and optionally a `bare` image for an empty hand.

A recipe's `shape` is rows of cells, top to bottom: a block ID is an ingredient and `null` must stay empty. The `output` replaces the ingredient at its `x` and `y`; `count` defaults to 1. `stations` are capabilities required on the row beneath:

```json
[
  {
    "id": "demo:amber",
    "output": {"block": "demo:amber", "count": 2, "x": 0, "y": 0},
    "shape": [["base:stone", "base:sand"]],
    "stations": ["forge"]
  }
]
```

Larger shapes take priority when matching; the recipe book lists recipes in definition order. A missing block definition keeps its saved ID and quantity and behaves like earth until its pack returns.

There is no Lua yet, no scripted generation and no dynamic native loading.

## Verify

```sh
ctest --test-dir build --output-on-failure
```

The tests use temporary saves, never your world. They cover keyboard interaction, the recipe book and dialogs not leaking into the world, crafting yields, saving and reading worlds (absent and unfamiliar properties, hand edits, no rewriting on load), trunks and natural backing in new territory only, and generation boundaries. They do not establish how anything looks or feels. To render a frame without a desktop:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/yonderest \
  --save /tmp/yonderest-render --seed 42 --frames 3 --screenshot /tmp/yonderest.bmp
```

## Limits

- The character has idle, walking, jumping, falling, crawling and back poses, and a single-frame swing. Crawling is also the pose in midair while S is held. The art is drawn tight to its 16x16 canvas, which will constrain customization.
- The view shows 40 by 22 blocks at any window size, scaled by whole multiples.
- Tool progression is one pick. There are no doors, lighting, fluids, creatures, sound, survival systems or other worlds.
- Recipes, yields and batching are experiments awaiting playtest. New construction can still accidentally complete a recipe.
- Loading and saving are synchronous. Errors print to the launching terminal. Only Linux is built and tested.

## License

Code, content and artwork in this repository are [MIT licensed](LICENSE). SDL3 (zlib license) and JsonCpp (MIT license) are external dependencies; nothing is vendored.
