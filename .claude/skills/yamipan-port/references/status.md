# YamiPan status (generated)

Generated 2026-11-09 10:48 at commit `f313599` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6598**, named **5982** (91%); library functions: 992
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1617, game-inferred 5044
- context-name ledger: 6661 entries; overrides of wrong library matches: 0
- placed in a source file: 6512/6598 in 139 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `001c0000` | 122/233 (52%) | game/scriptrunner.cpp |
| `001b0000` | 129/242 (53%) | game/scriptrunner.cpp |
| `002d0000` | 156/256 (61%) | game/drawsys.cpp, game/areaclip.cpp, game/ripplesheet.cpp |
| `001a0000` | 195/301 (65%) | game/scriptrunner.cpp, game/stagescene.cpp, game/characterbrain.cpp |
| `00180000` | 248/336 (74%) | game/stageitem.cpp, game/stagelight.cpp, game/stageobject.cpp |
| `001f0000` | 158/185 (85%) | game/movieplayer.cpp, game/stageparty.cpp, game/scenerender.cpp |
| `002e0000` | 160/178 (90%) | rena/rena_draw.cpp, rena/rena_model.cpp, game/layoutpaths.cpp |
| `00280000` | 221/232 (95%) | game/playermotion.cpp, game/stagecar.cpp, game/demoevent.cpp |

## Reconstruction (src/)

- reconstructed FS2 functions: **6097**; stubs: **3**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/areaclip.cpp` | 18 | 0 |
| `src/game/areagrid.cpp` | 11 | 0 |
| `src/game/boxgrid.cpp` | 29 | 0 |
| `src/game/characterbrain.cpp` | 142 | 0 |
| `src/game/characterground.cpp` | 20 | 0 |
| `src/game/characterinput.cpp` | 11 | 0 |
| `src/game/characterroute.cpp` | 150 | 0 |
| `src/game/charactersound.cpp` | 39 | 0 |
| `src/game/clock.cpp` | 14 | 0 |
| `src/game/datetime.cpp` | 2 | 0 |
| `src/game/debugsys.cpp` | 3 | 0 |
| `src/game/demoevent.cpp` | 103 | 0 |
| `src/game/demofleck.cpp` | 13 | 0 |
| `src/game/drawsys.cpp` | 166 | 0 |
| `src/game/fileloader.cpp` | 53 | 0 |
| `src/game/fontdata.cpp` | 43 | 0 |
| `src/game/framefilter.cpp` | 5 | 0 |
| `src/game/gadgetview.cpp` | 31 | 0 |
| `src/game/gameoversequence.cpp` | 12 | 0 |
| `src/game/gamepacket.cpp` | 6 | 0 |
| `src/game/gamepaths.cpp` | 14 | 0 |
| `src/game/gamesystem.cpp` | 18 | 0 |
| `src/game/gfxtarget.cpp` | 15 | 0 |
| `src/game/gsblockmap.cpp` | 14 | 0 |
| `src/game/gsluminance.cpp` | 7 | 0 |
| `src/game/gsutil.cpp` | 12 | 0 |
| `src/game/heap.cpp` | 17 | 0 |
| `src/game/helpscreen.cpp` | 1 | 0 |
| `src/game/itemphysics.cpp` | 13 | 0 |
| `src/game/kunitoris.cpp` | 93 | 0 |
| `src/game/layoutpaths.cpp` | 24 | 0 |
| `src/game/mapobject.cpp` | 69 | 0 |
| `src/game/mathutil.cpp` | 18 | 0 |
| `src/game/memcard.cpp` | 55 | 0 |
| `src/game/menupack.cpp` | 9 | 0 |
| `src/game/menuprogset.cpp` | 11 | 0 |
| `src/game/menuscene.cpp` | 14 | 0 |
| `src/game/modelpose.cpp` | 92 | 0 |
| `src/game/movieplayer.cpp` | 41 | 0 |
| `src/game/navimenu.cpp` | 30 | 0 |
| `src/game/nodetransform.cpp` | 31 | 0 |
| `src/game/numlock.cpp` | 27 | 0 |
| `src/game/packtables.cpp` | 52 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/pakarchive.cpp` | 17 | 0 |
| `src/game/pausemenu.cpp` | 7 | 0 |
| `src/game/pausemenustate.cpp` | 12 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |
| `src/game/playermotion.cpp` | 663 | 0 |
| `src/game/prgmccard.cpp` | 7 | 0 |
| `src/game/progress.cpp` | 25 | 0 |
| `src/game/rand48.cpp` | 3 | 0 |
| `src/game/rcnttimer.cpp` | 4 | 0 |
| `src/game/ripplesheet.cpp` | 7 | 0 |
| `src/game/routefilter.cpp` | 38 | 0 |
| `src/game/routeplayer.cpp` | 98 | 0 |
| `src/game/rumblelist.cpp` | 10 | 0 |
| `src/game/s2ddata.cpp` | 9 | 0 |
| `src/game/scene.cpp` | 1 | 0 |
| `src/game/sceneeffects.cpp` | 7 | 0 |
| `src/game/scenepairs.cpp` | 18 | 0 |
| `src/game/scenepool.cpp` | 11 | 0 |
| `src/game/scenerender.cpp` | 32 | 0 |
| `src/game/screen.cpp` | 18 | 0 |
| `src/game/scriptrunner.cpp` | 488 | 2 |
| `src/game/seplayer.cpp` | 35 | 0 |
| `src/game/sevenkeys.cpp` | 32 | 0 |
| `src/game/skymist.cpp` | 17 | 0 |
| `src/game/sldi.cpp` | 1 | 0 |
| `src/game/spkrom.cpp` | 13 | 0 |
| `src/game/stagealert.cpp` | 8 | 0 |
| `src/game/stagecamera.cpp` | 59 | 0 |
| `src/game/stagecar.cpp` | 59 | 0 |
| `src/game/stagecharacter.cpp` | 83 | 0 |
| `src/game/stagecollision.cpp` | 58 | 0 |
| `src/game/stagedoor.cpp` | 125 | 0 |
| `src/game/stageelevator.cpp` | 27 | 0 |
| `src/game/stagefog.cpp` | 12 | 0 |
| `src/game/stageitem.cpp` | 137 | 0 |
| `src/game/stagelight.cpp` | 64 | 0 |
| `src/game/stageloop.cpp` | 10 | 0 |
| `src/game/stagemenu.cpp` | 29 | 0 |
| `src/game/stagemodel.cpp` | 35 | 0 |
| `src/game/stageobject.cpp` | 44 | 0 |
| `src/game/stageobjlist.cpp` | 45 | 0 |
| `src/game/stageparticle.cpp` | 50 | 0 |
| `src/game/stageparty.cpp` | 87 | 0 |
| `src/game/stageplayer.cpp` | 58 | 0 |
| `src/game/stageresult.cpp` | 33 | 0 |
| `src/game/stagescene.cpp` | 260 | 0 |
| `src/game/stagesound.cpp` | 45 | 0 |
| `src/game/streamring.cpp` | 17 | 0 |
| `src/game/stringlist.cpp` | 14 | 0 |
| `src/game/subsceneobject.cpp` | 50 | 0 |
| `src/game/sysdata.cpp` | 36 | 0 |
| `src/game/thread.cpp` | 19 | 0 |
| `src/game/titlescreen.cpp` | 3 | 0 |
| `src/game/uidrawer.cpp` | 50 | 0 |
| `src/game/uigame.cpp` | 33 | 0 |
| `src/game/uiitemlist.cpp` | 21 | 0 |
| `src/game/uilayout.cpp` | 22 | 0 |
| `src/game/uisequence.cpp` | 29 | 0 |
| `src/game/uitrack.cpp` | 99 | 0 |
| `src/game/uiwidget.cpp` | 56 | 0 |
| `src/game/uiworkarea.cpp` | 4 | 0 |
| `src/game/vcmtable.cpp` | 52 | 0 |
| `src/game/vectortrack.cpp` | 16 | 0 |
| `src/game/vehiclepose.cpp` | 21 | 0 |
| `src/game/viewcamera.cpp` | 3 | 0 |
| `src/game/vu0math.cpp` | 33 | 0 |
| `src/game/wstring.cpp` | 5 | 0 |
| `src/rena/rena_2d_texture_tim2.cpp` | 8 | 0 |
| `src/rena/rena_2d_texture_vram.cpp` | 11 | 0 |
| `src/rena/rena_camera.cpp` | 1 | 0 |
| `src/rena/rena_draw.cpp` | 22 | 0 |
| `src/rena/rena_drawtarget.cpp` | 1 | 0 |
| `src/rena/rena_model.cpp` | 34 | 0 |
| `src/rena/rena_pkt_vif1pktmanage.cpp` | 13 | 0 |
| `src/rena/tim2.cpp` | 21 | 0 |
| `src/rx/filecd.c` | 35 | 0 |
| `src/rx/filehdd.c` | 13 | 0 |
| `src/rx/filehost.c` | 12 | 0 |
| `src/rx/rxfile.c` | 108 | 0 |
| `src/rx/rxsnd.c` | 229 | 0 |
| `src/rx/rxsndstr.c` | 38 | 0 |
| `src/rx/rxsndstradpcm.c` | 10 | 0 |
| `src/rx/rxsndstrbank.c` | 48 | 0 |
| `src/rx/rxsndstrmem.c` | 16 | 0 |
| `src/rx/rxsndstrpcm.c` | 10 | 0 |
| `src/rx/rxsndstrstream.c` | 18 | 0 |
| `src/rx/rxsndut.c` | 6 | 0 |
| `src/rx/rxsys.c` | 126 | 0 |
| `src/rx/rxthread.c` | 53 | 0 |
| `src/sdpr2/sdpr2.c` | 36 | 0 |
| `src/sdpr2/sdpr2drv.c` | 30 | 0 |
| `src/sdpr2/sdpr2pair.c` | 21 | 0 |
| `src/sdpr2/sdpr2single.c` | 24 | 0 |
| `src/sound/sound.cpp` | 97 | 0 |

## Translation units (one session each: `tus.py`)

- 176 TUs: todo 62, verify 140, done 46
- next: **None** (nothing on the frontier and no queued TU)
- pinned queue: (empty)
- verify: `game/areaclip.cpp` 18/59 -- Proved by areaclipcmp.py: whole ctor on the smallest file, 1,140 sampled box and cell pairs on the other 19, 4,000 frames, 134,343 setClipped, 931 hidden charac
- verify: `game/areagrid.cpp` 11/11 -- Cut out of unassigned@002681e0 and pinned 0x283df0-0x2849c0 (batch 090, 4 names; saved by name_ctx). Nothing in the game runs it: the smoke run stays on the lan
- verify: `game/boxgrid.cpp` 29/29 -- All 29 functions of 0x2647e0..0x2676e0 covered: BoxGrid ctor / find / contains / cellTouchesBox, quadContainsPoint, quadCrossesSegment written; std::max / min /
- verify: `game/characterbrain.cpp` 140/167 -- All of 0x1a68b0..0x1a7a20 that has a host slot and all of 0x1d85b0..0x1dec30 are written (batch 137). 28 forwarders wait for natives to name their slots (listed
- verify: `game/characterground.cpp` 20/20 -- All code of the TU is written. Seen in a 285 s New Game run, no crash: F_PC01 stands at z 1139.2 on the ray's floor (attr 59, landed) for the 1320 frames logged
- verify: `game/characterinput.cpp` 11/11 -- All code of the TU is written; the control hook's input half now runs in stageUpdateCharacterMotions. Seen in a 285 s New Game run with the scripted left stick 
- verify: `game/characterroute.cpp` 150/175 -- All of 0x21efa0..0x22fc50 is written (batch 155). Proved: wayoutcmp.py ran the ROM 0x227f30 beside the host findWayOut on 12 stages rute files, 1800 questions, 
- verify: `game/charactersound.cpp` 39/39 -- All of 0x2a31b0..0x2a49c0 plus getAttackVoice 0x2a25b0. New names playHitSound, playTrapHitVoice, playQuestionVoice, enum SoundKind. Bug fixed in game/playermot
- verify: `game/clock.cpp` 14/14 -- Source only. clock.h corrected: +0x18 is a PtrList pointer (made on the first child, grown by 16), not an inline u16 pair; the fields moved to a base ClockNode 
- verify: `game/datetime.cpp` 2/2 -- Cut out of the frontier pick unassigned@0023c020 (the tail of the 0x171300 run) and pinned 0x23de20-0x23e740. getLocalDateTime reads sceCdReadClock, converts wi
- verify: `game/demoevent.cpp` 103/107 -- All of the TU is written or accounted for (batch 116). Added: DemoStream setFont and drawSubtitles, the fsm readers 0x28fa90..0x28ff60 (range added here; the ho
- verify: `game/demofleck.cpp` 13/13 -- All of the TU is written (batch 161): DemoFleck 0x2a1240, its dtor, thunks draw and step; FleckSet draw 0x2a18c0, step 0x2a1eb0, spawnLarge, spawnSmall, drawLar
- verify: `game/drawsys.cpp` 164/164 -- All 164 of 0x2d46a0-0x2dac70 are written or listed. New: drawSysAddModel 0x2d5140 (draw group, name flags, wave surfaces, plant textures), drawSysFindKind1Surfa
- verify: `game/fileloader.cpp` 53/53 -- All 53 functions of 0x1319f0..0x132f20 are in src/game/fileloader.cpp with game/fileloader.h; FileJob / FileLoader / PackFile left gamehooks.h (which includes f
- verify: `game/fontdata.cpp` 41/41 -- Cut out of unassigned@00171300 and pinned 0x1fcb40-0x1fe6b0 (batch 056, 31 names: the FontData byte variants countLinesMb / lineWidthFixed / lineWidthFixedMb / 
- verify: `game/framefilter.cpp` 5/5 -- All 5 functions of 0x2849c0..0x286790 written (batch 207: gsPageWidth, gsFrameHalve, gsFrameTap, gsFrameDouble; was unassigned@002849c0). The game code keeps ev
- verify: `game/gadgetview.cpp` 31/53 -- Split from unassigned@002a7b10, batch 205. Handle SceneOwnedObject (vtable 0x318ca0, body at +4, slots 1..15 jump to the body's same slot) and body GadgetView (
- verify: `game/gameoversequence.cpp` 12/17 -- All 17 written, batch 151. Forced on node 4 by a temporary hook (removed): letterbox bars ramp in and out and reach both edges at 4:3 and 21:9; CAM_DEAD swings 
- verify: `game/gamepacket.cpp` 6/6 -- All 6 functions written; source-only, callcheck clean. Header now declares the rena Vif1PktManage members the TU calls (ctor, virtual dtor, AddBuffer, AddList, 
- verify: `game/gamepaths.cpp` 14/14 -- All 14 functions of 0x28ff60..0x2908d0 written (batches 054-055, 25 names, Ghidra saved). Path builders from a StageSelect: digit 1 or 2 (variant 0 or not), let
- verify: `game/gamesystem.cpp` 18/18 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N
- verify: `game/gfxtarget.cpp` 15/15 -- Re-pinned to 0x12dfa0..0x12f190: the display records 0x4ae280 and gp-0x7ad8..-0x7ac8 are used only there; RcntTimer starts at 0x12f190. Two GfxDisplay records (
- verify: `game/gsblockmap.cpp` 14/14 -- All 14 functions of 0x142dd0..0x144080 are in src/game/gsblockmap.cpp with the new game/gsblockmap.h (GsBlockMap, TexArea, TexAreaList, TexAreaSlot moved out of
- verify: `game/gsluminance.cpp` 7/7 -- Was unassigned@002b6b90, between menuscene and prgmccard; only caller SightJack::drawPaletteView 0x1ff010 in game/stageparty.cpp, whose comment now points here.
- verify: `game/gsutil.cpp` 12/12 -- Cut out of unassigned@00171300 and pinned 0x230980-0x231410 (batch 048: GsTexture::readTarget 0x231290, rena base Tim2Texture ctor 0x2e2920 / dtor 0x2e2810 / lo
- verify: `game/heap.cpp` 17/17 -- Game code, not MSL: heap and RNG share one object (sdata 0x31a880..0x31a88c, bss 0x329970 then 0x3299c0, sbss 0x31ad80). All 17 functions written; batch 031 nam
- verify: `game/helpscreen.cpp` 1/1 -- Split out of unassigned@002acff0 with game/skymist.cpp; the code went in with 5a6a7e5. Not seen: the smoke run does not open the help screen from the menus.
- verify: `game/itemphysics.cpp` 13/15 -- All of 0x23c010..0x23de20, batch 187 (draw renamed endFrame; getNodeFrame, getMeanVelocity, keepsMoving, placeNodes). New file game/itemphysics.h holds the clas
- verify: `game/kunitoris.cpp` 93/93 -- Batch 229 named 0x2c81b0 kunitorisRandom (j randNext2, only the inline spawnGroup calls it), 0x2c7ee0 KunitorisGroup::copyPair, 0x2c7b70 / 0x2cab50 KunitorisDra
- verify: `game/layoutpaths.cpp` 24/24 -- All 24 functions of 0x2ec160..0x2ec8e0 written, no new names, Ghidra untouched. Prototypes moved from gamehooks.h to layoutpaths.h, which gamehooks.h includes. 
- verify: `game/mapobject.cpp` 70/70 -- unassigned@00176480 turned out to be this file; pinned with 0x174a10 on, and game/stagecamera.cpp pinned to 0x1775b0..0x177be0. Written: StageObject::breakPart 
- verify: `game/mathutil.cpp` 18/18 -- Source only. New mathutil.h (normalizeAngle moved there from gamehooks.h; uigame.cpp includes it; pi constants are kMath* to avoid uigame / uiwidget locals) and
- verify: `game/memcard.cpp` 55/55 -- All 55 functions written (source only, not built). New this run: Memcard::stepSave (mkdir, then data / icon1.ico / icon.sys each open, write, flush, close; Save
- verify: `game/menupack.cpp` 9/9 -- All 9 written, range now 0x2c3d30..0x2c42a0 (0x2c42a0 is slot 1 of Vt_00319f00, an inline virtual of the class at 0x2ca870, left for it). Renamed getSaveResourc
- verify: `game/menuprogset.cpp` 11/11 -- Both sights draw: the rifle's soft mask and spot, the R_64C's round hole, ticks, spot and cross, over PacketList_Scene2d. Proved by sightcmp.py: 400 random fram
- verify: `game/menuscene.cpp` 14/14 -- All 14 functions of 0x2b5740..0x2b6b90 written (batch 083, 32 names, saved in Ghidra): the whole menu flow, architecture.md Modules has the map. Build, callchec
- verify: `game/modelpose.cpp` 92/92 -- All 92 written, no stubs left in the TU (batch 111). Second run wrote the five solvers, solveLeg, settle and settleFixed, the sw bones swing with its spring and
- verify: `game/movieplayer.cpp` 41/41 -- Cut out of unassigned@001953f0 (its tail 0x1faa30..0x1fcb40; the rest of that run is still unassigned). New game/movieplayer.h; gamehooks.h lost MoviePlayer and
- verify: `game/navimenu.cpp` 30/30 -- Seen in the user's own playthrough: after stage 1 the navigator appears, the cursor moves and picking the next node starts that stage. Not looked at: the overvi
- verify: `game/nodetransform.cpp` 30/30 -- 0x186a00 setDirty, 0x186a10 rebuildMatrix, 0x186a20 getMatrix, 0x186a30 getWorldMatrix lie in the lights' run (stagelight.cpp 0x1866f0..0x186ee0) but are StageN
- verify: `game/numlock.cpp` 27/29 -- Split off unassigned@002a6980 (batch 197). NumLock: map object of oMK21_06 / oSJ08_07, now built by createMapObject; kind 5; code from StageResult +0x90 (rename
- verify: `game/packtables.cpp` 52/52 -- Every slice function was an out-of-line MSL member the host already has from the standard library: ListMap (map of int to EntryList), NameMap (map of string to 
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/pakarchive.cpp` 17/17 -- All 17 functions of 0x231410..0x231bb0 are in src/game/pakarchive.cpp with game/pakarchive.h (the fileloader stand-in completed). The file keeps its EE layout o
- verify: `game/pausemenu.cpp` 7/7 -- New game/pausemenu.cpp and .h (PauseMenuChoice moved from gamehooks.h, PauseMenuScreen for answers 4..9, help pages 0 and 7). Fix outside the file: StageScene::
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/playermotion.cpp` 583/588 -- 0x26d020..0x26d090 joined: GunMoveCondition's deleting dtor (vtable 0x3173b0), placed by CodeWarrior right before its onApply 0x26d090 rather than with its sibl
- verify: `game/prgmccard.cpp` 7/7 -- All 7 written from 0x2b85a0..0x2b9ef0. Picture: Load Game header, empty bar and the no memory card text, the right message for the host empty slot; decide close
- verify: `game/progress.cpp` 25/25 -- All 24 functions written, source-only (callcheck clean, layout lint OK; g++ -fsyntax-only clean with a stub scetypes.h). progress.h holds NaviTable / NaviNode (
- verify: `game/ripplesheet.cpp` 7/10 -- Batch 164. What the notes called stage 0x1b's boss film is a 15 by 15 ripple sheet, 0x2d10f0..0x2d3700, SightJack +0xb4: the frame bent, pushed from a tint, cre
- verify: `game/routefilter.cpp` 36/36 -- All of 0x23f290..0x242e80 is written (batch 153). It cannot run yet: every RoutePlayer method it calls is a stub, so findPath and pathLength answer -1 and a wal
- verify: `game/routeplayer.cpp` 100/100 -- Every remaining function was an MSL template instance the host gets from the standard library: areaPath's reverse_copy and copy iterator ops (vector iterators a
- verify: `game/rumblelist.cpp` 10/10 -- Batch 163. KeepList 0x2bc100..0x2bcd20 is the pad's rumble: RumbleList at scene +0x30 plays named tracks of init pack item 0x14 (ALERT, PETITJACK, DAMAGE1, KAKO
- verify: `game/s2ddata.cpp` 9/9 -- Host: S2dRef<T> (s2ddata.h) is a 32-bit offset from the field itself, 0 null, implicit T* conversion, copy deleted; S2dData, S2dObject, S2dWidgetDef, S2dImage a
- verify: `game/scene.cpp` 1/1 -- gameSceneThread written in full: loadInitPack, runStartupSequence, overlayThreadStart, then forever loadMenuPack, the scene from sScenes (one retail entry, Menu
- verify: `game/scenepairs.cpp` 18/18 -- All 18 of 0x171300..0x171a30 written, batch 117, plus StageMapObject's sideHook0..3 and setPosition, setRotation, setYaw in mapobject.cpp (0x174a10..0x174aa0, 0
- verify: `game/scenerender.cpp` 35/47 -- Batch 234 named 15 functions: RenaSpotLight/RenaPointLight getKind (2/1), getAngle (spot +0x7c, point -1), getRadius (+0x70), upload (0x2e00f0/0x2e05f0, host fi
- verify: `game/screen.cpp` 18/18 -- All 18 functions of 0x2676e0..0x2681e0 written (batch 037, 33 names, Ghidra saved). Overlay thread slot 6 draws the BG movie screen layout (init pack file 2dEU/
- verify: `game/scriptrunner.cpp` 488/669 -- The queue, frontier and unassigned runs were empty. Batch 242 named the helpers nativeDemoBind, nativeUseLockGadget, nativeSpawnEffectPlaced and nativeIsWalking
- verify: `game/seplayer.cpp` 35/35 -- Cut from unassigned@002908d0: game/seplayer.cpp is 0x2908d0..0x2937e0 (SoundTrack 17, SePlayer 18, batch 078: fsmFindTrack, fsmTrackHasText, seDistanceVolume, s
- verify: `game/sevenkeys.cpp` 32/35 -- All 35 functions written (thunks by the host's inheritance). Two dials of four positions; turning dial 1 turns dial 0 too. Not visible yet: the keys are a mid-g
- verify: `game/skymist.cpp` 17/23 -- Was the frontier's unassigned@002acff0 (split: game/helpscreen.cpp holds runHelpScreen). Batches 201-202. Drawn through renaBeginEffects and YamiPan_ModelDrawQu
- verify: `game/sldi.cpp` 1/1 -- Cut out of unassigned@002c42a0 and pinned 0x2d0a10-0x2d0d10; no new names, no Ghidra change. Scratch test: all 114 SLDI files on the disc unpack to the header s
- verify: `game/spkrom.cpp` 13/13 -- All 13 of 0x152730..0x153108 written. The entries keep offsets on the host (the ROM relocates them to pointers in place), strings through spkString. Host: argv[
- verify: `game/stagealert.cpp` 8/8 -- All 8 of 0x263b90..0x2640e8 written, class moved to game/stagealert.h, GameOptions::_10 is alertMode (GameAlertMode), 0x263b90 is beforeFileEvent and queueFiles
- verify: `game/stagecamera.cpp` 54/54 -- unassigned@00177d00 (47 functions) was the tail of the camera file and the head of the character file. Pinned: stagecamera.cpp gets 0x177d00..0x179a80 (StageCam
- verify: `game/stagecar.cpp` 59/59 -- No code changed this run; door and drive proven on a temporary build, removed (New Game forced to stage 7, 300 s, no crash). The old pad pressed X every 80 read
- verify: `game/stagecharacter.cpp` 87/96 -- unassigned@0017e140 was the tail of this file: StageList<StageObject *> 0x17e140..0x17e400 and 63 thunks 0x17e470..0x17e860, now listed at the end of stagechara
- verify: `game/stagecollision.cpp` 57/75 -- unassigned@002cf510 (resetStageGroupTable, a reached stub) is resetSurfaceTables, renamed in Ghidra (batch 211) and pinned into game/stagecollision.cpp beside s
- verify: `game/stagedoor.cpp` 126/133 -- unassigned@00171ad0 turned out to be inline functions of this object file: runner table defaults, StageObject accessors, DoorRecord setters; pinned here and lis
- verify: `game/stageelevator.cpp` 27/27 -- All 27 functions of 0x2939b0..0x295470 are written as StageElevator members (batch 184). This run only deleted the dead free-function stubs elevatorCall .. elev
- verify: `game/stagefog.cpp` 12/12 -- All 12 functions of 0x2ce0c0..0x2cedb0 written: SmokeFog (hatsu_fog, I_HATUEN), GlowFog (koh_fog, kinds 5 and 0xc), CharacterFog::attachTo. fogcmp.py: 32,703 op
- verify: `game/stageitem.cpp` 139/139 -- All 31 uncounted functions of 0x17fce0..0x184860 checked against the ROM and listed in the Written in headers block of stageitem.cpp: getKind of every kind, the
- verify: `game/stagelight.cpp` 64/101 -- All of 0x184860..0x186ee0 written. New Game run: lights build and join the scene with no fault, 114 stubs reached, the stop is unchanged (drawSysCurrent null in
- verify: `game/stageloop.cpp` 10/10 -- Cut out of unassigned@00171300 and pinned 0x17f150-0x17fce0 (batch 073, 45 names; Ghidra saved by name_ctx). New src/game/stageloop.h defines gStageScene (exter
- verify: `game/stagemenu.cpp` 29/29 -- All 29 functions written (batch 183); the frontier unassigned@0022fc50 was six MSL vector helpers, now listed in game/characterroute.cpp, and StageMenuFade. Pro
- verify: `game/stagemodel.cpp` 36/40 -- Run 0x188d60..0x189cd4 joined this TU (16 functions, batch 195, all game-inferred): MotionMap and BoneMap dtor, insert, its recursion, node ctor and dtor, find,
- verify: `game/stageobject.cpp` 43/43 -- All 43 functions are written, no stubs; the old partial status was stale, which is why tus.py next picked it. This run compared setProperty 0x189f40, getPropert
- verify: `game/stageobjlist.cpp` 45/45 -- Cut out of unassigned@00171300 and pinned 0x193900-0x1953f0 (batch 058, 45 names; Ghidra saved). Boxgrid's frontier callees 0x193780..0x1938f8 are MSL vector in
- verify: `game/stageparticle.cpp` 51/86 -- unassigned@001920d0 was no file: the object's template instances, 17 functions, batch 186, record removed, range pinned to this TU. Written: game/mslsort.h (0x1
- verify: `game/stageparty.cpp` 84/90 -- Batch 233. 0x2081b0..0x208330 is JackFill, vtable 0x316030: draw 0x2081b0 and ctor 0x2082e0 were already written here; the deleting dtor 0x208280 is JackFill::~
- verify: `game/stageplayer.cpp` 59/59 -- SphereVolume now overrides sideHook0..3 (empty, gamehooks.h). The volumes have three bases: main table (vptr +0xf4), a 4-slot side-hook base at +0x18 (thunks -0
- verify: `game/stageresult.cpp` 33/33 -- All 32 functions of 0x2afc20..0x2b0d50 written or listed as MSL map/vector instances (batch 036, 44 names, Ghidra saved). StageResult: counter[0] continues (add
- verify: `game/stagescene.cpp` 257/259 -- Fix for the user's crash at the start of the mission after stage 1. isMotionTarget 0x175c40 was a stub, so node 4's crow oCM99_20 had no pose and native 0xb1 fa
- verify: `game/stagesound.cpp` 45/45 -- Batch 077 (31 names: the 18 slice functions, abs 0x121f08, SoundTrack hasSubtitles / getSubtitle / setVolume, SePlayer setTable / setPitch, StageNodeBase ctor 0
- verify: `game/streamring.cpp` 17/21 -- All 21 functions accounted for: the 17 ring functions written, the thunks 0x151f70 / 0x151f80 / 0x152730 / 0x152740 call the rx functions directly. The classes 
- verify: `game/subsceneobject.cpp` 51/51 -- No code change: 0x269da0 is the -0x40 adjustor thunk of SubSceneModelObject::~SubSceneModelObject 0x269c30 (vtable entry 0x316cb4), already named in Ghidra (bat
- verify: `game/sysdata.cpp` 36/36 -- Cut out of unassigned@00171300 (the frontier pick, 5138 functions) and pinned 0x23af30-0x23c020. One SystemData block of 0x620 bytes at 0x598b30: SaveData +0 (0
- verify: `game/thread.cpp` 19/19 -- All 19 functions written; thread.h has ThreadSlotRecord, ThreadStackDef, ThreadDetourStack, FrameSync, JobRecord with ThreadState / ThreadDetourState / JobCance
- verify: `game/titlescreen.cpp` 3/3 -- All 3 functions of 0x231c90..0x231d40 written, batch 091 names loadingTick 0x231d20 and loadingBegin 0x231d30 (both j overlayShowLoading, fileloader.cpp now cal
- verify: `game/uidrawer.cpp` 50/50 -- game/uidrawer.cpp 0x24dfa0..0x252840: all 50 written (batch 047), source-only so verify. This run: interpolate 0x24dfa0, drawText 0x24eba0, setScissorPixels 0x2
- verify: `game/uigame.cpp` 33/33 -- Batch 052 (4 names, saved by name_ctx). Re-pinned game/uigame.cpp to 0x25fa50..0x263b90: every caller of 0x263b90..0x2640e8 passes gStageScene +0xf4, one class,
- verify: `game/uilayout.cpp` 22/22 -- Cut from unassigned@0023e740 (473 functions) as game/uilayout.cpp 0x243390..0x246230; batch 038 named these 22 and 28 callees (UiSequence / UiActor / UiAnim / U
- verify: `game/uisequence.cpp` 29/29 -- Cut from unassigned@00246230 as game/uisequence.cpp 0x246230..0x249550; batches 039-040 named these 29 and the callees they need (UiMatrix 0x24e580..0x24e8a0, U
- verify: `game/uitrack.cpp` 99/109 -- Second run on the file (batch 043, 20 names). Written from the asm: UiRotateToTrack / UiRotateTrack::apply (world = parent * xform, angle wrapped to -pi..pi, xf
- verify: `game/uiwidget.cpp` 56/56 -- Cut out of unassigned@00252840 (the frontier pick) as 0x25b8e0..0x25fa50, batch 066 (74 names: the widget classes by their type strings from the class table 0x2
- verify: `game/uiworkarea.cpp` 4/4 -- All 4 of 0x243350..0x243390 written. Released unassigned@0017fce0 untouched (third time) because New Game was the untried road into a stage and these were on it
- verify: `game/vcmtable.cpp` 52/52 -- Cut out of unassigned@00296d60 (the frontier pick) as 0x296d60..0x299d90, batches 063-065 (59 names, incl. the binary's std::string instances 0x297fb0.. and new
- verify: `game/vectortrack.cpp` 16/16 -- All 16 functions written in src/game/vectortrack.cpp; the seven that lived inline in gamehooks.h, where the tools do not count them, moved to the .cpp, and the 
- verify: `game/vehiclepose.cpp` 21/21 -- 21 of 21 functions, no stub. unassigned@0027cab0 split: 0x27dc40..0x280010 is this file, the rest joined game/playermotion.cpp. vehiclecmp.py: 40 scripts, 6040 
- verify: `game/vu0math.cpp` 33/33 -- All 33 functions of 0x135990-0x1366c0 written from the asm (the decompiler loses VU0 field masks), in src/game/vu0math.cpp; vu0math.h is prototypes plus invertA
- verify: `game/wstring.cpp` 5/5 -- Cut out of unassigned@002b0d50 and pinned 0x2d4150-0x2d46a0 (batch 053 named wstrcmp 0x2d4150 and wstrstr 0x2d42a0; Ghidra saved). New src/game/wstring.h holds 
- verify: `rena/rena_2d_texture_tim2.cpp` 8/8 -- The queue picked a stray one-function TU with the bare key rena_2d_texture_tim2.cpp (inferred file without its directory, hazards.md). Fixed by pinning rena/ren
- verify: `rena/rena_2d_texture_vram.cpp` 11/11 -- Cut out of unassigned@002e0820 as rena/rena_2d_texture_vram.cpp 0x2e0840..0x2e1700 (name a guess, no path string). GsRegionAlloc and GsRegion moved from the sta
- verify: `rena/rena_camera.cpp` 2/2 -- rena/rena_camera.cpp pins 0x2e2970..0x2e2cf0. RenaCamera::RenaCamera 0x2e2970 is DrawSysCamera's rena base, already written as that class's ctor in game/drawsys
- verify: `rena/rena_draw.cpp` 19/41 -- Skin written: drawSkinnedMesh 0x2e43e0, skinnedPartOutside 0x2e68a0, buildPalette 0x2e9480, skinVertex in model_renderer.cpp. Seen in New Game runs: the boy of 
- verify: `rena/rena_drawtarget.cpp` 1/1 -- One function, written from the ROM asm: guard byte gp-0x7940 (sbss 0x31af30); first call stores 100, 100, 100 at 0x5a52d0..d8 and 1.0f at 0x5a52e0, sets the gua
- verify: `rena/rena_model.cpp` 34/34 -- Written: createInstance 0x2e5210, ModelInstance::init 0x2e4800, ModelSurface 0x2e3e10, buildPartRecs 0x2e4f50, setFrames, renaStepTexAnim, modelFindPart, findGr
- verify: `rena/rena_pkt_vif1pktmanage.cpp` 13/17 -- Pinned 0x2eaff0-0x2ec110 under the new src/rena/ with the game's own file name (strings in Reset / AddList); the stale inferred key rena_pkt_vif1pktmanage.cpp r
- verify: `rena/tim2.cpp` 20/20 -- Batch 235 named the 11 functions at 0x2e9d90..0x2eaeac after Sony's tim2.c, and the run is pinned to rena/tim2.cpp (now 0x2e97a8..0x2eaeb0). The two psm jump ta
- verify: `rx/filecd.c` 35/35 -- All 35 of 0x1565c8..0x157db8 written. rxcmp.py: 34 of 35 identical to RoR with jump targets and data masked; the one difference is rxFileCdFindDir, 0x18 longer 
- verify: `rx/filehdd.c` 13/13 -- All 13 functions of the HDD device driver written: driver table fill, root prefix join and set, busy flag, size / create / read / write through rxSys file calls
- verify: `rx/filehost.c` 12/12 -- All 12 written from the FS2 decompilation; identical to RosePan rxfile.cpp host section except the open-failure printf: fmt 0x3018d8 with filehost.c and lines 1
- verify: `rx/rxfile.c` 108/108 -- rxfile core is RoR's prebuilt rx: rxcmp.py over both ranges finds 106 of 108 identical (jumps, lui, gp masked); rxFileOnCreate 0x154518 / rxFileOnGetSize 0x1545
- verify: `rx/rxsnd.c` 229/229 -- All 229 of 0x159bc0..0x162178 in src/rx/rxsnd.c (C), with new src/rx/rxsnd.h (C and C++) and src/rx/rxthread.h (the seven rxTh calls). 215 functions are RoR's w
- verify: `rx/rxsndstr.c` 38/38 -- All 38 of 0x1634b8..0x164398 written, no Ghidra change. rxsndstr.h now has RxSndStrSlot (EE 0x9c, layout host), RxSndStrFormat (the 8-entry format table), RxSnd
- verify: `rx/rxsndstradpcm.c` 10/10 -- Complete code, no sound: rxSndStrAdpcmOpen opens a real rxSndAdpcm stream; the block the drivers silent side in the same file reports each open stream from the 
- verify: `rx/rxsndstrbank.c` 48/48 -- Complete in source-only mode (no build yet, so not compiled: check it compiles once bootstrap 7 lands). Bank slot is layout host (pointers); RXWS file records (
- verify: `rx/rxsndstrmem.c` 16/16 -- All 16 functions of 0x1660e0..0x166b50 written, no Ghidra change. The stream record and the pools are host layout, private to the file; SetMarkers and SetTags r
- verify: `rx/rxsndstrpcm.c` 10/10 -- All 10 functions of 0x164398..0x164c38 written, no name changed. Mono / stereo only, no tracks, pitch or attribute in update, writes of any size, start clamped 
- verify: `rx/rxsndstrstream.c` 18/18 -- All 18 functions of 0x1656e8..0x1660e0 written, no Ghidra change. Sources are host layout (a pointer), private to the file; rxSndStrStreamOpen joins the header.
- verify: `rx/rxsndut.c` 6/6 -- All 6 written in src/rx/rxsndut.c. Data: debug flags 0x2f5f10 (boot value 7 = error / BG-call / register prints) and the format tables 0x2f5f18 PCM (id 1) / 0x2
- verify: `rx/rxsys.c` 126/126 -- rxsys is RoR's prebuilt rx: rxcmp.py '^rxSys' finds 126 of 126 identical (jumps, lui and gp offsets masked) and the data words and console font equal RoR's at +
- verify: `rx/rxthread.c` 53/53 -- Every rxTh function is RoR's word for word, so the file is RosePan's rxthread.cpp in C with FS2 addresses (architecture.md rx has the data and bss map). rxthrea
- verify: `sdk/libmpeg.cpp` 0/0 -- src/yamipan/media (MpegStream, MpegDecoder), sdk/libmpeg.cpp thin, YamiPan_DrawUiMovie to UiCanvas::drawMacroblocks. The black after the card was the opening fi
- verify: `sdk/mcerror.cpp` 0/1 -- Pinned 0x121b80..0x121ef0 as sdk/mcerror.cpp (read as libmc's: between the read-fast tail and libc abort, maps commands FS2 never issues). McCmd and McErrno in 
- verify: `sdpr2/sdpr2.c` 36/36 -- All 36 functions written (0x1681c0..0x16c870), no new names, no Ghidra change. sdpr2.h is the full header: Sdpr2Type (19-slot function table, layout host), Sdpr
- verify: `sdpr2/sdpr2drv.c` 30/30 -- All 27 functions plus 3 starts Ghidra had swallowed (batch 203: sdpr2DrvRegister 0x1700d0, sdpr2DrvUnregister 0x1700f8, sdpr2SeDrvUninstall 0x1712e0; created an
- verify: `sdpr2/sdpr2pair.c` 21/21 -- Code complete against the ROM asm; sound only, nothing on screen shows it (smoke run reaches the language select as before, sdpr2PairInit no longer a stub). ROM
- verify: `sdpr2/sdpr2single.c` 24/24 -- All 24 functions written (sdpr2Single* type table filled in sdpr2SingleInit, sdpr2CalcPan / CalcPanDeg / CalcPanVolumes / UnwrapAngle / CalcLevels) plus data 0x
- verify: `sound/sound.cpp` 97/98 -- All 98 of 0x144080..0x148db0 written, one file, no Ghidra change (batch 013 names held). New src/sound/sound.h takes the sound stand-ins out of game/gamehooks.h
- verify: `unassigned@00116fb8` 0/0 -- This key is empty now: its whole run is two lib_ranges (architecture.md Address map, naming table row batch 025). Game code calling __extendsfdf2 / __cmpdf2 / _
- verify: `unassigned@0012c9b8` 0/0 -- This key is empty now: 0x12c9b8..0x12cce0 is newlib (layout.json lib_ranges libc row, architecture.md libc bullet, batch 199 also names __sinit 0x123910, __sfvw
- verify: `unassigned@001dff40` 0/0 -- The run 0x1dff40..0x1e06e4 held levelBlocked and ladderBlocked (already written) and 0x1e05b0, now PlayerMotion::winsStruggle (batch 218): true when this motion
- verify: `unassigned@002176d0` 0/0 -- This key is empty now. 0x2176d0 isViewLinked was already written in src/game/stagecharacter.cpp, now in its range; 0x217700..0x217a80 joined game/playermotion.c
- verify: `unassigned@0021b340` 0/0 -- 0x21b340..0x21ba04 joined game/playermotion.cpp: all 8 functions reconstructed there (dropLongWeapon, takeItem, shout, reload, findLooseWeapon, heardKind, answe
- verify: `unassigned@0029f2a0` 0/0 -- Both written from the ROM. findNumLockTarget: first object of StageObjectList group 1 whose name the number-lock handle (StageScene owned[1], +0x90) has a pair 
- verify: `unassigned@002a0040` 0/0 -- 0x29fdb0..0x2a11b8 joined game/scriptrunner.cpp (batch 220, 10 names, saved in Ghidra): the sort, median, two partitions and selection sort of each instance, th
- verify: `unassigned@002b0d50` 0/0 -- The run was one instruction: j 0x1441a0 (SoundStream::getDataWritten) with a0 unchanged, the out-of-line copy of an inline forward, the first function before De
- verify: `yamipan/rendering/model_renderer.cpp` 0/0 -- Fix for the user's crash on turning left in node 4, confirmed fixed by the user's play. drawBatch read batch.uvs unchecked. The batch is a single unskinned tria

Recent sessions:

- 2026-11-09 10:47 `game/routeplayer.cpp` verify: 52 MSL vector, set, sort and lower_bound instances 0x254e10..0x255080, 0x258bc0..0x25b850 named (batch 247) and listed in a ledger; the TU counts 100 of 100
- 2026-11-09 10:25 `game/stageitem.cpp` verify: stageitem.cpp 139 of 139: the 31 header-inline slots and the StageNodeList instance listed and named, CollisionBodyArray::clear written
- 2026-11-09 10:12 `game/packtables.cpp` verify: 43 MSL map, set, vector and string instances 0x2c0d00..0x2c3d30 named (batch 244) and listed in a ledger; the TU counts 52 of 52
- 2026-11-09 09:54 `game/playermotion.cpp` verify: GunMoveCondition::~GunMoveCondition 0x26d020 named and pinned (was unassigned@0026d020)
- 2026-11-09 09:45 `game/scriptrunner.cpp` verify: 28 out-of-line inline copies on the frontier named (batch 242) and listed with the 6 ScriptRunner accessors in a ledger; scriptLabelCell is 0x1c97f0
- 2026-11-09 08:27 `game/subsceneobject.cpp` verify: SubSceneModelObject dtor thunk 0x269da0 pinned (was unassigned@00269da0)
- 2026-11-09 08:20 `game/playermotion.cpp` verify: MotionLink::onRequest 0x1ec640 named and pinned (was unassigned@001ec640)
- 2026-11-09 08:09 `game/playermotion.cpp` verify: 0x1e5d50..0x1e5d60 joined (was unassigned@001e5d50): MotionLink::onRun, the base's empty slot +0x1c, already in playermotion.h; batch 240 also named the empty d

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `0017e260` | StageList<StageObject*>::erase | 54 | game | missing | game/stagecharacter.cpp |
| `0017e1b0` | StageList<StageObject*>::pushBack | 25 | game | missing | game/stagecharacter.cpp |
| `00121b80` | mcErrorCode | 19 | game | missing | sdk/mcerror.cpp |
| `002a7800` | NmlPose::getDial | 14 | game | missing | game/numlock.cpp |
| `002b3640` | SkyPose::getDial | 11 | game | missing | game/sevenkeys.cpp |
| `00228610` | FUN_00228610 | 9 | - | missing | game/characterroute.cpp |
| `00236910` | BoundingSphere::BoundingSphere | 9 | game | missing | game/stagecollision.cpp |
| `001f4e20` | FUN_001f4e20 | 8 | - | missing | game/stagecollision.cpp |
| `001f4d30` | FUN_001f4d30 | 7 | - | missing | game/stagecollision.cpp |
| `00206f80` | BearingVector::~BearingVector | 6 | lib-ctx | missing | game/stageparty.cpp |
| `001dd3f0` | CharacterBrain::setEnabled | 6 | game | missing | game/characterbrain.cpp |
| `001871c0` | PartSwitch::~PartSwitch | 6 | game | missing | game/stagemodel.cpp |
| `001f4c30` | FUN_001f4c30 | 6 | - | missing | game/stagecollision.cpp |
| `002dd6e0` | ClipIndexList::insert | 6 | game | missing | game/areaclip.cpp |
| `001f4f20` | FUN_001f4f20 | 6 | - | missing | game/stagecollision.cpp |
| `00187220` | PartSwitch::PartSwitch | 5 | game | missing | game/stagemodel.cpp |
| `001f4bb0` | FUN_001f4bb0 | 5 | - | missing | game/stagecollision.cpp |
| `002e7e90` | FUN_002e7e90 | 5 | - | missing | rena/rena_draw.cpp |
| `00186b80` | FUN_00186b80 | 4 | - | missing | game/stagelight.cpp |
| `001920d0` | WordVector::resize | 4 | lib-ctx | missing | game/stageparticle.cpp |
| `001927b0` | WordVector::erase | 4 | lib-ctx | missing | game/stageparticle.cpp |
| `002b3660` | SkyPose::setDial | 4 | game | missing | game/sevenkeys.cpp |
| `001725d0` | FUN_001725d0 | 4 | - | missing | game/stagedoor.cpp |
| `002de000` | ClipSet::ClipSet | 4 | game | missing | game/areaclip.cpp |
| `002ddcf0` | ClipSet::eraseTree | 4 | game | missing | game/areaclip.cpp |
| `002ddf40` | ClipSet::insert | 4 | game | missing | game/areaclip.cpp |
| `002362f0` | BoundingSphere::toBox | 4 | game | missing | game/stagecollision.cpp |
| `001937f0` | BoxColumn::clear | 4 | lib-ctx | missing | game/stageparticle.cpp |
| `002a7820` | NmlPose::setDial | 4 | game | missing | game/numlock.cpp |
| `00206f70` | BearingVector::clear | 3 | lib-ctx | missing | game/stageparty.cpp |
| `002dd9f0` | ClipIndexList::reserve | 3 | game | missing | game/areaclip.cpp |
| `002dd300` | std::sort(u_short) | 3 | game | missing | game/areaclip.cpp |
| `002dd260` | std::lower_bound(u_short) | 3 | game | missing | game/areaclip.cpp |
| `002e7610` | uploadMeshMatrix | 3 | game | missing | rena/rena_draw.cpp |
| `001dd3e0` | CharacterBrain::isEnabled | 2 | game | missing | game/characterbrain.cpp |
| `00231d40` | boxContains | 2 | game | missing | game/stagecollision.cpp |
| `00193210` | EffectSortList::reserve | 2 | lib-ctx | missing | game/stageparticle.cpp |
| `00192150` | mslSort_Effects | 2 | lib-ctx | missing | game/stageparticle.cpp |
| `00193410` | EffectSortList::~EffectSortList | 2 | lib-ctx | missing | game/stageparticle.cpp |
| `001b54a0` | StageNodeBase::setParent | 2 | game | missing | game/scriptrunner.cpp |

