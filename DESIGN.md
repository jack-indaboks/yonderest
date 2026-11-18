# Yonderest

Yonderest is a free and open-source, native 2D pixel-art fantasy sandbox about discovering places, making homes in them, and connecting them.

This document describes the intended game and its architecture. It is not an inventory of implemented features. Build milestones belong in [ROADMAP.md](ROADMAP.md).

## The experience

Discovery gives players places to care about; building and connection make those places theirs. Something visible invites a journey. Somewhere reached invites a home. Going farther adds possibilities without making the first home worthless.

The core loop is exploration, discovery, gathering, crafting, building, and reaching farther. Learning how the world works enriches that experience, but solving its geology is not the point. Coherent environments make places feel real and help players find what they need.

There is no prescribed ending, mandatory storyline, or boss-gated progression. Hostile creatures and combat may provide challenges, but major systems, including portals, remain accessible through noncombat play.

Homes, outposts, and routes form an expanding personal geography. Differences in resources give distant places practical value as well as beauty. Settlement management, elaborate economies, and production chains are possible additions, not requirements of this loop.

## Worlds and travel

A universe begins with one persistent world. Exploration and crafting eventually make portals to other worlds possible. Portals require substantial investment rather than mandatory combat.

The starting world does not contain every resource available in the universe. Other worlds offer different environments, resource ecosystems, and possibilities, not merely stronger replacements for everything already found. Each world contains multiple biomes; a world's identity is not one biome stretched across a planet.

Travel expands the network rather than replacing it. Existing homes and routes retain value as new worlds become reachable.

**Open:** Portal construction, destination discovery, and whether players can seek a particular kind of world or initially travel blindly.

## Geography and chunks

Each world is a side-scrolling block landscape with generous but finite height and a horizontally wrapping circumference measured in chunks. Traveling continuously east or west returns to the starting location. The vertical axis does not wrap.

Chunks partition the world in both horizontal and vertical directions. They are independently generated, loaded, and persisted regions, not full-height columns. Chunk dimensions use powers of two.

Vertical geography matters. Worlds have room for meaningful underground depth and occasional sky biomes, not just a surface with a shallow underside. Exploring the ground does not implicitly generate all the terrain above or below it. A later sky-generation module can populate previously ungenerated sky above familiar ground without altering saved chunks.

For persistence and generation, explored territory means generated and persisted chunks, including any loading margin beyond what the player has personally visited. The extent of that margin therefore affects which territory remains available for future generation.

All spatial generation respects horizontal periodicity: terrain, geology, climate, resources, and ecology. The first and last chunk columns are ordinary neighbors, not special borders repaired after generation. Movement and interaction likewise treat the seam as continuous geography.

Floating terrain fits within the world's vertical bounds. A deep lava sea is a possible lower environment, separate from the actual world boundary; it does not imply a fluid simulation.

**Open:** Exact chunk dimensions and shape, world circumference and height, loading margins, and upper and lower boundary behavior.

## Causally coherent generation

World identity comes from related underlying conditions, such as geological composition, atmosphere, temperature, humidity, and potentially gravity or magical influences. Those conditions influence terrain, resources, vegetation, creatures, and appearance rather than supplying independent randomized lists.

The relationships may be simple. Humidity and substrate can determine where moss grows; mineral composition can influence stone color and deposits. The objective is coherent places, not comprehensive physical realism.

Generation has three conceptual responsibilities:

- Establish the identities and parameters of worlds within a universe.
- Derive world-scale environmental fields, material families, and ecological conditions.
- Produce concrete local terrain and content in chunks.

These are responsibilities, not a requirement for three runtime subsystems. Environmental modeling primarily belongs to generation. Continuous simulation exists for ongoing gameplay behavior, not to keep every process that shaped the landscape running forever.

Given the same seed and unchanged generation configuration, terrain is consistent regardless of exploration order, including across the horizontal seam.

**Open:** The environmental model, the extent of differences in physical rules between worlds, and which environmental processes have ongoing gameplay behavior.

## Persistence and generation changes

A chunk is generated on first encounter and persisted as complete world state. Later visits load that saved state, including player changes. The generator initializes new territory; it does not reconstruct explored territory from a seed plus an edit list.

Saved terrain is authoritative. Adding, removing, or updating generation modules never implicitly regenerates it.

New generation respects persisted neighbors. At a boundary between generations, variation on the new generator's side should settle into the terrain its rules would otherwise have produced within roughly one or two chunks. This applies vertically as well as horizontally, including the world seam. The exploration-order guarantee applies while generation rules are unchanged; changing them can produce different results according to which territory was already saved.

Boundary information may include elevation, slope, geological layers, water levels, or environmental fields. The transition mechanism and necessary metadata remain to be designed; a separate post-generation blending subsystem is not prescribed.

World parameters and generated material definitions persist wherever they are needed to keep the world consistent. Save and resume preserve player position, inventory, and changes to the world.

**Open:** Serialization, individual versus grouped chunk storage, save timing, and the boundary representation used when generation changes.

## Materials, building, and crafting

Players gather materials, craft useful things, and construct homes and connections. Resource distribution makes exploration consequential. Building is a reason to explore and a way of giving discovered places lasting significance.

Worlds can have procedurally derived material vocabularies. Composition, hardness, density, durability, appearance, and uses may vary in related ways. Shared functional families can support familiar activities while unusual materials introduce new capabilities.

Materials have stable identities and definitions across terrain, inventories, crafting, and travel. Their saved meaning does not depend on their incidental position in a runtime array.

Appearance and gameplay properties draw on the same material definition where practical. Shared textures, palettes, tints, overlays, and procedural details allow distinctive materials without requiring a unique authored texture for every variation.

### Spatial crafting experiment

For the next prototype, crafting happens by assembling ingredient blocks in the world. Completing a valid configuration transforms it automatically, rather than opening a crafting-table interface. Playtesting will determine whether this is enjoyable; neither automatic activation nor the current WASD-and-mouse controls are permanent commitments.

Some recipes require a working surface underneath. Ingredients are consumed; the required surface remains. A forge is used by literally building over it, not by being inside a nearby bonus radius.

A workbench provides batching. For batch size N, each ingredient placement consumes N units from inventory, and the completed recipe yields its ordinary output multiplied by N. This changes repetition, not the material cost per result. Unfinished ingredient quantities must survive saving and dismantling without changing their meaning.

Stations compose through physical layout. To batch a recipe that requires a forge, put the forge and workbench beside each other and build the ingredient configuration over both. Neither station is consumed, and this does not require a separate bulk version of the recipe.

**Open:** Material-family definitions, property ranges, resource-specific behavior, and how far crafting follows material properties rather than explicit recipes.

## Modules and extension

Yonderest is a cohesive flagship game built on an extensible engine. Flagship-first determines which functionality is developed first, not a bias toward coupling or privileged first-party code. First-party content and community extensions use the same fundamental extension mechanisms.

Shared operations are implemented once and generalized where that serves the game's uses. Engine responsibilities, content, and presentation have clear ownership so additional features and other games can extend the system without untangling the flagship from its internals.

The extension model has three tiers:

- **Declarative content packs:** Blocks, items, recipes, textures, sprites, and supported generation definitions without programming.
- **Standard Lua scripts:** Gameplay extensions through the public game-facing API.
- **Native C++ extensions or custom builds:** Deeper changes and separate games beyond the scripting API.

Lua and declarative content are the portable extension mechanisms. Dynamic native loading on mobile and a universal stable native ABI are not requirements.

Major features are separately selectable where practical. Players can decline a generation update without necessarily declining unrelated later content. Dependencies are explicit; arbitrary combinations are not promised compatible. A versioned flagship manifest defines the standard configuration.

### Missing content

Disabling a module does not erase its saved identifiers. For example, a block saved as `crystal_expansion:blue_crystal` retains that identity even when the defining module is absent.

The provisional interpretation is a common dirt-like fallback with ordinary collision, mining, and gameplay behavior. That interpretation does not replace the saved identifier. Restoring the module restores the original interpretation wherever the content has not been changed. Deliberate player actions can modify or destroy it.

**Open:** Module declaration and compatibility details, the supported generation extension surface, and the final appearance and behavior of unknown content.

## Runtime and presentation

The selected stack is C++20, SDL3, CMake, and standard Lua. Native Linux is the first platform; macOS, Windows, Android, and iOS are intended later targets.

C++ owns world state and API contracts. Lua runs gameplay scripts against a public interface rather than accessing engine internals directly. Script-visible references need defined lifetimes across chunk loading and unloading. LuaLS annotations describe the API; where practical they are generated from its binding definitions rather than maintained as a parallel type system.

The world model and game logic remain independent of SDL-specific presentation details. The flagship uses pixel-art tiles and sprites. A terminal presentation using ASCII or Unicode is a possible optional edition, not the flagship. The world and its content are not defined in terms of either pixels or terminal glyphs.

**Open:** Lua API shape and reference lifetime, mobile packaging, and whether multiplayer will become part of the game. Multiplayer is not an established requirement with a chosen synchronization model.

## Art, licensing, and community

Code is licensed under MIT. Content and artwork have their own licensing decisions; their licenses and provenance must be clear.

Placeholder art can establish readable geography and interactions before the visual identity is complete. AI-generated artwork may be used with transparent provenance. Human-created replacements and texture packs are encouraged, including compatibility with procedural palettes and material variation.

Community selection of future flagship artwork and an ongoing relationship with an artist are possibilities, not arrangements already made. Previous art can remain available where its licensing permits.

**Open:** Flagship content and asset licenses, final art direction, community selection and compensation arrangements, and commercialization. Monetization is not a prerequisite for making the game.
