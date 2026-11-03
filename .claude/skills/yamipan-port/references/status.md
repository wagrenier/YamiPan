# YamiPan status (generated)

Generated 2026-11-03 12:49 at commit `ee99b22` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6599**, named **1949** (30%); library functions: 987
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1519, game-inferred 1078
- context-name ledger: 2597 entries; overrides of wrong library matches: 0
- placed in a source file: 1887/6599 in 58 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `001a0000` | 0/301 (0%) | - |
| `001b0000` | 0/242 (0%) | - |
| `001c0000` | 0/233 (0%) | - |
| `00200000` | 0/122 (0%) | - |
| `00210000` | 0/231 (0%) | - |
| `00220000` | 0/167 (0%) | - |
| `00270000` | 0/202 (0%) | - |
| `00310000` | 0/9 (0%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **1642**; stubs: **0**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/boxgrid.cpp` | 29 | 0 |
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
| `src/game/memcard.cpp` | 55 | 0 |
| `src/game/movieplayer.cpp` | 41 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/pakarchive.cpp` | 17 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |
| `src/game/progress.cpp` | 24 | 0 |
| `src/game/s2ddata.cpp` | 9 | 0 |
| `src/game/scene.cpp` | 1 | 0 |
| `src/game/screen.cpp` | 18 | 0 |
| `src/game/stageobjlist.cpp` | 45 | 0 |
| `src/game/stageresult.cpp` | 32 | 0 |
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
| `src/rx/rxsys.c` | 126 | 0 |
| `src/rx/rxthread.c` | 53 | 0 |
| `src/sdpr2/sdpr2.c` | 36 | 0 |
| `src/sound/sound.cpp` | 97 | 0 |

## Translation units (one session each: `tus.py`)

- 78 TUs: todo 47, verify 44
- next: **rx/rxsndut.c** (frontier score 13)
- pinned queue: (empty)
- verify: `game/boxgrid.cpp` 29/29 -- All 29 functions of 0x2647e0..0x2676e0 covered: BoxGrid ctor / find / contains / cellTouchesBox, quadContainsPoint, quadCrossesSegment written; std::max / min /
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
- verify: `game/memcard.cpp` 55/55 -- All 55 functions written (source only, not built). New this run: Memcard::stepSave (mkdir, then data / icon1.ico / icon.sys each open, write, flush, close; Save
- verify: `game/movieplayer.cpp` 41/41 -- Cut out of unassigned@001953f0 (its tail 0x1faa30..0x1fcb40; the rest of that run is still unassigned). New game/movieplayer.h; gamehooks.h lost MoviePlayer and
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/pakarchive.cpp` 17/17 -- All 17 functions of 0x231410..0x231bb0 are in src/game/pakarchive.cpp with game/pakarchive.h (the fileloader stand-in completed). The file keeps its EE layout o
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/progress.cpp` 24/24 -- All 24 functions written, source-only (callcheck clean, layout lint OK; g++ -fsyntax-only clean with a stub scetypes.h). progress.h holds NaviTable / NaviNode (
- verify: `game/s2ddata.cpp` 9/9 -- Host: S2dRef<T> (s2ddata.h) is a 32-bit offset from the field itself, 0 null, implicit T* conversion, copy deleted; S2dData, S2dObject, S2dWidgetDef, S2dImage a
- verify: `game/scene.cpp` 1/1 -- gameSceneThread written in full: loadInitPack, runStartupSequence, overlayThreadStart, then forever loadMenuPack, the scene from sScenes (one retail entry, Menu
- verify: `game/screen.cpp` 18/18 -- All 18 functions of 0x2676e0..0x2681e0 written (batch 037, 33 names, Ghidra saved). Overlay thread slot 6 draws the BG movie screen layout (init pack file 2dEU/
- verify: `game/stageobjlist.cpp` 45/45 -- Cut out of unassigned@00171300 and pinned 0x193900-0x1953f0 (batch 058, 45 names; Ghidra saved). Boxgrid's frontier callees 0x193780..0x1938f8 are MSL vector in
- verify: `game/stageresult.cpp` 32/32 -- All 32 functions of 0x2afc20..0x2b0d50 written or listed as MSL map/vector instances (batch 036, 44 names, Ghidra saved). StageResult: counter[0] continues (add
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
- verify: `rx/rxsys.c` 126/126 -- rxsys is RoR's prebuilt rx: rxcmp.py '^rxSys' finds 126 of 126 identical (jumps, lui and gp offsets masked) and the data words and console font equal RoR's at +
- verify: `rx/rxthread.c` 53/53 -- Every rxTh function is RoR's word for word, so the file is RosePan's rxthread.cpp in C with FS2 addresses (architecture.md rx has the data and bss map). rxthrea
- verify: `sdk/mcerror.cpp` 0/1 -- Pinned 0x121b80..0x121ef0 as sdk/mcerror.cpp (read as libmc's: between the read-fast tail and libc abort, maps commands FS2 never issues). McCmd and McErrno in 
- verify: `sdpr2/sdpr2.c` 36/36 -- All 36 functions written (0x1681c0..0x16c870), no new names, no Ghidra change. sdpr2.h is the full header: Sdpr2Type (19-slot function table, layout host), Sdpr
- verify: `sound/sound.cpp` 97/98 -- All 98 of 0x144080..0x148db0 written, one file, no Ghidra change (batch 013 names held). New src/sound/sound.h takes the sound stand-ins out of game/gamehooks.h
- verify: `unassigned@00116fb8` 0/0 -- This key is empty now: its whole run is two lib_ranges (architecture.md Address map, naming table row batch 025). Game code calling __extendsfdf2 / __cmpdf2 / _

Recent sessions:

- 2026-11-03 12:49 `rx/rxsndstrbank.c` verify: XWS banks: 48 functions, the RXWS chunk walkers, request / mark / branch / track getters and OpenBody, file records in rxsndstrbank.h
- 2026-11-03 12:30 `rx/rxsndstr.c` verify: rx/rxsndstr.c: all 38 stream-core functions written (slot and format tables, play / stop / setters, format callbacks)
- 2026-11-03 12:13 `game/streamring.cpp` verify: StreamRing / FileStreamRing cut out of unassigned@00148db0 as game/streamring.cpp 0x151f70..0x152750: all 17 written, the 4 rx jump thunks mapped to the rx call
- 2026-11-03 11:53 `sdpr2/sdpr2.c` verify: all 36 written: the SDPR2 core and public API, type registry, 64 handle slots, inline checks in sdpr2check.h
- 2026-11-03 11:31 `rx/rxthread.c` verify: all 53 functions written: RosePan's rxthread carried to C, all 53 identical to RoR (rxcmp.py), VBlank tail 0x159928 and core 0x166b50 with FS2 data and bss addr
- 2026-11-03 11:14 `rx/rxfile.c` verify: all 108 functions written: RosePan's rxfile core carried to C, 106 identical to RoR (rxcmp.py), rxFileOnCreate / rxFileOnGetSize with FS2's longer failure messa
- 2026-11-03 10:55 `rx/rxsys.c` verify: all 126 functions written: RosePan's rxsys carried to C, every function RoR's word for word (new rxcmp.py), FS2 addresses
- 2026-11-03 10:34 `rx/rxsnd.c` verify: rx/rxsnd.c: all 229 functions written (RosePan's rxsnd carried to C, FS2's differences and four hidden GetActiveList functions)

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `rx/rxsndut.c` | todo | 0/0/6 | 0.4 | 13 | 0 |
| `unassigned@00133800` | todo | 0/0/65 | 11.2 | 13 | 0 |
| `unassigned@00171300` | todo | 0/0/829 | 131.7 | 10 | 0 |
| `unassigned@002b0d50` | todo | 0/0/371 | 138.7 | 10 | 0 |
| `unassigned@00299d90` | todo | 0/0/215 | 62.3 | 9 | 0 |
| `rena_2d_texture_tim2.cpp` | todo | 0/0/7 | 3.5 | 9 | 0 |
| `unassigned@00252840` | todo | 0/0/100 | 35.5 | 7 | 0 |
| `unassigned@001953f0` | todo | 0/0/1313 | 395.4 | 7 | 0 |
| `unassigned@00231c90` | todo | 0/0/61 | 36.3 | 7 | 0 |
| `unassigned@0012c9b8` | todo | 0/0/6 | 0.8 | 6 | 0 |
| `unassigned@002908d0` | todo | 0/0/67 | 18.8 | 6 | 0 |
| `sdpr2/sdpr2single.c` | todo | 0/0/24 | 7.0 | 4 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00121b80` | mcErrorCode | 19 | game | missing | sdk/mcerror.cpp |
| `00162268` | rxSndUtGetDebugFlags | 7 | lib-ctx | missing | rx/rxsndut.c |
| `0012cab8` | vsprintf | 6 | lib-ctx | missing | - |
| `0029efe0` | getNaviMenu | 5 | game | missing | - |
| `001937f0` | FUN_001937f0 | 4 | - | missing | - |
| `002955d0` | getNaviTable | 4 | game | missing | - |
| `0025a850` | mslTreeIncrement | 4 | lib-ctx | missing | - |
| `00231d30` | overlayShowLoading | 3 | native | missing | - |
| `00231d20` | overlayShowLoading | 3 | native | missing | - |
| `001339c0` | normalizeAngle | 3 | game | missing | - |
| `002e1c50` | Tim2Texture::upload | 3 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `0025b590` | FUN_0025b590 | 3 | - | missing | - |
| `0028fb80` | fsmFindText | 2 | game | missing | - |
| `0029efd0` | getSaveExtra | 2 | game | missing | - |
| `002c4140` | getSePlayer | 2 | game | missing | - |
| `0028fd30` | fsmRelocate | 2 | game | missing | - |
| `001d6a80` | StageSound::playNamed | 2 | game | missing | - |
| `0016f9e0` | sdpr2CalcPanDeg | 2 | lib-ctx | missing | sdpr2/sdpr2single.c |
| `00243380` | getUiWorkArea | 2 | game | missing | - |
| `00135210` | Clock::createChild | 2 | game | missing | - |
| `001357b0` | Clock::resetTime | 2 | game | missing | - |
| `00170088` | sdpr2DrvInit | 2 | lib-ctx | missing | sdpr2/sdpr2drv.c |
| `0016e328` | sdpr2PairInit | 2 | lib-ctx | missing | sdpr2/sdpr2pair.c |
| `0016f808` | sdpr2SingleInit | 2 | lib-ctx | missing | sdpr2/sdpr2single.c |
| `00162220` | rxSndUtUpdate | 2 | lib-ctx | missing | rx/rxsndut.c |
| `00152740` | rxFileCallSizeHook | 2 | native | missing | game/streamring.cpp |
| `001351b0` | Clock::~Clock | 2 | game | missing | - |
| `002e2920` | Tim2Texture::Tim2Texture | 2 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `002e1b70` | Tim2Texture::putTex0 | 2 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `00162278` | rxSndUtCheckBgCall | 2 | lib-ctx | missing | rx/rxsndut.c |
| `002c3d70` | getSaveResource | 2 | game | missing | - |
| `00193880` | FUN_00193880 | 2 | - | missing | - |
| `002ec660` | optionLayoutPath | 1 | game | missing | - |
| `001d6a90` | StageSound::stopNamed | 1 | game | missing | - |
| `00292db0` | SePlayer::stop | 1 | game | missing | - |
| `0029e4f0` | PrgScaleNavi::PrgScaleNavi | 1 | game | missing | - |
| `0029ae30` | PrgScaleNaviScroll::PrgScaleNaviScroll | 1 | game | missing | - |
| `002b9d60` | PrgMcCard::PrgMcCard | 1 | game | missing | - |
| `002e14d0` | GsRegionAlloc::GsRegionAlloc | 1 | game | missing | - |
| `002d7cf0` | drawSysInit | 1 | game | missing | - |

