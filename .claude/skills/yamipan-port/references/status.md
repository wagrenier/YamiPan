# YamiPan status (generated)

Generated 2026-11-03 00:34 at commit `a9e8ce5` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6595**, named **1208** (18%); library functions: 987
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1478, game-inferred 359
- context-name ledger: 1837 entries; overrides of wrong library matches: 0
- placed in a source file: 1197/6595 in 36 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `00180000` | 0/336 (0%) | - |
| `00190000` | 0/247 (0%) | - |
| `001a0000` | 0/301 (0%) | - |
| `001b0000` | 0/242 (0%) | - |
| `001c0000` | 0/233 (0%) | - |
| `001d0000` | 0/237 (0%) | - |
| `001e0000` | 0/75 (0%) | - |
| `001f0000` | 0/185 (0%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **224**; stubs: **0**

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
| `src/game/sysdata.cpp` | 37 | 0 |
| `src/game/thread.cpp` | 19 | 0 |

## Translation units (one session each: `tus.py`)

- 52 TUs: todo 44, verify 12
- next: **unassigned@00171300** (frontier score 10)
- pinned queue: (empty)
- verify: `game/boxgrid.cpp` 29/29 -- All 29 functions of 0x2647e0..0x2676e0 covered: BoxGrid ctor / find / contains / cellTouchesBox, quadContainsPoint, quadCrossesSegment written; std::max / min /
- verify: `game/datetime.cpp` 2/2 -- Cut out of the frontier pick unassigned@0023c020 (the tail of the 0x171300 run) and pinned 0x23de20-0x23e740. getLocalDateTime reads sceCdReadClock, converts wi
- verify: `game/gamesystem.cpp` 17/17 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N
- verify: `game/heap.cpp` 17/17 -- Game code, not MSL: heap and RNG share one object (sdata 0x31a880..0x31a88c, bss 0x329970 then 0x3299c0, sbss 0x31ad80). All 17 functions written; batch 031 nam
- verify: `game/memcard.cpp` 55/55 -- All 55 functions written (source only, not built). New this run: Memcard::stepSave (mkdir, then data / icon1.ico / icon.sys each open, write, flush, close; Save
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/progress.cpp` 24/24 -- All 24 functions written, source-only (callcheck clean, layout lint OK; g++ -fsyntax-only clean with a stub scetypes.h). progress.h holds NaviTable / NaviNode (
- verify: `game/sysdata.cpp` 37/37 -- Cut out of unassigned@00171300 (the frontier pick, 5138 functions) and pinned 0x23af30-0x23c020. One SystemData block of 0x620 bytes at 0x598b30: SaveData +0 (0
- verify: `game/thread.cpp` 19/19 -- All 19 functions written; thread.h has ThreadSlotRecord, ThreadStackDef, ThreadDetourStack, FrameSync, JobRecord with ThreadState / ThreadDetourState / JobCance
- verify: `sdk/mcerror.cpp` 0/1 -- Pinned 0x121b80..0x121ef0 as sdk/mcerror.cpp (read as libmc's: between the read-fast tail and libc abort, maps commands FS2 never issues). McCmd and McErrno in 
- verify: `unassigned@00116fb8` 0/0 -- This key is empty now: its whole run is two lib_ranges (architecture.md Address map, naming table row batch 025). Game code calling __extendsfdf2 / __cmpdf2 / _

Recent sessions:

- 2026-11-03 00:34 `game/boxgrid.cpp` verify: BoxGrid cut out of unassigned@002647e0: a 2D n x n lookup grid over a BoxSet's oriented boxes (cell lists from VU0 box/cell overlap tests, point lookup), plus i
- 2026-11-03 00:16 `game/progress.cpp` verify: Save-progress queries cut out of unassigned@002647e0: the navigator table's unlock conditions (NaviTable, 22 methods), SaveData::hasArchive and stageDefaultReco
- 2026-11-02 23:53 `game/thread.cpp` verify: thread system, frame sync and job worker: all 19 functions of 0x130a20..0x1319f0 reconstructed
- 2026-11-02 23:30 `sdk/mcerror.cpp` verify: mcErrorCode, libmc's error encoder: libmc command and result to SCE_ERROR_ENCODE(0x01080000, errno or Sony code)
- 2026-11-02 23:12 `game/heap.cpp` verify: heap allocator and RNG cut out of unassigned@001037c0 and pinned 0x1037c0-0x104b40: Heap (alloc, free, realloc, marks, gap list), initGlobalHeap / shutdownGloba
- 2026-11-02 22:51 `game/memcard.cpp` verify: memory card module complete: stepSave, stepLoad, stepCheckAll, stepScan and buildIconSys written, the four implicit operator= explained; stepScan looks for the 
- 2026-11-02 22:39 `game/memcard.cpp` partial: memory card module cut out of unassigned@002647e0 and pinned 0x2a9c70-0x2acff0: McDevice libmc wrapper, SaveFile save image and module state written, Memcard ma
- 2026-11-02 22:11 `game/datetime.cpp` verify: RTC date module: getLocalDateTime and DateTime::setFormat cut out of unassigned@0023c020; libscf 0x2640e8..0x2647d8 named and made a library range

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `unassigned@00171300` | todo | 0/0/2913 | 786.2 | 10 | 0 |
| `unassigned@002acff0` | todo | 0/0/688 | 209.3 | 8 | 0 |
| `game/gfxtarget.cpp` | todo | 0/0/8 | 2.6 | 7 | 0 |
| `game/gamepacket.cpp` | todo | 0/0/6 | 0.6 | 6 | 0 |
| `sound/sound.cpp` | todo | 0/0/98 | 18.8 | 6 | 0 |
| `unassigned@00133800` | todo | 0/0/65 | 11.2 | 5 | 0 |
| `game/rcnttimer.cpp` | todo | 0/0/4 | 0.2 | 4 | 0 |
| `unassigned@00148db0` | todo | 0/0/146 | 37.4 | 3 | 0 |
| `rena_pkt_vif1pktmanage.cpp` | todo | 0/0/13 | 1.9 | 2 | 0 |
| `game/spkrom.cpp` | todo | 0/0/13 | 2.4 | 2 | 0 |
| `unassigned@00296d60` | todo | 0/0/267 | 74.1 | 2 | 0 |
| `unassigned@0012ea60` | todo | 0/0/7 | 1.7 | 1 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00121b80` | mcErrorCode | 19 | game | missing | sdk/mcerror.cpp |
| `001937f0` | FUN_001937f0 | 4 | - | missing | - |
| `002b0580` | getStageResult | 4 | game | missing | - |
| `0012e0e0` | gfxFlipDisplay | 4 | game | missing | game/gfxtarget.cpp |
| `00147ee0` | SoundConfig::setOutputMode | 3 | game | missing | sound/sound.cpp |
| `0029efd0` | getSaveExtra | 2 | game | missing | - |
| `002c3d70` | getSaveResource | 2 | game | missing | - |
| `00193880` | FUN_00193880 | 2 | - | missing | - |
| `001527a0` | rxInit | 1 | lib-ctx | missing | game/spkrom.cpp |
| `0012e3f0` | gfxInitRenderTargets | 1 | game | missing | game/gfxtarget.cpp |
| `001486e0` | soundInit | 1 | game | missing | sound/sound.cpp |
| `0012f260` | rcnt1Start | 1 | game | missing | game/rcnttimer.cpp |
| `00151290` | debugSysInit | 1 | game | missing | - |
| `0012f190` | RcntTimer::reset | 1 | game | missing | game/rcnttimer.cpp |
| `0012f1b0` | RcntTimer::~RcntTimer | 1 | game | missing | game/rcnttimer.cpp |
| `00193780` | FUN_00193780 | 1 | - | missing | - |
| `002c3d30` | getSaveResourceAux | 1 | game | missing | - |
| `002b00d0` | StageResult::getPerfectBits | 1 | game | missing | - |
| `0012e050` | gfxGetCurrentDisplay | 1 | game | missing | game/gfxtarget.cpp |
| `0012ead0` | gfxSetDisplayOffset | 1 | game | missing | - |
| `0012dee0` | GamePacketManager::GamePacketManager | 1 | game | missing | game/gamepacket.cpp |
| `00135390` | MasterClock::MasterClock | 1 | game | missing | - |
| `00135210` | Clock::createChild | 1 | game | missing | - |
| `001351b0` | Clock::~Clock | 1 | game | missing | - |
| `0012f200` | RcntTimer::RcntTimer | 1 | game | missing | game/rcnttimer.cpp |
| `001938f0` | FUN_001938f0 | 1 | - | missing | - |
| `00151270` | debugSysUpdate | 1 | game | missing | - |
| `00148500` | soundUpdate | 1 | game | missing | sound/sound.cpp |
| `00151280` | debugSysShutdown | 1 | game | missing | - |
| `00148540` | soundShutdown | 1 | game | missing | sound/sound.cpp |
| `00135330` | MasterClock::~MasterClock | 1 | game | missing | - |
| `0012de80` | GamePacketManager::~GamePacketManager | 1 | game | missing | game/gamepacket.cpp |
| `0012e3a0` | gfxReleaseRenderTargets | 1 | game | missing | game/gfxtarget.cpp |
| `00152750` | rxTerm | 1 | lib-ctx | missing | game/spkrom.cpp |
| `002362a0` | boxContainsPoint | 1 | game | missing | - |
| `002eaff0` | Vif1PktManage::GetUsedSize | 1 | lib-ctx | missing | rena_pkt_vif1pktmanage.cpp |
| `002eb010` | Vif1PktManage::GetBufferSize | 1 | lib-ctx | missing | rena_pkt_vif1pktmanage.cpp |
| `0012de00` | GamePacketManager::openList | 1 | game | missing | game/gamepacket.cpp |
| `0012dd30` | GamePacketManager::closeList | 1 | game | missing | game/gamepacket.cpp |
| `0012ddc0` | GamePacketManager::endFrame | 1 | game | missing | game/gamepacket.cpp |

