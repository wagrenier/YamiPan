# YamiPan status (generated)

Generated 2026-11-03 06:59 at commit `50ec473` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6595**, named **1655** (25%); library functions: 987
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1495, game-inferred 798
- context-name ledger: 2293 entries; overrides of wrong library matches: 0
- placed in a source file: 1580/6595 in 50 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `00180000` | 0/336 (0%) | - |
| `001a0000` | 0/301 (0%) | - |
| `001b0000` | 0/242 (0%) | - |
| `001c0000` | 0/233 (0%) | - |
| `00200000` | 0/122 (0%) | - |
| `00210000` | 0/231 (0%) | - |
| `00220000` | 0/167 (0%) | - |
| `00270000` | 0/202 (0%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **593**; stubs: **0**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/boxgrid.cpp` | 29 | 0 |
| `src/game/datetime.cpp` | 2 | 0 |
| `src/game/fontdata.cpp` | 31 | 0 |
| `src/game/gamepaths.cpp` | 14 | 0 |
| `src/game/gamesystem.cpp` | 17 | 0 |
| `src/game/gfxtarget.cpp` | 15 | 0 |
| `src/game/gsutil.cpp` | 12 | 0 |
| `src/game/heap.cpp` | 17 | 0 |
| `src/game/memcard.cpp` | 55 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |
| `src/game/progress.cpp` | 24 | 0 |
| `src/game/s2ddata.cpp` | 9 | 0 |
| `src/game/scene.cpp` | 1 | 0 |
| `src/game/screen.cpp` | 18 | 0 |
| `src/game/stageresult.cpp` | 32 | 0 |
| `src/game/sysdata.cpp` | 37 | 0 |
| `src/game/thread.cpp` | 19 | 0 |
| `src/game/uidrawer.cpp` | 50 | 0 |
| `src/game/uigame.cpp` | 32 | 0 |
| `src/game/uilayout.cpp` | 22 | 0 |
| `src/game/uisequence.cpp` | 29 | 0 |
| `src/game/uitrack.cpp` | 99 | 0 |
| `src/game/wstring.cpp` | 5 | 0 |

## Translation units (one session each: `tus.py`)

- 72 TUs: todo 55, verify 26
- next: **unassigned@00171300** (frontier score 17)
- pinned queue: (empty)
- verify: `game/boxgrid.cpp` 29/29 -- All 29 functions of 0x2647e0..0x2676e0 covered: BoxGrid ctor / find / contains / cellTouchesBox, quadContainsPoint, quadCrossesSegment written; std::max / min /
- verify: `game/datetime.cpp` 2/2 -- Cut out of the frontier pick unassigned@0023c020 (the tail of the 0x171300 run) and pinned 0x23de20-0x23e740. getLocalDateTime reads sceCdReadClock, converts wi
- verify: `game/fontdata.cpp` 31/31 -- Cut out of unassigned@00171300 and pinned 0x1fcb40-0x1fe6b0 (batch 056, 31 names: the FontData byte variants countLinesMb / lineWidthFixed / lineWidthFixedMb / 
- verify: `game/gamepaths.cpp` 14/14 -- All 14 functions of 0x28ff60..0x2908d0 written (batches 054-055, 25 names, Ghidra saved). Path builders from a StageSelect: digit 1 or 2 (variant 0 or not), let
- verify: `game/gamesystem.cpp` 17/17 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N
- verify: `game/gfxtarget.cpp` 15/15 -- Re-pinned to 0x12dfa0..0x12f190: the display records 0x4ae280 and gp-0x7ad8..-0x7ac8 are used only there; RcntTimer starts at 0x12f190. Two GfxDisplay records (
- verify: `game/gsutil.cpp` 12/12 -- Cut out of unassigned@00171300 and pinned 0x230980-0x231410 (batch 048: GsTexture::readTarget 0x231290, rena base Tim2Texture ctor 0x2e2920 / dtor 0x2e2810 / lo
- verify: `game/heap.cpp` 17/17 -- Game code, not MSL: heap and RNG share one object (sdata 0x31a880..0x31a88c, bss 0x329970 then 0x3299c0, sbss 0x31ad80). All 17 functions written; batch 031 nam
- verify: `game/memcard.cpp` 55/55 -- All 55 functions written (source only, not built). New this run: Memcard::stepSave (mkdir, then data / icon1.ico / icon.sys each open, write, flush, close; Save
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/progress.cpp` 24/24 -- All 24 functions written, source-only (callcheck clean, layout lint OK; g++ -fsyntax-only clean with a stub scetypes.h). progress.h holds NaviTable / NaviNode (
- verify: `game/s2ddata.cpp` 9/9 -- Host: S2dRef<T> (s2ddata.h) is a 32-bit offset from the field itself, 0 null, implicit T* conversion, copy deleted; S2dData, S2dObject, S2dWidgetDef, S2dImage a
- verify: `game/scene.cpp` 1/1 -- gameSceneThread written in full: loadInitPack, runStartupSequence, overlayThreadStart, then forever loadMenuPack, the scene from sScenes (one retail entry, Menu
- verify: `game/screen.cpp` 18/18 -- All 18 functions of 0x2676e0..0x2681e0 written (batch 037, 33 names, Ghidra saved). Overlay thread slot 6 draws the BG movie screen layout (init pack file 2dEU/
- verify: `game/stageresult.cpp` 32/32 -- All 32 functions of 0x2afc20..0x2b0d50 written or listed as MSL map/vector instances (batch 036, 44 names, Ghidra saved). StageResult: counter[0] continues (add
- verify: `game/sysdata.cpp` 37/37 -- Cut out of unassigned@00171300 (the frontier pick, 5138 functions) and pinned 0x23af30-0x23c020. One SystemData block of 0x620 bytes at 0x598b30: SaveData +0 (0
- verify: `game/thread.cpp` 19/19 -- All 19 functions written; thread.h has ThreadSlotRecord, ThreadStackDef, ThreadDetourStack, FrameSync, JobRecord with ThreadState / ThreadDetourState / JobCance
- verify: `game/uidrawer.cpp` 50/50 -- game/uidrawer.cpp 0x24dfa0..0x252840: all 50 written (batch 047), source-only so verify. This run: interpolate 0x24dfa0, drawText 0x24eba0, setScissorPixels 0x2
- verify: `game/uigame.cpp` 32/32 -- Batch 052 (4 names, saved by name_ctx). Re-pinned game/uigame.cpp to 0x25fa50..0x263b90: every caller of 0x263b90..0x2640e8 passes gStageScene +0xf4, one class,
- verify: `game/uilayout.cpp` 22/22 -- Cut from unassigned@0023e740 (473 functions) as game/uilayout.cpp 0x243390..0x246230; batch 038 named these 22 and 28 callees (UiSequence / UiActor / UiAnim / U
- verify: `game/uisequence.cpp` 29/29 -- Cut from unassigned@00246230 as game/uisequence.cpp 0x246230..0x249550; batches 039-040 named these 29 and the callees they need (UiMatrix 0x24e580..0x24e8a0, U
- verify: `game/uitrack.cpp` 99/109 -- Second run on the file (batch 043, 20 names). Written from the asm: UiRotateToTrack / UiRotateTrack::apply (world = parent * xform, angle wrapped to -pi..pi, xf
- verify: `game/wstring.cpp` 5/5 -- Cut out of unassigned@002b0d50 and pinned 0x2d4150-0x2d46a0 (batch 053 named wstrcmp 0x2d4150 and wstrstr 0x2d42a0; Ghidra saved). New src/game/wstring.h holds 
- verify: `sdk/mcerror.cpp` 0/1 -- Pinned 0x121b80..0x121ef0 as sdk/mcerror.cpp (read as libmc's: between the read-fast tail and libc abort, maps commands FS2 never issues). McCmd and McErrno in 
- verify: `unassigned@00116fb8` 0/0 -- This key is empty now: its whole run is two lib_ranges (architecture.md Address map, naming table row batch 025). Game code calling __extendsfdf2 / __cmpdf2 / _

Recent sessions:

- 2026-11-03 06:59 `game/gfxtarget.cpp` verify: display records, flip, copy packet and GS allocator set-up: all 15 written (re-pinned 0x12dfa0..0x12f190, batch 057)
- 2026-11-03 06:37 `game/fontdata.cpp` verify: fonts: the knj glyph table FontData and the printers TextPrinter / FontPrinter, cut out of unassigned@00171300 (all 31 written)
- 2026-11-03 06:16 `game/gamepaths.cpp` verify: the stage path builders and the resident init / language packs, cut out of unassigned@002681e0 (all 14 written)
- 2026-11-03 05:50 `game/wstring.cpp` verify: The 16-bit string helpers wstrcmp, wstrstr, wstrFromAscii, wstrcpy, wstrlen: all 5 written, source-only, not built
- 2026-11-03 05:34 `game/uigame.cpp` verify: Fourth run: the text variables (UiWidget::expandTextVariable over the held-item message table, formatTextValue with 28 value names: counts, ammo, objective, rec
- 2026-11-03 05:15 `game/uigame.cpp` partial: Third run: setGameValue / getGameValue (all 40 kinds from the jump tables, the clock amounts), the out-of-line player hooks, stepWeaponPreview, isHighestWinsSta
- 2026-11-03 04:57 `game/uigame.cpp` partial: Second run: testGameState (41 named states), findSprite (key-config and item / weapon sprites), scanItems / scanWeapons, getDoorState, getStagePackData, getButt
- 2026-11-03 04:35 `game/uigame.cpp` partial: The S2D UI game side cut out of unassigned@00252840: UiFont, fMsg and TimM find / relocate, the layout sound and game-value hooks, both target binds, inStage: 1

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `unassigned@00171300` | todo | 0/0/2228 | 541.5 | 17 | 0 |
| `unassigned@001319f0` | todo | 0/0/53 | 4.9 | 16 | 0 |
| `game/gamepacket.cpp` | todo | 0/0/6 | 0.6 | 12 | 0 |
| `unassigned@00142dd0` | todo | 0/0/14 | 4.6 | 10 | 0 |
| `unassigned@00296d60` | todo | 0/0/267 | 74.1 | 10 | 0 |
| `unassigned@002b0d50` | todo | 0/0/371 | 138.7 | 8 | 0 |
| `rena_2d_texture_tim2.cpp` | todo | 0/0/7 | 3.5 | 8 | 0 |
| `unassigned@00133800` | todo | 0/0/65 | 11.2 | 8 | 0 |
| `unassigned@002908d0` | todo | 0/0/67 | 18.8 | 6 | 0 |
| `sound/sound.cpp` | todo | 0/0/98 | 18.8 | 6 | 0 |
| `unassigned@0012c9b8` | todo | 0/0/6 | 0.8 | 5 | 0 |
| `unassigned@00252840` | todo | 0/0/156 | 51.6 | 4 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00121b80` | mcErrorCode | 19 | game | missing | sdk/mcerror.cpp |
| `0012cab8` | vsprintf | 5 | lib-ctx | missing | - |
| `0029efe0` | getNaviMenu | 5 | game | missing | - |
| `0012de00` | GamePacketManager::openList | 4 | game | missing | game/gamepacket.cpp |
| `0012dd30` | GamePacketManager::closeList | 4 | game | missing | game/gamepacket.cpp |
| `001937f0` | FUN_001937f0 | 4 | - | missing | - |
| `002955d0` | getNaviTable | 4 | game | missing | - |
| `00132c10` | FileJob::~FileJob | 3 | game | missing | - |
| `00142dd0` | TexArea::getList | 3 | game | missing | - |
| `00131f00` | PackFile::PackFile | 3 | game | missing | - |
| `00131d70` | PackFile::load | 3 | game | missing | - |
| `00297fb0` | FUN_00297fb0 | 3 | - | missing | - |
| `001339c0` | normalizeAngle | 3 | game | missing | - |
| `002e1c50` | Tim2Texture::upload | 3 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `00147ee0` | SoundConfig::setOutputMode | 3 | game | missing | sound/sound.cpp |
| `0029efd0` | getSaveExtra | 2 | game | missing | - |
| `002c4140` | getSePlayer | 2 | game | missing | - |
| `002e2920` | Tim2Texture::Tim2Texture | 2 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `00193880` | FUN_00193880 | 2 | - | missing | - |
| `001d6a80` | StageSound::playNamed | 2 | game | missing | - |
| `002e1b70` | Tim2Texture::putTex0 | 2 | lib-ctx | missing | rena_2d_texture_tim2.cpp |
| `00132cc0` | FileJob::FileJob | 2 | game | missing | - |
| `00132bc0` | FileJob::start | 2 | game | missing | - |
| `00132280` | FileLoader::startLoad | 2 | game | missing | - |
| `002c3d70` | getSaveResource | 2 | game | missing | - |
| `00143510` | TexAreaList::place | 2 | game | missing | - |
| `001e66c0` | PlayerMotion::refresh | 1 | game | missing | - |
| `002ec660` | optionLayoutPath | 1 | game | missing | - |
| `0012dee0` | GamePacketManager::GamePacketManager | 1 | game | missing | game/gamepacket.cpp |
| `00135390` | MasterClock::MasterClock | 1 | game | missing | - |
| `00135210` | Clock::createChild | 1 | game | missing | - |
| `001351b0` | Clock::~Clock | 1 | game | missing | - |
| `00142df0` | TexArea::resetLists | 1 | game | missing | - |
| `001d6a90` | StageSound::stopNamed | 1 | game | missing | - |
| `00292db0` | SePlayer::stop | 1 | game | missing | - |
| `00151280` | debugSysShutdown | 1 | game | missing | - |
| `00148540` | soundShutdown | 1 | game | missing | sound/sound.cpp |
| `00135330` | MasterClock::~MasterClock | 1 | game | missing | - |
| `0012de80` | GamePacketManager::~GamePacketManager | 1 | game | missing | game/gamepacket.cpp |
| `00152750` | rxTerm | 1 | lib-ctx | missing | game/spkrom.cpp |

