# YamiPan status (generated)

Generated 2026-11-03 15:23 at commit `a5c37cf` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6599**, named **2099** (32%); library functions: 987
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1521, game-inferred 1228
- context-name ledger: 2749 entries; overrides of wrong library matches: 0
- placed in a source file: 2009/6599 in 63 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `001b0000` | 0/242 (0%) | - |
| `001c0000` | 0/233 (0%) | - |
| `00210000` | 0/231 (0%) | - |
| `00220000` | 0/167 (0%) | - |
| `00270000` | 0/202 (0%) | - |
| `00310000` | 0/9 (0%) | - |
| `00180000` | 4/336 (1%) | - |
| `001e0000` | 1/75 (1%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **1770**; stubs: **0**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/boxgrid.cpp` | 29 | 0 |
| `src/game/clock.cpp` | 14 | 0 |
| `src/game/datetime.cpp` | 2 | 0 |
| `src/game/fileloader.cpp` | 53 | 0 |
| `src/game/fontdata.cpp` | 31 | 0 |
| `src/game/gamepacket.cpp` | 6 | 0 |
| `src/game/gamepaths.cpp` | 14 | 0 |
| `src/game/gamesystem.cpp` | 17 | 0 |
| `src/game/gfxtarget.cpp` | 15 | 0 |
| `src/game/gsblockmap.cpp` | 14 | 0 |
| `src/game/gsutil.cpp` | 12 | 0 |
| `src/game/heap.cpp` | 17 | 0 |
| `src/game/mathutil.cpp` | 18 | 0 |
| `src/game/memcard.cpp` | 55 | 0 |
| `src/game/movieplayer.cpp` | 41 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/pakarchive.cpp` | 17 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |
| `src/game/progress.cpp` | 25 | 0 |
| `src/game/s2ddata.cpp` | 9 | 0 |
| `src/game/scene.cpp` | 1 | 0 |
| `src/game/screen.cpp` | 18 | 0 |
| `src/game/seplayer.cpp` | 35 | 0 |
| `src/game/stageloop.cpp` | 9 | 0 |
| `src/game/stageobjlist.cpp` | 45 | 0 |
| `src/game/stageresult.cpp` | 32 | 0 |
| `src/game/stagesound.cpp` | 45 | 0 |
| `src/game/streamring.cpp` | 17 | 0 |
| `src/game/sysdata.cpp` | 37 | 0 |
| `src/game/thread.cpp` | 19 | 0 |
| `src/game/uidrawer.cpp` | 50 | 0 |
| `src/game/uigame.cpp` | 32 | 0 |
| `src/game/uilayout.cpp` | 22 | 0 |
| `src/game/uisequence.cpp` | 29 | 0 |
| `src/game/uitrack.cpp` | 99 | 0 |
| `src/game/uiwidget.cpp` | 56 | 0 |
| `src/game/vcmtable.cpp` | 52 | 0 |
| `src/game/wstring.cpp` | 5 | 0 |
| `src/rena/rena_pkt_vif1pktmanage.cpp` | 13 | 0 |
| `src/rx/rxfile.c` | 108 | 0 |
| `src/rx/rxsnd.c` | 229 | 0 |
| `src/rx/rxsndstr.c` | 38 | 0 |
| `src/rx/rxsndstrbank.c` | 48 | 0 |
| `src/rx/rxsndut.c` | 6 | 0 |
| `src/rx/rxsys.c` | 126 | 0 |
| `src/rx/rxthread.c` | 53 | 0 |
| `src/sdpr2/sdpr2.c` | 36 | 0 |
| `src/sound/sound.cpp` | 97 | 0 |

## Translation units (one session each: `tus.py`)

- 85 TUs: todo 50, verify 50
- next: **unassigned@001953f0** (frontier score 42)
- pinned queue: (empty)
- verify: `game/boxgrid.cpp` 29/29 -- All 29 functions of 0x2647e0..0x2676e0 covered: BoxGrid ctor / find / contains / cellTouchesBox, quadContainsPoint, quadCrossesSegment written; std::max / min /
- verify: `game/clock.cpp` 14/14 -- Source only. clock.h corrected: +0x18 is a PtrList pointer (made on the first child, grown by 16), not an inline u16 pair; the fields moved to a base ClockNode 
- verify: `game/datetime.cpp` 2/2 -- Cut out of the frontier pick unassigned@0023c020 (the tail of the 0x171300 run) and pinned 0x23de20-0x23e740. getLocalDateTime reads sceCdReadClock, converts wi
- verify: `game/fileloader.cpp` 53/53 -- All 53 functions of 0x1319f0..0x132f20 are in src/game/fileloader.cpp with game/fileloader.h; FileJob / FileLoader / PackFile left gamehooks.h (which includes f
- verify: `game/fontdata.cpp` 31/31 -- Cut out of unassigned@00171300 and pinned 0x1fcb40-0x1fe6b0 (batch 056, 31 names: the FontData byte variants countLinesMb / lineWidthFixed / lineWidthFixedMb / 
- verify: `game/gamepacket.cpp` 6/6 -- All 6 functions written; source-only, callcheck clean. Header now declares the rena Vif1PktManage members the TU calls (ctor, virtual dtor, AddBuffer, AddList, 
- verify: `game/gamepaths.cpp` 14/14 -- All 14 functions of 0x28ff60..0x2908d0 written (batches 054-055, 25 names, Ghidra saved). Path builders from a StageSelect: digit 1 or 2 (variant 0 or not), let
- verify: `game/gamesystem.cpp` 17/17 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N
- verify: `game/gfxtarget.cpp` 15/15 -- Re-pinned to 0x12dfa0..0x12f190: the display records 0x4ae280 and gp-0x7ad8..-0x7ac8 are used only there; RcntTimer starts at 0x12f190. Two GfxDisplay records (
- verify: `game/gsblockmap.cpp` 14/14 -- All 14 functions of 0x142dd0..0x144080 are in src/game/gsblockmap.cpp with the new game/gsblockmap.h (GsBlockMap, TexArea, TexAreaList, TexAreaSlot moved out of
- verify: `game/gsutil.cpp` 12/12 -- Cut out of unassigned@00171300 and pinned 0x230980-0x231410 (batch 048: GsTexture::readTarget 0x231290, rena base Tim2Texture ctor 0x2e2920 / dtor 0x2e2810 / lo
- verify: `game/heap.cpp` 17/17 -- Game code, not MSL: heap and RNG share one object (sdata 0x31a880..0x31a88c, bss 0x329970 then 0x3299c0, sbss 0x31ad80). All 17 functions written; batch 031 nam
- verify: `game/mathutil.cpp` 18/18 -- Source only. New mathutil.h (normalizeAngle moved there from gamehooks.h; uigame.cpp includes it; pi constants are kMath* to avoid uigame / uiwidget locals) and
- verify: `game/memcard.cpp` 55/55 -- All 55 functions written (source only, not built). New this run: Memcard::stepSave (mkdir, then data / icon1.ico / icon.sys each open, write, flush, close; Save
- verify: `game/movieplayer.cpp` 41/41 -- Cut out of unassigned@001953f0 (its tail 0x1faa30..0x1fcb40; the rest of that run is still unassigned). New game/movieplayer.h; gamehooks.h lost MoviePlayer and
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/pakarchive.cpp` 17/17 -- All 17 functions of 0x231410..0x231bb0 are in src/game/pakarchive.cpp with game/pakarchive.h (the fileloader stand-in completed). The file keeps its EE layout o
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/progress.cpp` 25/25 -- All 24 functions written, source-only (callcheck clean, layout lint OK; g++ -fsyntax-only clean with a stub scetypes.h). progress.h holds NaviTable / NaviNode (
- verify: `game/s2ddata.cpp` 9/9 -- Host: S2dRef<T> (s2ddata.h) is a 32-bit offset from the field itself, 0 null, implicit T* conversion, copy deleted; S2dData, S2dObject, S2dWidgetDef, S2dImage a
- verify: `game/scene.cpp` 1/1 -- gameSceneThread written in full: loadInitPack, runStartupSequence, overlayThreadStart, then forever loadMenuPack, the scene from sScenes (one retail entry, Menu
- verify: `game/screen.cpp` 18/18 -- All 18 functions of 0x2676e0..0x2681e0 written (batch 037, 33 names, Ghidra saved). Overlay thread slot 6 draws the BG movie screen layout (init pack file 2dEU/
- verify: `game/seplayer.cpp` 35/35 -- Cut from unassigned@002908d0: game/seplayer.cpp is 0x2908d0..0x2937e0 (SoundTrack 17, SePlayer 18, batch 078: fsmFindTrack, fsmTrackHasText, seDistanceVolume, s
- verify: `game/stageloop.cpp` 9/9 -- Cut out of unassigned@00171300 and pinned 0x17f150-0x17fce0 (batch 073, 45 names; Ghidra saved by name_ctx). New src/game/stageloop.h defines gStageScene (exter
- verify: `game/stageobjlist.cpp` 45/45 -- Cut out of unassigned@00171300 and pinned 0x193900-0x1953f0 (batch 058, 45 names; Ghidra saved). Boxgrid's frontier callees 0x193780..0x1938f8 are MSL vector in
- verify: `game/stageresult.cpp` 32/32 -- All 32 functions of 0x2afc20..0x2b0d50 written or listed as MSL map/vector instances (batch 036, 44 names, Ghidra saved). StageResult: counter[0] continues (add
- verify: `game/stagesound.cpp` 45/45 -- Batch 077 (31 names: the 18 slice functions, abs 0x121f08, SoundTrack hasSubtitles / getSubtitle / setVolume, SePlayer setTable / setPitch, StageNodeBase ctor 0
- verify: `game/streamring.cpp` 17/21 -- All 21 functions accounted for: the 17 ring functions written, the thunks 0x151f70 / 0x151f80 / 0x152730 / 0x152740 call the rx functions directly. The classes 
- verify: `game/sysdata.cpp` 37/37 -- Cut out of unassigned@00171300 (the frontier pick, 5138 functions) and pinned 0x23af30-0x23c020. One SystemData block of 0x620 bytes at 0x598b30: SaveData +0 (0
- verify: `game/thread.cpp` 19/19 -- All 19 functions written; thread.h has ThreadSlotRecord, ThreadStackDef, ThreadDetourStack, FrameSync, JobRecord with ThreadState / ThreadDetourState / JobCance
- verify: `game/uidrawer.cpp` 50/50 -- game/uidrawer.cpp 0x24dfa0..0x252840: all 50 written (batch 047), source-only so verify. This run: interpolate 0x24dfa0, drawText 0x24eba0, setScissorPixels 0x2
- verify: `game/uigame.cpp` 32/32 -- Batch 052 (4 names, saved by name_ctx). Re-pinned game/uigame.cpp to 0x25fa50..0x263b90: every caller of 0x263b90..0x2640e8 passes gStageScene +0xf4, one class,
- verify: `game/uilayout.cpp` 22/22 -- Cut from unassigned@0023e740 (473 functions) as game/uilayout.cpp 0x243390..0x246230; batch 038 named these 22 and 28 callees (UiSequence / UiActor / UiAnim / U
- verify: `game/uisequence.cpp` 29/29 -- Cut from unassigned@00246230 as game/uisequence.cpp 0x246230..0x249550; batches 039-040 named these 29 and the callees they need (UiMatrix 0x24e580..0x24e8a0, U
- verify: `game/uitrack.cpp` 99/109 -- Second run on the file (batch 043, 20 names). Written from the asm: UiRotateToTrack / UiRotateTrack::apply (world = parent * xform, angle wrapped to -pi..pi, xf
- verify: `game/uiwidget.cpp` 56/56 -- Cut out of unassigned@00252840 (the frontier pick) as 0x25b8e0..0x25fa50, batch 066 (74 names: the widget classes by their type strings from the class table 0x2
- verify: `game/vcmtable.cpp` 52/52 -- Cut out of unassigned@00296d60 (the frontier pick) as 0x296d60..0x299d90, batches 063-065 (59 names, incl. the binary's std::string instances 0x297fb0.. and new
- verify: `game/wstring.cpp` 5/5 -- Cut out of unassigned@002b0d50 and pinned 0x2d4150-0x2d46a0 (batch 053 named wstrcmp 0x2d4150 and wstrstr 0x2d42a0; Ghidra saved). New src/game/wstring.h holds 
- verify: `rena/rena_pkt_vif1pktmanage.cpp` 13/17 -- Pinned 0x2eaff0-0x2ec110 under the new src/rena/ with the game's own file name (strings in Reset / AddList); the stale inferred key rena_pkt_vif1pktmanage.cpp r
- verify: `rx/rxfile.c` 108/108 -- rxfile core is RoR's prebuilt rx: rxcmp.py over both ranges finds 106 of 108 identical (jumps, lui, gp masked); rxFileOnCreate 0x154518 / rxFileOnGetSize 0x1545
- verify: `rx/rxsnd.c` 229/229 -- All 229 of 0x159bc0..0x162178 in src/rx/rxsnd.c (C), with new src/rx/rxsnd.h (C and C++) and src/rx/rxthread.h (the seven rxTh calls). 215 functions are RoR's w
- verify: `rx/rxsndstr.c` 38/38 -- All 38 of 0x1634b8..0x164398 written, no Ghidra change. rxsndstr.h now has RxSndStrSlot (EE 0x9c, layout host), RxSndStrFormat (the 8-entry format table), RxSnd
- verify: `rx/rxsndstrbank.c` 48/48 -- Complete in source-only mode (no build yet, so not compiled: check it compiles once bootstrap 7 lands). Bank slot is layout host (pointers); RXWS file records (
- verify: `rx/rxsndut.c` 6/6 -- All 6 written in src/rx/rxsndut.c. Data: debug flags 0x2f5f10 (boot value 7 = error / BG-call / register prints) and the format tables 0x2f5f18 PCM (id 1) / 0x2
- verify: `rx/rxsys.c` 126/126 -- rxsys is RoR's prebuilt rx: rxcmp.py '^rxSys' finds 126 of 126 identical (jumps, lui and gp offsets masked) and the data words and console font equal RoR's at +
- verify: `rx/rxthread.c` 53/53 -- Every rxTh function is RoR's word for word, so the file is RosePan's rxthread.cpp in C with FS2 addresses (architecture.md rx has the data and bss map). rxthrea
- verify: `sdk/mcerror.cpp` 0/1 -- Pinned 0x121b80..0x121ef0 as sdk/mcerror.cpp (read as libmc's: between the read-fast tail and libc abort, maps commands FS2 never issues). McCmd and McErrno in 
- verify: `sdpr2/sdpr2.c` 36/36 -- All 36 functions written (0x1681c0..0x16c870), no new names, no Ghidra change. sdpr2.h is the full header: Sdpr2Type (19-slot function table, layout host), Sdpr
- verify: `sound/sound.cpp` 97/98 -- All 98 of 0x144080..0x148db0 written, one file, no Ghidra change (batch 013 names held). New src/sound/sound.h takes the sound stand-ins out of game/gamehooks.h
- verify: `unassigned@00116fb8` 0/0 -- This key is empty now: its whole run is two lib_ranges (architecture.md Address map, naming table row batch 025). Game code calling __extendsfdf2 / __cmpdf2 / _

Recent sessions:

- 2026-11-03 15:23 `game/seplayer.cpp` verify: SoundTrack and SePlayer complete: StDT / SEDT tables as EE-layout S2dRef images, the SeHandle bits, the voice records and master fade, placed play, subtitles by
- 2026-11-03 14:58 `game/stagesound.cpp` verify: StageSound complete: cue queries and channel pick, music play / stop / volume, positional sources (addSource on the scene-node base, findSource, setSourcePitch,
- 2026-11-03 14:39 `game/stagesound.cpp` partial: StageSound update: update 0x1d5580 with updateSlots 0x1d5e40 and updateChannels 0x1d56a0, stageSoundLocateFacing 0x1d7a30 / stageSoundLocate 0x1d79c0 (27 of 45 
- 2026-11-03 14:16 `game/stagesound.cpp` partial: StageSound cut out of unassigned@001953f0 (0x1d3dc0..0x1d7c50, 45 functions): 22 written -- life, reset, slots, channels, turn-down / up, effect play / stop, st
- 2026-11-03 14:02 `game/stageloop.cpp` verify: The stage loop: stageThread (slot 3) with its clear, game-over, pause and restart helpers, stageLoad and setStageLoader; all 9 written, source-only, not built
- 2026-11-03 13:41 `game/clock.cpp` verify: Cut out of unassigned@00133800 and pinned 0x1350d0-0x135990; all 14 written: Clock (create, setPaused, createChild, getDeltaSeconds, getSeconds, advance, resetT
- 2026-11-03 13:40 `game/mathutil.cpp` verify: Cut out of unassigned@00133800 and pinned 0x133800-0x1350d0; all 18 written: copyVec4, matrixToRotYXZ / matrixToRotZYX, normalizeAngle / wrapAngle, colorToFloat
- 2026-11-03 13:09 `rx/rxsndut.c` verify: rx sound utility layer: all 6 functions (init / exit / update of the rxsndstr layers, the PCM and ADPCM format tables, debug flags, BG-call check)

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `unassigned@001953f0` | todo | 0/0/972 | 242.5 | 42 | 0 |
| `unassigned@002b0d50` | todo | 0/0/371 | 138.7 | 14 | 0 |
| `unassigned@002681e0` | todo | 0/0/514 | 156.6 | 14 | 0 |
| `unassigned@0017fce0` | todo | 0/0/399 | 76.4 | 13 | 0 |
| `rena_2d_texture_tim2.cpp` | todo | 0/0/7 | 3.5 | 9 | 0 |
| `unassigned@00299d90` | todo | 0/0/215 | 62.3 | 9 | 0 |
| `unassigned@0012c9b8` | todo | 0/0/6 | 0.8 | 7 | 0 |
| `unassigned@00231c90` | todo | 0/0/61 | 36.3 | 7 | 0 |
| `unassigned@00252840` | todo | 0/0/100 | 35.5 | 7 | 0 |
| `unassigned@001fe6b0` | todo | 0/0/563 | 197.2 | 7 | 0 |
| `rx/rxsndstrstream.c` | todo | 0/0/18 | 2.4 | 5 | 0 |
| `sdpr2/sdpr2single.c` | todo | 0/0/24 | 7.0 | 4 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00121b80` | mcErrorCode | 19 | game | missing | sdk/mcerror.cpp |
| `0012cab8` | vsprintf | 6 | lib-ctx | missing | - |
| `00198fc0` | StageScene::drawPaused | 5 | game | missing | - |
| `0029efe0` | getNaviMenu | 5 | game | missing | - |
| `001937f0` | FUN_001937f0 | 4 | - | missing | - |
| `00283df0` | seDistanceVolume | 4 | game | missing | - |
| `00177270` | BoneNode::find | 4 | game | missing | - |
| `00198170` | StageScene::raycast | 4 | game | missing | - |
| `002bc5d0` | KeepList::removeUnkept | 4 | game | missing | - |
| `0025a850` | mslTreeIncrement | 4 | lib-ctx | missing | - |
| `0028fb80` | fsmFindText | 3 | game | missing | - |
| `00231d30` | overlayShowLoading | 3 | native | missing | - |
| `00231d20` | overlayShowLoading | 3 | native | missing | - |
| `0028fd30` | fsmRelocate | 3 | game | missing | - |
| `0028fc80` | fsmFindTrack | 3 | game | missing | - |
| `0019bbe0` | StageScene::~StageScene | 3 | game | missing | - |
| `002e1c50` | Tim2Texture::upload | 3 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `0025b590` | FUN_0025b590 | 3 | - | missing | - |
| `0029efd0` | getSaveExtra | 2 | game | missing | - |
| `002c4140` | getSePlayer | 2 | game | missing | - |
| `0016f9e0` | sdpr2CalcPanDeg | 2 | lib-ctx | missing | sdpr2/sdpr2single.c |
| `00243380` | getUiWorkArea | 2 | game | missing | - |
| `00170088` | sdpr2DrvInit | 2 | lib-ctx | missing | sdpr2/sdpr2drv.c |
| `0016e328` | sdpr2PairInit | 2 | lib-ctx | missing | sdpr2/sdpr2pair.c |
| `0016f808` | sdpr2SingleInit | 2 | lib-ctx | missing | sdpr2/sdpr2single.c |
| `00152740` | rxFileCallSizeHook | 2 | native | missing | game/streamring.cpp |
| `00184370` | StageNodeList::erase | 2 | game | missing | - |
| `001a0560` | stdtEntryCount | 2 | game | missing | - |
| `002e2920` | Tim2Texture::Tim2Texture | 2 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `001a3f90` | StageFiles::~StageFiles | 2 | game | missing | - |
| `0019c180` | StageScene::StageScene | 2 | game | missing | - |
| `001a3db0` | StageFiles::loadSubSceneList | 2 | game | missing | - |
| `001a38e0` | StageFiles::loadCommon | 2 | game | missing | - |
| `001a37e0` | StageFiles::load | 2 | game | missing | - |
| `002e1b70` | Tim2Texture::putTex0 | 2 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `002c3d70` | getSaveResource | 2 | game | missing | - |
| `00193880` | FUN_00193880 | 2 | - | missing | - |
| `002ec660` | optionLayoutPath | 1 | game | missing | - |
| `001660e0` | rxSndStrMemInit | 1 | lib-ctx | missing | rx/rxsndstrmem.c |
| `001656e8` | rxSndStrStreamInit | 1 | lib-ctx | missing | rx/rxsndstrstream.c |

