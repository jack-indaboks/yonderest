# Changelog

What changed in each tagged version of Yonderest, newest first. What is planned next is in [ROADMAP.md](ROADMAP.md).

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
