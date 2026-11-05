# What is missing for the 3D and for playing the character (analysis, 2026-11-04)

The user's request, 2026-11-04: "do a new analysis of anything missing for 3D and getting control of the
main character ingame". This file answers it from the code as it stood at the end of the
`game/demoevent.cpp` run. Its order is what the code needs, and it is a proposal: the queue in
`tools/reverse/data/tus.json` and "Next up" in open-items.md stay the user's.

**How the numbers were made.** `tools/reverse/reach.py <roots>` walks the direct calls under a function
and counts the game functions with no reconstruction yet (library code left out). It cannot see a virtual
slot or a script native, so every number is a **floor**: good for comparing two roots, not an estimate of
work. Instruction counts are MIPS instructions in the ROM. Refresh them with the same command, since they
shrink as TUs land. A stub with address 0 (`StageCharacter::StageCharacter`) reads as written; its callees
are still counted.

## Short answer

- **To see the opening properly:** `ModelPose`, then the sub-scene's objects, then skinned meshes. The
  event starts now; its camera and its characters are bones, and a pose has no bones yet.
- **To walk the character:** five things have to exist together, and nothing of them can be seen working
  alone: the pose, collision, `ScenePairTable` (the frame's four action phases), `StageCharacter` with
  its motion, ground and brain, and the player logic that reads the pad. The pad itself is ready.
- **The floor for control is about 60,000 instructions** (646 functions): the closure of the character's
  vtable, its per-frame roots and the input. The 3D list below adds 14,000 that are not in it; with the
  pose and collision the whole set is **812 functions, 84,817 instructions**. Again a floor: virtual calls
  under those roots are not counted.
- **Nothing found so far looks unreadable.** The formats (motion files, collision, packs) are parsed by
  ROM code that is in reach; the size is the problem, not a mystery. That is from reading the closures,
  not from writing them, so it may not survive the pose TU.

## What already works

Stage files load; the map's models draw with texture, depth and perspective in any aspect; the stage
script VM runs, the lights and fog are the script's own; the mission now exists, so `Root` and the other
objects' scripts run each frame; the opening event d0101 is requested by the script (proven below); the
host pad gives the game a DualShock 2 in pressure mode (`sdk/libpad.cpp`, `yamipan/io/input_system.cpp`)
and `StageMainCamera`'s step is written (never run, nothing links it).

**What a New Game run shows today (2026-11-04, second `game/playermotion.cpp` run):** the event, then the room from the linked main camera, with F_PC01 whole at his mark, playing his idle, which Move now asks for. **The chain from `CharacterWants` to moving works:** with a temporary test that filled `moveKind`, he walked, turned by `moveYaw`, ran through w_run, turned on the spot and crouched, whole, with the main camera following (`progress/20261104-f_pc01-run-test.png`), and straight through the walls. `controlStep` is whole since the third run (steps, feet, stamina, fidgets, all waiting on the ground). Two things are missing for control: **the input 0x236b90** (nothing fills the wants) and **the ground** (`CharacterGround`, so collision and floors do nothing yet). The first half of this paragraph supersedes the older state that follows. The rest of this paragraph is the earlier state. Black from the frame the stage starts. `Root` asks for d0101 (native 0x1f2: a fade to black, then the event), and the event draws nothing yet. Held off in a test build, the stage is there and correct, so this is the event's missing objects and not the map. Before the event existed, an unlinked camera stood at the world origin, empty space about 9,500 units from stage 1 (architecture.md "Where stage 1 is"), where the map objects' dirty bug had piled them up: it looked like a place and was not one (hazards.md).

## The opening: why d0101 and what stops it

**Proven from the script (stage 1's object script, entry `Root`):** the entry tests `menuMode == 1`,
then property 0x3c of its target at 0x21c8. Zero goes to 0x2558: native 0xac starts the music
`AMB_ST01_01`, native 0x1f2 asks for label 0x4a, **`d0101`**. The other way (0x254c) asks for `d0106` after
a chain of partner and player placement. So the question left open in open-items.md ("Unproven: that the
opening is the event d0101") is answered for a first entry into the stage.

**Seen in a New Game run (2026-11-04, 200 s, no crash):** the script asks, `StageFileEvent` loads the pack, the stage loop builds the sub-scene, the event's own script runs (natives 0x145, 0x159, 0x163, 0x144 reached), `DemoCamera` is built, `ModelPose::load` is called and is a stub, and the voice stream stops at `rxSndStrAdpcmOpen`. **No picture of the event exists**: the last shot of that run shows no opening, so "the opening from the event's own camera" is not done. What stops it, in order:

1. ~~`ModelPose` (step 1 below)~~ **written 2026-11-04**, with native 0x159 that starts a cut: `bn_top` moves through the cut in a New Game run. The screen stays black after the intro movie, so it is not seen on a picture; the solvers and the sw bones' swing are written too, tested only outside the game, since no character uses a pose yet (open-items.md). Nothing in the port calls `DemoCamera::playCutNamed` (vtable +0x264) yet either: a script native reaches it through the slot, which one is unread, and without it the bones have no cut to play.
2. ~~The event's objects~~ **written 2026-11-04 (`game/subsceneobject.cpp`), with natives 0x144, 0x145, 0x148, 0x163, 0x164:** the first pictures of the opening exist, from the event's own camera with no `YAMIPAN_CAMERA` (open-items.md). Before that: `SubSceneCharacter`, `SubSceneItem`, `SubSceneProp`, `SubSceneMapObject`, `SubSceneGateObject` (0x2bb680..0x2bc100, 42 functions, 2,703 i with `DemoStream::drawSubtitles` and the stream calls): **the event's objects are stubs and never enter the scene**, so nothing but what the parent left can draw. The VM keeps running only behind a `PC:` guard (a null target becomes the runner itself).
3. ~~The event's natives, by reach: 0x144, 0x145, 0x159, 0x163 first (0x163 is a fade).~~ All five written; `[stub] scriptNative_` no longer shows in a run of the first cuts.
4. The voice: the host's `rxSndStr*` streams are stubs, so the wait in `enterSubScene` gives up after 60 frames (a `PC:` bound). **Since 2026-11-04 a silent bridge in `rx/rxsndstradpcm.c` ends the stream after the voice's length**: before it the event never ended, because it waits for its voice. d0101 now ends at frame 1276 (51.02 s) and the stage comes back.
   **After the event the screen is black for one reason: no player, so no linked camera (step 4 below; native 0x70 is written, the character is what is missing).** The fade is right: `Root` fades in on the third frame. With `YAMIPAN_CAMERA` the room shows there (open-items.md).
5. `gsFrameFilter` 0x2849c0, `demoFilmDraw`, `demoFleckDraw`: the event's look, each needing a host pass. Subtitles are done (2026-11-04, the file's own text and times on the event's picture).


**2026-11-05:** `game/characterground.cpp` written: he stands on the floor; walls are written and not yet seen working. The rest of the 2026-11-04 text stands.

**2026-11-05, later:** ledges work with the Use button and its prompt (`game/uiitemlist.cpp`, open-items.md); ladders and gaps are written, stage 1 has none to test on.

**2026-11-05, later still:** doors are written and switched off (`kDoorsEnabled`): with them on a stage run crashes (open-items.md, Done, game/playermotion.cpp fifth run).

**2026-11-05, later still:** firearms are written and run with a fake rifle in a test: stance, aim, shots, ammo; the scope view was black until `ViewCamera` had its frame step (open-items.md, Done, game/playermotion.cpp tenth run).

**2026-11-05, later still:** New Game's first segment plays in first person: stage 1's Root forces `camview`, which has its frame step now, and the scope shows the zoomed room (open-items.md, Done, game/viewcamera.cpp). Root's property stores work; its property reads do not yet.

**2026-11-05, later still:** the melee stance and blows work in New Game on stage 1 (R1, then Cross); nothing there can be locked or hit yet, and the camera in the stance is not proven (open-items.md, Done, game/playermotion.cpp ninth run).

**2026-11-05, later still:** a character's properties and its status block are written, so stage 1's scripts read and store them: Root and F_PC01 go on to natives 0x3a, 0x49, 0xcf, 0x3e, 0x3f (open-items.md, Done, game/playermotion.cpp twelfth run).

**2026-11-05, later still:** stage 1's natives are written but 0x13c, 0x13d, 0x5b and 0x20f, and the notices show: the first tutorial line is on screen in New Game (open-items.md, Done, game/playermotion.cpp thirteenth run).

**2026-11-05, later still:** he opens a door and walks through it into the next room with the pad alone; game/playermotion.cpp is closed at `verify` (open-items.md, Done, fourteenth run). The queue goes on to game/stagecharacter.cpp, then game/characterbrain.cpp.

**2026-11-05, later still:** the brain is written (game/characterbrain.cpp, `verify`): the class, its eight script slots, its commands and the forwarding runner. Nobody walks by themselves yet: the route 0x22a310..0x22f270 is stubs, and 0x21b9b0 that gives a brain its scripts is unwritten (open-items.md, Next up).

**2026-11-05, later still:** game/stageplayer.cpp, `verify`: `beforeScripts` / `afterScripts`, visibility, sight and hearing are written, and the volumes enter the scene. **The "move" 0x20d8c0 in the table below is `updateVisibility`: it moves nothing.** Nobody looks or listens yet: the route is next for that (open-items.md, Done).

## The order for control

Each step needs the ones above it. Sizes are the unwritten closure of its roots.

| # | what | roots | floor | what it gives |
|---|---|---|---|---|
| 1 | **Pose**: bones, motion files, playback, the solver | `ModelPose::load` 0x14ff90, `update` 0x14f4b0, `play` / `playFile`, `solve` 0x14e4d0, ctor, `ModelMotion` | 57 functions, 6,640 i | The camera's bones, the bone map (`bn_head`, on which `StageSound` and `onLinkChanged` depend), every animated object |
| 2 | ~~**Collision**: map faces, spheres, segments, heights~~ **written 2026-11-04** | `StageCollision::sphereHits` 0x232870, `segmentHits`, `raycast` 0x233810, `heightAt` 0x2342c0, `getNormal`, `CollisionFile::relocate` 0x23ae40, `CollisionBody` 0x1f6690, `SceneRecordBuckets::collide` 0x1f5250 | 32 functions, 4,533 i | Standing on the map. Also `setupMapObject`'s collision half, left out so far |
| 3 | ~~**The frame's phases**~~ **written 2026-11-04** | `ScenePairTable` 0x171300..0x1718xx: `add`, `actionStart` / `Control` / `Movement` / `Terminate`, called by `StageScene::update` and stubbed | 14 functions, 356 i | Without it no character part is ever stepped, whatever else is written. The smallest step on this list |
| 4 | **The character, built** (0x17b3e0..0x17df60 and the motion core written 2026-11-04; the motion's links, the ground and the brain are next) | `StageCharacter` ctor 0x17de30, `setup` 0x17c700 (under it: the motion 0x1ed410, 1,638 i; the ground 0x1f3d20; the parameter tables 0x17b3e0, 0x17bd10; the gear 0x216aa0) | 223 functions, 24,586 i | A character in the scene. **Native 0x70 must come with it:** `Root` calls it with `F_PC01`, and that is what reaches `setPlayer` and links the main camera; `setupCameras` does so only in a scene with no script. Then `YAMIPAN_CAMERA` can go |
| 5 | **The character, each frame** | `beforeScripts` 0x21c7c0, `afterScripts` 0x21c310, `canSpawn`, `spawn` 0x218630, `stageUpdateCharacterMotions` 0x17df60, `PlayerMotion::refresh` 0x1e66c0, `CharacterBrain::update` 0x1dd330 | with 4: 274 functions, 28,501 i | It stands, turns and animates |
| 6 | **The player**: pad to action (input, the frame's step and the wants written 2026-11-04: he walks, through walls; the move 0x20d8c0 and the action states are left) | `CharacterInput` 0x239c60, its reader 0x239d50 (710 i) and **the frame's step 0x236b90 (0x2f00 bytes): it writes `CharacterWants`, the block the conditions read**, the move 0x20d8c0 (887 i), the action states 0x21cc70 | 19 functions, 3,495 i for the input half, and 0x236b90 on top: the old count missed it | **Control.** 0x239d50 reads `GamePad`, the menus and the scene's input focus; 0x20d8c0 applies `getMoveScale`. Conditions and Walk are written: this is the last piece before the pad moves him |
| 7 | **The natives of stage 1** | 57 natives in the object script, 6 written | 137 functions, 8,735 i | The stage's logic. `F_PC01` is the player's own entry: 0xf7, 0xf8, 0xb5, 0x6f, 0xaf, 0x53, 0x7a, 0x45, 0xb4, 0x94, 0x3a. An unwritten native pops and skips, so this can go by reach (`[stub] scriptNative_<id>`), one group per run |
| 8 | **Everything the character's vtable reaches** | `reach.py --slots 0x31227c 180` | 513 functions, 44,878 i; with 4..6, 646 functions, 59,794 i | The honest floor: it holds steps 4..6 and, by their position, probably the AI, weapons and sight-jacking (not read) |

Steps 4..6 are unnamed code: 246 of the 274 functions have no name yet, so each run names as it goes.
**Step 3 did show on screen, against this file's first guess: d0101's characters move since `game/scenepairs.cpp`. The map's collision (step 2) is written and agrees with brute force on stage 1's data; nothing shows it until step 4 with its draw.**

## The 3D list

Grouped by what the picture needs, with what is known about each. Together: 144 functions, 17,727 i;
91 functions, 13,951 i of them are not in the character's set.

**Needed to see characters at all**
- ~~**Skinned meshes**~~ **written 2026-11-04** (`rena/rena_draw.cpp`): the boy of d0101 and the sea of stage 1
  are on screen, and `game/scenepairs.cpp` moves them (architecture.md "Skinning").
- **The per-object draw**: `StageNodeTree::draw` 0x190a80 (4,502 i with its callees) and `update` 0x190a90
  (3,457 i). Particles, smoke, lamps. Today every object draws by the map walk only.
- **`SceneModelHolder::draw`** 0x2d0550 (2,009 i).
- **`SceneBlock98` / `SceneBufferSet`** 0x2ce0c0..0x2cebd0 (about 1,300 i): per-character blocks; setup
  needs `createBlock98`.

**Needed for a stage that is right**
- **Doors and special map objects**: `createMapObject`'s classes 0x174940, 0x28e5a0, 0x295360, 0x2a7340,
  0x2b2a60 come out as plain objects, which stand in their places but do not open; `applyDoorRecord` 0x173880, `attachToRoot` 0x173cb0. 19 functions,
  1,166 i.
- **`AreaClip`** 0x2dbd10 (2,483 i): reached every frame, a stub.
- **`gsFrameFilter`** 0x2849c0 (1,903 i, a GS frame effect): the filter of d0101 and the candidate for the
  circling thing. It writes GS passes, so by the user's rule it needs a host pass, not packets.

**Fidelity, left out of the model draw** (open-items.md has the details)
- Back-face culling (`ModelFlag_TagC`, VU1 0x128); nothing culls.
- The spotlight (VU1 0x545), the ghost's alpha (0x011) and pass 0x2e7960, the moved vertices of material
  flag 8 (0x186), fade 0x2e96a0 / 0x2e9220 (226 i together).
- `ModelMaterial_Kind1`'s two frames, the alpha fringe (0x2fdb60's second pass), mips (0x2e7240),
  shadows, `m_altClut` for `_s` textures. The record's point and spot lights are filled and unread.

**Performance, before characters.** Every vertex is transformed and lit on the game thread and uploaded
whole; a stage ran at about 10 fps in three runs of four (open-items.md). It needs a per-model cache before skinned
meshes multiply it, as the model_renderer notes say.

## What stays outside

Sound streams (`rxSndStrMem`/`Stream`/`Pcm`/`Adpcm`, SDPR2) are stubs, so the voice of an event is silent.
The game-over and clear sequences (79 functions, 6,346 i with `GameOverSequence::update`), the pause menu
state (0x207440..0x207ef0), `PlayerOverlay` (1,255 i) and the item list are needed to finish a stage, not
to walk in it.

## A proposal for the queue

**Queued 2026-11-04:** the steps below are in `tools/reverse/data/tus.json`, in this order; open-items.md "Decisions" has why. The queue is what runs read, and this list is the reasoning.

1. `game/modelpose.cpp` (0x149510..0x151020): step 1. **Also what the opening's camera needs.**
2. The sub-scene's objects (0x2bb680..0x2bc100, 42 functions, 2,703 i), then skinned meshes: the opening on
   screen, the first picture to compare with the game.
3. `game/scenepairs.cpp` (step 3) and the collision file (step 2).
4. `StageCharacter`, in slices: 0x17b3e0..0x17df60 first, then the motion 0x1e0000..0x1f0000, the ground
   0x1f3d20.., the brain 0x1dbca0..0x1df0b0.
5. The player logic 0x20a000..0x21d000, by what a run reaches.
6. The natives by reach, all along.
