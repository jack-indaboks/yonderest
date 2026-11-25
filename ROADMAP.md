# Yonderest roadmap

This roadmap says when features arrive. What each feature is belongs to [DESIGN.md](DESIGN.md), the game as it currently runs is described in [README.md](README.md), and what each version changed is in [CHANGELOG.md](CHANGELOG.md).

## Versions

A **build** is one round of building, followed by testing, fixes and bringing the documents into line. Every published build bumps the patch version. Each build also carries the outstanding feedback on how the game feels, so feel is tuned continuously rather than in separate passes.

- **Prototype, 0.0.x.** The game's bones are being settled, and anything may break between builds, saved worlds included. Each build is effectively its own milestone.
- **Alpha, from 0.1.0.** The bones are settled enough to build on without breaking everything, and worlds are expected to persist. Milestones are named updates that bump the minor version, each made of several builds. A milestone may still break compatibility.
- **Stable, from 1.0.0.** Compatibility is kept within a major version, including every extension interface modules rely on: a breaking change to one, such as the generator interface, bumps the major version.

## Prototype: settling the bones

0.1.0 arrives when nothing is left that would break a persistent world or a content pack, and the core loop is complete enough to be worth testing over time.

1. **0.0.8: The generator as a native module.** The engine loading native modules at defined extension points, the generator as the first of them, and generator add-ons passed to it as declarative content.
2. **0.0.9: The 32-pixel look.** The 32-pixel art grid, the adaptive view, a new character drawn on a shareable canvas, redrawn world and interface art, and the parallax backdrop.
3. **Carrying and storage.** The finite backpack, stack sizes, in-world storage, and objects that carry their own state.

A closer search for remaining bones may add builds before 0.1.0.

## Alpha: named updates

Their order is provisional.

- **Reasons to travel.** Several distinct biomes, a broader range of materials and construction, a module configuration players can use, and reliable return journeys across a world worth inhabiting.
- **The connected universe.** Portals, additional persistent worlds and their different resource ecosystems, making homes and outposts a geography that spans worlds.
- **Scripting.** A Lua scripting module, once there is gameplay worth scripting.

## Unsequenced

Deeper underground environments, sky biomes, other platforms, two players on one screen, and everything under DESIGN's Possibilities have no place in the sequence yet. Each gets its own scope when it is taken up.
