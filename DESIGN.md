# Yonderest

Yonderest is a free and open-source, native 2D pixel-art fantasy sandbox about discovering places, making homes in them, and connecting them.

This document describes the intended game and its architecture. It is not an inventory of implemented features. Build milestones belong in [ROADMAP.md](ROADMAP.md).

## The experience

Discovery gives players places to care about; building and connection make those places theirs. Something visible invites a journey. Somewhere reached invites a home. Going farther adds possibilities without making the first home worthless.

The core loop is exploration, discovery, gathering, crafting, building, and reaching farther. Learning how the world works enriches that experience, but solving its geology is not the point. Coherent environments make places feel real and help players find what they need.

Yonderest's mechanics are developed from this experience and its own physical, playful world. Blocks are a medium for that game, not a reason to inherit another sandbox's controls, inventory, progression or interaction assumptions. References to other games identify particular qualities of feel; they do not supply a template for the rest.

There is no prescribed ending, mandatory storyline, or boss-gated progression. Hostile creatures and combat may provide challenges, but major systems, including portals, remain accessible through noncombat play.

Homes, outposts, and routes form an expanding personal geography. Differences in resources give distant places practical value as well as beauty. Settlement management, elaborate economies, and production chains are possible additions, not requirements of this loop.

## Worlds and travel

A universe begins with one persistent world. Exploration and crafting eventually make portals to other worlds possible. Portals require substantial investment rather than mandatory combat.

The starting world does not contain every resource available in the universe. Other worlds offer different environments, resource ecosystems, and possibilities, not merely stronger replacements for everything already found. Each world contains multiple biomes; a world's identity is not one biome stretched across a planet.

Travel expands the network rather than replacing it. Existing homes and routes retain value as new worlds become reachable.

**Open:** Portal construction, destination discovery, and whether players can seek a particular kind of world or initially travel blindly.

Ordinary doors leading to separate pocket interiors are not an established feature. The current inclination is away from them: a mundane door should not quietly require another dimension to make a house work in a side-scrolling world. Portals explicitly connect different places and need not follow the same convention. A pantry-like door acting as the front of storage, rather than an entrance to another room, is a possibility to explore.

## Geography and chunks

Each world is a side-scrolling block landscape with generous but finite height and a horizontally wrapping circumference measured in chunks. Traveling continuously east or west returns to the starting location. The vertical axis does not wrap.

Chunks partition the world in both horizontal and vertical directions. They are independently generated, loaded, and persisted regions, not full-height columns. Chunk dimensions use powers of two.

Vertical geography matters. Worlds have room for meaningful underground depth and occasional sky biomes, not just a surface with a shallow underside. Exploring the ground does not implicitly generate all the terrain above or below it. A later sky-generation module can populate previously ungenerated sky above familiar ground without altering saved chunks.

For persistence and generation, explored territory means generated and persisted chunks, including any loading margin beyond what the player has personally visited. The extent of that margin therefore affects which territory remains available for future generation.

All spatial generation respects horizontal periodicity: terrain, geology, climate, resources, and ecology. The first and last chunk columns are ordinary neighbors, not special borders repaired after generation. Movement and interaction likewise treat the seam as continuous geography.

Floating terrain fits within the world's vertical bounds. A deep lava sea is a possible lower environment, separate from the actual world boundary; it does not imply a fluid simulation.

**Open:** Exact chunk dimensions and shape, world circumference and height, loading margins, and upper and lower boundary behavior.

## Embodied interaction and background depth

The desired player feel is immediate and character-centered, with Mario-like movement as a reference for that quality, not a template for the whole game. The character is one block tall, not two, so the immediate forward interaction does not ambiguously span two block rows. The player's ordinary reach is one block directly in front, not an adjustable radius or remote target. Ranged attacks initially mean pointing and shooting into the world, not selecting a distant target. The early version is keyboard-only; mouse interaction waits until there is a concrete reason for it rather than being inherited as a default.

The keyboard vocabulary is A/D for lateral movement, Space as the sole jump key, and W to turn the character toward the background. W is a change of facing, not a held modifier: the background block behind the character is now simply the block they face. A lateral turn faces the foreground again. Holding S crouches, crawls when moving, and aims taking/leaving beneath the feet; it does not turn the character toward the camera. With S held while facing the background, that downward target is in the background layer. Other contextual downward movement, such as on ladders or in water, remains to be worked through.

Placement puts the held block in the first whole cell beyond the character's facing edge unless downward aim directs it below. Straddling a tile boundary does not cause placement to choose an overlapping cell and refuse it. Direct access and empty space are enough: a placed block does not have to touch another block. Blocks may be placed in midair. This applies to foreground and rear-wall construction, not just a special scaffolding exception.

Right-hand functions are chosen for ergonomics first, mnemonics second. The trial uses H for Hand, J for hit/break/pick up, K for interact, L for leaving/placing the held object, and I for inventory management. H stays beside J/K because hand selection and scrolling form one workflow; it is not being moved to semicolon. The intended resting position is J/K/L/semicolon, with the index finger reaching H. P pauses; Escape closes an open panel before offering quit. The trial repeats J while held, but each distinct Place press makes one request, including a buffered underfoot request.

Sprint uses held semicolon while moving with A/D, with no double-tap gesture and without waiting for stamina. The first trial changes horizontal speed without changing the jump impulse. Sustained pinky comfort, the speed difference and running-jump feel need playtesting. Stamina is strongly anticipated later alongside health and hunger. If sprint does not earn this binding, inventory or pause are fallback candidates, not relocating Hand.

Tool use and placement are explicitly distinct. J is the primary hitting or gathering action; K interacts with the addressed block. The empty hand can gather hand-gatherable blocks, while a breaking-focused tool breaks as its primary use. That consistency is the reason for trialing this mapping rather than making K a universal pickup action; playtesting, not button-name theory, decides its feel. A held building material does not inherit mining, and other tools can have different uses. The exact blocks requiring particular tools remain open; the prototype includes one craftable pick and one tool-gated material to make the distinction testable. Using a held object must not silently become placing it. Setting down a tool places that same block, which need not block movement. Broader tool actions and contextual empty-hand affordances remain open.

Facing the work while backing away is the desired construction presentation, including across gaps while no placement action is held. This need not introduce a different movement mechanic: ordinary directional input can move the character while the active work determines their visual orientation. Ordinary gameplay should not demand that the player simultaneously steer their character and an independent target cursor. There is no world-target highlight: the addressed foreground, background or downward cell is not outlined or marked.

### Quick hand selection

Inventory management has its own UI, opened with I. Readied objects are marked and highlighted in their existing inventory positions, not moved into another container or partitioned into tools and building materials. The quick selector uses that smaller readied subset, not the entire inventory. It permits zero to twelve readied entries. Twelve is chosen for the clock-like presentation; chunk-size power-of-two requirements do not apply to this menu capacity.

The selector appears as a radial popup around the character, with the selected object highlighted above the head. While it is open, J/K step backward/forward through the wheel instead of invoking tool actions. WASD remains movement: choosing a tool while walking is part of the intended interaction. It is the open chooser, not empty hand by itself, that owns J/K; outside the chooser, empty-hand actions can still operate on the world. Selection input must not accidentally execute a newly chosen tool's action on closing the popup.

Each distinct Hand press performs the next state transition: holding an object to empty hand; empty hand to the chooser; chooser to equipping the selected object and closing. The chooser defaults to the last-used object when it is available in the ready set. Thus two rapid presses from a held object open selection, two from empty hand can restore the previous object, and three from a held object can reselect it. These are immediate ordinary transitions, not a double-tap timing window or a hold-to-open gesture. In this prototype, an empty chooser reports no readied objects with an icon, not text. A depleted held stack returns to empty hand; unavailable readied entries are omitted from the wheel, and an unavailable last selection falls back to the first available readied entry. Readiness marks remain in the one inventory so they can be managed or reused after gathering more.

This scheme needs no separate previous/next keys: J/K are available for selection while the chooser owns input. Number-row shortcuts or a distinct last-selected swap are not required for the first trial. The complete inventory layout, readiness ordering controls and transfer/storage workflows remain open.

### Early-game jump-building candidate

The current development focus is the unaided early-game version, not a construction cursor or assistant. Jump-building must make basic scaffolding easy without requiring perfect reflexes or a narrowly timed placement while airborne. Early constraints can come from physical access and resources, not unreliable execution of an obvious construction intent.

The candidate gesture is S + Space + Place. S may be held before jumping: crouching does not suppress Space, and jumping does not cancel the downward aim. While airborne and still holding S, pressing Place requests a block beneath the character. There is no requirement to jump first, then press down in midair, then hit a narrow placement window. The left middle finger can hold S, a thumb can jump, and the right hand can place; whether that feels comfortable requires playtesting.

Placement timing is forgiving. If Place is pressed before the sprite has fully cleared the intended block space, the request waits for clearance and then places without requiring another press. One press is one placement request, not repeated placements caused by a pending request being checked repeatedly. The intended interaction should work throughout the airborne phase; takeoff, descent, landing and obstruction cases need testing against that goal. Holding Place does not repeat.

Without S, forward or background placement while jumping targets the block directly before the sprite at the moment Place is pressed. Jumping naturally brings that target two or even three rows higher; it needs no upward reach accommodation or automatic choice of height. This is distinct from the forgiving clearance buffer for placing beneath the character. The scaffold interaction must not turn every airborne placement into placement beneath the character or retarget a forward placement later in the jump.

### Construction assistant: planned, deferred

The assistant is a retained direction for the game, but is deliberately deferred because it is too much complexity for the present early-game work. It is a separate, player-controlled assistant in the world, rather than a hammer acting as a remote cursor. It has a limited inventory and can only place, not serve as another mining or combat avatar. Arrow keys control its movement. The player retains ordinary direct interactions; the assistant's own position supplies its access to construction sites. Its identity and place in the arc are kept; this is not a finalized control scheme or an instruction to implement it in the next pass.

The assistant can travel independently of the player's arm's reach. It may be able to fly, potentially through an upgrade; its starting locomotion and collision rules are not selected. The camera remains centered on the player and does not scroll to follow the assistant. Visibility is the intended practical distance tether: the visible screen defines the working area, rather than an arbitrary radius measured in blocks from the player. The precise screen-edge behavior, including what happens when player movement scrolls past the assistant, remains open.

The proposed hand-role split is right hand on arrows to move the assistant, left hand issuing its Place command in addition to moving the player. Candidates include X/C/V or R near WASD; right Ctrl and right Shift are other possibilities, not selected bindings. Enter is a reach from the arrows and keypad-only bindings do not cover all keyboards. One candidate assistant command activates or deactivates it when co-located with the player and places from its cargo otherwise. Activation, packing and the exact meaning of co-location remain to be worked through; this does not establish a remote recall command.

Construction assistance has a place in the game arc: early direct placement is constrained by the player's body, while obtaining an assistant makes more ambitious work easier. The early action should remain responsive and legible; the limitation is capability, not deliberately bad input handling. Capacity or mobility upgrades could develop that help further without increasing the player's arm's reach. Acquisition, progression and the starting ability needed to improve a five-block-high rear-wall task are not yet specified. The assistant must not become an excuse to leave early scaffolding dependent on precise jump/place timing.

The assistant places from its own limited inventory. Two owners carrying blocks do not require tools and building materials to become disjoint item classes or require the player to maintain two categorical hotbars. Loading, selecting and recovering the assistant's cargo still need design, using the same carried/placed object vocabulary. Its form, deployment or stowing, the placement gesture, behavior off screen, and movement/loading/persistence footprint remain open. A targeting ghost's temporary visibility is not by itself a decision about a world assistant's lifetime or cargo.

### Background construction

An interactable, nonblocking background layer gives houses actual rear walls rather than an interior open to the sky. It can also let the character walk past a tree trunk or ladder while still being able to address it. Tree trunks are envisaged as background by default; canopies remain foreground. Layer membership, collision and climbing behavior are related but distinct questions. No depth-traversal simulation or pocket interior follows automatically from adding a background layer.

Spatial crafting is foreground-only. Background blocks do not supply ingredients, required empty cells or stations, and rear-wall construction cannot complete a recipe. This is the chosen next-pass behavior and may remain permanent; that broader decision can wait for a later design pass.

The prototype's dark, terrain-shaped background behind excavated ground looks wrong near the surface. This is presentation feedback, not an intended permanent silhouette of where terrain used to be. Surface openings, underground backing, constructed rear walls and eventual lighting need a coherent treatment rather than one universal black fill.

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

A new chunk takes boundary conditions only from its immediate neighbors; those conditions carry forward whatever transition remains. At a boundary between generations, variation on the new generator's side should settle into the terrain its rules would otherwise have produced within roughly one or two chunks. This applies vertically as well as horizontally, including the world seam. The exploration-order guarantee applies while generation rules are unchanged; changing them can produce different results according to which territory was already saved.

Boundary information may include elevation, slope, geological layers, water levels, or environmental fields, according to the generator's needs. A separate post-generation blending subsystem is not prescribed.

World parameters and generated material definitions persist wherever they are needed to keep the world consistent. Save and resume preserve player position, inventory, and changes to the world.

**Open:** Serialization, individual versus grouped chunk storage, save timing, and the boundary representation used when generation changes.

## Materials, building, and crafting

Players gather materials, craft useful things, and construct homes and connections. Resource distribution makes exploration consequential. Building is a reason to explore and a way of giving discovered places lasting significance.

Worlds can have procedurally derived material vocabularies. Composition, hardness, density, durability, appearance, and uses may vary in related ways. Shared functional families can support familiar activities while unusual materials introduce new capabilities.

Materials have stable identities and definitions across terrain, inventories, crafting, and travel. Their saved meaning does not depend on their incidental position in a runtime array.

Appearance and gameplay properties draw on the same material definition where practical. Shared textures, palettes, tints, overlays, and procedural details allow distinctive materials without requiring a unique authored texture for every variation.

### Everything-is-a-block direction

The unifying idea to explore is one object vocabulary across terrain, carried gear, storage and placed objects, not separate item and block identities. A sword can be set down, render as a sword-shaped block, and allow the character to walk past it. Being a block does not mean filling a square visually or blocking movement.

The implications remain open: occupancy, mutable or unique object state, wielding and using a carried block, stacking, storage, and the treatment of moving actors. The direction does not prescribe a particular serialization or entity architecture merely by its name. Inventory and storage need design in their own right, not only a larger hotbar.

### Spatial crafting experiment

The current prototype experiments with crafting by assembling ingredient blocks in the world. Completing a valid configuration transforms it automatically, rather than opening a crafting-table interface. Automatic completion remains the next trial's behavior, with a more intentional finish gesture possible later. Playtesting will determine whether physical assembly is enjoyable. The executable uses the keyboard-first interaction described above, with foreground-only recipe matching.

Some recipes require a working surface underneath. Ingredients are consumed; the required surface remains. A forge is used by literally building over it, not by being inside a nearby bonus radius.

The forge was envisaged primarily for metallurgy and making metal tools, not as a generic melting or firing station. Whether generalizing it to glass, bricks and similar products serves the game remains open. The prototype's separate timber ingredient beneath the main ingredients but above the forge feels unnecessarily messy; that arrangement is not a settled requirement. Station requirements, fuel and ingredient geometry need further thought rather than copying the prototype's recipe shapes into the intended design.

A workbench provides batching. For batch size N, each ingredient placement consumes N units from inventory, and the completed recipe yields its ordinary output multiplied by N. This changes repetition, not the material cost per result. Unfinished ingredient quantities must survive saving and dismantling without changing their meaning.

Stations compose through physical layout. To batch a recipe that requires a forge, put the forge and workbench beside each other and build the ingredient configuration over both. Neither station is consumed, and this does not require a separate bulk version of the recipe.

**Open:** Material-family definitions, property ranges, resource-specific behavior, and how far crafting follows material properties rather than explicit recipes.

## Character, tools, and risk

These are unprioritized directions for exploration, not a committed feature list or build sequence:

- Light RPG elements, including a modest skill tree and character customization. What improves, what is chosen, and how progression serves exploration and building remain open.
- Tools that enable breaking particular blocks or constructing particular structures, plus activities such as fishing with a fishing pole. Their affordances, requirements and relationship to progression are not yet specified.
- Inventory and storage that fit the carried/placed block model and keyboard-first play. The quick hand-selection trial is described above; wider capacity, organization, equipment and transfer behavior remain to be designed.
- Health, hunger, stamina, suffocation, combat, falling, lava, fire and related risks. Stamina is strongly anticipated and belongs with this later work, not as a prerequisite for the current movement trial. Severity and interactions remain open; listing the other risks does not establish their inclusion. Any chosen systems must still serve the game's exploration, homes and connections, without introducing mandatory combat gates to major systems.

## Modules and extension

Yonderest is a cohesive flagship game built on an extensible engine. Flagship-first determines which functionality is developed first, not a bias toward coupling or privileged first-party code. First-party content and community extensions use the same fundamental extension mechanisms.

Shared operations are implemented once and generalized where that serves the game's uses. Engine responsibilities, content, and presentation have clear ownership so additional features and other games can extend the system without untangling the flagship from its internals.

The extension model has three tiers:

- **Declarative content packs:** Blocks, recipes, textures, sprites, and supported generation definitions without programming. Carried and stored objects participate in this content path; the everything-is-a-block direction explores their shared model rather than introducing a separate item type here.
- **Standard Lua scripts:** Gameplay extensions through the public game-facing API.
- **Native C++ extensions or custom builds:** Deeper changes and separate games beyond the scripting API.

Lua and declarative content are the portable extension mechanisms. Dynamic native loading on mobile and a universal stable native ABI are not requirements.

Major features are separately selectable where practical. Players can decline a generation update without necessarily declining unrelated later content. Dependencies are explicit; arbitrary combinations are not promised compatible. A versioned flagship manifest defines the standard configuration.

Extensions can introduce distinct semantic actions rather than being forced to overload the base tool actions or placement. An "eat anything" mod could add a separately bound Eat action, for example. Primary use, interaction and placement are a base vocabulary, not a closed limit on all game verbs. The registration and binding interface remains to be designed.

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

Code and the prototype's original material data and code-drawn placeholder visuals are licensed under MIT. This does not decide the license for future flagship artwork. Other content and artwork have their own licensing decisions; their licenses and provenance must be clear.

Placeholder art can establish readable geography and interactions before the visual identity is complete. AI-generated artwork may be used with transparent provenance. Human-created replacements and texture packs are encouraged, including compatibility with procedural palettes and material variation.

Community selection of future flagship artwork and an ongoing relationship with an artist are possibilities, not arrangements already made. Previous art can remain available where its licensing permits.

**Open:** Flagship content and asset licenses, final art direction, community selection and compensation arrangements, and commercialization. Monetization is not a prerequisite for making the game.
