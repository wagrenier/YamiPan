# Frame-counted rules: the inventory for a native frame rate (2026-11-09)

Step 1 of "Native 60 Hz" in roadmap-framerate.md: every rule in the game layer that assumes one game frame is 1/25 s,
with its kind and the fix that would make it rate-free. **First pass**: the shared mechanisms and the visible
systems are read; "To check" below is what is left. Nothing in the game layer is changed
yet; the user decides whether option C goes ahead. **The 50 Hz dry run (2026-11-09, branch `framerate/50hz`)
converted the shared mechanisms, the main camera, particles, item physics, the stage clocks and voice delays; its
second pass the rest of the systems table** (roadmap-framerate.md "The 50 Hz dry run" has the list, what was seen
and what was not run). Mission notices turned out to be in seconds (`kNoticeFps`, pausemenustate.cpp); only the timed
records count frames. Missed by the first pass: movies decode a picture a game frame (`MoviePlayer::drawFrame`), now on the tick.

## How it was made

`tools/reverse/framerules.py` scans `src/game`, `src/rena`, `src/sound` function by function (the `FS2 0x` markers):
a function that reads the clock is marked time-based, and in every function the lines that count (`++`, `--`,
`+= 1`), step (`+=` / `-=` a constant or a member), scale (`*=`), ease (`approach*`), roll (`randNext`) or name a
frame constant are candidates. First run: **882 candidate lines in 362 functions of 77 files** (`work/framerules.json`).
A candidate is not a rule: a counter in a loader or a list's `m_count++` is not frame-paced. The tables below are the
candidates read and kept. A rule written in another shape (a local counted and stored back, a step inside a helper
called each frame) is missed by the scan; the shared mechanisms were found by following callers instead.

`s` below is the frame's length in PAL frames: dt / 0.04 (60 Hz: 0.41667; 50 Hz: 0.5).

| fix | for | rate-free form | exact? |
|---|---|---|---|
| **T** | a counter of frames | count seconds (or s-sized fractions) against n x 0.04 s | to the frame: 2.4 x n is rounded at 60 Hz, exact at 50 |
| **V** | a step a frame (speed, turn) | step x s | yes |
| **A** | a change of a step a frame (gravity) | x s^2 and the same order of updates | close: Euler with a smaller step |
| **E** | a linear ease (x += (target - x) x r) | r -> 1 - (1 - r)^s | yes, for the linear ones |
| **Q** | `approachVec3` / `approachAnglesXZ` (step = rate x d^2, capped) | d -> d / (1 + (rate / 0.04) x d x dt), cap x s | no: the PAL steps are not that curve (overshoot when rate x d > 1) |
| **R** | a random roll each frame | p -> 1 - (1 - p)^s, or roll on a 25 Hz sub-tick | in distribution |
| **D** | a duration multiplied (`*= damping`, `size *= growth`) | factor^s | yes |
| **K** | must stay at 25 Hz (save data, the ROM's own counts shown to the player) | an accumulator that ticks every 0.04 s | yes |

## Shared mechanisms: one fix, many users

| mechanism | where | what counts frames | users | fix |
|---|---|---|---|---|
| UI tracks | `UiTrack::step` uitrack.cpp 1262 | one integer frame of an S2D track a game frame (keyed and per-frame tracks) | every layout animation: menus, HUD, prompts (`UiSequence::draw` 235) | T with a fractional frame; keyed tracks interpolate, per-frame tracks hold |
| UI tweens | `UiTween::step` uiwidget.cpp 1472 | `frame++` against `length` | navimenu 1219, uiwidget 1053 | T (fractional frame) |
| vector tracks | `VectorTrack::step` vectortrack.cpp 56 | `frame++` against each key's `frames` | SceneRender's light, fog and clear-colour transitions (scenerender.cpp 370, 438-448); Kunitoris' movers and faders | T (fractional frame) |
| pad repeat | `Pad::updateRepeat` pad.cpp 162 | `kPadRepeatFirst` 7, `kPadRepeatNext` 9 frames | every menu that repeats a held direction | T |
| eases | `approachVec3` / `approachAnglesXZ` mathutil.cpp 297 | one step of rate x d^2 a frame | the main camera (below), sight-jack aim (stageparty.cpp 279), the lamp's lean (`leanBodyAt` playermotion.cpp 6085), the dive camera (8272) | Q at each caller (the helper takes no time) |
| sound fades | `SoundStream::updateFade` sound.cpp 302, `SePlayer::update` seplayer.cpp 855 | `m_fadeFrame++` to `m_fadeFrames`, `m_masterFadeFrame++` | music, streams, the master fade; `StageSound::playMusic(fadeFrames)` already scales 30 fps frames by 25 / 30 (stagesound.cpp 26) | T |

## Systems

| system | where | rule | fix |
|---|---|---|---|
| main camera | stagecamera.cpp 434-734 | five `approachVec3` eases (focus and eye offsets, position, look, turn), `kFollowMaxStep` 25, `kLookMaxStep` 20, `kTurnMaxStep` 0.314 a frame; zoom +/- `kZoomStep` 20 a frame; `m_blockedFrames++` | Q, V, T |
| particles | stageparticle.cpp 283-537, 620 | `age++`, `life--`, `frame++` (sprite frames), `velocity[2] -= gravity`, `size *= sizeGrowth`, `rotation += spin`; emission `m_delay++` and its rolls; `m_age++` | T, A, D, V, R |
| critters | stageparticle.cpp 1299-1701 | spawn timer, `kCritterTurnStep` a frame, `m_timer` waits (rolled in frames), sprite `m_frameTicks`, fall speed, flee frames | T, V, R; run in whole 25 Hz steps, drawn between the last two (`Critter::keepPlace`, 2026-11-18) |
| item physics | itemphysics.cpp 290, 346, 365 | `velocity *= damping`, `position[2] -= kGravity`, `kLandSpeed` compared with a speed a frame (`keepsMoving` is already in seconds) | D, A, V on the threshold; run in whole 25 Hz steps, the owner drawn between the last two (`ItemPhysics::drawBetweenSteps`, 2026-11-18) |
| sight-jack | stageparty.cpp 532, 1200, 1375-1418 | `m_holdFrames` (`kHoldFrames`), `m_markerFrames`, `m_visionFade`, `m_flashFrames`, `m_visionFrames`, `m_bindFlash` | T |
| the jack's film | stageparty.cpp 1657, 1666 | `corner.progress += corner.step`, `++corner.shown` | V, T |
| game over | gameoversequence.cpp 84, 93, 217-222, 313 | `m_timer--` twice; the death camera's swing (`m_swingSpeed +/- m_swingAccel`, `m_angle += m_swingSpeed`); `SceneDirector` `m_frame++` | T, A, V |
| demo flecks | demofleck.cpp 219 | `++fleck.age` against `life` | T |
| mission notices and timed records | demoevent.cpp 686, 766, 790 | `record.frames--` a frame (`StageMission::tickPending`); the frames are script arguments (scriptrunner.cpp 2564, 4899), so the data counts PAL frames | T |
| sub-scene script waits | scriptrunner.cpp 783 | a wait converted through the clock, then `frames--` in a sub-scene: one frame shorter | T (one frame: 40 ms at 25 Hz) |
| fire and scope | characterinput.cpp 312-318, 584-600, 880 | `m_fireFrames` against `kFireHoldFrames`; `m_scopePitch += m_recoil * shake` a frame and its ease; the look's `lerpVec4(kLookEase)` (the recoil's decay already goes by `framesPerSecond(dt)`) | T, V, E |
| item prompt | uiitemlist.cpp 211 | the open / close animation's `animFrame++` | T |
| status | stageplayer.cpp (`StageCharacter::stepStatus`) | counters and a roll a frame | T, R |
| character sounds | charactersound.cpp 717 (`kFramesPerSecond = 25`), `StageCharacter::updateSounds` | voice delays in PAL frames; a counter and two rolls a frame | T, R |
| HUD | uigame.cpp 1282 | `sUiWeaponCycle++` (the weapon preview's cycle) | T |
| play time | stageresult.cpp 162 | `save.playTime++`, shown / 25 (uigame.cpp 1004), saved | **K** |
| UI widgets | uiwidget.cpp 671 (`PrgMsgAniX`), 991 (`PrgRot`, `kPrgRotTurnStep`), `PrgTransMsg::advance` | a frame or a step a frame | T, V |
| navigation menu | navimenu.cpp (`PrgScaleNavi::advance`) | counters a frame | T |
| Kunitoris | kunitoris.cpp (14 counters, `kKunitorisShotFrames` 1312, `kKunitorisDropFrames` 1340) | the whole minigame in frames | T throughout |
| number and seven-key locks | gadgetview.cpp (`NumLockView::frame`), sevenkeys.cpp (`SevenKeysView::frame`), numlock / sevenkeys `easeDials(.., frames)` | frame counters, dial eases over n frames | T |
| cars, cut speed | stagecar.cpp 1020 (`advance(1.0f)`), demoevent.cpp 439 | a fixed step a frame | V |

## Already time-based (no change)

The 22 files that read the clock: animation (`ModelPose`, `StageModel::advance(frames)` with frames = dt / (1/30)),
root motion and `applyMovement`, character timers (`sideHook0`'s flinch, unarmed, endurance and light-hurt clocks, run
and stance times), routes (characterroute.cpp, 97 clock reads), brains (`waitFrames` through dt), map objects,
doors, `StageDaylight`, rumble (`updateRumble(dt / (1/30))`), `ItemPhysics::keepsMoving`. Their only trap is
`(int)(1.0f / dt)`, 18 places (roadmap-framerate.md: one fix in `getDeltaSeconds`). Settled as time-based on a
second read, though they count frames or take a step:
- script waits: `ScriptOp_SetWait` turns a wait in 30ths into frames through the clock (scriptrunner.cpp 776), and
  `m_wait--` counts those down (only the sub-scene's one frame less is a frame rule, in the table);
- texture animation: `drawSysTick((int)(dt x 300))` (stagescene.cpp 3160) into `renaStepTexAnim`;
- ripples: `advanceWaves(.., seconds x pi)` (ripplesheet.cpp 127);
- the alert: `m_frames++`, then turned into 30ths by dt (stagealert.cpp 81-83);
- the sky mist: lives drawn in 30 fps frames (`kFrameRate`), aged by `deltaSeconds` (the first pass's table in
  roadmap-framerate.md had it as frames).

The menu files the scan found nothing in (titlescreen, helpscreen, pausemenu, menuscene, progress, screen,
framefilter, viewcamera) have no per-frame step of their own: they animate through UI tracks and tweens (above).

## To check

- `CritterNest`'s warm-up (`kCritterWarmFrames` steps at construction, stageparticle.cpp 1279): a whole number of
  frames simulated ahead, s-sized at another rate.
- The 22 time-based files beyond their clock reads: a time-based function can still count (`readFire` did). The
  scan marks a whole function as timed, so their candidates were read only where listed above.

## What the scan cannot see

Rules in a shape it does not match, and data authored in frames that code only passes on (S2D tracks, effect
definitions' lives and gravities, motion lists): those are fixed where they are consumed, which is why the shared
mechanisms are fixed in one place. The test is the one in roadmap-framerate.md: the same pad script at 25 and at
the new rate, positions and timings compared over time.
