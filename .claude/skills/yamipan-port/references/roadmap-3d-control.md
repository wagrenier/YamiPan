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

**What a New Game run shows today:** black from the frame the stage starts, whatever `YAMIPAN_CAMERA` says. `Root` asks for d0101 (native 0x1f2: a fade to black, then the event), and the event draws nothing yet. Held off in a test build, the stage is there and correct, so this is the event's missing objects and not the map. Before the event existed, an unlinked camera stood at the world origin, empty space about 9,500 units from stage 1 (architecture.md "Where stage 1 is"), where the map objects' dirty bug had piled them up: it looked like a place and was not one (hazards.md).

## The opening: why d0101 and what stops it

**Proven from the script (stage 1's object script, entry `Root`):** the entry tests `menuMode == 1`,
then property 0x3c of its target at 0x21c8. Zero goes to 0x2558: native 0xac starts the music
`AMB_ST01_01`, native 0x1f2 asks for label 0x4a, **`d0101`**. The other way (0x254c) asks for `d0106` after
a chain of partner and player placement. So the question left open in open-items.md ("Unproven: that the
opening is the event d0101") is answered for a first entry into the stage.

**Seen in a New Game run (2026-11-04, 200 s, no crash):** the script asks, `StageFileEvent` loads the pack, the stage loop builds the sub-scene, the event's own script runs (natives 0x145, 0x159, 0x163, 0x144 reached), `DemoCamera` is built, `ModelPose::load` is called and is a stub, and the voice stream stops at `rxSndStrAdpcmOpen`. **No picture of the event exists**: the last shot of that run shows no opening, so "the opening from the event's own camera" is not done. What stops it, in order:

1. ~~`ModelPose` (step 1 below)~~ **written 2026-11-04**, with native 0x159 that starts a cut: `bn_top` moves through the cut in a New Game run. The screen stays black after the intro movie, so it is not seen on a picture; the solvers are stubs (open-items.md). Nothing in the port calls `DemoCamera::playCutNamed` (vtable +0x264) yet either: a script native reaches it through the slot, which one is unread, and without it the bones have no cut to play.
2. `SubSceneCharacter`, `SubSceneItem`, `SubSceneProp`, `SubSceneMapObject`, `SubSceneGateObject` (0x2bb680..0x2bc100, 42 functions, 2,703 i with `DemoStream::drawSubtitles` and the stream calls): **the event's objects are stubs and never enter the scene**, so nothing but what the parent left can draw. The VM keeps running only behind a `PC:` guard (a null target becomes the runner itself).
3. The event's natives, by reach: 0x144, 0x145, 0x159, 0x163 first (0x163 is a fade).
4. The voice: the host's `rxSndStr*` streams are stubs, so the wait in `enterSubScene` gives up after 60 frames (a `PC:` bound) and the event's length comes from its script alone.
5. `gsFrameFilter` 0x2849c0, `demoFilmDraw`, `demoFleckDraw`: the event's look, each needing a host pass.


## The order for control

Each step needs the ones above it. Sizes are the unwritten closure of its roots.

| # | what | roots | floor | what it gives |
|---|---|---|---|---|
| 1 | **Pose**: bones, motion files, playback, the solver | `ModelPose::load` 0x14ff90, `update` 0x14f4b0, `play` / `playFile`, `solve` 0x14e4d0, ctor, `ModelMotion` | 57 functions, 6,640 i | The camera's bones, the bone map (`bn_head`, on which `StageSound` and `onLinkChanged` depend), every animated object |
| 2 | **Collision**: map faces, spheres, segments, heights | `StageCollision::sphereHits` 0x232870, `segmentHits`, `raycast` 0x233810, `heightAt` 0x2342c0, `getNormal`, `CollisionFile::relocate` 0x23ae40, `CollisionBody` 0x1f6690, `SceneRecordBuckets::collide` 0x1f5250 | 32 functions, 4,533 i | Standing on the map. Also `setupMapObject`'s collision half, left out so far |
| 3 | **The frame's phases** | `ScenePairTable` 0x171300..0x1718xx: `add`, `actionStart` / `Control` / `Movement` / `Terminate`, called by `StageScene::update` and stubbed | 14 functions, 356 i | Without it no character part is ever stepped, whatever else is written. The smallest step on this list |
| 4 | **The character, built** | `StageCharacter` ctor 0x17de30, `setup` 0x17c700 (under it: the motion 0x1ed410, 1,638 i; the ground 0x1f3d20; the parameter tables 0x17b3e0, 0x17bd10; the gear 0x216aa0) | 223 functions, 24,586 i | A character in the scene. **Native 0x70 must come with it:** `Root` calls it with `F_PC01`, and that is what reaches `setPlayer` and links the main camera; `setupCameras` does so only in a scene with no script. Then `YAMIPAN_CAMERA` can go |
| 5 | **The character, each frame** | `beforeScripts` 0x21c7c0, `afterScripts` 0x21c310, `canSpawn`, `spawn` 0x218630, `stageUpdateCharacterMotions` 0x17df60, `PlayerMotion::refresh` 0x1e66c0, `CharacterBrain::update` 0x1dd330 | with 4: 274 functions, 28,501 i | It stands, turns and animates |
| 6 | **The player**: pad to action | `CharacterInput` 0x239c60 and its reader 0x239d50 (710 i), the move 0x20d8c0 (887 i), the action states 0x21cc70, 0x1e6d60 | 19 functions, 3,495 i for the input half | **Control.** 0x239d50 reads `GamePad`, the menus and the scene's input focus; 0x20d8c0 applies `getMoveScale` |
| 7 | **The natives of stage 1** | 57 natives in the object script, 6 written | 137 functions, 8,735 i | The stage's logic. `F_PC01` is the player's own entry: 0xf7, 0xf8, 0xb5, 0x6f, 0xaf, 0x53, 0x7a, 0x45, 0xb4, 0x94, 0x3a. An unwritten native pops and skips, so this can go by reach (`[stub] scriptNative_<id>`), one group per run |
| 8 | **Everything the character's vtable reaches** | `reach.py --slots 0x31227c 180` | 513 functions, 44,878 i; with 4..6, 646 functions, 59,794 i | The honest floor: it holds steps 4..6 and, by their position, probably the AI, weapons and sight-jacking (not read) |

Steps 4..6 are unnamed code: 246 of the 274 functions have no name yet, so each run names as it goes.
**The first run that can show anything is step 4 with its draw, so steps 1..3 go by on `verify`.**

## The 3D list

Grouped by what the picture needs, with what is known about each. Together: 144 functions, 17,727 i;
91 functions, 13,951 i of them are not in the character's set.

**Needed to see characters at all**
- **Skinned meshes** 0x2e43e0 (524 i) and 0x2e68a0 (335 i): skipped today (`boneCount != 0`). Palette: bone
  world * inverseBinds[b] * M, M = boneBinds[mesh.bone] or unit; the 7-tag batches carry bone ids and
  weights (architecture.md "The batches of a model"). Needs step 1.
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

1. `game/modelpose.cpp` (0x149510..0x151020): step 1. **Also what the opening's camera needs.**
2. The sub-scene's objects (0x2bb680..0x2bc100, 42 functions, 2,703 i), then skinned meshes: the opening on
   screen, the first picture to compare with the game.
3. `game/scenepairs.cpp` (step 3) and the collision file (step 2).
4. `StageCharacter`, in slices: 0x17b3e0..0x17df60 first, then the motion 0x1e0000..0x1f0000, the ground
   0x1f3d20.., the brain 0x1dbca0..0x1df0b0.
5. The player logic 0x20a000..0x21d000, by what a run reaches.
6. The natives by reach, all along.
