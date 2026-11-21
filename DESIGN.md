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

A universe begins with one persistent world. Exploration and crafting eventually make portals to other worlds possible. Portals require substantial investment rather than mandatory combat.

The starting world does not contain every resource available in the universe. Other worlds offer different environments, resource ecosystems, and possibilities, not merely stronger replacements for everything already found. Each world contains multiple biomes; a world's identity is not one biome stretched across a planet.

The starting world is expansive. Circling it is an epic journey, not an afternoon's walk, and arriving back where one began is an achievement. Worlds reached through portals take size as one of their parameters, alongside everything else that distinguishes them: some rival the starting world, and some are small enough to hold a single biome.

Travel expands the network rather than replacing it. Existing homes and routes retain value as new worlds become reachable.

**Open:** Portal construction, destination discovery, and whether players can seek a particular kind of world or initially travel blindly.

Ordinary doors leading to separate pocket interiors are not an established feature. The inclination is away from them: a mundane door should not quietly require another dimension to make a house work in a side-scrolling world. Portals explicitly connect different places and need not follow the same convention.

## Geography and chunks

Each world is a side-scrolling block landscape with generous but finite height and a horizontally wrapping circumference measured in chunks. Traveling continuously east or west returns to the starting location. The vertical axis does not wrap.

Chunks partition the world in both horizontal and vertical directions. They are independently generated, loaded, and persisted regions, not full-height columns. Chunk dimensions use powers of two.

Vertical geography matters. Worlds have room for meaningful underground depth and occasional sky biomes, not just a surface with a shallow underside. Exploring the ground does not implicitly generate all the terrain above or below it. A later sky-generation module can populate previously ungenerated sky above familiar ground without altering saved chunks.

For persistence and generation, explored territory means generated and persisted chunks, including any loading margin beyond what the player has personally visited. The extent of that margin therefore affects which territory remains available for future generation.

All spatial generation respects horizontal periodicity: terrain, geology, climate, resources, and ecology. The first and last chunk columns are ordinary neighbors, not special borders repaired after generation. Movement and interaction likewise treat the seam as continuous geography.

Landscape features have their own scale. Hills, climate belts and regions are sized for the land they shape, not stretched to fit the circumference, so a larger world holds more places rather than broader versions of the same ones.

Floating terrain fits within the world's vertical bounds. A deep lava sea is a possible lower environment, separate from the actual world boundary; it does not imply a fluid simulation.

**Open:** Exact chunk dimensions, world circumferences and heights, loading margins, and upper and lower boundary behavior.

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

Underground, the rear layer is as varied as the foreground, with its own resources, perhaps slightly richer than the foreground's, which gives mining a reason even where the screen already shows what lies a few blocks down. Above ground, open sky lies behind the foreground.

Behind the rear layer is a backdrop that cannot be touched: a muted texture of the biome's base material, sky above ground, stone below it, and the local base material in other biomes. The backdrop is drawn from where it is, as the sky is; it is not generated or saved as blocks, so there is no third block layer. Removing a foreground block exposes the rear-layer block behind it, and removing that exposes the backdrop.

Generated rear-layer material and its removal persist as world state. Saved terrain keeps whatever rear layer it was saved with, including none. Lighting is separate work.

Crafting reads the foreground only: rear-layer blocks supply neither ingredients, required empty cells nor stations, and building a rear wall never completes a recipe. Whether the rear layer ever takes part in crafting is open.

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

A new chunk takes boundary conditions only from its immediate neighbors; those conditions carry forward whatever transition remains. At a boundary between generations, the new generator's side settles into the terrain its rules would otherwise produce within roughly one or two chunks. This applies vertically as well as horizontally, including across the world seam. The exploration-order guarantee applies while generation rules are unchanged; changing them can produce different results according to which territory was already saved.

Boundary information may include elevation, slope, geological layers, water levels, or environmental fields, according to the generator's needs. A separate post-generation blending subsystem is not prescribed.

World parameters and generated material definitions persist wherever they are needed to keep the world consistent. Save and resume preserve player position, inventory, and changes to the world.

### Save representation

Saves are ordinary JSON plaintext, readable and editable outside the game. Persistence supports geographically bounded loading and writing without reading or rewriting the whole explored world. There is no database in persistence.

The representation describes game meaning through named properties, stable content identifiers and explicit spatial structure, not C++ memory layouts or positional records wrapped in JSON strings. JSON is read into the game's own plain data where a file is loaded, and written from it where a file is saved; the rest of the game never handles JSON. No library mirrors C++ structures into JSON automatically, since a code change would then silently change the save format.

Adding an optional property requires neither rewriting existing saves nor a global format version. Absence has a meaning appropriate to the feature: a layer absent from older terrain, for example, means that layer is empty. Removing a property leaves no dummy positional slot. Properties the game does not recognize neither invalidate a save nor disappear when the game rewrites the file. A genuine change of meaning may need interpretation, but there is no ladder of upgrades for every build and no automatic backup on each upgrade.

JSON is data, not a graph-mirroring system. Separate occurrences of a material reference its definition by identity; they do not require shared live-node proxies or transclusion.

Each world is one folder holding its world file, its player file and one file per chunk. Nothing is written per action: taking and placing only change memory. A changed chunk is written when it leaves memory, and everything changed is written when the window loses focus, on quitting, and once a minute if anything changed. Each file is replaced whole, so a crash can lose up to the last minute. A small "Saved" shows on the once-a-minute write. The menu offers Resume, Controls, Fullscreen and Quit, and quitting always saves.

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

Yonderest is a cohesive flagship game built on an extensible engine. Flagship-first determines which functionality is developed first, not a bias toward coupling or privileged first-party code. First-party content and community extensions use the same fundamental extension mechanisms.

Shared operations are implemented once and generalized where that serves the game's uses. Engine responsibilities, content, and presentation have clear ownership so additional features and other games can extend the system without untangling the flagship from its internals.

The extension model has three tiers:

- **Declarative content packs:** Blocks, recipes, textures, sprites, and supported generation definitions without programming. Carried and stored objects participate in this content path as blocks, not as a separate item type.
- **Standard Lua scripts:** Gameplay extensions through the public game-facing API.
- **Native C++ extensions or custom builds:** Deeper changes and separate games beyond the scripting API.

Declarative content is JSON, like saves. Its familiar syntax and tooling lower the entry cost for modders, including those coming from Minecraft. Sharing a notation does not require content definitions and saved state to have identical structures or inherit Minecraft's game model. Images remain image assets, and behavior remains code; JSON replaces neither. Art travels by convention, the same way for first-party packs and mods: a pack's images are found by name, and later packs override earlier ones.

Lua and declarative content are the portable extension mechanisms. Dynamic native loading on mobile and a universal stable native ABI are not requirements.

Major features are separately selectable where practical. Players can decline a generation update without necessarily declining unrelated later content. Dependencies are explicit; arbitrary combinations are not promised compatible. A versioned flagship manifest defines the standard configuration.

Extensions can introduce distinct semantic actions rather than being forced to overload the base tool actions or placement. An "eat anything" mod could add a separately bound Eat action, for example. Primary use, interaction and placement are a base vocabulary, not a closed limit on all game verbs. The registration and binding interface remains to be designed.

### Missing content

Disabling a module does not erase its saved identifiers. For example, a block saved as `crystal_expansion:blue_crystal` retains that identity even when the defining module is absent.

The provisional interpretation is a common dirt-like fallback with ordinary collision, mining, and gameplay behavior, drawn so that the missing identity is recognizable. That interpretation does not replace the saved identifier. Restoring the module restores the original interpretation wherever the content has not been changed. Deliberate player actions can modify or destroy it.

**Open:** Module declaration and compatibility details, the supported generation extension surface, and the final appearance and behavior of unknown content.

## Runtime and presentation

The stack is C++20, SDL3, CMake, and standard Lua. The game is platform-neutral: its code depends on no single operating system, and supporting a platform means adding a thin surface for it, not reworking the game. Linux, macOS, Windows, Android, and iOS are all intended.

The build never downloads its own dependencies: everything it needs is installed on the machine or kept in the repository.

C++ owns world state and API contracts. Lua runs gameplay scripts against a public interface rather than accessing engine internals directly. Script-visible references need defined lifetimes across chunk loading and unloading. LuaLS annotations describe the API; where practical they are generated from its binding definitions rather than maintained as a parallel type system.

C++ handles JSON input and output and can expose the relevant data to Lua as tables and scalar values through the game API; scripting does not require a second JSON parser. The binding distinguishes JSON null from Lua nil, and empty JSON arrays from empty objects, where those distinctions matter.

The world model and game logic remain independent of SDL-specific presentation details. The flagship uses pixel-art tiles and sprites. The world and its content are not defined in terms of either pixels or terminal glyphs.

Playing together starts as two players on one screen in one process: a second set of keys and a shared view, with no synchronization model needed.

**Open:** Lua API shape and reference lifetime, mobile packaging, and whether networked multiplayer becomes part of the game.

## Art, licensing, and community

### Visual direction

The target is appealing fantasy pixel art, not deliberately coarse placeholders or a nostalgic console or debug-screen treatment. Terraria is a reference for the broader visual experience; Habitica is a reference for compact, enjoyable character art and its fantasy register, not merely a resolution measurement. These are references for qualities, not assets to clone or templates for other game systems.

Art is drawn at 16 native pixels per block, with recognizable, expressive shapes and color clusters composed on that grid rather than a larger illustration shrunk until it is barely recognizable. Colors are rich, not dusty hues with pastel highlights. Shading within a material stays subtle and describes its form; color richness does not require exaggerating local contrast. Cartoon proportions are expected: a tool can be large against the body, and a crawl is a squash, not the standing figure laid flat.

A handful of poses share one 16x16 pixel map: standing, walking, jumping, falling, crawling and striking in profile; a back view with its own two-step shuffle for moving along the wall; an upward strike from behind; and, in midair with S held, a leapfrog posture that carries the block beneath the body rather than on the back. Left is the mirror of right. The held object is in the camera-side hand, so turning around switches hands. The back view still shows the held object. A tool is one image drawn as the standing pose holds it; the game moves it to each pose's hand, mirrors it and turns it for a strike, so every tool, including a mod's, works in every pose. Building materials are recognizable when carried. A front view belongs to customization, for inspecting clothes; play does not need one.

The silhouette permits later skin, hair, clothing and armor overlays on the same pose maps. That calls for consistent drawing coordinates, not a rig or an elaborate animation system.

World art renders crisply at integer scale without smoothing its source pixels. Source-pixel density and camera framing are distinct choices: the art grid does not by itself settle viewport size, display zoom or how much world is visible. Placeholder art is unfinished art on the same grid, never a different resolution or visual register.

### Licensing and community

Code and the prototype's original material data and code-drawn placeholder visuals are licensed under MIT. This does not decide the license for future flagship artwork. Other content and artwork have their own licensing decisions; their licenses and provenance must be clear.

Placeholder art can establish readable geography and interactions before the visual identity is complete. AI-generated artwork may be used with transparent provenance. Human-created replacements and texture packs are encouraged, including compatibility with procedural palettes and material variation.

Community selection of future flagship artwork and an ongoing relationship with an artist are possibilities, not arrangements already made. Previous art can remain available where its licensing permits.

**Open:** Flagship content and asset licenses, the remaining asset set, community selection and compensation arrangements, and commercialization. Monetization is not a prerequisite for making the game.

## Possibilities

These directions are worth keeping in view without committing to them. None has a settled shape or a place in the sequence, and listing one does not establish its inclusion.

- **A construction assistant.** A separate, player-controlled helper in the world, not a remote cursor and not extra reach for the player. It can only place, from its own limited cargo, and works within the visible screen while the camera stays with the player. It might fly, perhaps through an upgrade. Its controls would need reconciling with arrow-key interface navigation, and its form, deployment, cargo handling, behavior off screen and place in progression are open. Unaided building must remain forgiving and useful without it.
- **The belt as gear.** Something worn and upgradable that adds carrying capacity, rather than a view into the backpack.
- **Light RPG elements.** A modest skill tree, and character customization of skin, hair, clothes and armor as overlays on the shared pose maps. A few alternate player characters, such as one with a purple shirt and long hair, could come before full customization.
- **Tools and activities.** Tools that enable breaking particular blocks or constructing particular structures, and activities such as fishing.
- **Risk.** Health, hunger, stamina, suffocation, combat, falling, lava, fire and related hazards, serving exploration, homes and connections without gating major systems behind combat. Restore points belong with hazards, if they come.
- **New kinds of place.** Urban biomes, and people who live in the world. A rare biome that lifts the familiar surface into a sky continent, with the next biome continuing from the lower ground beneath it and slopes bringing altitudes back to normal within several chunks.
- **A terminal edition.** An ASCII or Unicode presentation of the same game, secondary to the pixel-art flagship.
- **Networked multiplayer**, beyond two players sharing a screen.
