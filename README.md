# Yonderest

A native, pixel-art sandbox about finding places and making homes in them. This is the **persistent first shelter** prototype, not the full game described in [DESIGN.md](DESIGN.md). The build sequence is in [ROADMAP.md](ROADMAP.md).

Walk a small wrapping world, gather its blocks, build a shelter, quit, and return to it.

## Build and run on Linux

You need a C++20 compiler, CMake 3.20 or newer, a build tool such as Make, SDL3 3.2 or newer, and SQLite3, including their development headers and CMake package files. Dependencies come from your system; the build downloads nothing. On Arch Linux, the packages are `base-devel cmake sdl3 sqlite`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/yonderest
```

The executable finds `content/` beside itself, not in the working directory. For a separate, reproducible world:

```sh
./build/yonderest --save ./shelter.sqlite3 --seed 42
```

Run that command again to resume. `--seed` only initializes a new save; an existing save keeps its own seed. Without `--seed`, a new world gets a seed from the clock. The seed and save path are printed at startup. `--help` lists the command-line options.

Optionally install a self-contained directory, still using the system SDL3 and SQLite libraries:

```sh
cmake --install build --prefix /your/path/yonderest
/your/path/yonderest/yonderest
```

## Play

| Input | Action |
| --- | --- |
| A/D or Left/Right | Walk; climb one-block steps automatically |
| Space, W or Up | Jump |
| Left mouse, held | Gather the targeted block |
| Right mouse, held | Place the selected material |
| 1-9 or mouse wheel | Select a material; wheel reaches additional hotbar pages |
| F1 | Help and pause |
| F5 | Save now |
| F11 | Toggle fullscreen |
| Esc or close window | Save and quit |

You begin empty-handed. Mossy ground, earth, stone, sand, timber, and leaves can all be gathered and placed. Blocks remain separate inventory materials; nothing needs crafting yet. The hotbar shows counts. Aiming is limited to five blocks and cannot reach through solid terrain. Place against an existing solid block, not inside yourself. The target outline indicates distance; the bottom message explains a refused action.

Find a clearing, gather some blocks nearby, and build a wall and roof. Leave a two-block-high opening to walk through. Jump or move closer when a wall hides the next roof position. East and west join continuously, including for collision and building. There is no fall damage, combat, or survival pressure in this slice.

## Saves and world size

The default save is `world.sqlite3` in SDL's per-user `yonderest` data directory, normally `$XDG_DATA_HOME/yonderest/` or `~/.local/share/yonderest/` on Linux. `--save FILE` selects another world. To back one up, quit and copy that file. Only one running game may write a given save; a second writer fails rather than sharing control.

This prototype uses 32x32-block chunks, 32 chunk columns and 8 chunk rows: **1,024 blocks around, 256 blocks high**. The vertical boundaries are solid and do not wrap. Ground height, humidity, trees and caves are horizontally periodic. Damp ground supports groves; drier reaches are sandy. These are simple environmental relationships, not a finished biome system.

Only chunks intersecting the view and a margin of six blocks horizontally and one vertically are retained. Physics and interactions can also request chunks. Each request generates or loads that single two-dimensional chunk, never the column above or below it. Generating a chunk claims its entire area as explored territory, including unseen cells at the edge of the view.

SQLite holds full chunk snapshots, world parameters, player position, selected material and inventory. Chunk snapshots use runs of stable material IDs, including empty space, not a seed plus edits. Generated and modified chunks are staged when evicted. A checkpoint commits them together with the player and inventory. Checkpoints happen every five seconds, on focus loss, on F5, and on normal exit. A crash can lose work since the last checkpoint, but cannot commit mined terrain without its matching inventory. Invalid saved chunks produce an error, not regenerated replacements.

Save format 1 has no migration support yet. The stored generation profile is `shelter-v1`; unknown profiles are refused. Saved chunks override the generator, but **transitions between different generator versions are not implemented**. Do not replace this generator in place and expect new terrain to blend with old territory.

## Content and code

There are three small parts:

- `world.hpp` / `world.cpp`: materials, seeded terrain, chunk residency, movement and block actions. No SDL types or calls.
- `save.hpp` / `save.cpp`: SQLite ownership and complete-state serialization.
- `app.cpp`: SDL input, camera, drawing and the desktop loop. `main.cpp` reports errors.

The base pack goes through the same `Content::load` entry point as added packs. To add a block, create a directory with a `blocks.txt` file:

```text
# ID           R   G   B  Solid  Name
demo:amber   220 146  52    1    "Amber"
```

```sh
./build/yonderest --pack /path/to/that/directory
```

Each non-comment line is a stable namespaced ID, three color channels (0-255), collision flag (0 or 1), and quoted display name. Duplicate IDs are errors. Added definitions appear in the hotbar but do not grant items or automatically add deposits to terrain. `content/base/terrain.txt` binds the existing native generator's material roles for new worlds. Those bindings are saved with the world. `--content DIR` selects a different content root containing `base/`.

An unavailable block definition uses an ordinary solid, mineable earth-colored fallback without changing its saved ID. Mining retains that ID in inventory. Restoring the pack restores interpretation, not blocks deliberately removed in its absence. Blocks are string IDs throughout; incidental hotbar or container order does not define their saved meaning.

This is a small shared content path, not a complete module system. Lua, manifests, dependencies, general generation definitions, and a stable scripting API are not implemented. Native changes remain ordinary C++ changes. A terminal skin could reuse the world and play code, but this build has only the graphical front end.

## Verify

```sh
ctest --test-dir build --output-on-failure
```

The world exercise gathers from a seeded natural clearing, constructs a shelter through the ordinary gameplay operations, evicts its chunks, and reloads it. It also exercises seam interactions, movement and collision, interrupted transactions, an added/removed/restored pack, and refusal of a damaged chunk. The desktop exercise feeds real SDL keyboard and mouse events through the application loop under SDL's dummy video driver, renders, and checks the resulting save. Tests use temporary saves, never your default world. They currently require Linux/POSIX facilities.

To render the actual executable without a desktop and capture a BMP:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/yonderest \
  --save /tmp/yonderest-render.sqlite3 --seed 42 --frames 3 \
  --screenshot /tmp/yonderest.bmp
```

Headless checks establish behavior and rendering, not the feel of physical input, fullscreen, or a particular display/GPU. A desktop playthrough remains the practical review.

## Limits and next step

There are no background walls, doors, lighting, fluids, crafting, creatures, sound, portals, multiple worlds, procedural material families, or finished artwork. A shelter is a persistent foreground construction, not a recognized object or a survival mechanic. Loading and saving are synchronous. Error messages go to the launching terminal. This is Linux-first, not tested or packaged on the other intended platforms.

Next: play this loop on a desktop, especially mining aim, roof construction and the round trip across the seam. Use that experience to settle movement/building feel before expanding into milestone 2's resource-driven exploration and crafting. Generation-update boundary continuity needs its own concrete implementation before shipping changed generators into existing worlds.

## License and provenance

Code, documentation, original base material data, and code-drawn placeholder visuals in this repository are [MIT licensed](LICENSE). The placeholders were authored by Archer with AI coding assistance: geometric SDL drawings and palette-based tile details, not imported artwork or image-model outputs. The UI uses SDL's built-in debug font. SDL3 is an external zlib-licensed dependency; SQLite is an external public-domain dependency. No third-party assets or dependency sources are vendored here.

MIT for these placeholders does not decide the licenses of future flagship or contributed artwork. Such assets will need their own clear licenses and provenance.
