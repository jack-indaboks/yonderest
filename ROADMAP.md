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

The first-expedition prototype supplies region-specific resources, automatic spatial crafting and physically combined work surfaces, including workbench batching over a neighboring forge. Existing terrain remains authoritative. The generation-update demonstration currently covers material transitions on unchanged geometry; arbitrary landform changes remain ahead. These are implemented parts of the loop, not acceptance of the whole milestone.

### Keyboard homebuilding prototype

The implemented keyboard homebuilding prototype supports building and backing a small home, forgiving scaffolding and switching readied objects while walking. The character is one block tall, with direct forward/background/downward interaction rather than a remote target or mouse cursor. Existing homes and world changes survive the format transition and subsequent save/resume.

The keyboard homebuilding prototype implements background construction, forgiving underfoot jump-building, and a minimal keyboard inventory/ready-selection interface. The readied subset is marked in the one inventory; its radial selector uses J/K and H for Hand without taking movement away from WASD. Outside the chooser, J hits/gathers and K interacts; L places and I manages inventory. W turns toward the background instead of being held. S crouches or crawls and aims beneath the feet, including in the background when facing it. Placement needs direct access and empty space, not attachment to another block. Foreground-only crafting keeps rear walls out of ingredient matching and station composition.

The trial uses held semicolon for sprint with an unchanged jump impulse, P for pause, held-J gathering and one placement per press. One small craftable pick and tool-gated ore make tool use testable without commissioning a progression system. [DESIGN.md](DESIGN.md) describes intended interaction; [README.md](README.md) describes the current executable, save migration and prototype choices. The author reports that unaided building is less awkward than anticipated. Falling still feels floaty despite acceptable jump height, and crafting did not get a playtest because the recipe reference was not discoverable enough. Wider inventory/storage and tool designs remain open.

### Readable first session refinement

Start the game, discover a recipe, make a pick, gather copper and return to improve a backed home without consulting the README. This is a refinement of the existing playable loop, not another feature suite. The design is ready to guide the pass; it is not a claim that the changes below already run.

The pass brings together:

- The chosen 16px art grid and the author's supplied character and pick: `scratch/sprite-final-right-16px.png` and `scratch/sprite-final-pick-16px.png`. These are the author's edited originals; leave them untouched and derive the handful of remaining side/back movement and action poses from them. Add visible held blocks/tools and a coherent visual treatment of existing materials and rear walls. The source files are currently in ignored scratch: the builder must copy the selected assets into the tracked shared content path so a clean checkout can run without scratch. Other scratch drawings are not automatically accepted. Use crisp scaling rather than an unrelated first-party-only asset path.
- A small HUD over the world view in a screen corner: clickable recipe-book and backpack icons labeled R and I. UI works with mouse or arrow keys and Enter, without requiring a pointer or restoring mouse world targeting. Remove permanent instruction/debug bands and all F-key bindings; provide discoverable menu access to the application functions they previously served.
- A small recipe overlay that can stay visible while building. R toggles it; Left/Right and optional mouse controls browse. It does not pause play, repurpose world action keys, require a pin/confirm step or change Escape's behavior. Actual open dialogs take precedence for UI navigation. Recipe shapes and station requirements must be readable without a wall of text.
- Clear quantities in the hand picker and inventory, legible unavailable readiness marks, and brief save feedback separate from actual pause state. This is not a full storage or inventory redesign.
- Less floaty falling while preserving the liked jump height and the forgiving building interaction. Recheck running jumps and both forward and underfoot placement together after movement tuning.
- Open sky above ground and material-matched natural background blocks underground. Natural backing is recoverable material in the same layer as built rear walls, not scenery. Removing foreground exposes it; breaking it exposes the void, not another generated underground texture. Persist removals and preserve already-explored terrain, including empty background cells; demonstrate new backing in newly generated territory rather than silently retrofitting old saves.

Carry forward the two already-identified implementation fixes: content copying must carry deletions as well as additions/edits, and generation transitions must take boundary conditions from immediate neighbors rather than scanning the whole old-territory footprint. This is not a commission for a general code audit or a universal solution to future landform changes. Keep the README focused on building, running, controls and actual limitations.

Review the actual result:

- Complete the first-session crafting/home-improvement loop using only keyboard input, then verify optional mouse UI navigation. Nothing requires an F-key or a mouse operation in the world.
- Consult and change the corner recipe while building. Test Escape with the reference alone visible and with a real dialog open; world actions must not leak through dialog activation or become recipe-navigation actions.
- Inspect actual native assets and the rendered application, not merely successful screenshot exports. Held objects, side/back poses, rear walls and materials must read at gameplay size; typography and icons must be clear without consuming the world view.
- Make side-held and back-held composites the first visual checkpoint, before extending the sprite into movement/action poses. The supplied character and pick anchor the design; they are not a complete approved pose sheet. Derive these views without redesigning the character, and bring an uncertain pose back for review rather than propagating a new interpretation through every animation.
- Repeat early S+Space+L placement, forward/background airborne placement, switching while walking, backing away and running jumps. Human responsiveness and feel remain the test of the refinement, not just whether each input handler runs.
- Load an existing shelter, improve it, save and restart. Foreground construction, placed rear walls, carried quantities and readiness must remain coherent; visual changes do not authorize regenerating saved terrain.
- In newly generated underground terrain, remove foreground, then gather the exposed background material. The resulting opening shows the void and remains empty after save/restart; no third terrain layer appears. Verify that existing saves did not acquire new backing.

The builder owns implementation, build/test/refinement and accurate build/run documentation. Surface meaningful implementation choices and their costs; bring back questions that change intended design before building on them. Return a runnable prototype, actual behavior, remaining limitations and a recommended next step. Passing checks do not substitute for looking at the result or for human playtesting.

The assistant remains a deferred possibility, not a necessary upgrade to make early building tolerable. This pass excludes its implementation, character customization, full tool progression, RPG systems, health/hunger/stamina and other survival hazards, new fluid/climbing systems, and a general crafting rebalance.

### Next refinement: JSON saves and content, less plumbing

Bring saves and the existing declarative content in line with DESIGN's Save representation. This bounded build pass is implemented and awaits the review below.

- Saves, saving and the pause menu as DESIGN's Save representation describes them. The conversion preserves stable content identities, both block layers, quantities in unfinished assemblies and the distinction between saved and ungenerated territory.
- Convert the author's existing world once, outside the game, leaving the SQLite source untouched.
- Convert existing block, recipe, terrain, character-pose, module and flagship definitions to JSON through the same first-party/community content path. Preserve their meaning and stable identities, including module selection and generation opt-outs. Replace the old custom parsers rather than keeping parallel legacy syntax support.
- Choose the JSON library as DESIGN's open item frames it, and report the choice and its integration costs.
- Keep Lua implementation out of this pass; the later scripting API can use the C++ JSON path.
- Carry the audit's concrete corrections into this round: put newly generated tree trunks in the background without rewriting saved terrain; remove construction's sticky facing state; prevent the content mirror from deleting its own source in an in-source build; correct zero-surplus crafting so it does not create carried stock; and stop carried icons from sharing a cache entry merely because their display names match.

Review a saved home by reading and editing JSON, then loading and resaving it. Demonstrate that an absent optional property receives its intended meaning, an unfamiliar additive property does not reject the save, and ordinary field additions do not trigger a migration. Recheck the building/crafting loop, module selection and missing-content identity, chunk loading, save/resume and the converted home's continuity. Verify that converted content retains the same generation, recipes, art references and module behavior. Update build/run documentation to describe what actually runs. This is a JSON conversion and removal of unnecessary plumbing, not a general code audit, gameplay redesign or graph store.

### Public readiness

The repository becomes public once its front door is right, with or after the JSON conversion above: the public tip must not carry the SQLite dependency, though earlier history may show it. The author changes the visibility himself. This is a small pass over what a newcomer meets, not a release, an announcement or a claim that the game is further along than it is.

The front door is what a first glance lands on: the repository's name, its one-line description and the top of the README. The game does not have to reach anything first. An early prototype that plainly says it is one is ready to be found.

The pass brings together:

- A README cut to what an arriving player needs: what the game is, how early it is, that it builds on Linux only for now, how to build and run it, the controls and the actual limitations. It opens with the plain statement of how early this is. Design reasoning stays in [DESIGN.md](DESIGN.md) and sequence stays here.
- A one-line repository description, proposed by the builder and set by the author.
- Tracked documents that agree with what runs and with each other. Everything tracked becomes public, this roadmap included: passes that have landed read as landed, and the art is described as one body under the MIT license, without distinguishing hand edits from generated work. `scratch/` remains ignored and its drafts are not part of the project.
- A reading of the whole history and tree for what should not travel with a public repository: machine paths, session or conversation references, saved worlds, unintended identities, and anything belonging to another project. Both `archer_indaboks` and `jack_indaboks` are intended identities. Findings come back with their cost; whether any of them justifies rewriting history is a decision, not a cleanup step.

Review the actual result:

- Clone the repository into an empty directory, follow only the README, and reach a running game.
- Read the name, the description and the top of the README cold. Within a minute they should say what this is and how early it is, and promise nothing the executable does not do.

This pass excludes packaged releases, other platforms and any announcement.

#### Order of work

1. Land the JSON refinement and the concrete corrections above. Replace the database and custom syntaxes rather than adding parallel persistence paths. Implement DESIGN's file-writing behavior, not the old database transaction model reproduced in files. Retain meaningful content/module identities and generation provenance, not obsolete serialization machinery.
2. Reconcile the tracked documents with the resulting executable. Mark landed refinements as landed, remove superseded instructions and private working references, and describe code, content and art as one MIT-licensed body. Cut the README to the newcomer-facing material above. Proposed repository description: "An early native 2D fantasy sandbox about exploring, building, and making places your own." The author sets the final description.
3. Complete the contextual publication review of the current tree and reachable history, including tags, commit metadata and asset metadata. A filename or keyword inventory is not clearance. Return actual exposures and the cost of addressing them; do not rewrite history automatically or remove the two intended identities.
4. From a fresh clone of the candidate commit, follow only the README to a running, rendered Linux game, without local saves, scratch assets or undocumented setup. Inspect the actual rendering and do a short disposable build/craft/save/reopen check. Use checks that answer the current changes, not an accumulating release-certification suite. This does not require the author to build an elaborate home or undertake extensive playtesting.
5. Return the public-readiness recommendation, concrete unresolved matters and the candidate commit. The author decides any history treatment, sets the description and changes visibility. A local commit is not publication; this pass does not authorize a push or visibility change.

## 3. The connected universe

Extend the established loop with additional persistent worlds, noncombat portal construction, and different resource ecosystems. Homes and outposts become a connected geography across worlds.

This stage develops the procedural material model and world identities far enough that reaching another world changes what players can find and do, not just what colors they see.

## Further reach

Deeper underground environments, occasional sky biomes, richer Lua extensions, and broader native extension support can grow with the game. Their exact order is not fixed by the headings above.

Other platforms, a terminal presentation, multiplayer, and community art arrangements have no committed milestone here. They should receive their own scope when taken up rather than be bundled into an indefinite final phase.

The construction assistant is a deferred possibility: a separately controlled, place-only helper with limited cargo and an on-screen working area around the player-centered camera, not extra player reach. Its necessity, place in progression, controls, acquisition, locomotion and other questions remain open. Current UI arrow-key navigation is not displaced by its earlier control proposal. Its implementation is not assigned to the next pass or a particular release milestone. Light RPG progression, character customization, tools and fishing, inventory/storage, and health or environmental hazards are also unsequenced design threads, not newly committed release requirements. [DESIGN.md](DESIGN.md) holds their intent and open questions. Their mention here assigns no priority among them.
