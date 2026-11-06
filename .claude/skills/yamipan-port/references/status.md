# YamiPan status (generated)

Generated 2026-11-06 02:24 at commit `a8937a2` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6599**, named **4765** (72%); library functions: 987
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1544, game-inferred 3844
- context-name ledger: 5388 entries; overrides of wrong library matches: 0
- placed in a source file: 5028/6599 in 110 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `00310000` | 0/9 (0%) | - |
| `002c0000` | 33/173 (19%) | game/packtables.cpp, game/menupack.cpp, game/stagecollision.cpp |
| `002d0000` | 91/256 (36%) | game/drawsys.cpp, game/fontdata.cpp, game/wstring.cpp |
| `00280000` | 104/232 (45%) | game/demoevent.cpp, game/playermotion.cpp, game/areagrid.cpp |
| `001c0000` | 108/233 (46%) | game/scriptrunner.cpp |
| `001b0000` | 115/242 (48%) | game/scriptrunner.cpp |
| `00170000` | 228/466 (49%) | sdpr2/sdpr2drv.c, game/stagedoor.cpp, game/scenepairs.cpp |
| `002a0000` | 119/241 (49%) | game/memcard.cpp, game/playermotion.cpp, game/stageresult.cpp |

## Reconstruction (src/)

- reconstructed FS2 functions: **4498**; stubs: **226**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/areagrid.cpp` | 11 | 0 |
| `src/game/boxgrid.cpp` | 29 | 0 |
| `src/game/characterbrain.cpp` | 142 | 0 |
| `src/game/characterground.cpp` | 20 | 0 |
| `src/game/characterinput.cpp` | 11 | 0 |
| `src/game/characterroute.cpp` | 144 | 0 |
| `src/game/clock.cpp` | 14 | 0 |
| `src/game/datetime.cpp` | 2 | 0 |
| `src/game/demoevent.cpp` | 102 | 0 |
| `src/game/drawsys.cpp` | 166 | 0 |
| `src/game/fileloader.cpp` | 53 | 0 |
| `src/game/fontdata.cpp` | 43 | 0 |
| `src/game/gameoversequence.cpp` | 12 | 0 |
| `src/game/gamepacket.cpp` | 6 | 0 |
| `src/game/gamepaths.cpp` | 14 | 0 |
| `src/game/gamesystem.cpp` | 17 | 0 |
| `src/game/gfxtarget.cpp` | 15 | 0 |
| `src/game/gsblockmap.cpp` | 14 | 0 |
| `src/game/gsutil.cpp` | 12 | 0 |
| `src/game/heap.cpp` | 17 | 0 |
| `src/game/layoutpaths.cpp` | 24 | 0 |
| `src/game/mapobject.cpp` | 15 | 0 |
| `src/game/mathutil.cpp` | 18 | 0 |
| `src/game/memcard.cpp` | 55 | 0 |
| `src/game/menupack.cpp` | 9 | 0 |
| `src/game/menuscene.cpp` | 14 | 0 |
| `src/game/modelpose.cpp` | 93 | 0 |
| `src/game/movieplayer.cpp` | 41 | 0 |
| `src/game/navimenu.cpp` | 30 | 0 |
| `src/game/nodetransform.cpp` | 17 | 0 |
| `src/game/packtables.cpp` | 8 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/pakarchive.cpp` | 17 | 0 |
| `src/game/pausemenustate.cpp` | 12 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |
| `src/game/playermotion.cpp` | 394 | 0 |
| `src/game/prgmccard.cpp` | 7 | 0 |
| `src/game/progress.cpp` | 25 | 0 |
| `src/game/rcnttimer.cpp` | 0 | 4 |
| `src/game/routefilter.cpp` | 38 | 0 |
| `src/game/routeplayer.cpp` | 45 | 0 |
| `src/game/s2ddata.cpp` | 9 | 0 |
| `src/game/scene.cpp` | 1 | 0 |
| `src/game/sceneeffects.cpp` | 7 | 0 |
| `src/game/scenepairs.cpp` | 18 | 0 |
| `src/game/scenepool.cpp` | 11 | 0 |
| `src/game/scenerender.cpp` | 23 | 0 |
| `src/game/screen.cpp` | 18 | 0 |
| `src/game/scriptrunner.cpp` | 440 | 2 |
| `src/game/seplayer.cpp` | 35 | 0 |
| `src/game/sldi.cpp` | 1 | 0 |
| `src/game/spkrom.cpp` | 13 | 0 |
| `src/game/stagealert.cpp` | 0 | 7 |
| `src/game/stagecamera.cpp` | 19 | 0 |
| `src/game/stagecharacter.cpp` | 14 | 0 |
| `src/game/stagecollision.cpp` | 56 | 0 |
| `src/game/stagedoor.cpp` | 19 | 0 |
| `src/game/stageitem.cpp` | 107 | 0 |
| `src/game/stagelight.cpp` | 64 | 0 |
| `src/game/stageloop.cpp` | 9 | 0 |
| `src/game/stagemenu.cpp` | 1 | 1 |
| `src/game/stagemodel.cpp` | 16 | 0 |
| `src/game/stageobject.cpp` | 44 | 0 |
| `src/game/stageobjlist.cpp` | 45 | 0 |
| `src/game/stageparty.cpp` | 52 | 0 |
| `src/game/stageplayer.cpp` | 33 | 0 |
| `src/game/stageresult.cpp` | 32 | 0 |
| `src/game/stagescene.cpp` | 259 | 0 |
| `src/game/stagesound.cpp` | 45 | 0 |
| `src/game/streamring.cpp` | 17 | 0 |
| `src/game/stringlist.cpp` | 14 | 0 |
| `src/game/subsceneobject.cpp` | 49 | 0 |
| `src/game/sysdata.cpp` | 36 | 0 |
| `src/game/thread.cpp` | 19 | 0 |
| `src/game/titlescreen.cpp` | 3 | 0 |
| `src/game/uidrawer.cpp` | 50 | 0 |
| `src/game/uigame.cpp` | 32 | 0 |
| `src/game/uiitemlist.cpp` | 21 | 0 |
| `src/game/uilayout.cpp` | 22 | 0 |
| `src/game/uisequence.cpp` | 29 | 0 |
| `src/game/uitrack.cpp` | 99 | 0 |
| `src/game/uiwidget.cpp` | 56 | 0 |
| `src/game/uiworkarea.cpp` | 4 | 0 |
| `src/game/vcmtable.cpp` | 52 | 0 |
| `src/game/vectortrack.cpp` | 10 | 0 |
| `src/game/viewcamera.cpp` | 3 | 0 |
| `src/game/wstring.cpp` | 5 | 0 |
| `src/lib/unassigned_stubs.cpp` | 0 | 176 |
| `src/rena/rena_2d_texture_tim2.cpp` | 7 | 0 |
| `src/rena/rena_draw.cpp` | 22 | 0 |
| `src/rena/rena_model.cpp` | 34 | 0 |
| `src/rena/rena_pkt_vif1pktmanage.cpp` | 13 | 0 |
| `src/rena/tim2.cpp` | 4 | 0 |
| `src/rx/filecd.c` | 35 | 0 |
| `src/rx/filehdd.c` | 0 | 2 |
| `src/rx/filehost.c` | 0 | 2 |
| `src/rx/rxfile.c` | 108 | 0 |
| `src/rx/rxsnd.c` | 229 | 0 |
| `src/rx/rxsndstr.c` | 38 | 0 |
| `src/rx/rxsndstradpcm.c` | 0 | 8 |
| `src/rx/rxsndstrbank.c` | 48 | 0 |
| `src/rx/rxsndstrmem.c` | 0 | 3 |
| `src/rx/rxsndstrpcm.c` | 0 | 8 |
| `src/rx/rxsndstrstream.c` | 0 | 5 |
| `src/rx/rxsndut.c` | 6 | 0 |
| `src/rx/rxsys.c` | 126 | 0 |
| `src/rx/rxthread.c` | 53 | 0 |
| `src/sdpr2/sdpr2.c` | 36 | 0 |
| `src/sdpr2/sdpr2drv.c` | 0 | 1 |
| `src/sdpr2/sdpr2pair.c` | 0 | 1 |
| `src/sdpr2/sdpr2single.c` | 0 | 2 |
| `src/sound/sound.cpp` | 97 | 0 |

## Translation units (one session each: `tus.py`)

- 222 TUs: todo 89, partial 39, verify 92, done 30
- next: **game/stageparty.cpp** (continuing a partial TU)
- pinned queue: (empty)
- partial: `game/stageparty.cpp` 49/60 -- Written, batch 157: JackLink 0x27c370 (the class that starts the party for everyone but a scenario's lead; the host had none, so L2 never reached SightJack::sta
- verify: `game/areagrid.cpp` 11/11 -- Cut out of unassigned@002681e0 and pinned 0x283df0-0x2849c0 (batch 090, 4 names; saved by name_ctx). Nothing in the game runs it: the smoke run stays on the lan
- verify: `game/boxgrid.cpp` 29/29 -- All 29 functions of 0x2647e0..0x2676e0 covered: BoxGrid ctor / find / contains / cellTouchesBox, quadContainsPoint, quadCrossesSegment written; std::max / min /
- verify: `game/characterbrain.cpp` 140/166 -- All of 0x1a68b0..0x1a7a20 that has a host slot and all of 0x1d85b0..0x1dec30 are written (batch 137). 28 forwarders wait for natives to name their slots (listed
- verify: `game/characterground.cpp` 19/19 -- All code of the TU is written. Seen in a 285 s New Game run, no crash: F_PC01 stands at z 1139.2 on the ray's floor (attr 59, landed) for the 1320 frames logged
- verify: `game/characterinput.cpp` 10/10 -- All code of the TU is written; the control hook's input half now runs in stageUpdateCharacterMotions. Seen in a 285 s New Game run with the scripted left stick 
- verify: `game/characterroute.cpp` 144/169 -- All of 0x21efa0..0x22fc50 is written (batch 155). Proved: wayoutcmp.py ran the ROM 0x227f30 beside the host findWayOut on 12 stages rute files, 1800 questions, 
- verify: `game/clock.cpp` 14/14 -- Source only. clock.h corrected: +0x18 is a PtrList pointer (made on the first child, grown by 16), not an inline u16 pair; the fields moved to a base ClockNode 
- verify: `game/datetime.cpp` 2/2 -- Cut out of the frontier pick unassigned@0023c020 (the tail of the 0x171300 run) and pinned 0x23de20-0x23e740. getLocalDateTime reads sceCdReadClock, converts wi
- verify: `game/demoevent.cpp` 102/106 -- All of the TU is written or accounted for (batch 116). Added: DemoStream setFont and drawSubtitles, the fsm readers 0x28fa90..0x28ff60 (range added here; the ho
- verify: `game/drawsys.cpp` 164/164 -- All 164 of 0x2d46a0-0x2dac70 are written or listed. New: drawSysAddModel 0x2d5140 (draw group, name flags, wave surfaces, plant textures), drawSysFindKind1Surfa
- verify: `game/fileloader.cpp` 53/53 -- All 53 functions of 0x1319f0..0x132f20 are in src/game/fileloader.cpp with game/fileloader.h; FileJob / FileLoader / PackFile left gamehooks.h (which includes f
- verify: `game/fontdata.cpp` 41/41 -- Cut out of unassigned@00171300 and pinned 0x1fcb40-0x1fe6b0 (batch 056, 31 names: the FontData byte variants countLinesMb / lineWidthFixed / lineWidthFixedMb / 
- verify: `game/gameoversequence.cpp` 12/17 -- All 17 written, batch 151. Forced on node 4 by a temporary hook (removed): letterbox bars ramp in and out and reach both edges at 4:3 and 21:9; CAM_DEAD swings 
- verify: `game/gamepacket.cpp` 6/6 -- All 6 functions written; source-only, callcheck clean. Header now declares the rena Vif1PktManage members the TU calls (ctor, virtual dtor, AddBuffer, AddList, 
- verify: `game/gamepaths.cpp` 14/14 -- All 14 functions of 0x28ff60..0x2908d0 written (batches 054-055, 25 names, Ghidra saved). Path builders from a StageSelect: digit 1 or 2 (variant 0 or not), let
- verify: `game/gamesystem.cpp` 17/17 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N
- verify: `game/gfxtarget.cpp` 15/15 -- Re-pinned to 0x12dfa0..0x12f190: the display records 0x4ae280 and gp-0x7ad8..-0x7ac8 are used only there; RcntTimer starts at 0x12f190. Two GfxDisplay records (
- verify: `game/gsblockmap.cpp` 14/14 -- All 14 functions of 0x142dd0..0x144080 are in src/game/gsblockmap.cpp with the new game/gsblockmap.h (GsBlockMap, TexArea, TexAreaList, TexAreaSlot moved out of
- verify: `game/gsutil.cpp` 12/12 -- Cut out of unassigned@00171300 and pinned 0x230980-0x231410 (batch 048: GsTexture::readTarget 0x231290, rena base Tim2Texture ctor 0x2e2920 / dtor 0x2e2810 / lo
- verify: `game/heap.cpp` 17/17 -- Game code, not MSL: heap and RNG share one object (sdata 0x31a880..0x31a88c, bss 0x329970 then 0x3299c0, sbss 0x31ad80). All 17 functions written; batch 031 nam
- verify: `game/layoutpaths.cpp` 24/24 -- All 24 functions of 0x2ec160..0x2ec8e0 written, no new names, Ghidra untouched. Prototypes moved from gamehooks.h to layoutpaths.h, which gamehooks.h includes. 
- verify: `game/mathutil.cpp` 18/18 -- Source only. New mathutil.h (normalizeAngle moved there from gamehooks.h; uigame.cpp includes it; pi constants are kMath* to avoid uigame / uiwidget locals) and
- verify: `game/memcard.cpp` 55/55 -- All 55 functions written (source only, not built). New this run: Memcard::stepSave (mkdir, then data / icon1.ico / icon.sys each open, write, flush, close; Save
- verify: `game/menupack.cpp` 9/9 -- All 9 written, range now 0x2c3d30..0x2c42a0 (0x2c42a0 is slot 1 of Vt_00319f00, an inline virtual of the class at 0x2ca870, left for it). Renamed getSaveResourc
- verify: `game/menuscene.cpp` 14/14 -- All 14 functions of 0x2b5740..0x2b6b90 written (batch 083, 32 names, saved in Ghidra): the whole menu flow, architecture.md Modules has the map. Build, callchec
- verify: `game/modelpose.cpp` 92/92 -- All 92 written, no stubs left in the TU (batch 111). Second run wrote the five solvers, solveLeg, settle and settleFixed, the sw bones swing with its spring and
- verify: `game/movieplayer.cpp` 41/41 -- Cut out of unassigned@001953f0 (its tail 0x1faa30..0x1fcb40; the rest of that run is still unassigned). New game/movieplayer.h; gamehooks.h lost MoviePlayer and
- verify: `game/navimenu.cpp` 30/30 -- Seen in the user's own playthrough: after stage 1 the navigator appears, the cursor moves and picking the next node starts that stage. Not looked at: the overvi
- verify: `game/nodetransform.cpp` 16/26 -- All of 0x1d7c50..0x1d85b0 written (batches 100, 101); rotation, translate and multiply helpers in vu0math.h. New Game run: 31,560 frames with no crash, the open
- verify: `game/packtables.cpp` 8/51 -- 3D path 1 of 5. Root cause: String and StringList were stubs, so every stage script split into zero lines. Written here: game/stringlist.cpp (marked done), Pack
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/pakarchive.cpp` 17/17 -- All 17 functions of 0x231410..0x231bb0 are in src/game/pakarchive.cpp with game/pakarchive.h (the fileloader stand-in completed). The file keeps its EE layout o
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/playermotion.cpp` 246/246 -- All 244 functions are written; callcheck is clean, with one call noted as unwritten: sideHook1's brain half 0x1dd2f0 -> 0x22e740, game/characterbrain.cpp. Seen,
- verify: `game/prgmccard.cpp` 7/7 -- All 7 written from 0x2b85a0..0x2b9ef0. Picture: Load Game header, empty bar and the no memory card text, the right message for the host empty slot; decide close
- verify: `game/progress.cpp` 25/25 -- All 24 functions written, source-only (callcheck clean, layout lint OK; g++ -fsyntax-only clean with a stub scetypes.h). progress.h holds NaviTable / NaviNode (
- verify: `game/routefilter.cpp` 36/36 -- All of 0x23f290..0x242e80 is written (batch 153). It cannot run yet: every RoutePlayer method it calls is a stub, so findPath and pathLength answer -1 and a wal
- verify: `game/routeplayer.cpp` 47/100 -- Complete and compared with the ROM: rutecmp.py runs the ROM's code on eerun.py beside the host over 12 stages' rute files, 3840 questions, 0 differences. The fi
- verify: `game/s2ddata.cpp` 9/9 -- Host: S2dRef<T> (s2ddata.h) is a 32-bit offset from the field itself, 0 null, implicit T* conversion, copy deleted; S2dData, S2dObject, S2dWidgetDef, S2dImage a
- verify: `game/scene.cpp` 1/1 -- gameSceneThread written in full: loadInitPack, runStartupSequence, overlayThreadStart, then forever loadMenuPack, the scene from sScenes (one retail entry, Menu
- verify: `game/scenepairs.cpp` 18/18 -- All 18 of 0x171300..0x171a30 written, batch 117, plus StageMapObject's sideHook0..3 and setPosition, setRotation, setYaw in mapobject.cpp (0x174a10..0x174aa0, 0
- verify: `game/scenerender.cpp` 24/34 -- The pin is closed: applyDebugAmbient, its call, the README paragraph and SceneRender::isKeyed with its two guards are gone; nothing was brightened. Measured in 
- verify: `game/screen.cpp` 18/18 -- All 18 functions of 0x2676e0..0x2681e0 written (batch 037, 33 names, Ghidra saved). Overlay thread slot 6 draws the BG movie screen layout (init pack file 2dEU/
- verify: `game/scriptrunner.cpp` 439/657 -- All 364 natives the scan finds a script calling are bound; py sssall.py --unwritten lists none called, 160 never called stay unwritten (the scan reads version 0
- verify: `game/seplayer.cpp` 35/35 -- Cut from unassigned@002908d0: game/seplayer.cpp is 0x2908d0..0x2937e0 (SoundTrack 17, SePlayer 18, batch 078: fsmFindTrack, fsmTrackHasText, seDistanceVolume, s
- verify: `game/sldi.cpp` 1/1 -- Cut out of unassigned@002c42a0 and pinned 0x2d0a10-0x2d0d10; no new names, no Ghidra change. Scratch test: all 114 SLDI files on the disc unpack to the header s
- verify: `game/spkrom.cpp` 13/13 -- All 13 of 0x152730..0x153108 written. The entries keep offsets on the host (the ROM relocates them to pointers in place), strings through spkString. Host: argv[
- verify: `game/stagecamera.cpp` 0/0 -- Written in src/game/scriptrunner.cpp: nativeSetPlayer 0x1c81a0, nativeScriptView 0x1c8dc0, isNullName 0x1c8f40, findCameraObject 0x1d1260, findLightObject 0x1b5
- verify: `game/stagecharacter.cpp` 14/14 -- All of the TU's code is written, the terminate hook included (0x179d30 joined the ranges; 0x214f20 too). Stretched limbs do not return: a temporary third-person
- verify: `game/stagecollision.cpp` 56/74 -- All of the TU's functions are written or mapped to StageList and the host vector. Seen on stage 1's data in a New Game run: against brute force over all 8165 fa
- verify: `game/stageitem.cpp` 108/139 -- All of 0x17fce0..0x184860 written or mapped (thunks listed, 0x180018 is padding). New Game run under gdb: StageBullet's ctor ran from setupCameras with no fault
- verify: `game/stagelight.cpp` 64/101 -- All of 0x184860..0x186ee0 written. New Game run: lights build and join the scene with no fault, 114 stubs reached, the stop is unchanged (drawSysCurrent null in
- verify: `game/stageloop.cpp` 9/9 -- Cut out of unassigned@00171300 and pinned 0x17f150-0x17fce0 (batch 073, 45 names; Ghidra saved by name_ctx). New src/game/stageloop.h defines gStageScene (exter
- verify: `game/stagemodel.cpp` 17/19 -- All of StageModel is written (init, bindBones, placeBones, setBaseMotion, advance, applyMotion, the play and find calls, the dtor). BoneNode and ModelLink were 
- verify: `game/stageobject.cpp` 43/43 -- All 43 functions are written, no stubs; the old partial status was stale, which is why tus.py next picked it. This run compared setProperty 0x189f40, getPropert
- verify: `game/stageobjlist.cpp` 45/45 -- Cut out of unassigned@00171300 and pinned 0x193900-0x1953f0 (batch 058, 45 names; Ghidra saved). Boxgrid's frontier callees 0x193780..0x1938f8 are MSL vector in
- verify: `game/stageplayer.cpp` 33/33 -- Fix for the user's report that nothing starts once the kid reaches downstairs. BoxVolume had no runner slot +0xfc, so no AREA_ script fired; ClearSequence was s
- verify: `game/stageresult.cpp` 32/32 -- All 32 functions of 0x2afc20..0x2b0d50 written or listed as MSL map/vector instances (batch 036, 44 names, Ghidra saved). StageResult: counter[0] continues (add
- verify: `game/stagescene.cpp` 256/258 -- Fix for the user's crash at the start of the mission after stage 1. isMotionTarget 0x175c40 was a stub, so node 4's crow oCM99_20 had no pose and native 0xb1 fa
- verify: `game/stagesound.cpp` 45/45 -- Batch 077 (31 names: the 18 slice functions, abs 0x121f08, SoundTrack hasSubtitles / getSubtitle / setVolume, SePlayer setTable / setPitch, StageNodeBase ctor 0
- verify: `game/streamring.cpp` 17/21 -- All 21 functions accounted for: the 17 ring functions written, the thunks 0x151f70 / 0x151f80 / 0x152730 / 0x152740 call the rx functions directly. The classes 
- verify: `game/subsceneobject.cpp` 50/50 -- All 50 written or recorded: the last 14 were header getKinds, empty hooks and thunks (ledger block at the end of the cpp; batch 113 named the thunks, saved). St
- verify: `game/sysdata.cpp` 36/36 -- Cut out of unassigned@00171300 (the frontier pick, 5138 functions) and pinned 0x23af30-0x23c020. One SystemData block of 0x620 bytes at 0x598b30: SaveData +0 (0
- verify: `game/thread.cpp` 19/19 -- All 19 functions written; thread.h has ThreadSlotRecord, ThreadStackDef, ThreadDetourStack, FrameSync, JobRecord with ThreadState / ThreadDetourState / JobCance
- verify: `game/titlescreen.cpp` 3/3 -- All 3 functions of 0x231c90..0x231d40 written, batch 091 names loadingTick 0x231d20 and loadingBegin 0x231d30 (both j overlayShowLoading, fileloader.cpp now cal
- verify: `game/uidrawer.cpp` 50/50 -- game/uidrawer.cpp 0x24dfa0..0x252840: all 50 written (batch 047), source-only so verify. This run: interpolate 0x24dfa0, drawText 0x24eba0, setScissorPixels 0x2
- verify: `game/uigame.cpp` 32/32 -- Batch 052 (4 names, saved by name_ctx). Re-pinned game/uigame.cpp to 0x25fa50..0x263b90: every caller of 0x263b90..0x2640e8 passes gStageScene +0xf4, one class,
- verify: `game/uilayout.cpp` 22/22 -- Cut from unassigned@0023e740 (473 functions) as game/uilayout.cpp 0x243390..0x246230; batch 038 named these 22 and 28 callees (UiSequence / UiActor / UiAnim / U
- verify: `game/uisequence.cpp` 29/29 -- Cut from unassigned@00246230 as game/uisequence.cpp 0x246230..0x249550; batches 039-040 named these 29 and the callees they need (UiMatrix 0x24e580..0x24e8a0, U
- verify: `game/uitrack.cpp` 99/109 -- Second run on the file (batch 043, 20 names). Written from the asm: UiRotateToTrack / UiRotateTrack::apply (world = parent * xform, angle wrapped to -pi..pi, xf
- verify: `game/uiwidget.cpp` 56/56 -- Cut out of unassigned@00252840 (the frontier pick) as 0x25b8e0..0x25fa50, batch 066 (74 names: the widget classes by their type strings from the class table 0x2
- verify: `game/uiworkarea.cpp` 4/4 -- All 4 of 0x243350..0x243390 written. Released unassigned@0017fce0 untouched (third time) because New Game was the untried road into a stage and these were on it
- verify: `game/vcmtable.cpp` 52/52 -- Cut out of unassigned@00296d60 (the frontier pick) as 0x296d60..0x299d90, batches 063-065 (59 names, incl. the binary's std::string instances 0x297fb0.. and new
- verify: `game/wstring.cpp` 5/5 -- Cut out of unassigned@002b0d50 and pinned 0x2d4150-0x2d46a0 (batch 053 named wstrcmp 0x2d4150 and wstrstr 0x2d42a0; Ghidra saved). New src/game/wstring.h holds 
- verify: `rena/rena_2d_texture_tim2.cpp` 7/7 -- All 7 written; class moved to rena/rena_2d_texture_tim2.h. One Tim2Texture per mip level of the first picture; m_image (+0x40) is the TIM2 file itself, not a co
- verify: `rena/rena_draw.cpp` 19/41 -- Skin written: drawSkinnedMesh 0x2e43e0, skinnedPartOutside 0x2e68a0, buildPalette 0x2e9480, skinVertex in model_renderer.cpp. Seen in New Game runs: the boy of 
- verify: `rena/rena_model.cpp` 34/34 -- Written: createInstance 0x2e5210, ModelInstance::init 0x2e4800, ModelSurface 0x2e3e10, buildPartRecs 0x2e4f50, setFrames, renaStepTexAnim, modelFindPart, findGr
- verify: `rena/rena_pkt_vif1pktmanage.cpp` 13/17 -- Pinned 0x2eaff0-0x2ec110 under the new src/rena/ with the game's own file name (strings in Reset / AddList); the stale inferred key rena_pkt_vif1pktmanage.cpp r
- verify: `rx/filecd.c` 35/35 -- All 35 of 0x1565c8..0x157db8 written. rxcmp.py: 34 of 35 identical to RoR with jump targets and data masked; the one difference is rxFileCdFindDir, 0x18 longer 
- verify: `rx/rxfile.c` 108/108 -- rxfile core is RoR's prebuilt rx: rxcmp.py over both ranges finds 106 of 108 identical (jumps, lui, gp masked); rxFileOnCreate 0x154518 / rxFileOnGetSize 0x1545
- verify: `rx/rxsnd.c` 229/229 -- All 229 of 0x159bc0..0x162178 in src/rx/rxsnd.c (C), with new src/rx/rxsnd.h (C and C++) and src/rx/rxthread.h (the seven rxTh calls). 215 functions are RoR's w
- verify: `rx/rxsndstr.c` 38/38 -- All 38 of 0x1634b8..0x164398 written, no Ghidra change. rxsndstr.h now has RxSndStrSlot (EE 0x9c, layout host), RxSndStrFormat (the 8-entry format table), RxSnd
- verify: `rx/rxsndstrbank.c` 48/48 -- Complete in source-only mode (no build yet, so not compiled: check it compiles once bootstrap 7 lands). Bank slot is layout host (pointers); RXWS file records (
- verify: `rx/rxsndut.c` 6/6 -- All 6 written in src/rx/rxsndut.c. Data: debug flags 0x2f5f10 (boot value 7 = error / BG-call / register prints) and the format tables 0x2f5f18 PCM (id 1) / 0x2
- verify: `rx/rxsys.c` 126/126 -- rxsys is RoR's prebuilt rx: rxcmp.py '^rxSys' finds 126 of 126 identical (jumps, lui and gp offsets masked) and the data words and console font equal RoR's at +
- verify: `rx/rxthread.c` 53/53 -- Every rxTh function is RoR's word for word, so the file is RosePan's rxthread.cpp in C with FS2 addresses (architecture.md rx has the data and bss map). rxthrea
- verify: `sdk/libmpeg.cpp` 0/0 -- src/yamipan/media (MpegStream, MpegDecoder), sdk/libmpeg.cpp thin, YamiPan_DrawUiMovie to UiCanvas::drawMacroblocks. The black after the card was the opening fi
- verify: `sdk/mcerror.cpp` 0/1 -- Pinned 0x121b80..0x121ef0 as sdk/mcerror.cpp (read as libmc's: between the read-fast tail and libc abort, maps commands FS2 never issues). McCmd and McErrno in 
- verify: `sdpr2/sdpr2.c` 36/36 -- All 36 functions written (0x1681c0..0x16c870), no new names, no Ghidra change. sdpr2.h is the full header: Sdpr2Type (19-slot function table, layout host), Sdpr
- verify: `sound/sound.cpp` 97/98 -- All 98 of 0x144080..0x148db0 written, one file, no Ghidra change (batch 013 names held). New src/sound/sound.h takes the sound stand-ins out of game/gamehooks.h
- verify: `unassigned@00116fb8` 0/0 -- This key is empty now: its whole run is two lib_ranges (architecture.md Address map, naming table row batch 025). Game code calling __extendsfdf2 / __cmpdf2 / _
- verify: `yamipan/rendering/model_renderer.cpp` 0/0 -- Fix for the user's crash on turning left in node 4, confirmed fixed by the user's play. drawBatch read batch.uvs unchecked. The batch is a single unskinned tria

Recent sessions:

- 2026-11-06 02:24 `game/stageparty.cpp` partial: the sight-jack starts and shows its noise: JackLink, ScreenFill, the draw gate
- 2026-11-06 01:34 `game/stageparty.cpp` partial: the sight-jack without its drawing: StageParty and SightJack logic
- 2026-11-06 00:46 `game/characterroute.cpp` verify: the 12 actions and their task classes, the TU is whole
- 2026-11-05 23:59 `game/routeplayer.cpp` verify: the rute file: RoutePlayer, RouteFile, RouteAreaSearch, all non-MSL functions of 0x252840..0x25b8e0
- 2026-11-05 23:22 `game/routefilter.cpp` verify: RouteFilter and its RouteSearch record: all 36 written or mapped to the host vector; findPath, pathLength, advance, the funnel; getFloorAttr signed as in the RO
- 2026-11-05 22:56 `game/characterroute.cpp` partial: the route, its looks, turns, walk, car and 21 actions; path search and 12 actions left
- 2026-11-05 22:21 `game/gameoversequence.cpp` verify: GameOverSequence, DeathCamera (CAM_DEAD) and SceneDirector, all 17 functions; the game over runs to Mission failed
- 2026-11-05 21:53 `game/scenepool.cpp` done: Squeeze about the centre: the strip, subtitles and prompt keep their shape at any aspect; sprite UVs inset

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `unassigned@00135990` | partial | 3/0/33 | 3.1 | 628 | 0 |
| `unassigned@001eedb0` | partial | 1/0/25 | 4.1 | 131 | 0 |
| `unassigned@00176480` | partial | 13/0/69 | 5.3 | 115 | 0 |
| `unassigned@0018c050` | todo | 0/9/36 | 17.9 | 80 | 2 |
| `game/vectortrack.cpp` | partial | 9/0/16 | 2.5 | 68 | 0 |
| `unassigned@0017e140` | todo | 0/0/78 | 1.4 | 51 | 0 |
| `unassigned@00289cc0` | todo | 0/17/59 | 23.1 | 47 | 0 |
| `unassigned@0027cab0` | partial | 3/2/59 | 13.1 | 46 | 0 |
| `unassigned@00190900` | partial | 2/8/34 | 11.8 | 45 | 5 |
| `unassigned@002a31b0` | partial | 1/18/22 | 5.9 | 39 | 0 |
| `unassigned@00186ee0` | partial | 1/3/6 | 0.8 | 37 | 1 |
| `unassigned@002764b0` | todo | 0/0/43 | 7.8 | 33 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00136690` | applyMatrix | 120 | game | missing | - |
| `002c1430` | FUN_002c1430 | 115 | - | missing | game/packtables.cpp |
| `00177270` | BoneNode::find | 112 | game | missing | - |
| `00135f20` | vecDistance | 81 | game | missing | - |
| `00136490` | scaleVec4 | 70 | game | missing | - |
| `00184370` | StageNodeList::erase | 58 | game | missing | game/stageitem.cpp |
| `001365a0` | vecDot | 56 | game | missing | - |
| `00136560` | vecNormalize | 55 | game | missing | - |
| `001eef90` | FUN_001eef90 | 44 | - | missing | - |
| `00136400` | identityMatrix | 41 | game | missing | - |
| `001ef590` | FUN_001ef590 | 37 | - | missing | - |
| `0017e260` | FUN_0017e260 | 34 | - | missing | - |
| `00276510` | MotionCondition::MotionCondition | 32 | game | missing | - |
| `00190890` | StageNodeTree::spawn | 31 | game | stub | - |
| `001ef0f0` | FUN_001ef0f0 | 29 | - | missing | - |
| `00136640` | multiplyMatrix | 26 | game | missing | - |
| `001842c0` | StageNodeList::pushBack | 26 | game | missing | game/stageitem.cpp |
| `00136500` | invertRigidMatrix | 23 | game | missing | - |
| `00136020` | vecDirection | 22 | game | missing | - |
| `00121b80` | mcErrorCode | 19 | game | missing | sdk/mcerror.cpp |
| `0027cc50` | MotionLink::MotionLink | 19 | game | missing | - |
| `00135fe0` | setVecLength | 18 | game | missing | - |
| `00296cb0` | VectorFade::setFrom | 18 | game | missing | game/vectortrack.cpp |
| `0018c2d0` | StageParticle::setRunning | 18 | game | stub | - |
| `002c0d90` | FUN_002c0d90 | 18 | - | missing | game/packtables.cpp |
| `001365d0` | vecCross | 17 | game | missing | - |
| `001362a0` | rotateMatrixZ | 16 | game | missing | - |
| `00296ca0` | VectorFade::setTo | 16 | game | missing | game/vectortrack.cpp |
| `0017e1b0` | FUN_0017e1b0 | 15 | - | missing | - |
| `0029a700` | FUN_0029a700 | 15 | - | missing | game/vectortrack.cpp |
| `00136430` | copyMatrix | 14 | game | missing | - |
| `001d1a00` | ScriptRunner::getEntry | 14 | game | missing | game/scriptrunner.cpp |
| `0022fe10` | FUN_0022fe10 | 14 | - | missing | - |
| `001ef260` | FUN_001ef260 | 13 | - | missing | - |
| `001d1280` | FUN_001d1280 | 13 | - | missing | game/scriptrunner.cpp |
| `001361c0` | rotateMatrixX | 12 | game | missing | - |
| `00135f70` | vecDistanceSq | 12 | game | missing | - |
| `00296cc0` | VectorFade::VectorFade | 12 | game | missing | game/vectortrack.cpp |
| `002c0d40` | FUN_002c0d40 | 12 | - | missing | game/packtables.cpp |
| `00136460` | translateMatrix | 11 | game | missing | - |

