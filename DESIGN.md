# Yonderest

Yonderest is a free and open-source, native 2D pixel-art fantasy sandbox about discovering places, making homes in them, and connecting them.

This document describes the game Yonderest intends to be, as far as that vision has been worked out: settled features are stated plainly, and features still taking shape are left open. It changes when the vision changes, not when something is built. The game as it currently runs is described in [README.md](README.md); when each feature arrives is in [ROADMAP.md](ROADMAP.md).

## The experience

Discovery gives players places to care about; building and connection make those places theirs. Something visible invites a journey. Somewhere reached invites a home. Going farther adds possibilities without making the first home worthless.

The core loop is exploration, discovery, gathering, crafting, building, and reaching farther. Learning how the world works enriches that experience, but solving its geology is not the point. Coherent environments make places feel real and help players find what they need.

Yonderest's mechanics are developed from this experience and its own physical, playful world. Blocks are a medium for that game, not a reason to inherit another sandbox's controls, inventory, progression or interaction assumptions. References to other games identify particular qualities of feel; they do not supply a template for the rest.

There is no prescribed ending, mandatory storyline, or boss-gated progression. Hostile creatures and combat may provide challenges, but major systems, including portals, remain accessible through noncombat play.

Homes, outposts, and routes form an expanding personal geography. Differences in resources give distant places practical value as well as beauty. Settlement management, elaborate economies, and production chains are possible additions, not requirements of this loop.

## Worlds and travel

A universe begins with one persistent world. Exploration and crafting eventually make portals to other worlds possible. The player belongs to the universe, not to any one world, and carries what they have between worlds. Portals require substantial investment rather than mandatory combat.

The starting world does not contain every resource available in the universe. Other worlds offer different environments, resource ecosystems, and possibilities, not merely stronger replacements for everything already found. Each world contains multiple biomes; a world's identity is not one biome stretched across a planet.

The starting world is expansive. Circling it is an epic journey, not an afternoon's walk, and arriving back where one began is an achievement. Worlds reached through portals take size as one of their parameters, alongside everything else that distinguishes them: some rival the starting world, and some are small enough to hold a single biome.

Travel expands the network rather than replacing it. Existing homes and routes retain value as new worlds become reachable.

**Open:** Portal construction, destination discovery, and whether players can seek a particular kind of world or initially travel blindly.

Ordinary doors leading to separate pocket interiors are not an established feature. The inclination is away from them: a mundane door should not quietly require another dimension to make a house work in a side-scrolling world. Portals explicitly connect different places and need not follow the same convention.

## Geography and chunks

Each world is a side-scrolling block landscape with generous but finite height and a horizontally wrapping circumference. Traveling continuously east or west returns to the starting location. The vertical axis does not wrap.

Chunks partition the world in both horizontal and vertical directions. They are independently generated, loaded, and persisted regions, not full-height columns. A chunk is 64 by 64 blocks in every world: chunk size is a standard of the game, not a property of a world, so anything built to the chunk, such as a hand-made chunk from a mod, fits everywhere. A region is 64 by 64 chunks, 4,096 blocks square; it groups chunks for storage.

World dimensions are whole numbers of chunks, chosen per world. The sizes offered when making a world double from one to the next. The starting world feels endless: 4,096 chunks around, over a quarter of a million blocks, and 16 chunks tall, 1,024 blocks, so that reaching the bottom takes effort too. A world can be as small as a few chunks in either direction, up to 65,536 chunks around and one region tall; a world as tall as a region and as wide is one shape for a vertical world.

Vertical geography matters. Worlds have room for meaningful underground depth and occasional sky biomes, not just a surface with a shallow underside. Exploring the ground does not implicitly generate all the terrain above or below it. A later sky-generation module can populate previously ungenerated sky above familiar ground without altering saved chunks.

For persistence and generation, explored territory means generated and persisted chunks, including any loading margin beyond what the player has personally visited. The extent of that margin therefore affects which territory remains available for future generation.

All spatial generation respects horizontal periodicity: terrain, geology, climate, resources, and ecology. The first and last chunk columns are ordinary neighbors, not special borders repaired after generation. Movement and interaction likewise treat the seam as continuous geography.

Landscape features have their own scale. Hills, climate belts and regions are sized for the land they shape, not stretched to fit the circumference, so a larger world holds more places rather than broader versions of the same ones.

Floating terrain fits within the world's vertical bounds. A deep lava sea is a possible lower environment, separate from the actual world boundary; it does not imply a fluid simulation.

**Open:** Loading margins, and upper and lower boundary behavior.

## Embodied interaction

The desired player feel is immediate and character-centered, with Mario-like movement as a reference for that quality, not a template for the whole game. The character is one block tall, so the immediate forward interaction does not ambiguously span two block rows. The character's reach is the block directly before them, not an adjustable radius or a remote target. Ranged attacks mean pointing and shooting into the world, not selecting a distant target. In-world play is keyboard-driven. The mouse is a convenience for interfaces, never for acting on the world, and no function requires it.

Jumps are high and generous; falling is brisk rather than floaty. Walking into a one-block step stops the character; climbing it is a jump, never an automatic step-up.

### Facing and reach

A and D walk and turn the character. Space is the only jump key, and it jumps whatever else is held. W turns the character toward the background: after a tap of W they face the rear wall, and the background block behind them is simply the block they face. Holding S crouches, crawls when moving, and aims taking and placing beneath the feet; pressed while facing the wall, it turns the character back to the side they last faced. Otherwise the last of these keys pressed decides facing, and releasing W while A or D is still held turns the character that way.

Holding W reaches up. While W is held, the character faces the wall, and taking and placing address the cell above their head, so they can dig straight up or roof themselves over, standing or in a jump. A and D still move the character while W is held, shuffling along the wall without turning, so a ceiling can be dug out or a roof laid while walking. The upward strike is always seen from behind. Climbing, swimming and other contextual vertical movement remain open.

Placement puts the held block in the first whole cell beyond the character's facing edge, unless S or W aims it below or above. A placed block does not have to touch another: direct access and empty space are enough, and blocks may be placed in midair. This applies to foreground and rear-wall construction alike, not just to scaffolding.

Objects that share the character's own space, such as a tool set down where they stand, can be taken back.

### Forgiveness

The game reads intent at the edges instead of demanding pixel precision, and it does so through one consistent rule rather than a collection of exceptions. A jump that would catch a block's corner by a few pixels slips past it, so the character can jump up into a one-block opening. A placement made while the body overlaps a cell boundary by a few pixels goes to the cell the player plainly means. The width of that margin is settled by feel.

The physical constraint creates work, not bad input handling. The player chooses what to do; carrying it out does not require reflexes beyond the intent.

### Bindings

Right-hand bindings are chosen for ergonomics first, mnemonics second. The resting position is J/K/L/semicolon, with the index finger reaching H: H for Hand, J to hit or gather, repeating while held, K to interact with the addressed block, and L to leave the held object in the world, once per press. I opens the backpack and R the recipe book. P pauses. Escape closes an open dialog or chooser before offering the menu; a passive reference on screen does not change what it does. Function keys are never used.

Held semicolon sprints while walking, without changing the jump impulse. Whether sprint keeps that binding depends on sustained comfort; inventory or pause are its fallbacks, not Hand. Stamina is anticipated alongside health and hunger.

Tool use and placement are distinct. J is the primary hitting or gathering action; K interacts. The empty hand can gather hand-gatherable blocks, while a breaking tool breaks as its primary use. A held building material does not inherit mining. Some materials need particular tools; which ones, and the wider range of tool actions and empty-hand affordances, remain open. Using a held object never silently becomes placing it. Setting down a tool places that same block, which need not block movement.

Lateral movement turns the character in the direction of travel, including while building, except while W is held. Placement does not latch facing. Ordinary play never asks the player to steer the character and an independent cursor at once. There is no world-target highlight: the addressed foreground, background, downward or upward cell is not outlined or marked.

### The belt

The belt is the set of up to twelve readied stacks in the backpack, presented as a ring around the character. Readying marks a stack where it sits; it does not move it into another container or divide tools from materials. Readiness belongs to the stack, not to the slot that holds it. When a readied stack is used up, its readiness ends with it, and an empty slot is never on the belt. A new kind of object lands in the first open slot of the pack and is readied automatically while fewer than twelve are readied. A further stack of a kind already carried joins that kind wherever it is and is never readied automatically. Un-readying one thing and readying another needs no rearranging of the pack. Twelve suits the clock-like ring; it is not a power-of-two constraint.

The ring shows each readied object with its carried quantity. The selected entry sits above the character's head and the others are subdued. While the ring is open, J/K step through it instead of acting on the world, and WASD keep moving: choosing a tool while walking is part of the interaction. It is the open ring, not the empty hand, that owns J/K.

Each Hand press makes the next transition: from a held object to the empty hand, from the empty hand to the ring, and from the ring to holding its selection. The ring opens on the last-used object when it is readied. These are immediate transitions, not double-tap windows or hold gestures, and closing the ring never triggers the chosen tool's action. Using up the held stack returns to the empty hand. The backpack can also put any carried object straight into the hand without readying it; the ring shows only the belt.

### Carrying and storage

Carrying is limited. The backpack has a finite number of slots, and each kind of object has a stack size of its own: plentiful materials stack deeply, while a unique tool stacks to one. Deciding what to carry is part of the game's challenge, not a nuisance to engineer away. When a readied stack has spares elsewhere in the pack, using it draws on the spares first, so the readied stack stays full until the last of that object is gone.

In-world storage gives homes their place in that challenge: what does not travel stays somewhere the player built. A pantry-like door acting as the front of storage, rather than an entrance to another room, is one form to explore. The forms of storage and the transfer between pack and storage remain to be designed.

### HUD and recipe reference

The HUD is a few clean, recognizable objects over the world in a screen corner, not a debug overlay or bands outside the play view: a closed recipe book embossed R and a backpack stamped I, which open their interfaces when clicked and teach their own keys. Health and related indicators join the same visual family when those systems exist.

Opened interfaces support the mouse and complete keyboard navigation with arrow keys and Enter. Number shortcuts may help where relevant but replace neither. The backpack is an actual management dialog. Saving, fullscreen, controls and quitting are reached through the menu. The H/J/K quick-hand workflow remains available during play.

The recipe book is a small reference that stays open while building, not a screen-dominating dialog. R shows or hides it; Left and Right turn its pages while it is visible, and the mouse can do the same. An open dialog takes precedence for interface navigation. It shows each recipe's physical arrangement, material icons, quantities, result and required working surfaces clearly, and it never crafts anything itself. It needs no pin action, confirmation press or focus mode, does not pause play, and takes over neither WASD nor H/J/K/L nor Escape merely by being visible.

Whatever the world can show, it shows: the held block or tool is recognizable in the character's hand, and facing and actions read through the sprite. Exact quantities belong to the ring and the backpack. There are no permanent captions for what is held or targeted, no coordinates, and no keyboard manual over the world. Save feedback is brief and separate from pause, which is shown only while play is actually paused.

### Building without help

Basic scaffolding is easy without help and without perfect reflexes. The gesture is S + Space + L: S may be held before jumping, crouching does not suppress Space, and jumping does not cancel the downward aim. Pressing L while airborne with S held places a block beneath the character. If L is pressed before the character has cleared the space, the request waits for clearance and then places, without another press. One press is one placement; holding L does not repeat. The gesture works throughout the airborne phase.

Without S or W, placement during a jump targets the cell directly before the character at the moment L is pressed. A jump naturally brings that cell two or three rows higher; it needs no reach accommodation, and a forward placement is never retargeted later in the jump.

## Background depth

Every world has two block layers. The rear layer is interactable and nonblocking: it gives houses real rear walls instead of interiors open to the sky, and it lets the character walk past a tree trunk or ladder while still being able to address it. Tree trunks stand in the rear layer; canopies remain in the foreground. Layer membership, collision and climbing are related but distinct questions. No depth traversal or pocket interior follows from having a rear layer.

Underground, the rear layer is as varied as the foreground in material, with its own deposits, perhaps slightly richer than the foreground's, hidden until the foreground before them is removed. That gives mining a reason even where the screen already shows what lies a few blocks down. The rear layer has no caves of its own: its variety is in what it holds, not in openings. Above ground, open sky lies behind the foreground.

Behind the rear layer is a backdrop that cannot be touched: a muted texture of the biome's base material, sky above ground, stone below it, and the local base material in other biomes. The backdrop is set back with parallax, so it reads as distance beyond reach rather than as blocks to mine. The generator decides what backdrop lies behind each place, so it can differ from biome to biome and with depth. Digging does not change it: the distance behind a hill stays where it was when the hill is gone. The backdrop is never a block, so there is no third block layer. Removing a foreground block exposes the rear-layer block behind it, and removing that exposes the backdrop.

Generated rear-layer material and its removal persist as world state. Saved terrain keeps whatever rear layer it was saved with, including none. Lighting is separate work.

Crafting reads the foreground only: rear-layer blocks supply neither ingredients, required empty cells nor stations, and building a rear wall never completes a recipe. Whether the rear layer ever takes part in crafting is open.

## Causally coherent generation

World identity comes from related underlying conditions, such as geological composition, atmosphere, temperature, humidity, and potentially gravity or magical influences. Those conditions influence terrain, resources, vegetation, creatures, and appearance rather than supplying independent randomized lists.

The relationships may be simple. Humidity and substrate can determine where moss grows; mineral composition can influence stone color and deposits. The objective is coherent places, not comprehensive physical realism.

Generation has three conceptual responsibilities:

- Establish the identities and parameters of worlds within a universe.
- Derive world-scale environmental fields, material families, and ecological conditions.
- Produce concrete local terrain and content in chunks.

These are responsibilities, not a requirement for three runtime subsystems. A world's generator module carries them; see Modules and extension. Environmental modeling primarily belongs to generation. Continuous simulation exists for ongoing gameplay behavior, not to keep every process that shaped the landscape running forever.

Given the same seed and unchanged generation configuration, terrain is consistent regardless of exploration order, including across the horizontal seam.

**Open:** The environmental model, the extent of differences in physical rules between worlds, and which environmental processes have ongoing gameplay behavior.

## Persistence and generation changes

A chunk is generated on first encounter and persisted as complete world state. Later visits load that saved state, including player changes. The generator initializes new territory; it does not reconstruct explored territory from a seed plus an edit list.

Saved terrain is authoritative. Adding, removing, or updating generation modules never implicitly regenerates it.

A new chunk takes boundary conditions only from its immediate neighbors; those conditions carry forward whatever transition remains. At a boundary between generations, the new generator's side settles into the terrain its rules would otherwise produce within roughly one or two chunks. This applies vertically as well as horizontally, including across the world seam. The exploration-order guarantee applies while generation rules are unchanged; changing them can produce different results according to which territory was already saved.

Boundary information may include elevation, slope, geological layers, water levels, or environmental fields, according to the generator's needs. A separate post-generation blending subsystem is not prescribed. What a generator keeps about each chunk for later neighbors is its own business. Land generated beside a chunk that was never generated, such as one built by hand, still meets it.

World parameters and generated material definitions persist wherever they are needed to keep the world consistent. Save and resume preserve player position, inventory, and changes to the world.

### Save representation

Saves are ordinary JSON plaintext, readable and editable outside the game. Persistence supports geographically bounded loading and writing without reading or rewriting the whole explored world. There is no database in persistence.

The representation describes game meaning through named properties, stable content identifiers and explicit spatial structure, not C++ memory layouts or positional records wrapped in JSON strings. JSON is read into the game's own plain data where a file is loaded, and written from it where a file is saved; the rest of the game never handles JSON. No library mirrors C++ structures into JSON automatically, since a code change would then silently change the save format. Each property is grouped under the module that owns it, in saves and in content alike, so two modules never claim the same name.

Adding an optional property requires neither rewriting existing saves nor a global format version. Absence has a meaning appropriate to the feature: a layer absent from older terrain, for example, means that layer is empty. Removing a property leaves no dummy positional slot. Properties the game does not recognize neither invalidate a save nor disappear when the game rewrites the file. A genuine change of meaning may need interpretation, but there is no ladder of upgrades for every build and no automatic backup on each upgrade.

JSON is data, not a graph-mirroring system. Separate occurrences of a material reference its definition by identity; they do not require shared live-node proxies or transclusion.

Each universe is one folder holding its player file and a folder for each world. A world folder holds its world file and its chunks, one file per chunk, gathered in a folder per region. Generating, loading and saving never hold up play: they happen alongside it, ahead of where the player is going. Nothing is written per action: taking and placing only change memory. A changed chunk is written when it leaves memory, and everything changed is written when the window loses focus, on quitting, and once a minute if anything changed. Each file is replaced whole, so a crash can lose up to the last minute. A small "Saved" shows on the once-a-minute write. The menu offers Resume, Controls, Fullscreen and Quit, and quitting always saves.

**Open:** Recovery behavior, and the boundary representation used when generation changes landforms rather than materials.

## Materials, building, and crafting

Players gather materials, craft useful things, and construct homes and connections. Resource distribution makes exploration consequential. Building is a reason to explore and a way of giving discovered places lasting significance.

Worlds can have procedurally derived material vocabularies. Composition, hardness, density, durability, appearance, and uses may vary in related ways. Shared functional families can support familiar activities while unusual materials introduce new capabilities.

Materials have stable identities and definitions across terrain, inventories, crafting, and travel. Their saved meaning does not depend on their incidental position in a runtime array.

Appearance and gameplay properties draw on the same material definition where practical. Shared textures, palettes, tints, overlays, and procedural details allow distinctive materials without requiring a unique authored texture for every variation.

### Everything is a block

One object vocabulary spans terrain, carried gear, storage and placed objects, without separate item and block identities. A sword can be set down, render as a sword-shaped block, and let the character walk past it. Being a block does not mean filling a square visually or blocking movement.

The implications remain open: occupancy, mutable or unique object state, wielding and using a carried block, and the treatment of moving actors. The direction does not prescribe a particular serialization or entity architecture merely by its name.

### Crafting as construction

Crafting is construction. Placing ingredient blocks in a recipe's physical arrangement transforms them when the pattern completes, rather than opening a crafting grid. Completion is shown by a brief transformation, never an abrupt swap. Automatic completion is the starting choice; a more intentional finish action remains possible. Physical assembly stays only if it proves enjoyable in play.

Some recipes require a working surface underneath. Ingredients are consumed; the required surface remains. A forge is used by literally building over it, not by being inside a nearby bonus radius.

The forge serves metallurgy and the making of metal tools, not generic melting or firing. Whether it also serves glass, bricks and similar products is open. A fuel layer between the ingredients and the forge is not a requirement. Station requirements, fuel and ingredient geometry need further thought.

A workbench provides batching. For batch size N, each ingredient placement consumes N units from inventory, and the completed recipe yields its ordinary output multiplied by N. This changes repetition, not the material cost per result. Unfinished ingredient quantities survive saving and dismantling without changing their meaning.

Stations compose through physical layout. To batch a recipe that requires a forge, put the forge and workbench beside each other and build the ingredient configuration over both. Neither station is consumed, and no separate bulk version of the recipe is needed.

**Open:** Material-family definitions, property ranges, resource-specific behavior, and how far crafting follows material properties rather than explicit recipes.

## Modules and extension

The flagship game is an engine and a curated collection of modules, not a single binary. The product is the flagship game, not the engine: modularity is a property of the game, which lets it be rebuilt part by part while it remains itself. Flagship-first determines which functionality is developed first, not a bias toward coupling or privileged first-party code: first-party modules use exactly the mechanisms open to everyone else. A versioned flagship manifest names the modules that make up the standard game.

Interfaces are cut where the flagship's own modules actually meet, sized to what those modules need, not generalized in advance for hypothetical replacements. Shared operations are implemented once and generalized where that serves the game's uses.

### The engine

The engine is the keeper of the world and the place where modules meet. It holds what exists, where it is and when it changed; it remembers it; and it lets modules see and change it in order. It decides what nothing means: meaning, behavior and appearance come from modules. Not understanding something is never permission to erase it, which is why saves keep properties the game does not recognize and missing content keeps its identity.

The engine has five responsibilities:

- **Space.** The shape of universes and worlds: a two-dimensional, horizontally wrapping, finite-height grid in chunks and regions, the layers of its cells, and where actors are.
- **Identity.** Stable identifiers for blocks, modules and worlds, and for each thing the properties that modules own. The engine keeps a module's properties without reading them.
- **Time.** The tick, the order of changes, and the scheduling of work ahead of play, so every module sees one consistent world.
- **Memory.** Persistence: what changed is kept, and saved state is authoritative.
- **Hosting.** Loading modules, checking interface versions, resolving dependencies, and carrying data and events between modules, so that modules react to changes rather than calling one another.

The test for any feature: if it must interpret what something means, it belongs to a module; if it only keeps, places, orders or connects, it belongs to the engine. The shape of space is the engine's; what fills it is declared by modules.

### Layers

The engine keeps the layers of each cell; modules declare which layers exist and what they hold. The flagship's content declares two block layers, the foreground and the rear layer (see Background depth), and modules depend on that declaration rather than on each other. Other modules can declare more: fluids, wiring or light are each a layer of the cell. A layer is either kept or derived. A kept layer is saved and filled when land is generated; in chunks saved before its module was added, it is empty. A derived layer, such as light or decoration computed from the other layers, is worked out whenever a chunk loads or changes, everywhere including older land, and is never generated or saved. Modules reach layer data in bulk, through handles, fast enough for work done every tick.

### The flagship's modules

The flagship is made of these modules, nearly all of them core: replacing one comes close to making a different game, which the architecture permits but the flagship does not need.

- **The generator** makes new land (see The generator below).
- **Physics** is shared ground: one per world, offering the rules of movement and collision to every actor, so the player and a creature agree on what is solid. It reads the world's parameters, so gravity can differ between worlds.
- **Embodiment** is the character: one block tall, facing, reach and the forgiveness margin, moving through physics.
- **Controls** turn input into intents, such as facing, aiming, placing, hitting and opening the ring, offered to the modules that act on them. Touch or gamepad controls are other controls modules.
- **Crafting** reads arrangements and transforms them.
- **Tools** give held objects their capabilities and requirements.
- **Inventory** keeps what the character carries, including the belt of what is ready.
- **Presentation** decides how everything is represented: whether a block is drawn in 16 pixels, 256 or a single character, how the view fits the screen, and how the interface looks. Texture packs are made for a particular presentation module, as add-ons are made for a particular generator. A terminal edition would be another presentation module.
- **Scripting** hosts scripts, starting with Lua.
- **Content** modules carry the flagship's blocks, recipes and layer declarations as data. The flagship's art is a texture pack for its presentation module.

Each module has its own version; the flagship's version is the version of the game.

### Native modules

A module is a folder holding its declaration, and optionally data and a native library; a content pack is simply a module without code, and one loader handles both. A native module's library attaches to the engine at a defined extension point through a narrow C interface. Any language that can speak that interface can build one. Each extension point owns its interface and its version, checked when a module loads. There is no universal binary interface for everything, and adding an extension point never breaks the existing ones. Desktop platforms load native modules at runtime; platforms that forbid loading code link the same interfaces in when the game is built, so modding there is more limited. Changes that no extension point reaches call for a custom build of the engine.

Everything crossing the boundary is plain C values and handles, never engine internals or raw JSON: the engine does the parsing. References held by a module have defined lifetimes across chunk loading and unloading. A native module runs with the full rights of any program; installing one is a decision to trust its author.

The engine is not the only thing that offers extension interfaces. Any module can offer its own, in whatever form suits it: a declarative format, a C interface, a scripting API. The Lua module offering a Lua API to scripts and a generator offering an interface to the mods that extend it are the same shape. A mod built on a module's interface depends on that module, and on the version of the interface it was made for.

### Declarative content

Content packs carry data without programming: blocks, recipes, textures, sprites, and the definitions that modules accept, such as biomes for a generator. Carried and stored objects participate in this content path as blocks, not as a separate item type. Each part of a pack belongs to whoever defines its format, the engine or a module, and the engine passes a module's parts to it without interpreting them.

Declarative content is JSON, like saves. Its familiar syntax and tooling lower the entry cost for modders, including those coming from Minecraft. Sharing a notation does not require content definitions and saved state to have identical structures or inherit Minecraft's game model. Images remain image assets, and behavior remains code; JSON replaces neither. Art travels by convention, the same way for first-party packs and mods: a pack's images are found by name, and later packs override earlier ones.

### Scripting

Scripting languages are native modules that host scripts. A Lua module embeds an interpreter and offers the game's extension points to scripts; it is expected first, not a privileged tier. Scripts are where most modding happens: a script is one file that runs wherever its host module exists, needs no compiler, and can be confined by its host. Platforms that cannot load native modules build the scripting module in, so scripts stay portable. Language bindings derive from the C interface definitions, including LuaLS annotations, rather than being maintained as parallel type systems. A binding preserves the distinctions data needs, such as JSON null against Lua nil and empty arrays against empty objects.

### The generator

A generator is a native module that attaches at the generation extension point, the first extension point. Each world names one. It decides which factors that world's generation can consider at all, and it does the work: given a chunk's place, the world's parameters, its neighbors' boundary information and the add-on data addressed to it, it fills the layers it knows, decides the backdrop, and keeps whatever data of its own it needs for later neighbors. A module that adds a kept layer fills that layer in new land, either through the engine or through an interface the generator offers. Through the engine, generation runs as passes: after the generator, each module that owns another kept layer fills its own, reading the layers before it, so a water module can place water by the terrain without knowing which generator made it. A layer that needs the generator's internal fields, such as humidity or biome, depends on the interface that generator offers. Its result depends only on those inputs, and it runs alongside play.

Generation mods take three forms:

- **Add-ons** are declarative: new biomes and adjustments to existing parameters, each written for one particular generator in the format that generator offers. Most generation mods are add-ons.
- **Patches** have logic of their own and extend a generator without replacing it, through whatever interface the generator offers for them, native or scripted.
- **Generators** themselves. Writing one is a far larger undertaking than an add-on, but installing one is no different from installing any other native module, and a new world can use any generator installed. A later generator is likely to offer a scripting surface, which may be what most distinguishes it.

A world can move to a different generator, whether the flagship's next one or any other installed. It keeps every chunk it has; the new generator makes only new territory, settling against chunks the previous one made. Each flagship release includes only its current generator, so a player who wants a world to continue with an older one keeps that module installed. Each chunk records which generator and which add-ons produced it.

Installing or upgrading the game checks every world's generator. A world whose generator would be missing, or would no longer load because the engine has moved past its interface version, is caught then, and resolved there: by an updated build of the same generator, or by moving the world to another. It is not discovered on opening the world.

### Selecting modules

Major features are separately selectable where practical. Players can decline a generation add-on, or keep a world on an older generator, without declining unrelated later content. Dependencies are explicit; arbitrary combinations are not promised compatible.

Extensions can introduce distinct semantic actions rather than being forced to overload the base tool actions or placement. An "eat anything" mod could add a separately bound Eat action, for example. Primary use, interaction and placement are a base vocabulary, not a closed limit on all game verbs.

### Missing content

Disabling a module does not erase its saved identifiers. For example, a block saved as `crystal_expansion:blue_crystal` retains that identity even when the defining module is absent.

The provisional interpretation is a common dirt-like fallback with ordinary collision, mining, and gameplay behavior, drawn so that the missing identity is recognizable. That interpretation does not replace the saved identifier. Restoring the module restores the original interpretation wherever the content has not been changed. Deliberate player actions can modify or destroy it.

**Open:** Module declaration and compatibility details; which extension points follow the generator, and the shape of each interface; how actions are registered and bound; and the final appearance and behavior of unknown content.

## Runtime and presentation

The engine is C++20 with SDL3 and CMake; the flagship's scripting module uses standard Lua. The game is platform-neutral: its code depends on no single operating system, and supporting a platform means adding a thin surface for it, not reworking the game. Linux, macOS, Windows, Android, and iOS are all intended.

The build never downloads its own dependencies: everything it needs is installed on the machine or kept in the repository.

The world model and game logic remain independent of SDL-specific presentation details. The flagship uses pixel-art tiles and sprites. The world and its content are not defined in terms of either pixels or terminal glyphs.

The engine is the program a player starts, and it runs the frame. Each frame it lets the presentation module, which owns the window, collect what happened, passes input on to the controls module, advances time, and asks presentation to draw. The window, the display and input devices belong to those modules; what the engine itself needs from the operating system, such as file locations and loading module libraries, is a small platform-specific part of the engine. On platforms where the operating system drives each frame, that part answers the operating system's calls instead of running a loop of its own. Supporting a platform therefore means that small part of the engine plus presentation and controls modules made for it.

Playing together starts as two players on one screen in one process: a second set of keys and a shared view, with no synchronization model needed.

**Open:** Mobile packaging, and whether networked multiplayer becomes part of the game.

## Art, licensing, and community

### Visual direction

Representation belongs to the presentation module. What follows is the direction of the flagship's presentation module and the texture pack made for it.

The target is appealing fantasy pixel art, not deliberately coarse placeholders or a nostalgic console or debug-screen treatment. Terraria is a reference for the broader visual experience; Habitica is a reference for compact, enjoyable character art and its fantasy register, not merely a resolution measurement. These are references for qualities, not assets to clone or templates for other game systems.

Art is drawn at 32 native pixels per block, with recognizable, expressive shapes and color clusters composed on that grid rather than a larger illustration shrunk until it is barely recognizable. Colors are rich, not dusty hues with pastel highlights. Shading within a material stays subtle and describes its form; color richness does not require exaggerating local contrast. Cartoon proportions are expected: a tool can be large against the body, and a crawl is a squash, not the standing figure laid flat.

A handful of poses share one 32x32 pixel map: standing, walking, jumping, falling, crawling and striking in profile; a back view with its own two-step shuffle for moving along the wall; an upward strike from behind; and, in midair with S held, a leapfrog posture that carries the block beneath the body rather than on the back. Left is the mirror of right. The held object is in the camera-side hand, so turning around switches hands. The back view still shows the held object. A tool is one image drawn as the standing pose holds it; the game moves it to each pose's hand, mirrors it and turns it for a strike, so every tool, including a mod's, works in every pose. Building materials are recognizable when carried. A front view belongs to customization, for inspecting clothes; play does not need one.

The silhouette permits later skin, hair, clothing and armor overlays on the same pose maps. That calls for consistent drawing coordinates, not a rig or an elaborate animation system. The character canvas is a stable standard, so avatars and overlays made for it can travel between games through mods.

World art renders crisply at integer scale without smoothing its source pixels. The view is close and personal while the world is vast: the game picks the largest whole-number scale that still shows a comfortable minimum of the world, about 30 blocks across on common displays, so a larger display shows somewhat more. The interface anchors to the edges and corners of whatever view results. Placeholder art is unfinished art on the same grid, never a different resolution or visual register.

### Licensing and community

Code and the prototype's original material data and code-drawn placeholder visuals are licensed under MIT. This does not decide the license for future flagship artwork. Other content and artwork have their own licensing decisions; their licenses and provenance must be clear.

Contributed and imported art may carry any Creative Commons license that leaves the game able to use it. Attribution and share-alike licenses suit it: share-alike binds the art and its modified versions, not the game around it. Non-commercial licenses are discouraged, since they close distribution doors as well as commercial ones. No-derivatives licenses are unsuitable, because tints, palettes and overlays modify the art they are applied to.

Placeholder art can establish readable geography and interactions before the visual identity is complete. AI-generated artwork may be used with transparent provenance. Human-created replacements and texture packs are encouraged, including compatibility with procedural palettes and material variation.

Community selection of future flagship artwork and an ongoing relationship with an artist are possibilities, not arrangements already made. Previous art can remain available where its licensing permits.

**Open:** The license of flagship artwork, the remaining asset set, community selection and compensation arrangements, and commercialization. Monetization is not a prerequisite for making the game.

## Possibilities

These directions are worth keeping in view without committing to them. None has a settled shape or a place in the sequence, and listing one does not establish its inclusion.

- **A construction assistant.** A separate, player-controlled helper in the world, not a remote cursor and not extra reach for the player. It can only place, from its own limited cargo, and works within the visible screen while the camera stays with the player. It might fly, perhaps through an upgrade. Its controls would need reconciling with arrow-key interface navigation, and its form, deployment, cargo handling, behavior off screen and place in progression are open. Unaided building must remain forgiving and useful without it.
- **The belt as gear.** Something worn and upgradable that adds carrying capacity, rather than a view into the backpack.
- **Light RPG elements.** A modest skill tree, and character customization of skin, hair, clothes and armor as overlays on the shared pose maps. A few alternate player characters, such as one with a purple shirt and long hair, could come before full customization.
- **Tools and activities.** Tools that enable breaking particular blocks or constructing particular structures, and activities such as fishing.
- **Risk.** Health, hunger, stamina, suffocation, combat, falling, lava, fire and related hazards, serving exploration, homes and connections without gating major systems behind combat. Restore points belong with hazards, if they come. Hidden lava sources in the rear layer, released by mining, are one such hazard.
- **New kinds of place.** Urban biomes, and people who live in the world. A rare biome that lifts the familiar surface into a sky continent, with the next biome continuing from the lower ground beneath it and slopes bringing altitudes back to normal within several chunks.
- **Hand-built chunks.** A mod that is a single hand-made chunk, set at a predetermined unexplored place as if already discovered, with the land around it generating to meet it. Leaving some of a hand-made chunk's cells to the generator would free such places from the chunk's size.
- **A terminal edition.** An ASCII or Unicode presentation of the same game, secondary to the pixel-art flagship.
- **Networked multiplayer**, beyond two players sharing a screen.
