# Yonderest roadmap

This is the build sequence, not a second design specification. [DESIGN.md](DESIGN.md) describes the game. Milestones describe playable outcomes rather than elapsed time; the first prototype will establish the actual pace of AI-assisted implementation and human review.

## 1. The persistent first shelter

The runnable prototype is implemented. The author built a bunker and confirmed it survived quitting and restarting (2026-11-17). See [README.md](README.md) for build instructions, controls, verification, and current limitations.

Walk through a small wrapping landscape, find somewhere worth stopping, gather blocks, build a crude shelter, quit, and return to find it unchanged.

The prototype brings together:

- One seeded world with finite height, independently managed chunks in both axes, and continuous horizontal wrapping.
- Simple terrain with enough recognizable variation to invite choosing a building site.
- Player movement, collision, mining, placement, and a minimal inventory.
- A few materials, readable placeholder visuals, and a small shared content path suitable for both first-party and extension content.
- Chunk persistence and save/resume for the player's position, inventory, and world changes.
- A native Linux executable with straightforward CMake build and run instructions.

The practical review is the whole experience: can a player reach a place, make something there, cross chunk boundaries and the world seam, and return after restarting without losing the world's continuity or their changes?

Repository setup belongs to this build: Git, the MIT license, a focused ignore file, and a README explaining how to build and play. It is not a separate infrastructure milestone.

Rich underground content and sky biomes are not required yet, but the chunk structure preserves their place. Crafting enters if this interaction needs it; it is not required just to place gathered blocks. Lua enters when there is useful scripted behavior to express. This does not postpone the shared extension architecture or make flagship content a privileged special case.

## 2. The first playable release

Develop the shelter prototype into a small but enjoyable exploration, gathering, crafting, and building loop:

- Several distinct biomes and reasons to travel between them.
- Basic crafting and a useful range of materials and construction choices.
- Declarative content packs and a usable module configuration.
- Reliable chunk loading, persistence, and return journeys across a world worth inhabiting.

The experiential question is whether discovery produces places the player wants to make their own, and whether traveling farther adds possibilities without discarding that attachment.

Generation-update continuity and missing-module behavior need concrete demonstrations as their supporting module mechanisms become usable, rather than remaining architectural promises.

The first-expedition prototype implements the first bounded pass toward this release: gather region-specific resources and return to improve an existing home. It experiments with automatic spatial crafting and physically combined work surfaces, including workbench batching over a neighboring forge. Existing terrain remains authoritative; controls remain unchanged apart from the interactions needed to use the new features. The next review is human playtesting of that loop, not acceptance of the whole milestone. The generation-update demonstration currently covers material transitions on unchanged geometry; arbitrary landform changes remain ahead.

### Keyboard homebuilding prototype

The next playable review is to build and back a small home entirely from the keyboard, make scaffolding without precise jump/place timing, and switch readied objects smoothly while walking without opening full inventory management. The character is one block tall, with direct forward/background/downward interaction rather than a remote target or mouse cursor. Existing homes and world changes must survive the transition and subsequent save/resume.

The keyboard homebuilding prototype implements background construction, forgiving underfoot jump-building, and a minimal keyboard inventory/ready-selection interface. The readied subset is marked in the one inventory; its radial selector uses J/K and H for Hand without taking movement away from WASD. Outside the chooser, J hits/gathers and K interacts; L places and I manages inventory. W turns toward the background instead of being held. S crouches or crawls and aims beneath the feet, including in the background when facing it. Placement needs direct access and empty space, not attachment to another block. Foreground-only crafting keeps rear walls out of ingredient matching and station composition.

Trial held semicolon for sprint with an unchanged jump impulse, P for pause, held-J gathering and one placement per press. One small craftable pick and tool-gated ore make tool use testable without commissioning a progression system. [DESIGN.md](DESIGN.md) describes the input grammar; [README.md](README.md) describes actual controls, save migration and prototype choices. Human feel review remains ahead; the wider inventory/storage and tool designs are not being declared complete.

Review the result by playing, not only by verifying individual inputs:

- Gather blocks, build a small home and give it a nonblocking rear wall, using no mouse for world interaction or required inventory operations.
- Hold S before jumping and place beneath the character, including an early press that waits for clearance. Confirm forward/background placement still targets the cell addressed by the one-block-tall character at the moment of the press. Try placing in midair without attachment to other blocks.
- Empty the hand, open the radial chooser, change selection with J/K, and equip while walking. Selection must not also fire a world action, and readied entries must remain part of the same inventory.
- Try sprinting and running jumps with the intended right-hand position; judge sustained pinky comfort as well as responsiveness.
- Load an existing shelter save, then save and restart after background construction and inventory changes. Foreground construction, new rear walls, carried quantities and player state must remain coherent.

The builder owns implementation, build/test/refinement and accurate build/run documentation. Return a runnable prototype, actual behavior, remaining limitations and a recommended next step; do not present passing automated checks as a substitute for the human feel review.

The construction assistant remains deferred and is excluded from this early-game pass. This prototype is not a commission for full tool progression, RPG systems, health/hunger/stamina or other survival hazards, or a general crafting rebalance.

## 3. The connected universe

Extend the established loop with additional persistent worlds, noncombat portal construction, and different resource ecosystems. Homes and outposts become a connected geography across worlds.

This stage develops the procedural material model and world identities far enough that reaching another world changes what players can find and do, not just what colors they see.

## Further reach

Deeper underground environments, occasional sky biomes, richer Lua extensions, and broader native extension support can grow with the game. Their exact order is not fixed by the headings above.

Other platforms, a terminal presentation, multiplayer, and community art arrangements have no committed milestone here. They should receive their own scope when taken up rather than be bundled into an indefinite final phase.

The planned construction assistant is deferred: a separately controlled, place-only helper with limited cargo and an on-screen working area around the player-centered camera, not extra player reach. Its place in the progression from hand-built shelters to easier construction is retained, while its controls, acquisition, locomotion and other open questions stay open. Its implementation is not assigned to the next pass or a particular release milestone. Light RPG progression, tools and fishing, inventory/storage, and health or environmental hazards are also unsequenced design threads, not newly committed release requirements. [DESIGN.md](DESIGN.md) holds their intent and open questions. Their mention here assigns no priority among them.
