# YamiPan status (generated)

Generated 2026-11-02 18:00 at commit `8915c46` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6670**, named **1042** (16%); library functions: 912
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1403, game-inferred 189
- context-name ledger: 1592 entries; overrides of wrong library matches: 0
- placed in a source file: 1031/6670 in 29 files

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

- reconstructed FS2 functions: **17**; stubs: **0**

| file | reconstructed | stubs |
|---|---|---|
| `src/game/gamesystem.cpp` | 17 | 0 |

## Translation units (one session each: `tus.py`)

- 42 TUs: todo 41, verify 1
- next: **game/pad.cpp** (frontier score 15)
- pinned queue: (empty)
- verify: `game/gamesystem.cpp` 17/17 -- All 17 functions of 0x12cd20..0x12dd30 are in src/game/gamesystem.cpp with gamesystem.h (GamePad : Pad, MaskedGamePad, GameSystem at EE offsets, layout host). N

Recent sessions:

- 2026-11-02 17:59 `game/gamesystem.cpp` verify: main, gameInit, the main loop, GameSystem (ctor, flipFrame, endFrameSync), GamePad / MaskedGamePad, gsClearVram, the frame-ready and DMAC handlers: all 17 funct

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `game/pad.cpp` | todo | 0/0/15 | 5.8 | 15 | 0 |
| `game/perfmeter.cpp` | todo | 0/0/9 | 2.1 | 12 | 0 |
| `unassigned@00171300` | todo | 0/0/5138 | 1442.7 | 12 | 0 |
| `game/thread.cpp` | todo | 0/0/19 | 3.8 | 8 | 0 |
| `unassigned@001037c0` | todo | 0/0/19 | 5.0 | 7 | 0 |
| `game/gamepacket.cpp` | todo | 0/0/6 | 0.6 | 6 | 0 |
| `game/gfxtarget.cpp` | todo | 0/0/8 | 2.6 | 6 | 0 |
| `unassigned@00133800` | todo | 0/0/65 | 11.2 | 5 | 0 |
| `unassigned@00116fb8` | todo | 0/0/54 | 21.6 | 4 | 0 |
| `unassigned@00148db0` | todo | 0/0/146 | 37.4 | 3 | 0 |
| `sound/sound.cpp` | todo | 0/0/98 | 18.8 | 3 | 0 |
| `rena_pkt_vif1pktmanage.cpp` | todo | 0/0/13 | 1.9 | 2 | 0 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00133610` | PerfMeter::addBar | 6 | game | missing | game/perfmeter.cpp |
| `00119b88` | atan2f | 4 | lib-ctx | missing | - |
| `0012fa90` | Pad::~Pad | 4 | game | missing | game/pad.cpp |
| `0023bd00` | ButtonMap::setDefaults | 4 | game | missing | - |
| `0012e0e0` | gfxFlipDisplay | 4 | game | missing | game/gfxtarget.cpp |
| `0012f300` | Pad::update | 3 | game | missing | game/pad.cpp |
| `00103870` | Heap::markedUsage | 3 | game | missing | - |
| `0012fcf0` | Pad::Pad | 3 | game | missing | game/pad.cpp |
| `0023baf0` | getGameOptions | 2 | game | missing | - |
| `00130a30` | threadSetSwitchHook | 2 | game | missing | game/thread.cpp |
| `0012fb10` | Pad::Pad(bool) | 1 | game | missing | game/pad.cpp |
| `00104600` | initGlobalHeap | 1 | game | missing | - |
| `001527a0` | rxInit | 1 | lib-ctx | missing | game/spkrom.cpp |
| `00130130` | padSysInit | 1 | game | missing | game/pad.cpp |
| `0012e3f0` | gfxInitRenderTargets | 1 | game | missing | game/gfxtarget.cpp |
| `001486e0` | soundInit | 1 | game | missing | sound/sound.cpp |
| `0012f260` | rcnt1Start | 1 | game | missing | game/rcnttimer.cpp |
| `0023de20` | getLocalDateTime | 1 | game | missing | - |
| `00104860` | rndInit | 1 | game | missing | - |
| `0023bb50` | systemDataInit | 1 | game | missing | - |
| `0023be70` | GameOptions::setDefaults | 1 | game | missing | - |
| `00151290` | debugSysInit | 1 | game | missing | - |
| `002a9c80` | memcardInit | 1 | game | missing | - |
| `00130c90` | threadSlotCreate | 1 | game | missing | game/thread.cpp |
| `00151270` | debugSysUpdate | 1 | game | missing | - |
| `00131260` | frameSyncWait | 1 | game | missing | game/thread.cpp |
| `0012ffa0` | padSysUpdate | 1 | game | missing | game/pad.cpp |
| `00148500` | soundUpdate | 1 | game | missing | sound/sound.cpp |
| `00130bc0` | threadYield | 1 | game | missing | game/thread.cpp |
| `001037c0` | Heap::setMark | 1 | game | missing | - |
| `002eaff0` | Vif1PktManage::GetUsedSize | 1 | lib-ctx | missing | rena_pkt_vif1pktmanage.cpp |
| `002eb010` | Vif1PktManage::GetBufferSize | 1 | lib-ctx | missing | rena_pkt_vif1pktmanage.cpp |
| `0012de00` | GamePacketManager::openList | 1 | game | missing | game/gamepacket.cpp |
| `00133600` | PerfMeter::drawBars | 1 | game | missing | game/perfmeter.cpp |
| `00132f20` | PerfMeter::drawDetail | 1 | game | missing | - |
| `0012dd30` | GamePacketManager::closeList | 1 | game | missing | game/gamepacket.cpp |
| `00133350` | PerfMeter::collectStats | 1 | game | missing | game/perfmeter.cpp |
| `0012ddc0` | GamePacketManager::endFrame | 1 | game | missing | game/gamepacket.cpp |
| `00133620` | PerfMeter::beginFrame | 1 | game | missing | game/perfmeter.cpp |
| `001352d0` | MasterClock::update | 1 | game | missing | - |

