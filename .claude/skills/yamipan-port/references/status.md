# YamiPan status (generated)

Generated 2026-11-02 21:25 at commit `251f55b` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6670**, named **1049** (16%); library functions: 912
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1405, game-inferred 195
- context-name ledger: 1600 entries; overrides of wrong library matches: 0
- placed in a source file: 1032/6670 in 29 files

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

- reconstructed FS2 functions: **41**; stubs: **0**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/gamesystem.cpp` | 17 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |

## Translation units (one session each: `tus.py`)

- 42 TUs: todo 39, verify 3
- next: **unassigned@00116fb8** (frontier score 13)
- pinned queue: (empty)
- verify: `game/gamesystem.cpp` 17/17 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr

Recent sessions:

- 2026-11-02 21:25 `game/perfmeter.cpp` verify: PerfMeter, all 10 functions: ctor, dtor, beginFrame, collectStats, resetStats, writeCsv, dumpGroups and the empty addBar / drawBars / drawDetail; RcntTimer prot
- 2026-11-02 18:09 `game/pad.cpp` verify: Pad layer written: PadPort detection state machine, Pad ctors/dtor, update, readSticks, updateRepeat, setVibration, listener table, padSysInit/Update/Shutdown (
- 2026-11-02 17:59 `game/gamesystem.cpp` verify: main, gameInit, the main loop, GameSystem (ctor, flipFrame, endFrameSync), GamePad / MaskedGamePad, gsClearVram, the frame-ready and DMAC handlers: all 17 funct

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `unassigned@00116fb8` | todo | 0/0/54 | 21.6 | 13 | 0 |
| `unassigned@00171300` | todo | 0/0/5138 | 1442.7 | 12 | 0 |
| `game/thread.cpp` | todo | 0/0/19 | 3.8 | 8 | 0 |
| `unassigned@001037c0` | todo | 0/0/19 | 5.0 | 7 | 0 |
| `game/gfxtarget.cpp` | todo | 0/0/8 | 2.6 | 6 | 0 |
| `game/gamepacket.cpp` | todo | 0/0/6 | 0.6 | 6 | 0 |
| `unassigned@00133800` | todo | 0/0/65 | 11.2 | 5 | 0 |
| `game/rcnttimer.cpp` | todo | 0/0/4 | 0.2 | 4 | 0 |
| `unassigned@00148db0` | todo | 0/0/146 | 37.4 | 3 | 0 |
| `sound/sound.cpp` | todo | 0/0/98 | 18.8 | 3 | 0 |
| `game/spkrom.cpp` | todo | 0/0/13 | 2.4 | 2 | 0 |
| `rena_pkt_vif1pktmanage.cpp` | todo | 0/0/13 | 1.9 | 2 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `0011ba00` | __extendsfdf2 | 9 | lib-ctx | missing | - |
| `0023bd00` | ButtonMap::setDefaults | 4 | game | missing | - |
| `00119b88` | atan2f | 4 | lib-ctx | missing | - |
| `0012e0e0` | gfxFlipDisplay | 4 | game | missing | game/gfxtarget.cpp |
| `00103870` | Heap::markedUsage | 3 | game | missing | - |
| `00130a30` | threadSetSwitchHook | 2 | game | missing | game/thread.cpp |
| `0023baf0` | getGameOptions | 2 | game | missing | - |
| `00130c90` | threadSlotCreate | 1 | game | missing | game/thread.cpp |
| `00151270` | debugSysUpdate | 1 | game | missing | - |
| `00131260` | frameSyncWait | 1 | game | missing | game/thread.cpp |
| `00148500` | soundUpdate | 1 | game | missing | sound/sound.cpp |
| `00130bc0` | threadYield | 1 | game | missing | game/thread.cpp |
| `00130f60` | threadSysInit | 1 | game | missing | game/thread.cpp |
| `00130e00` | threadRunMain | 1 | game | missing | game/thread.cpp |
| `00130e50` | threadSysShutdown | 1 | game | missing | game/thread.cpp |
| `002a9c70` | memcardLibEnd | 1 | native | missing | - |
| `00151280` | debugSysShutdown | 1 | game | missing | - |
| `00148540` | soundShutdown | 1 | game | missing | sound/sound.cpp |
| `00135330` | MasterClock::~MasterClock | 1 | game | missing | - |
| `0012de80` | GamePacketManager::~GamePacketManager | 1 | game | missing | game/gamepacket.cpp |
| `0012e3a0` | gfxReleaseRenderTargets | 1 | game | missing | game/gfxtarget.cpp |
| `00152750` | rxTerm | 1 | lib-ctx | missing | game/spkrom.cpp |
| `001045a0` | shutdownGlobalHeap | 1 | game | missing | - |
| `00104600` | initGlobalHeap | 1 | game | missing | - |
| `001527a0` | rxInit | 1 | lib-ctx | missing | game/spkrom.cpp |
| `0012e3f0` | gfxInitRenderTargets | 1 | game | missing | game/gfxtarget.cpp |
| `001486e0` | soundInit | 1 | game | missing | sound/sound.cpp |
| `0012f260` | rcnt1Start | 1 | game | missing | game/rcnttimer.cpp |
| `0023de20` | getLocalDateTime | 1 | game | missing | - |
| `00104860` | rndInit | 1 | game | missing | - |
| `0023bb50` | systemDataInit | 1 | game | missing | - |
| `0023be70` | GameOptions::setDefaults | 1 | game | missing | - |
| `00151290` | debugSysInit | 1 | game | missing | - |
| `002a9c80` | memcardInit | 1 | game | missing | - |
| `0012f190` | RcntTimer::reset | 1 | game | missing | game/rcnttimer.cpp |
| `0012f1b0` | RcntTimer::~RcntTimer | 1 | game | missing | game/rcnttimer.cpp |
| `0012dee0` | GamePacketManager::GamePacketManager | 1 | game | missing | game/gamepacket.cpp |
| `00135390` | MasterClock::MasterClock | 1 | game | missing | - |
| `00135210` | Clock::createChild | 1 | game | missing | - |
| `001351b0` | Clock::~Clock | 1 | game | missing | - |

