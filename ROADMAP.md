# Yonderest roadmap

This roadmap says when features arrive. What each feature is belongs to [DESIGN.md](DESIGN.md), the game as it currently runs is described in [README.md](README.md), and what each version changed is in [CHANGELOG.md](CHANGELOG.md).

## Versions

A **build** is one round of building, followed by testing, fixes and bringing the documents into line. Every published build bumps the patch version. Each build also carries the outstanding feedback on how the game feels, so feel is tuned continuously rather than in separate passes.

- **Prototype, 0.0.x.** The game's bones are being settled, and anything may break between builds, saved worlds included. Each build is effectively its own milestone. The engine and each module carry versions of their own; the flagship's version is the game's.
- **Alpha, from 0.1.0.** The bones are settled enough to build on without breaking everything, and worlds are expected to persist. Milestones are named updates that bump the minor version, each made of several builds. A milestone may still break compatibility.
- **Stable, from 1.0.0.** Compatibility is kept within a major version, including every extension interface modules rely on: a breaking change to one, such as the generator interface, bumps the major version.

## Prototype: settling the bones

0.1.0 arrives when the engine's interfaces and every core module exist, the game works on them, and nothing is left that would break a persistent world or a content pack. At 0.1.0 the engine and each module move into repositories of their own, which the flagship pins as submodules.

1. **0.0.8: The explosion.** The project takes its modular shape: the engine and each core module in a directory of its own, built as its own library with its own version, the existing code restructured along the engine's interfaces, and every module's interfaces and dependencies declared. The game plays as before, except that crafting no longer tells placed blocks from natural ones.
2. **0.0.9: The generator.** Native modules loaded at runtime through versioned interfaces, layers declared by modules, generation in passes, and the generator rebuilt as the first module on its interface. Loading claims only the land the view actually nears.
3. **Module by module.** Each following build develops one module together with the interfaces it touches. Their order is provisional:
   - physics, with movement measured in blocks: the character a whole block, the center rule, whole-block jumps, and smooth movement between cells saved as cells;
   - interaction and controls: actions framed by the engine and given meaning by modules;
   - crafting, with a batched result standing as one block holding the whole output;
   - inventory, with carrying and storage: the finite backpack, stack sizes, in-world storage and objects that carry their own state;
   - display, with the 32-pixel look: the art grid, the adaptive view, a new character on a shareable canvas, redrawn world and interface art, and the parallax backdrop.

A closer search for remaining bones may add builds before 0.1.0.

## Alpha: named updates

Their order is provisional.

- **Reasons to travel.** Several distinct biomes, a broader range of materials and construction, a module configuration players can use, and reliable return journeys across a world worth inhabiting.
- **The connected universe.** Portals, additional persistent worlds and their different resource ecosystems, making homes and outposts a geography that spans worlds.
- **Scripting.** A Lua scripting module, once there is gameplay worth scripting.

## Unsequenced

Deeper underground environments, sky biomes, other platforms, two players on one screen, and everything under DESIGN's Possibilities have no place in the sequence yet. Each gets its own scope when it is taken up.
