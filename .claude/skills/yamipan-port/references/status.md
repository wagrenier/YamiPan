# YamiPan status (generated)

Generated 2026-11-03 18:46 at commit `66fa5c1` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6599**, named **2281** (35%); library functions: 987
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1536, game-inferred 1401
- context-name ledger: 2937 entries; overrides of wrong library matches: 0
- placed in a source file: 2290/6599 in 66 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `001b0000` | 0/242 (0%) | - |
| `001c0000` | 0/233 (0%) | - |
| `00220000` | 0/167 (0%) | - |
| `00270000` | 0/202 (0%) | - |
| `00310000` | 0/9 (0%) | - |
| `00210000` | 3/231 (1%) | - |
| `001e0000` | 1/75 (1%) | - |
| `00180000` | 5/336 (1%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **1955**; stubs: **220**

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
| `src/game/menuscene.cpp` | 14 | 0 |
| `src/game/movieplayer.cpp` | 41 | 0 |
| `src/game/pad.cpp` | 14 | 0 |
| `src/game/pakarchive.cpp` | 17 | 0 |
| `src/game/perfmeter.cpp` | 10 | 0 |
| `src/game/progress.cpp` | 25 | 0 |
| `src/game/rcnttimer.cpp` | 0 | 4 |
| `src/game/s2ddata.cpp` | 9 | 0 |
| `src/game/scene.cpp` | 1 | 0 |
| `src/game/screen.cpp` | 18 | 0 |
| `src/game/seplayer.cpp` | 35 | 0 |
| `src/game/spkrom.cpp` | 13 | 0 |
| `src/game/stagealert.cpp` | 0 | 2 |
| `src/game/stageloop.cpp` | 9 | 0 |
| `src/game/stageobjlist.cpp` | 45 | 0 |
| `src/game/stageresult.cpp` | 32 | 0 |
| `src/game/stagescene.cpp` | 112 | 13 |
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
| `src/lib/unassigned_stubs.cpp` | 0 | 168 |
| `src/rena/rena_2d_texture_tim2.cpp` | 7 | 0 |
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

- 90 TUs: todo 50, partial 2, verify 54, done 1
- next: **game/stagescene.cpp** (continuing a partial TU)
- pinned queue: (empty)
- partial: `game/stagescene.cpp` 112/258 -- Written this run (batch 082, 35 names): parseTexture, parseModel, parseMotion with readMotionList 0x1a1df0, parseMap, parseSndseh with fileWithSuffix 0x1a4370, 
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
- verify: `game/menuscene.cpp` 14/14 -- All 14 functions of 0x2b5740..0x2b6b90 written (batch 083, 32 names, saved in Ghidra): the whole menu flow, architecture.md Modules has the map. Build, callchec
- verify: `game/movieplayer.cpp` 41/41 -- Cut out of unassigned@001953f0 (its tail 0x1faa30..0x1fcb40; the rest of that run is still unassigned). New game/movieplayer.h; gamehooks.h lost MoviePlayer and
- verify: `game/pad.cpp` 14/15 -- All 15 functions of 0x12f290..0x130a20 reconstructed; pad.h complete with PadBit / PadTerm / PadPhase / PadMotor / PadListen enums and PadPort at its EE layout 
- verify: `game/pakarchive.cpp` 17/17 -- All 17 functions of 0x231410..0x231bb0 are in src/game/pakarchive.cpp with game/pakarchive.h (the fileloader stand-in completed). The file keeps its EE layout o
- verify: `game/perfmeter.cpp` 10/10 -- Source only, not compiled. Range pinned 0x132f20-0x133800 (drawDetail joined). Batch 024 named PerfMeter::writeCsv 0x132f90, PerfMeter::resetStats 0x133590, spr
- verify: `game/progress.cpp` 25/25 -- All 24 functions written, source-only (callcheck clean, layout lint OK; g++ -fsyntax-only clean with a stub scetypes.h). progress.h holds NaviTable / NaviNode (
- verify: `game/s2ddata.cpp` 9/9 -- Host: S2dRef<T> (s2ddata.h) is a 32-bit offset from the field itself, 0 null, implicit T* conversion, copy deleted; S2dData, S2dObject, S2dWidgetDef, S2dImage a
- verify: `game/scene.cpp` 1/1 -- gameSceneThread written in full: loadInitPack, runStartupSequence, overlayThreadStart, then forever loadMenuPack, the scene from sScenes (one retail entry, Menu
- verify: `game/screen.cpp` 18/18 -- All 18 functions of 0x2676e0..0x2681e0 written (batch 037, 33 names, Ghidra saved). Overlay thread slot 6 draws the BG movie screen layout (init pack file 2dEU/
- verify: `game/seplayer.cpp` 35/35 -- Cut from unassigned@002908d0: game/seplayer.cpp is 0x2908d0..0x2937e0 (SoundTrack 17, SePlayer 18, batch 078: fsmFindTrack, fsmTrackHasText, seDistanceVolume, s
- verify: `game/spkrom.cpp` 13/13 -- All 13 of 0x152730..0x153108 written. The entries keep offsets on the host (the ROM relocates them to pointers in place), strings through spkString. Host: argv[
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
- verify: `rena/rena_2d_texture_tim2.cpp` 7/7 -- All 7 written; class moved to rena/rena_2d_texture_tim2.h. One Tim2Texture per mip level of the first picture; m_image (+0x40) is the TIM2 file itself, not a co
- verify: `rena/rena_pkt_vif1pktmanage.cpp` 13/17 -- Pinned 0x2eaff0-0x2ec110 under the new src/rena/ with the game's own file name (strings in Reset / AddList); the stale inferred key rena_pkt_vif1pktmanage.cpp r
- verify: `rx/filecd.c` 35/35 -- All 35 of 0x1565c8..0x157db8 written. rxcmp.py: 34 of 35 identical to RoR with jump targets and data masked; the one difference is rxFileCdFindDir, 0x18 longer 
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

- 2026-11-03 18:46 `yamipan/rendering/texture_store.cpp` done: rendering::TextureStore written and bound through the seam: the language screen is the first textured picture and equals its assets
- 2026-11-03 18:31 `rena/rena_2d_texture_tim2.cpp` verify: reconstruct Tim2Texture, all 7 functions (verify)
- 2026-11-03 18:13 `game/menuscene.cpp` verify: reconstruct the menu flow, 14 functions (verify)
- 2026-11-03 17:41 `rx/filecd.c` verify: rx/filecd.c: the cdrom0: driver carried over from RosePan with FS2's rxFileCdFindDir
- 2026-11-03 17:39 `game/spkrom.cpp` verify: game/spkrom.cpp: rxInit, rxTerm, the SPK.ROM index (load, locate, size, load file), the rx memory hooks; with the Pan layer's virtual disc behind sceCdRead
- 2026-11-03 16:37 `game/stagescene.cpp` partial: stage script keyword handlers, bg and particle targets, small scene members; StreamTable::relocate shared; DrawLayerList is AreaClip
- 2026-11-03 16:19 `game/stagescene.cpp` partial: stage script reader: parseScript, splitLines, reset, target / end blocks, script and character targets, StageFiles ctor / dtor; StageList and StringMap template
- 2026-11-03 15:55 `game/stagescene.cpp` partial: StageScene ctor / dtor and 17 small members written (30 of 258); batch 080 names every scene member class (54 names)

| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |
|---|---|---|---|---|---|
| `unassigned@00148db0` | todo | 0/14/125 | 35.6 | 56 | 2 |
| `unassigned@002b6b90` | todo | 0/22/272 | 115.8 | 47 | 3 |
| `unassigned@002681e0` | todo | 0/17/514 | 156.6 | 35 | 0 |
| `unassigned@0017fce0` | todo | 0/11/399 | 76.4 | 24 | 0 |
| `game/rcnttimer.cpp` | todo | 0/4/4 | 0.2 | 19 | 3 |
| `unassigned@002d46a0` | todo | 0/8/252 | 51.5 | 17 | 2 |
| `unassigned@001fe6b0` | todo | 0/16/563 | 197.2 | 16 | 0 |
| `rx/rxsndstrstream.c` | todo | 0/5/18 | 2.4 | 15 | 2 |
| `game/stagescene.cpp` | partial | 112/13/258 | 67.6 | 15 | 0 |
| `unassigned@00231c90` | todo | 0/6/61 | 36.3 | 13 | 0 |
| `unassigned@00299d90` | todo | 0/8/215 | 62.3 | 13 | 0 |
| `rx/rxsndstrmem.c` | todo | 0/3/16 | 2.6 | 13 | 2 |

## Frontier (called by reconstructed code, not written yet)

| FS2 addr | name | call sites | provenance | state | inferred file |
|---|---|---|---|---|---|
| `00121b80` | mcErrorCode | 19 | game | missing | sdk/mcerror.cpp |
| `00149250` | StringList::~StringList | 14 | game | stub | - |
| `00149100` | StringList::split | 9 | game | stub | - |
| `001494c0` | StringList::tail | 8 | game | stub | - |
| `0012cab8` | vsprintf | 6 | lib-ctx | missing | - |
| `00149230` | StringList::splitString | 5 | game | stub | - |
| `0029efe0` | getNaviMenu | 5 | game | stub | - |
| `001937f0` | FUN_001937f0 | 4 | - | missing | - |
| `00284640` | AreaGrid::AreaGrid | 4 | game | stub | - |
| `00283df0` | seDistanceVolume | 4 | game | stub | - |
| `00177270` | BoneNode::find | 4 | game | stub | - |
| `00284560` | AreaGrid::addId | 4 | game | stub | - |
| `002bc5d0` | KeepList::removeUnkept | 4 | game | stub | - |
| `0025a850` | mslTreeIncrement | 4 | lib-ctx | missing | - |
| `002c3e10` | loadMenuPack | 4 | game | stub | - |
| `00284580` | AreaGrid::~AreaGrid | 4 | game | stub | - |
| `0028fb80` | fsmFindText | 3 | game | stub | - |
| `00231d30` | overlayShowLoading | 3 | native | missing | - |
| `00231d20` | overlayShowLoading | 3 | native | missing | - |
| `0028fd30` | fsmRelocate | 3 | game | stub | - |
| `0028fc80` | fsmFindTrack | 3 | game | stub | - |
| `00189840` | FUN_00189840 | 3 | - | missing | - |
| `0025b590` | FUN_0025b590 | 3 | - | missing | - |
| `0029efd0` | getSaveExtra | 2 | game | stub | - |
| `0023ae40` | CollisionFile::relocate | 2 | game | stub | - |
| `001d3a40` | StageScript::relocate | 2 | game | stub | - |
| `002c4140` | getSePlayer | 2 | game | stub | - |
| `0016f9e0` | sdpr2CalcPanDeg | 2 | lib-ctx | stub | sdpr2/sdpr2single.c |
| `00243380` | getUiWorkArea | 2 | game | stub | - |
| `002acff0` | runHelpScreen | 2 | game | stub | - |
| `00170088` | sdpr2DrvInit | 2 | lib-ctx | stub | sdpr2/sdpr2drv.c |
| `0016e328` | sdpr2PairInit | 2 | lib-ctx | stub | sdpr2/sdpr2pair.c |
| `0016f808` | sdpr2SingleInit | 2 | lib-ctx | stub | sdpr2/sdpr2single.c |
| `002ec610` | mcBaseLayoutPath | 2 | game | stub | - |
| `00243370` | setUiWorkArea | 2 | game | stub | - |
| `00152740` | rxFileCallSizeHook | 2 | native | missing | game/streamring.cpp |
| `00184370` | StageNodeList::erase | 2 | game | stub | - |
| `00149040` | String::String | 2 | game | stub | - |
| `001a38e0` | StageFiles::loadCommon | 2 | game | stub | game/stagescene.cpp |
| `002bc040` | SubSceneMapObject::SubSceneMapObject | 2 | game | stub | - |

