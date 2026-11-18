# Unlocking the frame rate: a first analysis (2026-11-09)

The game runs at 25 fps because the PAL ROM waits two 50 Hz fields per frame. This file has what sets that rate,
how much of the game measures time and how much counts frames, the three ways to go faster, and the first steps of
the one recommended. **The user chose A (2026-11-09); steps 1 to 4 are done ("Option A so far" below).**

## Where 25 Hz comes from

- **The field clock.** `gfxInitRenderTargets` 0x12e3f0 (and `gfxFlipDisplay` 0x12e0e0 on a display switch) call
  `sceGsResetGraph(.., SCE_GS_PAL, ..)` (`game/gfxtarget.cpp` 230, 236); `sdk/libgraph.cpp` turns that into
  `InterruptController::setVideoStandard(Pal)`, and the interrupt thread pulses a vblank every 1/50 s
  (`yamipan/ee/interrupts.cpp`, `kPalFieldRate`).
- **The frame interval.** `main` passes `threadSysInit(gameFrameReady, kVsyncsPerFrame = 2)`. Each vblank,
  `frameSyncThread` 0x131930 asks `gameFrameReady` 0x12db70: ready when the main loop waits and 2 fields have passed.
  `frameSyncWait` 0x131260 then returns the fields the frame took, and `gameMainThread` runs one frame.
- **The game clock.** `GameSystem::flipFrame` calls `MasterClock::update(vsyncs)` 0x1352d0. The master clock is
  built as `MasterClock(12, 2, variableRate = false, ..)`: **12 ticks a frame whatever the fields**, at 300 ticks a
  second (`kClockTimePerSecond`, data 0x300e48). 12 x 25 = 300: the clock is in real seconds on PAL. A late frame
  still gets 12 ticks, so the game slows down rather than skipping.
- **The present.** The finished frame goes through `UiCanvas` to `Renderer::submit` (the newest frame wins; an
  unread one is dropped). The render thread presents at the monitor's rate and shows the last game frame again when
  none is new. So the window already runs at 60 Hz or more; it shows each game frame two or three times.

## How much of the game is time-based (measured)

**A time-based core.** The ROM has 113 calls to `Clock::getDeltaSeconds` 0x1353d0 and 105 to `getSeconds` 0x135420
(`ghidra.py refs`). The data is authored in 30 fps units and converted through the clock:
- animation: `ModelPose` advances in 30 Hz motion frames that callers compute as `dt / (1/30)`
  (`StageCharacter::moveStep`, `StageItem`, `MapObject`, `StageDoor`); root motion moves the characters;
- AI and routes: `characterroute.cpp` (97 clock uses), `CharacterBrain::waitFrames` (30 fps frames through dt);
- scripts: `scriptFramesTo30` / `scriptFramesFrom30` 0x1d37c0 convert with `(int)(1 / dt)`;
- the mission clock: `stageFps` 0x196300 = `(int)(1 / dt)`;
- character data in thirtieths (`CharProp_ReviveTime`, `ShotInterval`, `AttackInterval`, ...).

**Systems that count frames** (first pass; grep for "a frame", `frames`, fixed steps; not exhaustive):

| system | where | what counts frames |
|---|---|---|
| camera ease | `stagecamera.cpp` 461, 689 | `approachVec3(1 / followDivisor, kFollowMaxStep)` once a frame |
| lock-on turn | `playermotion.cpp` 6977 | `kLockTurnLimit`, 6 degrees a frame |
| particles | `stageparticle.cpp` | life, emit delay, velocity, gravity, colour keys in frames; `substeps` a frame; critters turn 6 degrees a frame |
| item physics | `itemphysics.cpp` 46, `stageitem.h` 113 | gravity 1 unit a frame each frame; `m_speed` units a frame |
| sky mist | `skymist.cpp` 88 | lives in 30 fps frames (a second read: aged by dt, time-based; framerate-inventory.md) |
| fades | `scenerender.cpp` 45, `stagesound` | 30-frame scene fades; music fades in frames x 25 / 30, pause fades 10 frames |
| voices | `charactersound.cpp` 717 | `kFramesPerSecond = 25`: PAL written into the code |
| play time | `stageresult.cpp` 162, `uigame.cpp` 272 | `playTime++` a frame, shown / 25; **saved to the memory card** |
| pad | `pad.cpp` | repeat after 7 frames, then every 2 |
| UI | `uiwidget`, `navimenu`, `kunitoris`, `stagemenu`, ... | speeds in pixels a frame, blinks and timers in frames |
| cars, cut scenes | `stagecar.cpp` 1020, `demoevent.cpp` 439 | `advance(1.0f)` and a fixed cut speed a frame |

**Traps in the time-based core:**
- The clock counts whole ticks: a rate is exact only when 300 / rate is a whole number (50, 60, 75, 100, 150 Hz).
  120, 144 and 165 Hz would need a fractional carry, so dt would alternate between two values.
- `(int)(1.0f / dt)` comes out one low at some rates (float rounding, checked with float32 arithmetic): **25 Hz
  -> 25, 30 Hz -> 29, 50 -> 50, 60 -> 59**, 75 -> 75. `stageFps` and `scriptFramesFrom30` would be off by one at
  30 and 60 Hz.
- `variableRate` is off: a frame that misses its fields slows the game.

## Three ways to go faster

### A. Interpolated presentation (recommended)

The game keeps running at 25 Hz exactly as the ROM does. The render thread draws at the monitor's rate and blends
the last two game frames, as the PC ports of other fixed-rate games do.

Why: no game code changes ("the game layer stays authentic"); every refresh rate works; scripts, AI, physics and
save data stay as on the console; the work is all in the Pan layer, and the renderer already has the right shape:
- `Frame` is plain data handed over whole, so two of them can be kept;
- the camera is per pass (`YamiPanModelPass::viewScreen`, `eye` -> `ModelPass`), apart from the objects;
- every model batch has its own world matrix (`ModelInstance::place`) and skinned batches their bone palettes
  (`Frame::modelPalettes`), so objects and bones can be blended on their own;
- particles are quads in the pass's space (`YamiPan_ModelDrawQuad`), not screen pixels.

The cost: one game frame of extra delay (40 ms at 25 Hz), and anything drawn as 2D screen vertices stays at 25 Hz.

The work, in order:
1. **Timing.** Stamp each `Frame` with the field count at its flip (the interrupt controller's field counter) and
   keep the previous frame beside the current one in `Renderer` (today `submit` drops it). Draw at "now minus one
   game frame" and blend by the fraction. The game frames must arrive evenly first (hazards.md, "MinGW's timed
   waits").
2. **The camera.** Blend each pass's camera. `viewScreen` is world -> screen in one matrix, so pass the view and the
   projection apart through the seam (`rena_draw.cpp` 676 / 706), blend the view (position lerp, rotation slerp),
   rebuild. The camera moves most of what is on screen, so this is most of the gain.
3. **Objects and bones.** Give each batch a key that stays the same across frames: `drawPrims` already has the
   game's `ModelInstance *` (`rena_draw.cpp` 297); add it to `YamiPan_ModelDrawBatch` and key by (model, batch).
   Blend the world matrices and each palette bone (translation lerp, rotation slerp). A batch with no partner in
   the previous frame is drawn as it is.
4. **Particles.** Key by (effect, particle slot); blend the corners.
5. **Cuts.** Do not blend across a camera cut, a teleport, a stage load, a pause or a jump in frame numbers; detect
   a cut from the game's camera change, or from a jump larger than some distance.
6. **A setting** in `yamipan.ini` and the overlay: off is today's picture.

What stays at 25 Hz: the 2D UI (`Frame::vertices` / `draws`), the frame filter, ripples, film overlay, shadow and
spot vertices built on the EE (`modelShadowVertices`, `modelSpotVertices`). They need a check that the jump is not
visible against the smooth camera, one subsystem at a time.

### B. NTSC timing, 30 Hz

Run the ROM at 30 fps as the Japanese version of the game presumably did: the field clock at 59.94 Hz and the master
clock at 10 ticks a frame. The units of the data (30 Hz motion frames, thirtieths, scripts' `frames30`, 30-frame
fades) suggest the frame-counting systems were tuned for 30 Hz, and the PAL conversion shows in a few places:
`kClockTicksPerFrame = 12`, `charactersound.cpp`'s 25, the `25 / 30` music fades, `soundInit(isPal)`, `uigame.cpp`'s
25 ticks a second. All of these would need their NTSC values, which only the Japanese ROM (Siren 2, NTSC-J, not
here) can confirm. Small change, 20 % faster, and it works with A (shorter blends, 33 ms of delay). Watch:
`stageFps` gives 29 there, and play time would be counted at 30 a second but shown as 25.

### C. Simulating at the monitor's rate

Set the master clock to 300 / rate ticks a frame and the interval to 1. The time-based core follows by itself; every
frame-counting system in the table (and those this pass did not find) needs a PC change that scales by dt (an ease
`r` becomes `1 - (1 - r)^(dt / 0.04)`, steps scale by `dt / 0.04`, frame counters become times), plus the
`(int)(1 / dt)` rounding and the whole-tick clock. That means changes across dozens of TUs of the game layer, changed
behaviour (collision, per-frame random rolls, AI thresholds), and every future TU having to be checked. Not
recommended; 50 Hz (exactly twice 25, 6 whole ticks, `stageFps` exact) is the only rate worth a later experiment.
"Native 60 Hz" below has a second, closer look.

## Option A so far (2026-11-09)

**Steps 1 and 2: the timing and the camera.** `InterruptController::lastFieldTime` (when the last field was due, on
the clock's even grid) stamps each `Frame` at `YamiPan_DrawUiKick` (`Frame::time`). `Renderer` keeps the frame
before the shown one (`m_previous`; a frame overtaken in the mailbox before the render thread took it is kept as the
previous one, so a render stall does not break the pairing), and each present asks `FrameBlend`
(`rendering/frame_blend.cpp`): progress = time since the newer frame's field / time between the two fields, clamped
to 0..1; nothing when the numbers are not consecutive or the two fields are over 100 ms apart. Below 1, the scene is
recorded again with the newer frame's passes, each `viewScreen` and `lights.eye` blended from the older frame's
(component-wise: every point moves on the chord, not the arc; `frame_blend.cpp` has the error). No blend when the
pass counts differ, a pair's `clearDepth` differs, or a camera cut: the eye moves over 300 units or turns over 40
degrees in one frame. Uploads stay once per game frame: the blended presents reuse the frame's buffers. Setting:
Graphics > Smooth motion (`smooth_motion`, on by default); test hooks in the tools README ("Platform contract").

**Seen** (stage 7, `YAMIPAN_TELEPORT=7`, 40 s, left stick scripted, `YAMIPAN_BLEND_LOG=1`; over Remote Desktop,
~32 presents a second): game frames 40 ms apart on the field grid; 1,238 of 1,329 presents blended; the main eye's
speed between presents runs smoothly across game frames (an easing camera: 29.4, 28.1, 26.9, 25.8, 24.8, 23.6 ...
units/s from one present to the next, with no step where a new frame arrives), where unblended it moves once a game
frame. The rest at 1.0: presents after a frame's
full 40 ms (correct), the stage's first frames, and three frames whose predecessor the mailbox had dropped while
the render thread wrote a shot. The shots show stage 7 drawn correctly under a blended camera.

**Seen again on stage 2** (`YAMIPAN_TELEPORT=2`, 45 s, walking and turning with the left stick), with the
overtaken-frame fix: 1,265 of 1,324 presents blended; the only unblended presents with passes are after a frame's
full 40 ms and the stage's first frame. Writing a shot holds the render thread about 300 ms (6 to 8 game frames go
by unshown); the first present after it blends from the overtaken frame (230 -> 238 at 0.258), as meant. The shots
show the girl in the rain woods drawn correctly.

**The test scene is stage 2** (the user, 2026-11-09): stage 7 is a poor one for walking, its opening takes control
away for a tutorial prompt after a few steps. `build.py --run 45 --pad "350:LSUP*200,560:LSLEFT*60,640:LSUP*150,
800:LSRIGHT*60,870:LSUP*150" --env YAMIPAN_TELEPORT=2 --env YAMIPAN_BLEND_LOG=1`.

**Step 3: objects, bones and lights (2026-11-09).** `YamiPanModelBatch::owner` carries the game's
`ModelInstance *` (set in `rena_draw.cpp` `drawPrims`, never read through). `ModelRenderer::drawCachedBatch` keys
each instance and each palette run by `ModelPlacementKey` {owner, the first batch's positions}; `collect` carries
them into `Frame::modelInstanceKeys` / `modelPaletteRuns`; the n-th placement of a key pairs with the n-th of the
other frame. `FrameBlend::blendPlacements` blends a pair when it can be the same part a frame later (origin within
300 units, no axis turned past 90 degrees or changed its length by half): axes blended and put back to their blended
length (a turned part keeps its size halfway), origin in a line; a palette run pairs only with one of the same
length. The pass blend also moves the point light, the spot's world -> light matrix (when its origin moved under
300 units and its scale is the same) and each shadow's projection (when no element changed by over 5 % of the
largest). The renderer uploads the blended instances and palettes on each blended present
(`ModelRenderer::uploadPlacements`) and the frame's own again once the blend reaches 1.

**Seen** (stage 2, 45 s walking and turning, `YAMIPAN_BLEND_LOG=1`; ~32 presents a second over Remote Desktop):
1,240 blended presents, on average every matrix of the frame paired and blended; the first skinned model's first
bone (the girl) moves 3 to 5.5 units every present while she walks, also between two presents of the same game
frame. Stage 7 with `YAMIPAN_BLEND_ALPHA=0.5`: Akiko mid-stride, the flashlight's spot on the road, nothing doubled or
out of step with its spot or shadow. Stage 2's shots: the girl in the rain woods, right.

**Step 4: particles (2026-11-09).** Every `YamiPan_ModelDrawQuad` quad (StageParticle, Critter, SkyMist) comes in
**view space**, inside an effects pass whose matrix is the projection E alone (`renaBeginEffects`, now
`YamiPanModelPass::viewSpace`). So blending the main pass's camera alone left them on the newer frame's camera. Two
fixes in `FrameBlend`: (1) `blendPasses` gives each view-space pass E . M_new^-1 . M_blend, where M is the main
pass's world -> screen matrix (the last non-view-space pass that clears the depth; the far passes before it look
through a scaled view): the main pass's matrix is the camera's view times the same E, so this is "back to the world,
then through the blended camera". (2) `blendQuads` pairs each quad by `YamiPanModelQuad::owner` + `slot` (a
StageParticle and its particle's index, a Critter and 0; the sky mist's wrapped puffs pass null, camera only),
carries the older one into the newer frame's view space (E_prev . M_prev^-1 . M_new . E_new^-1) and blends the
corners there when its first corner moved under 150 units (more is a new particle in a reused slot). The renderer
uploads the blended placed vertices per present (`ModelRenderer::uploadPlacedVertices`) and the frame's own after.

**Seen** (stage 2, 45 s walking and turning, the rain): 77 % of the quads paired and blended (the rest just born or
reborn far off), the same share with the camera still and moving. The carry checks out: a blended quad's own step
in the frame, after it, is 22 units with the camera still and 24 with it moving (4.5 units a present); a wrong carry
would add the camera's move. Shots: the rain, the wall, the girl, right.

**Not blended:** batches on the placed path (the far passes that cull, batches too large for a chunk: their vertices
are placed on the game thread); the sky mist's own drift (camera only); whatever the game projects to 2D itself (the
UI layer). Shadows are matched by slot and a 5 % test, not by owner; two characters swapping shadow slots would snap
rather than sweep.

## Native 60 Hz: what option C would take (2026-11-09)

The user asked what a real 60 fps would need, without blending: the game's own logic run 60 times a second. A second
look at C, with the reconstruction nearly complete (6,044 functions, 3 stubs), so the code can be read whole.

**The pace: small.**
- The field clock at exactly 60 Hz (not NTSC's 59.94: 300 / 59.94 is not whole), `kVsyncsPerFrame` 1,
  `kClockTicksPerFrame` 5. No clock is ever scaled (nothing writes `m_scale`; only pause), so `Clock::advance`'s
  `>> 8` loses nothing at 5 ticks. Nothing else counts vblanks: only `MasterClock::update` and the frame sync.
- **`(int)(1.0f / dt)` gives 59** (float32, checked: 5 / 300 is 0.016666668, one ulp above 1/60). 18 places turn dt
  into a rate that way (`stageFps`, `scriptFramesFrom30`, revive frames, the gun's `spread.rate`, ...); all their
  timers would run 1.7 % short. One fix covers all of them: `getDeltaSeconds` one ulp lower at 60 Hz
  (0.016666666, 1 / dt = 60.000004 -> 60), a timing change of 1e-7, instead of 18 edits.
- Sound: RXSNDDRV ticks at the rate `soundInit` gives it (`rxSndSetFrameRate` 50 / 60, SDPR2's pair mode), not the
  game's frame; it can keep its 50 or take the NTSC values the driver already has. The EE flushing sound commands
  once a game frame more often is harmless.

**The time-based core follows by itself.** 22 files read the clock (characterroute 97 calls, playermotion 50,
stageitem, stagescene, stagecar, characterinput, scriptrunner, characterbrain, stageparty, mapobject, ...):
animation, root motion, routes, script waits, character timers. They already turn dt into 30 Hz frames and back.

**Everything else counts frames, and each rule needs a rate-free form.** The other ~117 game files include every
UI, effect, item, menu and sound file. Not counted site by site yet; the first pass's table above is the kinds:

| kind | examples | rate-free form | the same as 25 Hz? |
|---|---|---|---|
| counters | pad repeat (7, then 2), 30-frame fades, blinks, voice delays (`kFramesPerSecond = 25`), particle lives, minigame timers | count seconds, or n x 2.4 rounded | 7 frames -> 16.8, so 17: off by up to half a 60 Hz frame |
| speeds a frame | UI pixels, item `m_speed`, `StageCar` `advance(1.0f)`, cut speed, 6 degrees a frame (lock-on, critters) | x dt / 0.04 | yes |
| accelerations a frame | item gravity (1 unit a frame, each frame), particle gravity | x (dt / 0.04)^2 | close: Euler with a smaller step lands elsewhere |
| eases | the camera's `approachVec3` (step = rate x distance^2, capped at 25 units a frame, no overshoot test), look and turn eases | the curve the steps approximate: for that rule, d / (1 + (rate / 0.04) x d x dt) | no: the PAL steps are not that curve (they overshoot when rate x d > 1). The camera would feel close, not identical |
| random rolls a frame | particle emission (22 rolls in stageparticle), ripples, fidgets | p -> 1 - (1 - p)^(dt / 0.04) | in distribution only |
| save data | `playTime++` a frame, shown / 25, on the memory card | keep a 25 Hz count (an accumulator) | yes, cards stay compatible |

**What it costs.**
- **Game-layer edits.** The game layer stays authentic by the user's rule, so this needs one marked convention (a
  helper such as `palFrames(dt)`, a `// PC 60 Hz:` comment, all behind a setting so 25 Hz stays the ROM's).
- **No reference to prove it against.** The cmp tools compare host and ROM at the ROM's rate; at 60 Hz nothing can.
  The test would be new: the same pad script at 25 and 60 Hz, the player's, camera's and enemies' positions compared
  over time, a measured deviation rather than zero.
- **Changed behaviour.** Collision with smaller steps; AI thresholds on per-frame distances crossed at other moments;
  the random stream used in another order (a run stops matching the console after the first roll).
- **A missed site runs 2.4 times fast.** Reading finds them, a grep only suggests candidates.
- **60 only.** The tick count must be whole: 50, 60, 75, 100, 150 Hz. 120 or 144 Hz need a fractional carry, or A
  on top of a 60 Hz game.

**What it gains over A:** input to picture, about 17 ms a game frame instead of 40 ms plus A's one-frame delay; the 2D
UI and what A leaves unblended (the sky mist's drift, the placed batches) at 60 too. The motion on screen is already
A's.

**If chosen, the order:**
1. The inventory: every per-frame rule in the ~117 files, kind and fix. **First pass done (2026-11-09):
   framerate-inventory.md**, from `framerules.py`'s candidates; its "to check" list is what is left.
2. **50 Hz first** as the dry run: 2x is exact for counters and speeds (n -> 2n, v -> v / 2), dt exact (6 ticks),
   `stageFps` 50. It tests the method and the 25-vs-faster comparison with no rounding in the way. **Started
   2026-11-09** ("The 50 Hz dry run" below; branch `framerate/50hz`).
3. 60 Hz: the ulp fix, the 2.4x counters, the eases. **Done 2026-11-09** ("60 Hz" below).
4. Keep A on top for refresh rates above 60.

## The 50 Hz dry run (2026-11-09)

Made on the branch `framerate/50hz` (worktree `../YamiPan-rate`: another session was debugging in the main checkout), merged into fs2-port the same day.

**The switch.** `game_rate` in yamipan.ini (`YAMIPAN_GAME_RATE`; 25 or 50, read once; anything else warns and runs
25), through the seam `YamiPan_GameRate` (`os/yamipan_time.h`). `game/gamerate.h` (PC only): at 50 the master clock
is built with 6 ticks and `threadSysInit` with an interval of 1, so the game loop runs every PAL field; `palTicks()`
is the 25 Hz frames that ended this game frame (from the master clock's own ticks: 0, 1, 0, 1 at 50; always 1 at 25)
and `palScale()` the frame's length in 25 Hz frames (0.5; 1 at 25). **At 25 everything is the ROM to the bit**: each
change multiplies by 1.0f or gates on a 1.

**Converted** (each marked `// PC: game_rate`): the shared mechanisms of framerate-inventory.md (UI tracks
`UiTrack::step`, tweens `UiTween::step`, vector tracks and fades, pad repeat, stream and master sound fades); the
widgets' own steps (`PrgMsgAniX`, `PrgRot`'s turn, zoom and pan, `PrgTransMsg`'s scroll: x `palScale`); the main
camera (its five `approachVec3` eases with rate and cap x `palScale`, Euler at the finer step; zoom steps; blocked
frames); particles (velocity, age, life, colour, size and sprite frame change on the 25 Hz tick; position and spin
move `palScale` of a step every frame, so a particle is where the ROM's is at each 25 Hz frame and in a line between;
between ticks nothing is emitted); item physics (a closed Verlet simulation: it runs whole 25 Hz frames, exact, so
thrown items still move at 25 Hz); the stage clocks and the saved play time (`StageResult::tick` on the tick);
character voice delays (`kFramesPerSecond = 25`).

**Second pass, the rest of the inventory (2026-11-09):** game over (its timers on the tick; the death camera's swing
speed on the tick, its angle by the share; the letterbox fade); demo flecks and mission timed records (on the tick;
notices were already in seconds, `kNoticeFps`); the demo camera's cut speed and the airborne car's `drive` (x share);
the sub-scene's one-frame shorter script wait (one 25 Hz frame in game frames); fire and scope (the hold fires once,
on the tick the 25 Hz repeat count reaches it; sway steps x share; scope and look eases through `palEase`); the item
prompt's keys and the HUD weapon cycle (on the tick); status (the hearing roll on the tick only, the hit mark and the
leave countdown); the sight-jack (hold, marker, vision, flash and bind counters on the tick; its aim's ease and the
film's crossfade by the share); the lamp's lean and the lock dials' eases (`palEase`); the dive camera (its share of
1 / flyFrames); critters (`StageNodeTree::update` runs the nests on the tick: whole 25 Hz frames, exact, moving in
25 Hz steps; the nest's warm-up is not that path); Kunitoris (after each of its `threadYield(1)`,
`kunitorisWholeFrame` sits out the frame between two ticks and keeps that frame's button edges for the next: the
whole minigame in 25 Hz frames, presses kept).

`palEase(rate)` (game/gamerate.h) is the linear ease's form, 1 - (1 - rate)^share, the rate itself at 25; a rate of 1
(a snap) stays a snap. The navigation menu and the locks' `frame()` have no frame rule of their own (tweens, buttons).

**What is left:** every row of framerate-inventory.md is converted. Kept at 25 Hz steps on purpose (exact, not smooth
at 50): item physics, critters, Kunitoris, UI tracks and tweens, sprite frames. Not read: the 22 time-based files
beyond their clock reads (framerate-inventory.md "To check"). Not run at 50: game over, the sight-jack, Kunitoris, the
locks, cars, cut scenes, menus and the map; a memory card save's play time.

**Seen** (stage 2, 45 s, the walk script; at 50 Hz with every pad-read number doubled, since the script counts pad
reads; `YAMIPAN_RATE_LOG=1`, `ratecmp.py`):
- the game frame is 20.0 ms of game time at 50 (40.0 at 25), 2,150 stage frames against 1,048 in the same time;
- the player covers 1,276 units in the first 10 s of walking at 25 and 1,288 at 50 (1 %); the camera's eye 1,326
  and 1,344; the eye's distance behind the player follows the same curve (288 / 293, 330 / 336, 259 / 259,
  213 / 213 at 0.5, 1, 6 and 10 s), so the converted eases hold the camera's feel;
- motion is even at 50: the player moves every frame, 3.3 units a frame against 7.2 (no frame without a move between
  two that move; the next / this step ratio's median 1.01 against 1.11);
- the two walks drift apart later (25 units at 8 s, 89 at 18 s): the scripted turns start a frame earlier or
  later, since the pad is read twice as often;
- particles (30 s idle, `YAMIPAN_BLEND_LOG`): a 50 Hz frame without a tick moves a quad 9.55 units, 19.1 a 25 Hz
  frame; the tick frames, which also carry the size and sprite changes, 14.65. The 25 Hz step (21.4) holds motion
  and those changes together, so the speeds agree; a per-particle log would make that exact;
- the shots at 50: the girl by the wall in the rain, as at 25.

Not checked: menus, the map, the sight-jack and anything in the "not converted" list; sound by ear; a frame
filter or film effect; the memory card's play time after a save.

## 60 Hz (2026-11-09)

`game_rate = 60`: the EE's field clock runs at 60 Hz whatever the game chose (`InterruptController`; the IOP keeps its
own V-blank in `iop/kernel.cpp`, so RXSNDDRV still ticks at the PAL 50 `soundInit` gave it), one field a frame, 5
master ticks a frame (300 / 60, whole). `Clock::getDeltaSeconds` returns one ulp less when the quotient would make
`(int)(1 / dt)` one low (5 / 300 is 0.016666668 in float: 59 -> 60); at 12 and 6 ticks it is already exact and
untouched. `palTicks` is then 1 on 5 frames of 12 (every 2 or 3), `palScale` 5 / 12; every conversion of the 50 Hz
passes holds as written (none assumed the ticks alternate).

**Found on the way, for 50 and 60:** a movie decodes a picture each time `PrgMovie` draws it, once a game frame, so at
50 a PAL stream played twice as fast as its sound. `MoviePlayer::drawFrame` now decodes on the 25 Hz tick and draws
the last picture between. The stream feeders fill by free space and need nothing.

**Seen** (stage 2, the walk script with every pad-read number x 2.4; `YAMIPAN_RATE_LOG`): 2,560 stage frames of
16.7 ms in 42.7 s; the player covers 1,293 units in the first 10 s of walking (25: 1,276, 50: 1,288), the eye 1,358
(1,326, 1,344); the eye's distance behind the player follows 25 Hz's (292 / 288, 337 / 330, 259 / 259, 213 / 213 at
0.5, 1, 6, 10 s); the player moves every frame, 2.6 units a frame, next / this ratio 1.00. Rain: 609 units/s by the
per-frame measure (50: 598; the measure adds the 25 Hz size changes). The shots: the girl in the rain, right.

**First person** (the user saw it slower, 2026-11-09): `Up` enters it on stage 2 (`CharacterInput::stepView`; the
top view's kind goes 9 -> 10). The rate log now has `fields` (the vsync total: more than one a frame is a late frame,
which slows the game, since the master clock adds a frame's ticks whatever the fields) and `view` (the top view's
kind). Measured at 25 and 50: no late frame in either view (1.00 field a frame at 50, 2.00 at 25); first-person
walking 120.0 units in its first second at 25 and 120.9 at 50, 381 / 387 in 4 s. The first-person walk is slower than
the third-person one in the ROM's own code (about 120 against 262 units in the first second, at 25).

## Native frames at 50 / 60: smooth motion only at 25 (2026-11-18)

The user, running `game_rate = 60` with Smooth motion on: the camera should move at a real 60 fps, and the stage
moved around on some frames. Measured on stage 2 at 60 (40 s, the walk script x 2.4, `YAMIPAN_BLEND_LOG`): nearly
every present was a new game frame drawn about a third of the way from the one before, so the blend only held the
picture two thirds of a frame back. 49 presents failed the cut test and jumped to the newer frame. On 102 presents (1
in 20) pieces of the stage were drawn between two copies of themselves, up to 280 units apart: a model drawn
several times under one `ModelPlacementKey` (one owner, one geometry) loses or gains a copy when one is culled, and
the n-th-with-n-th pairing then matches each later copy with its neighbour (samePart's 300 units let it through).

**Now:** `Renderer::blendsFrames()` = Smooth motion on **and** game_rate 25 (`Renderer::kRomGameRate`). At 50 / 60
each frame is shown as it comes, newest first, nothing blended, no frame of delay; the overlay greys the option out
with the rate in its tooltip. At 25, `FrameBlend::blendPlacements` skips every key whose count differs between the
two frames (`ambiguousKeys`, instances and palette runs): those placements are drawn as they are. The `[blend]` log
line says why a present was not blended (`off`, `unpaired`, `passes`, `cut`).

**Seen:** at 60, 1,975 presents, 1,928 new frames, none blended; while walking the main eye moves every frame (1,119
frames, mean 7.6 units, 4 steps over 1.6 times the one before, the walk's turns). At 25, 2,123 of 2,228 presents
blended, 1,001 of 1,007 placements on average (the rest the ambiguous ones). Shots at both: the girl in the rain.

**Still at 25 Hz steps inside a 60 Hz game** (kept so on purpose by the 50 Hz pass, so no blend could smooth them,
the renderer having no 25 Hz phase): thrown items (`ItemPhysics`, the owner's matrix written on the tick), critters
(`StageNodeTree::update` runs nests on the tick), UI tracks and tweens, sprite frames. Making them move each frame
means either running them at the native step or drawing them between their last two 25 Hz states in the game layer.

## Measuring

The machine is used over Remote Desktop, which caps presents at 10-30 a second: judge by timestamps (game frame times
and the render thread's blend fractions in a log), not by watching. For the picture test, a shot with the blend held
at 0.5 must show every model halfway between the two frames' shots, with nothing doubled or missing.
