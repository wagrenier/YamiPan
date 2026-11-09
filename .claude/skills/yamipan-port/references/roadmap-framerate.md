# Unlocking the frame rate: a first analysis (2026-11-09)

The game runs at 25 fps because the PAL ROM waits two 50 Hz fields per frame. This file has what sets that rate,
how much of the game measures time and how much counts frames, the three ways to go faster, and the first steps of
the one recommended. **The user chose A (2026-11-09); steps 1 and 2 are done ("Option A so far" below).**

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
| sky mist | `skymist.cpp` 88 | lives in frames |
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
Graphics > Smooth camera (`smooth_motion`, on by default); test hooks in the tools README ("Platform contract").

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

**Known limit until step 3:** objects stand where the newer frame has them while the camera blends, so a character
the camera follows can judder against a smooth background by up to one frame's step.

**Next:** step 3 (objects and bones), keyed by the game's `ModelInstance *` through `YamiPan_ModelDrawBatch`; the
instances and palettes then need uploading per blended present.

## Measuring

The machine is used over Remote Desktop, which caps presents at 10-30 a second: judge by timestamps (game frame times
and the render thread's blend fractions in a log), not by watching. For the picture test, a shot with the blend held
at 0.5 must show every model halfway between the two frames' shots, with nothing doubled or missing.
