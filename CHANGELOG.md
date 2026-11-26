# Changelog

What changed in each tagged version of Yonderest, newest first. What is planned next is in [ROADMAP.md](ROADMAP.md).

## 0.0.8

- Yonderest is now an engine and a collection of modules, each in a folder of its own: the generator, physics, interaction, crafting, the inventory, keyboard controls and the pixel display, with the flagship's blocks, recipes and art as packs. The game plays as before, apart from the two changes below.
- Crafting counts every block standing in a recipe's shape, natural or placed, so a formation in a hillside can complete a recipe by accident.
- Blocks are no longer mirrored at random across the world: every block of a kind is drawn the same way.
- Saves and packs keep each property under the part of the game it belongs to. Worlds from 0.0.7 and earlier no longer open.

## 0.0.7

- Worlds from 0.0.6 and earlier no longer open. They stay where they were, and version 0.0.6 can still open them.
- A new world is vast: 4,096 chunks around, over a quarter of a million blocks, and 1,024 blocks deep. Circling it takes hours.
- Hills, climate and biomes have their own size, so the larger world holds more places rather than stretched ones. Hills rise higher than before.
- Underground goes much deeper. Far down the caves open into larger caverns, and copper continues in veins beneath the uplands.
- The rear layer underground holds deposits of its own, a little richer than the ones in front, found by mining.
- New land is made, loaded and saved alongside play, ahead of you, so walking, sprinting or falling into new land never stalls the game, and neither does saving. The window opens once the land around you is ready.
- A save is a folder in `yonderest/saves`, holding you and your world. Name one with `--save NAME`, or give a folder elsewhere with `--save-path DIR`. `--size AROUNDxTALL` sets a new world's size in chunks, and `-h` works like `--help`. `--keep-generation` is gone.
- Chunks are 64 by 64 blocks in every world, and their files are gathered in a folder per region of 64 by 64 chunks. Each chunk records where the ground lay when it was made.
- Land generated beside a chunk the generator did not make, such as one built by hand, rises or falls to meet it and takes on its materials, fading back to its own within a chunk or two.
- In a jump, the target is one block up from the moment you leave the ground. While the body straddles two rows, J and L act on the other row if the one aimed at cannot take the action.

## 0.0.6 - 2026-11-21

- Hold W to reach up: dig out the block above your head or place one there, standing or in a jump. While W is held, A and D shuffle sideways without turning. A tap of W still turns you toward the background.
- Edges are less picky. A jump that catches a corner by a few pixels slips past it, so you can jump into an opening one block high, and a placement while you overlap the next cell by a few pixels goes there, beneath you in a jump as well as in front.
- J first takes back an object you are standing in, such as a tool you set down.
- Anything new you pick up is added to the picker while there is room.
- The picker fades every object but the selected one.
- A completed recipe dissolves into light and resolves into what it makes, instead of swapping at once.
- New poses: a shuffle while facing the background, an upward strike seen from behind, and a leapfrog over the block you are carrying when in midair with S held. Character packs can draw them as `back-2`, `strike-up` and `leapfrog`.

## 0.0.5 - 2026-11-20

- Worlds are folders of plain JSON files that can be read and edited by hand. Saves from earlier versions cannot be opened.
- Content packs are JSON: blocks, recipes, terrain, character poses, modules and the flagship manifest.
- The game saves when the window loses focus, when quitting, and once a minute if anything changed, with a brief "Saved".
- Properties the game does not recognize in a save are kept when it rewrites the file.
- Tree trunks in newly generated land stand in the background, behind their canopies.
- Blocks from a missing pack are drawn as unknown by their identity, not by their name.
- Fixed: crafting with nothing left over no longer adds an empty entry to the inventory; placing no longer leaves the character stuck facing one way.

## 0.0.4 - 2026-11-20

- Art on a 16-pixel grid: a character with walking, jumping, falling, crawling, back and striking poses, held tools in hand and carried blocks shown.
- A recipe book (R) and backpack (I) in the corner of the screen, usable with the keyboard or mouse. The recipe book can stay open while building.
- A menu (Esc) replaces all function-key bindings.
- Quantities in the chooser and inventory, and save feedback separate from pause.
- Falling is less floaty, with the same jump height.
- Newly generated underground land has natural rear-layer material, matching the rock around it, which can be gathered.

## 0.0.3 - 2026-11-18

- Keyboard-only play with a one-block-tall character: W turns to the rear wall, S crawls and aims beneath the feet, J hits or gathers, K interacts and L places. The mouse no longer acts on the world.
- Rear walls: a second, walk-through block layer for building the inside of a home.
- Blocks can be placed in midair, and holding S while jumping and placing builds scaffolding beneath the character.
- A chooser of up to twelve readied objects, opened with H.
- Held semicolon sprints.
- A stone pick, needed to mine copper ore.

## 0.0.2 - 2026-11-17

- Crafting by building: assembling blocks in a recipe's shape transforms them.
- A workbench and a forge. Shapes built over a forge use it, and a workbench beside it multiplies each batch by 1, 5 or 10.
- Regional resources: sand in dry reaches, clay in damp low groves and copper ore in rocky uplands.
- Planks, glass, bricks and copper tiles to build with.
- Existing homes keep their terrain; only newly generated land uses the new generation.

## 0.0.1 - 2026-11-16

- A small world, 1,024 blocks around and 256 high, that wraps east to west.
- Walking, jumping, gathering and placing blocks, with an inventory of what was gathered.
- The world, the character's position and inventory, and every change survive quitting and returning.
- Blocks can be added through content packs.
