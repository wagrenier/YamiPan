# YamiPan status (generated)

Generated 2026-11-03 02:44 at commit `44f366e` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6595**, named **1501** (23%); library functions: 987
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1478, game-inferred 650
- context-name ledger: 2128 entries; overrides of wrong library matches: 0
- placed in a source file: 1408/6595 in 42 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `00180000` | 0/336 (0%) | - |
| `00190000` | 0/247 (0%) | - |
| `001a0000` | 0/301 (0%) | - |
| `001b0000` | 0/242 (0%) | - |
| `001c0000` | 0/233 (0%) | - |
| `001e0000` | 0/75 (0%) | - |
| `001f0000` | 0/185 (0%) | - |
| `00200000` | 0/122 (0%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **413**; stubs: **0**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/boxgrid.cpp` | 29 | 0 |
| `src/game/datetime.cpp` | 2 | 0 |
| `src/game/gamesystem.cpp` | 17 | 0 |
| `src/game/heap.cpp` | 17 | 0 |
| `src/game/memcard.cpp` | 55 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |
| `src/game/progress.cpp` | 24 | 0 |
| `src/game/scene.cpp` | 1 | 0 |
| `src/game/screen.cpp` | 18 | 0 |
| `src/game/stageresult.cpp` | 32 | 0 |
| `src/game/sysdata.cpp` | 37 | 0 |
| `src/game/thread.cpp` | 19 | 0 |
| `src/game/uilayout.cpp` | 22 | 0 |
| `src/game/uisequence.cpp` | 29 | 0 |
| `src/game/uitrack.cpp` | 87 | 0 |

## Translation units (one session each: `tus.py`)

- 61 TUs: todo 50, partial 1, verify 17
- next: **game/uitrack.cpp** (continuing a partial TU)
- pinned queue: (empty)
- partial: `game/uitrack.cpp` 87/109 -- Cut from unassigned@00249550 as game/uitrack.cpp 0x249550..0x24d530 (0x24d530 S2dData::getImage starts the S2D file). Batches 041-042 named 126 (every track cla
- verify: `game/boxgrid.cpp` 29/29 -- All 29 functions of 0x2647e0..0x2676e0 covered: BoxGrid ctor / find / contains / cellTouchesBox, quadContainsPoint, quadCrossesSegment written; std::max / min /
- verify: `game/datetime.cpp` 2/2 -- Cut out of the frontier pick unassigned@0023c020 (the tail of the 0x171300 run) and pinned 0x23de20-0x23e740. getLocalDateTime reads sceCdReadClock, converts wi
- verify: `game/gamesystem.cpp` 17/17 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N
- verify: `game/heap.cpp` 17/17 -- Game code, not MSL: heap and RNG share one object (sdata 0x31a880..0x31a88c, bss 0x329970 then 0x3299c0, sbss 0x31ad80). All 17 functions written; batch 031 nam
- verify: `game/memcard.cpp` 55/55 -- All 55 functions written (source only, not built). New this run: Memcard::stepSave (mkdir, then data / icon1.ico / icon.sys each open, write, flush, close; Save
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/progress.cpp` 24/24 -- All 24 functions written, source-only (callcheck clean, layout lint OK; g++ -fsyntax-only clean with a stub scetypes.h). progress.h holds NaviTable / NaviNode (
- verify: `game/scene.cpp` 1/1 -- gameSceneThread written in full: loadInitPack, runStartupSequence, overlayThreadStart, then forever loadMenuPack, the scene from sScenes (one retail entry, Menu
- verify: `game/screen.cpp` 18/18 -- All 18 functions of 0x2676e0..0x2681e0 written (batch 037, 33 names, Ghidra saved). Overlay thread slot 6 draws the BG movie screen layout (init pack file 2dEU/
- verify: `game/stageresult.cpp` 32/32 -- All 32 functions of 0x2afc20..0x2b0d50 written or listed as MSL map/vector instances (batch 036, 44 names, Ghidra saved). StageResult: counter[0] continues (add
- verify: `game/sysdata.cpp` 37/37 -- Cut out of unassigned@00171300 (the frontier pick, 5138 functions) and pinned 0x23af30-0x23c020. One SystemData block of 0x620 bytes at 0x598b30: SaveData +0 (0
- verify: `game/thread.cpp` 19/19 -- All 19 functions written; thread.h has ThreadSlotRecord, ThreadStackDef, ThreadDetourStack, FrameSync, JobRecord with ThreadState / ThreadDetourState / JobCance
- verify: `game/uilayout.cpp` 22/22 -- Cut from unassigned@0023e740 (473 functions) as game/uilayout.cpp 0x243390..0x246230; batch 038 named these 22 and 28 callees (UiSequence / UiActor / UiAnim / U
- verify: `game/uisequence.cpp` 29/29 -- Cut from unassigned@00246230 as game/uisequence.cpp 0x246230..0x249550; batches 039-040 named these 29 and the callees they need (UiMatrix 0x24e580..0x24e8a0, U
- verify: `sdk/mcerror.cpp` 0/1 -- Pinned 0x121b80..0x121ef0 as sdk/mcerror.cpp (read as libmc's: between the read-fast tail and libc abort, maps commands FS2 never issues). McCmd and McErrno in 
- verify: `unassigned@00116fb8` 0/0 -- This key is empty now: its whole run is two lib_ranges (architecture.md Address map, naming table row batch 025). Game code calling __extendsfdf2 / __cmpdf2 / _

Recent sessions:

- 2026-11-03 02:44 `game/uitrack.cpp` partial: game/uitrack.cpp 0x249550..0x24d530 cut out: the track factory UiAnim::init, 41 track classes, UiTrack step / isDone / setPlay; 97 of 109 written
- 2026-11-03 02:22 `game/uisequence.cpp` verify: reconstruct 29 functions (verify)
- 2026-11-03 02:03 `game/uilayout.cpp` verify: UiLayout, the S2D 2D layout player: script interpreter (30 opcodes, 17 condition kinds), sequence management, nested layout base UiSubLayout; all 22 functions w
- 2026-11-03 01:36 `game/screen.cpp` verify: The 2D screens: overlay thread (BG movie screen, loading indicator), modal runLayout / runLayoutFile, stage e/h, archive and option screens, game-over and notic
- 2026-11-03 01:19 `game/stageresult.cpp` verify: StageResult (0x599700: counters, two stage clocks, clock layout, mode) and the game-pack loader written; MSL map instances mapped to the host
- 2026-11-03 00:50 `game/scene.cpp` verify: Scene controller gameSceneThread (thread slot 1) and its SceneDef table at 0x2fabd0; callees named (packs, startup sequence, overlay thread, draw env clear)
- 2026-11-03 00:34 `game/boxgrid.cpp` verify: BoxGrid cut out of unassigned@002647e0: a 2D n x n lookup grid over a BoxSet's oriented boxes (cell lists from VU0 box/cell overlap tests, point lookup), plus i
- 2026-11-03 00:16 `game/progress.cpp` verify: Save-progress queries cut out of unassigned@002647e0: the navigator table's unlock conditions (NaviTable, 22 methods), SaveData::hasArchive and stageDefaultReco

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `unassigned@0024d530` | todo | 0/0/255 | 89.4 | 156 | 0 |
| `unassigned@002681e0` | todo | 0/0/595 | 177.6 | 19 | 0 |
| `unassigned@00171300` | todo | 0/0/2851 | 749.7 | 17 | 0 |
| `game/gamepacket.cpp` | todo | 0/0/6 | 0.6 | 12 | 0 |
| `unassigned@001319f0` | todo | 0/0/53 | 4.9 | 12 | 0 |
| `game/gfxtarget.cpp` | todo | 0/0/8 | 2.6 | 8 | 0 |
| `sound/sound.cpp` | todo | 0/0/98 | 18.8 | 6 | 0 |
| `unassigned@002b0d50` | todo | 0/0/632 | 194.3 | 6 | 0 |
| `unassigned@00296d60` | todo | 0/0/267 | 74.1 | 5 | 0 |
| `unassigned@00133800` | todo | 0/0/65 | 11.2 | 5 | 0 |
| `unassigned@0012ea60` | todo | 0/0/7 | 1.7 | 5 | 0 |
| `game/rcnttimer.cpp` | todo | 0/0/4 | 0.2 | 4 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00121b80` | mcErrorCode | 19 | game | missing | sdk/mcerror.cpp |
| `0024fe60` | UiDrawer::setPrim | 13 | game | missing | - |
| `0024d810` | S2dData::getVar | 12 | game | missing | - |
| `0024df60` | S2dData::getString | 11 | game | missing | - |
| `0024d530` | S2dData::getImage | 11 | game | missing | - |
| `0024d570` | S2dData::getImageEntry | 11 | game | missing | - |
| `0024fd40` | UiDrawer::setPrimColor | 10 | game | missing | - |
| `0024f7a0` | UiDrawer::setTexture | 7 | game | missing | - |
| `00290630` | getLangPackItem | 7 | game | missing | - |
| `00231360` | GsTexture::GsTexture | 7 | game | missing | - |
| `002907c0` | getInitPackItem | 6 | game | missing | - |
| `00260100` | UiLayout::stopSound | 5 | game | missing | - |
| `0024fcb0` | UiDrawer::setGsReg | 5 | game | missing | - |
| `0024fae0` | UiDrawer::~UiDrawer | 4 | game | missing | - |
| `0012de00` | GamePacketManager::openList | 4 | game | missing | game/gamepacket.cpp |
| `0012dd30` | GamePacketManager::closeList | 4 | game | missing | game/gamepacket.cpp |
| `001937f0` | FUN_001937f0 | 4 | - | missing | - |
| `0012e0e0` | gfxFlipDisplay | 4 | game | missing | game/gfxtarget.cpp |
| `0024e6d0` | UiMatrix::copy | 4 | game | missing | - |
| `00132c10` | FileJob::~FileJob | 3 | game | missing | - |
| `0012f160` | gfxGetTexArea | 3 | game | missing | - |
| `00142dd0` | TexArea::getList | 3 | game | missing | - |
| `00252260` | UiDrawer::drawTexGouraud | 3 | game | missing | - |
| `00297fb0` | FUN_00297fb0 | 3 | - | missing | - |
| `002524a0` | UiDrawer::drawGouraud | 3 | game | missing | - |
| `0024e710` | UiMatrix::concat | 3 | game | missing | - |
| `0024d640` | S2dData::findObject | 3 | game | missing | - |
| `00290800` | getInitPackData | 3 | game | missing | - |
| `0024e9d0` | UiDrawer::setPixelMode | 3 | game | missing | - |
| `0024ea10` | UiDrawer::setTranslate | 3 | game | missing | - |
| `00147ee0` | SoundConfig::setOutputMode | 3 | game | missing | sound/sound.cpp |
| `00252680` | UiDrawer::drawFlat | 3 | game | missing | - |
| `0024ffa0` | UiDrawer::setBlend | 2 | game | missing | - |
| `0029efd0` | getSaveExtra | 2 | game | missing | - |
| `00193880` | FUN_00193880 | 2 | - | missing | - |
| `001d6a80` | StageSound::playNamed | 2 | game | missing | - |
| `0024e990` | UiDrawer::setMode | 2 | game | missing | - |
| `00132cc0` | FileJob::FileJob | 2 | game | missing | - |
| `00132bc0` | FileJob::start | 2 | game | missing | - |
| `00252040` | UiDrawer::drawTexFlat | 2 | game | missing | - |

