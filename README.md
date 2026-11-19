# Yonderest

A native, pixel-art sandbox about finding places and making homes in them. This is the **keyboard homebuilding prototype**, a bounded step toward milestone 2, not the full game in [DESIGN.md](DESIGN.md). The build sequence is in [ROADMAP.md](ROADMAP.md).

Find a place, make a home with a real rear wall, explore for useful materials, and bring them back to a physical workshop. The first shelter remains yours when the game grows.

## Build and run on Linux

You need a C++20 compiler, CMake 3.20 or newer, a build tool such as Make, SDL3 3.2 or newer, and SQLite3, including their development headers and CMake package files. Dependencies come from your system; the build downloads nothing. On Arch Linux, the packages are `base-devel cmake sdl3 sqlite`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/yonderest
```

The executable finds `content/` beside itself, not in the working directory. For a separate, reproducible world:

```sh
./build/yonderest --save ./expedition.sqlite3 --seed 42
```

Run the same command again to resume. `--seed` only initializes a new save; an existing save keeps its own seed. Without `--seed`, a new world gets a seed from the clock. The seed and save path are printed at startup. `--help` lists all options.

To install a self-contained directory, still using the system SDL3 and SQLite libraries:

```sh
cmake --install build --prefix /your/path/yonderest
/your/path/yonderest/yonderest
```

## Play

| Input | Action |
| --- | --- |
| A/D | Walk; climb one-block steps automatically |
| Space | Jump (the only jump key) |
| W | Turn toward the background; no need to hold it |
| S, held | Crouch/crawl and aim beneath the feet |
| Semicolon, held | Sprint; the jump impulse stays unchanged |
| J, held | Hit/gather with the empty hand or a held breaking tool |
| K | Interact; a faced workbench cycles batch 1, 5, 10 |
| L | Place the held object; one placement per press |
| H | Held object -> empty hand -> radial chooser -> equip |
| J/K, while choosing | Previous/next available readied object, not a world action |
| I | Open/close inventory management (pauses movement) |
| F1 / F2 | Help / recipe plans; both pause |
| P or Escape | Pause and offer quit; Escape closes an open panel first |
| Q, while paused | Save and quit; P, Enter or Escape resumes |
| F5 / F11 | Save now / toggle fullscreen |

You begin with an empty hand and no free materials. Forward interaction addresses the first whole cell beyond the facing edge of the one-block-tall character, including when their body straddles a tile boundary. W turns away to address the background behind the character; a lateral turn faces the foreground again. S changes aim to the cell below without changing facing. W followed by held S therefore addresses the background below. No cursor, mouse operation, number-row hotbar or remote reach is used.

J gathers hand-gatherable blocks with empty hand. A held building material does not inherit mining. K interacts instead of picking up the addressed block. This pass has one small tool trial: **copper ore requires the stone pick**, whose primary J action breaks/gathers. Other current materials remain hand-gatherable. L sets down the pick itself as a nonblocking, pick-shaped block; it can be walked past and recovered with empty-hand J. This is not a complete tool progression.

### Inventory and quick hands

I opens the one inventory. WASD or arrows move the highlight, J marks/unmarks an entry as readied (a gold underline), K equips the highlighted carried object and closes, and H closes to empty hand. Readying never moves quantities into another container. Entries are in stable material-ID order; the wheel follows readiness-mark order. There is no sorting or transfer workflow yet.

H advances immediately on each distinct press; there is no double-tap timer. From a held object, H empties the hand, H again opens the radial chooser, and H again equips. From empty hand, two presses restore the previous object if it is still readied and available. J/K cycle only while the chooser is open, and WASD movement remains available. A held selection key does not turn into a tool action when the chooser closes.

Zero to twelve objects can be marked. An empty chooser displays a crossed-out selector icon, not text. Depletion returns to empty hand. Depleted marked entries stay in their inventory positions but are omitted from the wheel until replenished; an unavailable last object defaults to the first available readied entry. With none available, H closes to empty hand.

### Build a home and rear wall

L places directly at the addressed cell. Empty space and physical access are enough; **blocks can be placed in midair without touching another block**. Solid foreground blocks cannot be placed inside the character. Rear walls occupy a separate layer, are darker, never block movement and never participate in spatial crafting. Each rear placement consumes one block, regardless of foreground bench batching. W turns toward that layer for either placing or gathering. Existing trees remain in their saved/generated foreground positions in this pass.

For scaffolding, hold S before jumping, then press Space and L. An early L press waits until the character clears the intended cell and places once without another press. During flight, placement uses the first fully clear cell beneath the feet. Near the end of descent there must still be a whole empty cell below; the game does not lift the character or place a solid block through their body. Releasing S, leaving the column, changing the held object, landing without clearance, or opening a panel cancels a waiting request without spending material. Holding L does not build repeatedly.

Without S, airborne foreground/rear placement uses the addressed cell at the instant of the press, not a target selected later in the jump. After forward placement, facing stays with the work while walking backward, even with L released. H, equipping an object, J/K world use or W releases that construction facing. This latch is a prototype choice for the backing-away trial, not a second cursor or additional reach.

Surface excavation now opens onto the scenery instead of leaving an opaque silhouette of the old terrain. Depth darkens the overall backdrop; constructed rear walls remain explicit world state. This is placeholder presentation, not lighting simulation.

### A first expedition

1. Gather timber and stone. Assemble timber into planks, then a workbench. Assemble stone and timber into a forge. F2 shows the actual shapes loaded from the content packs. A stone pick uses three stone blocks above a centered timber block; it needs no station and unlocks gathering copper ore.
2. Find a home site and put the forge and workbench beside each other. Leave clear space above them and somewhere to stand alongside the assembly.
3. Seek sandy reaches for sand, damp low groves for clay, and rocky uplands for copper ore. Ore and clay also occur below their corresponding regions. Resources are related to elevation and humidity, not scattered uniformly.
4. Return with materials. Assemble them over the forge to make framed glass, bricks, or copper tiles. Glass has ordinary solid collision but a transparent-looking center, so you can make windows. These are building choices, not a combat progression.
5. Try batching at the same workshop, improve the home, then quit and reopen it.

Existing homes, workshops and routes remain useful. The region label marks old-generation chunks with `(old)` where appropriate; a new region name does not repaint explored land. In an old save, travel into previously ungenerated chunks for new deposits. Already-explored surface chunks may extend beyond where you personally walked.

### Craft by building

There is no ingredient-grid crafting interface. Build the configuration in the world; completing it transforms it automatically. F2 is a reference, not a button that performs crafting. Move alongside the shape and jump or scaffold when a wall obstructs access. Ingredients may hang in midair; recipes can still explicitly require a station underneath.

Some starter patterns, with `T` = timber, `P` = planks, `S` = stone, and `.` = empty:

```text
Planks x4       Workbench x1       Forge x1
TT              PPP                SSS
TT              P.P                S.S
                                   STS
```

Fired building materials use two matching resource blocks above one timber fuel block:

```text
clay  clay       sand  sand       ore   ore
wood  empty      wood  empty      wood  empty
```

These require a forge on the row immediately underneath the shape and yield four bricks, glass blocks, or copper tiles. The forge is not consumed. The initial patterns and yields are experimental, not final balance.

In this prototype, ingredients must be blocks placed by the player. Generated terrain and blocks from the old shelter save are not silently reclassified as ingredients. Mine and replace old blocks if you intentionally want to use them in an assembly. A completed recipe places one result at its designated ingredient position and puts the remaining output in inventory. These two behaviors are provisional implementation choices for playtesting.

Recipes can still match ordinary **new** construction: for example, a 2x2 timber wall makes planks. There is no separate build/craft mode or confirmation step. This is part of the automatic-crafting experiment, not a promise that accidental matches are solved. One transformation is performed per player block action; outputs are not recursively auto-crafted into further recipes in that action.

### Workbench batching

Face a workbench and press K to choose N. Hold S if it is beneath your feet. Build a recipe over it: each ingredient placement consumes N units, represented by one visible block marked with its quantity. Finishing yields the recipe's normal output multiplied by N. This reduces repetition; it does not grant a material discount.

For a forged batch, the ingredient shape must span **both** the forge and its neighboring workbench. A bench nearby but outside the shape does not contribute. Both stations remain afterward.

An existing partial assembly keeps its original N when you change the bench setting. Dismantling an ingredient refunds its full represented quantity. Quantities, placed-ingredient status, workbench settings, and unfinished patterns survive eviction and restart. If different partial patterns or benches imply incompatible quantities, placement refuses rather than choosing an inventory cost silently. Use a clear working area with one bench under a shape.

A workbench's setting belongs to its placed world state. Picking it up returns the ordinary workbench item; placing that item starts at N = 1.

## Saves and generation updates

The default save is `world.sqlite3` in SDL's per-user `yonderest` data directory, normally `$XDG_DATA_HOME/yonderest/` or `~/.local/share/yonderest/` on Linux. `--save FILE` selects another world. Only one running game may write a save. To back up a current world manually, quit and copy the file.

The world has 32x32-block chunks, 32 columns and 8 rows: **1,024 blocks around, 256 high**. Horizontal travel and all resource fields wrap; solid vertical boundaries do not. Only requested two-dimensional chunks are generated. The view retains a six-block horizontal margin and a one-block vertical margin; physics, assembly matching and interactions can request chunks too. Requesting any part claims that whole chunk as generated territory.

SQLite stores full chunk snapshots, not a seed plus edits. World parameters, stable material IDs, cell quantities, station settings, player position and inventory share atomic checkpoints. Chunks staged during eviction commit with the matching player state every five seconds, on focus loss, on F5, and on normal exit. A crash can lose work since the last checkpoint, not commit ingredients without their matching inventory or craft results. Damaged chunks produce an error, never regenerated replacements.

### Opening an earlier save

The first opening upgrades world metadata to save format 3. **Before committing an expedition-format (2) upgrade, the game creates `FILE.before-keyboard.sqlite3` beside the original save.** Opening an original shelter-format (1) save instead uses `FILE.before-expedition.sqlite3`. These are consistent SQLite backups of the previous committed save, published only after the copy completes and never overwritten. Earlier executables can open a copy of the corresponding backup, not the upgraded world.

Existing chunk payloads remain unchanged until edited. Format-1 and format-2 cells read as foreground, with an empty background layer. New/edited chunks store both layers, complete cell quantities, placed-ingredient flags and station settings in format 3. Readiness marks, last held object, facing and crouch posture checkpoint with the inventory. Shrinking the old 1.7-block character preserves their foot position by moving the sprite's top down 0.7 blocks. An old selected object becomes readied if carried stock exists; otherwise the hand starts empty. No existing construction is regenerated or moved.

By default, new territory uses `expedition-v1`. To try crafting without accepting that generation update yet:

```sh
./build/yonderest --save ./shelter.sqlite3 --keep-generation
```

This still upgrades the save format, with the appropriate backup. On a new world the flag selects `shelter-v1`; on an existing world it retains the stored profile. It does not downgrade an already-upgraded generation profile. New clay and copper deposits need expedition generation, while the old materials still support planks, stations and glass.

### What continuity currently means

This update deliberately preserves the previous height, cave and tree geometry. It adds resource-bearing regions through material changes, fading them in over 64 blocks from the fixed footprint of old-generation chunks. Distances wrap horizontally and include vertical neighbors. After two chunks, new material fields reach their ordinary seeded result. New exploration order does not change this transition.

This is a concrete, limited generation-update mechanism, **not a universal solution for future height, cave or sky-island changes**. Those changes will need boundary information appropriate to their geometry before they ship. Neither old nor new explored chunks are implicitly regenerated.

## Content and code

The world/play code has no SDL types or calls. Its concrete parts are:

- `content.cpp`: material, recipe, module and manifest loading.
- `terrain.cpp`: seeded landform, regional resources and the first update's transition.
- `world.cpp`: chunk residency, both layers, movement and coordinate block operations.
- `play.cpp`: direct keyboard interaction, hand transitions, readiness and buffered jump-building.
- `craft.cpp`: physical pattern matching, retained supports and quantity-aware assembly.
- `save.cpp`: SQLite ownership and full-state serialization.
- `app.cpp`: SDL input, camera, placeholder drawings and recipe reference. `main.cpp` reports errors.

First-party and added packs use the same `Content::load` entry point. The versioned flagship manifest is `content/flagship.txt`:

```text
yonderest 2
pack base 1
pack expedition 1
pack workshop 1
terrain expedition
```

`--manifest FILE` selects another configuration; `--content DIR` changes the content root. Packs declare their identities, versions and minimum dependencies in `module.txt`; absent dependencies and version mismatches are errors. The manifest's `terrain` line selects a pack's `terrain.txt` for new worlds or the supported shelter-to-expedition update. Saved worlds keep their stored material bindings. `--pack DIR` adds another pack; the original blocks-only pack format remains supported.

A block line gives a stable ID, three color channels, collision flag, quoted name, then optional appearance, station, breaking-tool capability and required-tool tags. IDs must be unique across the loaded packs. Use `-` to leave an intermediate tag empty:

```text
demo:amber 220 146 52 1 "Amber"
demo:bench 160 109 64 1 "Bench" bench workbench
demo:pick 151 162 174 0 "Pick" pick - pick
demo:ore 101 116 124 1 "Ore" ore - - pick
```

Supported appearances include `grain` (default), `clay`, `ore`, `planks`, `bricks`, `glass`, `metal`, `bench`, `forge` and `pick`. Unknown appearances use the basic grain drawing. The shared `tool` tag supplies a breaking capability, and `required-tool` asks for that capability when gathering. Appearance, collision and capabilities are separate fields; a pack can define another working surface with the same `forge` or `workbench` capability. Blocks are never identified by palette or inventory order.

An optional `recipes.txt` defines physical configurations:

```text
recipe demo:amber demo:amber 2 0 0
at 0 0 base:stone
at 1 0 base:sand
end
```

The header gives recipe ID, output ID, normal output count, and output offset. `at x y ID` marks a consumed ingredient; `empty x y` requires empty space. Coordinates run right and down; the footprint must fit the world. `requires forge` requires that station capability somewhere on the row immediately under the footprint. The output position must replace an ingredient. Workbench batching is a shared operation, not a second recipe. Larger patterns have priority, with stable recipe-ID order breaking ties, independent of pack load order. The flagship patterns avoid conflicting completed recipes.

Added definitions do not grant free items or automatically create terrain deposits. A missing block definition retains its saved ID and quantity and uses solid, mineable earth-like behavior. Restoring its pack restores interpretation, including station settings, not blocks deliberately removed. Removing workshop recipes does not scan or consume existing constructions.

The module configuration and explicit recipes are usable, but Lua, general scripted generation, a stable scripting API, procedural material families and dynamic native loading are not implemented. Generation profiles are native C++ functions with data-bound material roles. A terminal skin could reuse world/play code; this build has only the GUI.

## Verify

```sh
ctest --test-dir build --output-on-failure
```

The current exercises focus on keyboard homebuilding: direct facing, crouch/crawl, sprint, hand selection, tools, rear walls, underfoot placement and save migration. All use temporary saves, never your default world.

These are behavior checks, not visual verification. To inspect the actual selector or scaffold scene:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/desktop-test ./build/content hand /tmp/yonderest-hands.bmp
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/desktop-test ./build/content build /tmp/yonderest-build.bmp
```

Open the resulting images to review the scenes.

To render the executable without a desktop:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/yonderest \
  --save /tmp/yonderest-render.sqlite3 --seed 42 --frames 3 \
  --screenshot /tmp/yonderest.bmp
```

For address/undefined-behavior sanitizer checks, configure a separate debug build with `-DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'` and `-DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'`, then build and run CTest there. Tests currently require Linux/POSIX facilities. Headless behavior checks do not establish visual correctness, physical input feel, fullscreen/display behavior or whether spatial crafting is fun.

## Limits and next step

This is an experiment toward milestone 2, not a completed first release. Recipes, yields and batching interactions need human playtesting. The recipe guide is laid out for the small starter patterns; unusually large or ingredient-heavy extension recipes can outgrow it. There are no doors, climbing, lighting simulation, fluids, creatures, combat, sound, portals, survival hazards or multiple worlds. Background trunks, wider storage and tool progression remain ahead. Rear walls coexist with foreground construction, not a separate interior dimension. Loading and saving are synchronous. Errors go to the launching terminal. Other intended platforms are not tested or packaged.

Fixes for the next build pass:

- Deleted content files remain active in an existing build directory. Content copying must carry deletions as well as additions and edits.
- Generation transitions scan the entire old-territory footprint instead of taking boundary conditions from immediate neighbors.

Next: build and back a small home entirely from the keyboard. Try early S+Space+L presses, floating placement near the jump apex, choosing while walking, backing away from work and running jumps. Judge responsiveness and sustained right-pinky comfort. Revisit the forge/workbench loop with the new reach and controls; automatic foreground recipes can still consume a newly built matching shape. Passing tests do not settle those feel questions.

## License and provenance

Code, documentation, original pack data and code-drawn placeholder visuals in this repository are [MIT licensed](LICENSE). The placeholders were authored by Archer with AI coding assistance: geometric SDL drawings and palette-based details, not imported artwork or image-model outputs. The UI uses SDL's built-in debug font. SDL3 is an external zlib-licensed dependency; SQLite is an external public-domain dependency. No dependency sources or third-party assets are vendored.

MIT for these placeholders does not decide future flagship or contributed artwork licenses. Such assets need their own licenses and provenance.
