# YamiPan status (generated)

Generated 2026-11-02 22:51 at commit `b1a39dc` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6597**, named **1140** (17%); library functions: 985
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1476, game-inferred 292
- context-name ledger: 1768 entries; overrides of wrong library matches: 0
- placed in a source file: 1126/6597 in 32 files

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

- reconstructed FS2 functions: **135**; stubs: **0**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/datetime.cpp` | 2 | 0 |
| `src/game/gamesystem.cpp` | 17 | 0 |
| `src/game/memcard.cpp` | 55 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |
| `src/game/sysdata.cpp` | 37 | 0 |

## Translation units (one session each: `tus.py`)

- 48 TUs: todo 42, verify 7
- next: **unassigned@001037c0** (frontier score 24)
- pinned queue: (empty)
- verify: `game/datetime.cpp` 2/2 -- Cut out of the frontier pick unassigned@0023c020 (the tail of the 0x171300 run) and pinned 0x23de20-0x23e740. getLocalDateTime reads sceCdReadClock, converts wi
- verify: `game/gamesystem.cpp` 17/17 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N
- verify: `game/memcard.cpp` 55/55 -- All 55 functions written (source only, not built). New this run: Memcard::stepSave (mkdir, then data / icon1.ico / icon.sys each open, write, flush, close; Save
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/sysdata.cpp` 37/37 -- Cut out of unassigned@00171300 (the frontier pick, 5138 functions) and pinned 0x23af30-0x23c020. One SystemData block of 0x620 bytes at 0x598b30: SaveData +0 (0
- verify: `unassigned@00116fb8` 0/0 -- This key is empty now: its whole run is two lib_ranges (architecture.md Address map, naming table row batch 025). Game code calling __extendsfdf2 / __cmpdf2 / _

Recent sessions:

- 2026-11-02 22:51 `game/memcard.cpp` verify: memory card module complete: stepSave, stepLoad, stepCheckAll, stepScan and buildIconSys written, the four implicit operator= explained; stepScan looks for the 
- 2026-11-02 22:39 `game/memcard.cpp` partial: memory card module cut out of unassigned@002647e0 and pinned 0x2a9c70-0x2acff0: McDevice libmc wrapper, SaveFile save image and module state written, Memcard ma
- 2026-11-02 22:11 `game/datetime.cpp` verify: RTC date module: getLocalDateTime and DateTime::setFormat cut out of unassigned@0023c020; libscf 0x2640e8..0x2647d8 named and made a library range
- 2026-11-02 21:55 `game/sysdata.cpp` verify: system data block: SaveData, StageSelect, GameOptions, FlagTable in SystemData 0x598b30; language and region strings; all 37 functions
- 2026-11-02 21:33 `unassigned@00116fb8` verify: Resolved as library, not game code: newlib float libm 0x116fb8..0x119be8 and libgcc (libgcc2 64-bit helpers + fp-bit soft-float) 0x119be8..0x11c6b0, 51 names by
- 2026-11-02 21:25 `game/perfmeter.cpp` verify: PerfMeter, all 10 functions: ctor, dtor, beginFrame, collectStats, resetStats, writeCsv, dumpGroups and the empty addBar / drawBars / drawDetail; RcntTimer prot
- 2026-11-02 18:09 `game/pad.cpp` verify: Pad layer written: PadPort detection state machine, Pad ctors/dtor, update, readSticks, updateRepeat, setVibration, listener table, padSysInit/Update/Shutdown (
- 2026-11-02 17:59 `game/gamesystem.cpp` verify: main, gameInit, the main loop, GameSystem (ctor, flipFrame, endFrameSync), GamePad / MaskedGamePad, gsClearVram, the frame-ready and DMAC handlers: all 17 funct

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `unassigned@001037c0` | todo | 0/0/19 | 5.0 | 24 | 0 |
| `unassigned@00121b80` | todo | 0/0/1 | 0.9 | 19 | 0 |
| `unassigned@002acff0` | todo | 0/0/688 | 209.3 | 8 | 0 |
| `unassigned@002647e0` | todo | 0/0/937 | 271.6 | 8 | 0 |
| `game/thread.cpp` | todo | 0/0/19 | 3.8 | 8 | 0 |
| `game/gfxtarget.cpp` | todo | 0/0/8 | 2.6 | 7 | 0 |
| `sound/sound.cpp` | todo | 0/0/98 | 18.8 | 6 | 0 |
| `game/gamepacket.cpp` | todo | 0/0/6 | 0.6 | 6 | 0 |
| `unassigned@00133800` | todo | 0/0/65 | 11.2 | 5 | 0 |
| `game/rcnttimer.cpp` | todo | 0/0/4 | 0.2 | 4 | 0 |
| `unassigned@00148db0` | todo | 0/0/146 | 37.4 | 3 | 0 |
| `game/spkrom.cpp` | todo | 0/0/13 | 2.4 | 2 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00121b80` | mcErrorCode | 19 | game | missing | - |
| `00103c50` | Heap::alloc | 12 | game | missing | - |
| `00296d00` | stageDefaultRecord | 6 | game | missing | - |
| `002b0580` | getStageResult | 4 | game | missing | - |
| `0012e0e0` | gfxFlipDisplay | 4 | game | missing | game/gfxtarget.cpp |
| `00147ee0` | SoundConfig::setOutputMode | 3 | game | missing | sound/sound.cpp |
| `00103aa0` | Heap::free | 3 | game | missing | - |
| `00103870` | Heap::markedUsage | 3 | game | missing | - |
| `0029efd0` | getSaveExtra | 2 | game | missing | - |
| `002c3d70` | getSaveResource | 2 | game | missing | - |
| `00130a30` | threadSetSwitchHook | 2 | game | missing | game/thread.cpp |
| `00104600` | initGlobalHeap | 1 | game | missing | - |
| `001527a0` | rxInit | 1 | lib-ctx | missing | game/spkrom.cpp |
| `0012e3f0` | gfxInitRenderTargets | 1 | game | missing | game/gfxtarget.cpp |
| `001486e0` | soundInit | 1 | game | missing | sound/sound.cpp |
| `0012f260` | rcnt1Start | 1 | game | missing | game/rcnttimer.cpp |
| `00104860` | rndInit | 1 | game | missing | - |
| `00151290` | debugSysInit | 1 | game | missing | - |
| `0012f190` | RcntTimer::reset | 1 | game | missing | game/rcnttimer.cpp |
| `0012f1b0` | RcntTimer::~RcntTimer | 1 | game | missing | game/rcnttimer.cpp |
| `002b00d0` | StageResult::getPerfectBits | 1 | game | missing | - |
| `002c3d30` | getSaveResourceAux | 1 | game | missing | - |
| `0012e050` | gfxGetCurrentDisplay | 1 | game | missing | game/gfxtarget.cpp |
| `0012ead0` | gfxSetDisplayOffset | 1 | game | missing | - |
| `0012dee0` | GamePacketManager::GamePacketManager | 1 | game | missing | game/gamepacket.cpp |
| `00135390` | MasterClock::MasterClock | 1 | game | missing | - |
| `00135210` | Clock::createChild | 1 | game | missing | - |
| `001351b0` | Clock::~Clock | 1 | game | missing | - |
| `0012f200` | RcntTimer::RcntTimer | 1 | game | missing | game/rcnttimer.cpp |
| `00130c90` | threadSlotCreate | 1 | game | missing | game/thread.cpp |
| `00151270` | debugSysUpdate | 1 | game | missing | - |
| `00131260` | frameSyncWait | 1 | game | missing | game/thread.cpp |
| `00148500` | soundUpdate | 1 | game | missing | sound/sound.cpp |
| `00130bc0` | threadYield | 1 | game | missing | game/thread.cpp |
| `00130f60` | threadSysInit | 1 | game | missing | game/thread.cpp |
| `00130e00` | threadRunMain | 1 | game | missing | game/thread.cpp |
| `00130e50` | threadSysShutdown | 1 | game | missing | game/thread.cpp |
| `00151280` | debugSysShutdown | 1 | game | missing | - |
| `00148540` | soundShutdown | 1 | game | missing | sound/sound.cpp |
| `00135330` | MasterClock::~MasterClock | 1 | game | missing | - |

