# Open items -- priorities, decisions, cross-TU questions, done log

The user's priorities first ("Next up"), then decisions with their date and who made them, then open
questions. Resolve items here when they are settled (move them to "Done" with the commit).

## Next up (in order)

0. **The user (2026-11-02): "start reversing actual code and try to match names as you go, my #1
   priority is getting source code back."** Library name seeding stops here (batch 021 was the last
   seeding batch; the "Next batches" list under item 2 is dropped -- library and game names now come
   at the TU that needs them). The runs go to source reconstruction as fast as possible, in this
   order, one per run:
   ~~(a) **bootstrap 5, the TU map**~~ **done 2026-11-02**: 16 library ranges in
       `data/layout.json`, 27 pinned TUs in `data/tus.json` (rx, rxsndstr, SDPR2, the boot's game
       code), `export.py` / `status.py` regenerated (architecture.md "Address map", "Modules").
       `tus.json` "queue" is left empty for (c); its settings carry the data root, slice 10240 and
       an empty smoke pad.
   ~~(b) **carry `tus.py` over from RosePan now**~~ **done 2026-11-02**: `tools/reverse/tus.py`
       with a **source-only mode while there is no `CMakeLists.txt`** (item 5): `check` runs
       callcheck on the TU's file and layoutcheck's lint only (no build, stubgen or smoke run),
       `finish` skips the build, refuses `--status done`, records `"build": "not built"` in the
       session and says so in the commit. `list` / `next` / `check` / `brief --no-stage` tried on
       the pinned TUs. The routine now runs one TU per run. **Next: (c).**
   **(c) started 2026-11-02: TU 1 `game/gamesystem.cpp` written (all 17 functions, `verify`,
   batch 022); TU 2 `game/pad.cpp` written (all 14 + the slot thunk, `verify`, batch 023; `pad.h`
   complete, `padSys*` now return int). Bootstrap 6 must give the host libpad a DualShock 2 in
   pressure mode (terminal mode 8 is the only one `Pad::update` reads: architecture.md "The pad
   layer") and the SCE_PAD* button bits in `sdk/libpad.h` (RosePan's lacks them). TU 3
   `game/perfmeter.cpp` written (all 10, `verify`, batch 024; RcntTimer moved to `game/rcnttimer.h`).
   Bootstrap 6 must give `sdk/fileio.h` the SDK 3.0 `sceOpen(const char *, int, ...)` (FS2 passes a
   mode, 0666; RosePan's takes two arguments) and the CRT `sprintf` / `strlen`.** Its callees' TUs are the frontier: `game/pad.cpp`, `game/gamepacket.cpp`,
   `game/perfmeter.cpp`, `game/thread.cpp`, `game/gfxtarget.cpp`, the clocks, the heap, sound,
   `rxInit` / SPK.ROM, system data. Choices made there, for the user to overrule:
   - **Headers for TUs not written yet are minimal** (`src/game/pad.h`, `clock.h`, `heap.h`,
     `perfmeter.h` (+ `RcntTimer`, `rcnt1Start`), `gamepacket.h` (+ rena's `Vif1PktManage`),
     `gfxtarget.h`, `sysdata.h` (options, ButtonMap, DateTime), `thread.h`); rx / sound / memcard /
     debug hooks / `gameSceneThread` share `src/game/gamehooks.h` until their TUs exist (move each
     prototype to its TU's header then).
   - **The SDK headers the game code includes are RosePan's names** (`sdk/eekernel.h`, `eeregs.h`,
     `libgraph.h`, `libdma.h`, `libpad.h`, `scetypes.h`) plus Sony's `sdk/eestruct.h` (`tD_STAT`) and
     `sdk/libvifpk.h` (`sceVif1Packet` with **`pCurrent` first**, as FS2's packet lists use it:
     RosePan's own `libvif.h` puts `pBase` first) and `ExitHandler()`: bootstrap 6 must provide them.
   - **`gsClearVram` is kept as the game wrote it** (packet + sceDmaSend on GIF) with a `PC:` note:
     with no GS emulator the host GIF channel drops it; host targets start cleared. The GS FINISH
     `GameSystem::flipFrame` writes into packet list 13 stays too (draws nothing).
   - **`main` keeps its name**; the platform layer's entry must call it (rename at bootstrap 6 if
     the host entry needs `main`).
   - Open: why `GamePad::read` 0x12d4f0 and the slot `GamePad::update` 0x12dc60 both exist; the
     names of heap marks 0 / 1 and the meter groups (guesses in `heap.h` / `perfmeter.h`);
     `Clock`'s byte +0x1c (MasterClock's last ctor argument); the empty `debugSys*` hooks' real role.
   (c) the queue (`tus.py next` already picks `game/gamesystem.cpp`, `main`'s TU, with the queue
       empty: "frontier empty: start from main"; a written TU's callees then raise the frontier): game code first (not library code), starting from `main` / `gameMainThread` /
       `GameSystem` and down what boot reaches (architecture.md "Program flow"), each TU written into
       `src/` with its header and names made at that TU.
   The platform layer (bootstrap 6) and first build (7) follow once the user has a toolchain; until
   then reconstructed TUs are `verify` (written, not compiled or seen on screen).

1. **The game**: Forbidden Siren 2, Europe, `SCES_538.51`, annotation tag `FS2` (decided, below;
   `yamipan.py` has `ANNOT`, the stub regex and the game-folder defaults since bootstrap 3).
2. Bootstrap (bootstrap.md, in order): identify the binary (done 2026-10-31: architecture.md
   "Binary and toolchain"; **CodeWarrior, stripped, SDK 3.0, rx + rena middleware, no sister**) ->
   its own Ghidra project (done:
   `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr`, `data/ghidra.json`) -> tools from
   RosePan on `ghidra_mcp.py` plus a 5.14.2 client (done 2026-10-31; `tus.py` / `autopilot.py` held
   back for step 7: Decisions) -> **name seeding (bootstrap 4, in progress)** -> TU map -> platform layer ->
   first build and run -> the queue. The scheduled runs do one step each.
   Bootstrap 4 so far (2026-10-31): snapshot `work/snapshots/20261031_084541.json` (6,393 names);
   `rtti.py` rewritten for CodeWarrior (490 vtables, architecture.md "Classes"); the second code island
   is the static-initialiser table (architecture.md "Static initialisers"); **910 function starts
   Ghidra lacks** in `data/missing_functions.json`. **Left for bootstrap 4, in this order:**
   ~~(a) create those functions~~ **done 2026-11-02 (batch 009)**: all 1,016 starts of
   `data/missing_functions.json` + 7 code pointers (`gameMainThread` and the six below) created and
   saved (Ghidra 6,395 -> 7,418 functions; 21 needed a second `create_function` call: ghidra.md),
   the 106 merged rx starts named with their batch 005-008 names, `rtti.py --missing` now finds 0
   (the JSON is `{}`) and `rxalign.py` pairs all six rx ranges one to one (0 entries);
   (b) seed names by reading with `name_ctx.py` batches (`tools/reverse/work/batches/NNN.json`, not
   committed; the ledger `data/context_names.json` is). **Batch 001 done (2026-10-31, 27 names)**:
   the entry chain, `main`, `exit` / `Exit` / `_InitSys`, the CodeWarrior + MSL runtime, the
   exception classes' destructors, operator new / delete and the global heap (architecture.md "Key
   functions and globals"). **Batch 002 done (2026-10-31, 26 names)**: `main`'s thread system,
   `gameInit`, heap init / teardown, libgraph sync and libkernl interrupt helpers (architecture.md
   "Program flow", "Thread system"). `gameMainThread` 0x12db00 failed: no Ghidra function there.
   **Code pointers passed as arguments are not in `data/missing_functions.json`** (it only has
   vtable slots and initialisers): 0x12db00, 0x12db70, 0x12dab0, 0x131840, 0x12dc20 were created
   and named in batch 009 (architecture.md "Program flow"). ~~A pass over lui/addiu pairs feeding
   a0..t1~~ **done 2026-11-02 (batch 010)**: `codeptrs.py` found 39 more starts (callbacks,
   rx memory hooks, SIF cmd handlers, array-element constructors passed to CodeWarrior's array
   new, function tables), all created; rerun gives 0. Left unnamed among them (read at their TU):
   the zero-and-return-this constructors 0x1716c0, 0x18bf20, 0x194910, 0x235390, 0x25f990,
   0x2cd970 (passed in a1 to the array-construct helper: name them with their class),
   0x1398c8 / 0x1398e8 (stored by 0x139848, before the SIF library), 0x140c88 / 0x1412f0 (word
   copy) and 0x140cc0 (word search) near `_InitSys`, 0x127c80 (returns 0), 0x113750 / 0x113898 (`_ch3dmaCSC` / `_ch4dma`, batch 012),
   0x11c928, ~~0x11feb8 / 0x120348 / 0x120750 (libmc callbacks)~~ (batch 020), 0x1395e0, 0x140840, 0x180018 (a lone
   nop in a table at 0x2f0e5c: check), 0x1b56f0, 0x1f22a0, 0x1fbe40..0x1fbf30 (six callbacks of
   0x1fbe20's family), 0x20ad20, 0x20b010, 0x2b5740, 0x2cd8a0. Its 29 `interior` hits are mostly
   constants that look like code addresses, plus **the first island's entry points** 0x2f3d60..
   0x2f4860, passed by `_InitSys`'s callees 0x140330..0x141390 (item (c)).
   **Batch 010 also named the SIF cmd / rpc library and the fileio head from RoR** by size
   alignment (architecture.md "Key functions and globals"; 0x13ac68 / 0x13ad00 named in batch 020).
   **Batch 003 done (2026-10-31, 34 names)**: `gameMainThread`'s and `gameInit`'s callees (pad,
   sound, rx init, SPK.ROM, GS / DMA / SIF / libpad library calls, options; architecture.md
   "Program flow").
   **Batch 004 done (2026-10-31, 52 names + 6 class names in `data/class_names.json`)**: the global
   object is `GameSystem` (ctor 0x12d0d0, `flipFrame` 0x12cdf0) with three pads (`Pad` / `GamePad` /
   `MaskedGamePad`), `GamePacketManager` on rena's `Vif1PktManage`, `PerfMeter` / `RcntTimer`,
   `MasterClock` / `Clock` / `PtrList`, libvifpk, `rxPrintf` (architecture.md "The global object").
   The pad update slots 0x12dc60 `GamePad::update` / 0x12dcb0 `MaskedGamePad::update` were created
   and named in batch 009 (0x130060 stays a thunk). Left unnamed: 0x133610 (empty, returns
   0, also called by 0x198970 / 0x199400: a PerfMeter bar? check those callers), 0x133600 /
   0x132f20 (empty), ~~0x12f6b0 / 0x12f600 (Pad::update's stick / post steps)~~ (batch 023: `Pad::readSticks` / `Pad::updateRepeat`), 0x119b88 (atan2f?),
   0x12d540 / 0x12d5a0 (GamePad ctor / dtor candidates, rtti.py).
   **Batch 005 done (2026-11-02, 253 names)**: main's teardown (memcard / sound / gfx / pad / rx;
   architecture.md "Program flow") and, on the user's tip (2026-11-02: "Rule of Rose had similar
   rx* names"), **the rx library from RosePan**: FS2 links RoR's rx (same functions, order and sizes,
   even `rxThFindCallback`'s bug), so the new `rxalign.py` aligned size sequences against
   `../RosePan/ror_functions.txt` (runs of >= 3) for rxsys 0x100350..0x1037b0, rxthread
   0x166b50..0x168150 and rxfile 0x153108..0x158e80; batch 003's rx guesses were renamed to RoR's
   (`rxThSetPrioIndex`, `rxThSetInterval`, `rxSysInit`, `rxSysSetBootPath`, `rxSysPrintf`,
   `rxThFindCallback`). Snapshot before it: `work/snapshots/20261102_132500.json`.
   **Batch 006 done (2026-11-02, 61 names)**: rx finished where it matches RoR -- rxsys
   0x100350..0x1037b0 + 0x153108..0x153518, rxfile to 0x159928, rxthread to 0x1681c0 (`rxalign.py`
   now fills gaps by span partition and breaks LCS ties by fewest breaks; reruns give 0 entries).
   Fixed batch 005: 0x1533f0..0x153450 were one slot off (now `rxSysGetFileSearchDir`,
   `rxSysPushSearchDir`, `rxSysPopSearchDir`, `rxSysGetLastName`) and 0x157c40 is RoR's
   `rxFileCdSetFindFunc`, not a FS2-only locate hook. **45 more rx starts Ghidra lacks** went into
   `data/missing_functions.json` ("rx from RosePan"; with 4(a); rerun the four `rxalign.py`
   ranges of architecture.md "rx" with `--batch` afterwards to name them).
   **Batch 007 done (2026-11-02, 204 names)**: rxsnd 0x159bc0..0x162178 from RoR (190 aligned,
   5 by reading, 3 FS2-only RPC wrappers) and rxthread's VBlank tail, which FS2 links at
   0x159928..0x159bc0 (architecture.md "rx"); 28 more merged rx starts in
   `data/missing_functions.json`. Snapshot before it: `work/snapshots/20261102_134502.json`.
   **Batch 008 done (2026-11-02, 114 names)**: rxsndut + rxsndstr 0x162178..0x166b50 by reading
   (all 113 Ghidra functions: XWS bank, stream core, PCM / ADPCM formats, stream source, memory
   stream; architecture.md "rx") + `strcmp` 0x128238; 33 merged starts with their names in
   `data/missing_functions.json` ("by reading, batch 008"; with 4(a)). Doubtful names, settle at
   the TU: `rxSndStrBankGetRequestRate` (FORM +10, rate or pitch?), `rxSndStrBankGetMarkParam`
   (MARK +4), `rxSndStrSetFloatParam` (0x163b40, +0x28, dirty bit 0x10 nobody reads),
   `rxSndStrPlayAt` 0x163738 (a bare tail call to `rxSndStrPlayEx`: why does it exist?).
   Snapshot before it: `work/snapshots/20261102_135750.json`.
   **Batch 009 done (2026-11-02, 113 names)**: bootstrap 4(a) above.
   **Batch 010 done (2026-11-02, 37 names)**: the code-pointer pass above.
   **Batch 011 done (2026-11-02, 108 names)**: **SDPR2** 0x1681c0..0x171300 by reading
   (architecture.md "rx", SDPR2; 10 merged starts created first with `ghidra.py mkfunc`). To settle
   at its TUs: `sdpr2PairPause` 0x16d3a8 calls the driver's *stop* slot (the one-voice type calls
   pause): bug or intent; `sdpr2SetSpread` / `sdpr2SetPairMode` / `sdpr2SetSurround` are names from
   what the values do, not from a string; `sdpr2CalcLevels` 0x16fbd0 unread; the A/B and C/D file cuts.
   **Batch 012 done (2026-11-02, 46 names, 15 merged starts created)**: rxMpeg was looked for
   and **is not linked** (RoR's `RxMpeg` is RoR game code; FS2's movie player is its own class at
   ~0x1fa880..~0x1fcbf0, named at its TU: architecture.md "Key functions and globals"); on the way
   **SDK 3.0 libmpeg 0x10c218..0x116978 and libipu 0x116978..0x116fb8 from RoR** (decoder members
   linked before mpeg.c, RoR after). Left unnamed there: 0x10c218, 0x111228, the accessors
   0x1123a0..0x112400 (DispWidth / Height / CenterOff?), RoR's own `FUN_` pairs (read at the TU).
   `rxalign.py` now skips RoR placeholders in `--batch` and takes `--lib`.
   **Batch 013 done (2026-11-02, 93 names + 3 class names)**: the game's sound layer
   0x144080..0x148d60 by reading (`SoundStream`, `SoundSink` / `SoundPcmSink` / `SoundAdpcmSink`,
   `SoundFeeder`, `SoundSeBank`, `SoundConfig`, the rx IOP / SPU memory hooks, `soundThreadLock`;
   architecture.md "The game's sound layer") + SDK iopheap 0x13ed90..0x13ef08 from RoR. To settle at
   the TU: the parameter order of `SoundSeBank::setPanPitchF` / `setPitch` (a float in f12 beside the
   object), output mode 0 = mono?, `SoundStream::freeBuffer`'s owned test (decompiles inverted), the
   class at 0x142e70..0x144040 (vptr +0x808, vtables 0x311100 / 0x311120) before it.
   **Batches 014-015 done (2026-11-02, 111 names, 14 merged starts created)**: `_InitSys`'s nine
   callees and the whole libkernl tail 0x13ef80..0x142dd0 from RoR (`rxalign.py 13ef80 142dd0
   2658c8 269710`, one to one), RoR's 14 placeholders there from FF3's list (FF2 symbols), the
   per-file `Copy` / `kCopy` pairs and TLB syscall stubs by reading (architecture.md "Key functions
   and globals", SDK). Doubts: the TLB stub names 0x140538..0x1405e8 follow ps2sdk's syscall
   numbers 0x55..0x59 (`ExpandScratchPad` 0x59 least sure); 0x141198 (`j InitTLB`, called by
   every exit.c entry) keeps Ghidra's thunk name; 0x142dd0 / 0x142df0 after the reboot-notify run
   are unread.
   **Batch 016 done (2026-11-02, 139 names, 75 starts created)**: libkernl's front 0x1366c0..
   0x139bc0 (klib.s by slot from RoR, glue / cache / intr / thread / deci2 / tty / kprintf /
   sifcmd head from RoR + FF3; architecture.md "Key functions and globals", SDK). Doubts: the
   seven klib.s slots RoR merged (0x55..0x59, 0x5b, 0x74) carry ps2sdk names; 0x1371c8 /
   0x1371d0 (glue, return -1) unnamed; `lseek` is FF3's low-confidence name; the kprintf.c
   helpers (`_kvprintf`, `bufferPutchar`, `deci2Output`, `serialOutput`, `kprintfSub`,
   `deci2Vprintf`, `kprintf`) and `getpid` / `sceSifGetDataTable` are names by reading, Sony's
   own unknown (RoR and FF3 have them unnamed); 0x123528 (glue's errno setter?) and the VU0
   math run 0x135a90..0x1366bc before klib.s are unread.
   **Batch 017 done (2026-11-02, 38 names)**: libkernl's fileio 0x13b7e8..0x13ed90 from RoR, one
   to one, every `SceStdio*Sema` string agreeing (+ `_fs_version`, `sceLseek64`, `get_iob` from
   FF3's FF2 symbols; architecture.md "Key functions and globals", SDK). Doubts: `_sceFsCallRpc` /
   `_sceFsCountOpenIob` are names by reading (Sony's own unknown); 0x13bc38 (rpc 0xfe) / 0x13bd78
   (rpc 0xfd) left unnamed (no callers, unnamed in RoR and FF3).
   **Batch 018 done (2026-11-02, 42 names, 7 merged starts created)**: libgraph / libdma / libdev
   0x1076e8..0x10ac60 from RoR (38 aligned, 4 by reading: `sceGszbufaddr`, `sceGsPutDrawEnv`,
   `sceGsSetDefTexEnv`, `sceDevVif1GetCnd`; architecture.md "Key functions and globals", SDK).
   Doubts: 0x109570 (RoR's unnamed 0x224) and RoR's unnamed libdev slots stay `FUN_`.
   **Batch 019 done (2026-11-02, 29 names, 1 merged start created)**: libcdvd 0x11c6b0..0x11e440
   from RoR (25 aligned, `_sceCdSC` 0x11d430 split off; `_sceCd_ncmd_prechk` / `sceCdNcmdDiskReady`
   from FF3's FF2-symbol list, `sceCdChgSys` from its string, `_sceCdGetDiskType` by reading;
   architecture.md "Key functions and globals", SDK). Doubts: `_sceCdGetDiskType` is a made-up
   internal name; 0x11dff0 (S-command RPC 0x25, two words, no callers) left unnamed; 0x11b800..
   0x11c6b0 before libcdvd is an unread cluster (not libcdvd). Snapshot before it:
   `work/snapshots/20261102_164802.json`.
   **Batch 020 done (2026-11-02, 35 names, 1 merged start created)**: libmc 0x11fb70..0x121b80
   (`rxalign.py 11fb70 121470 24d220 24eb28`: 19 aligned once the range ran to `sceMcGetEntSpace`
   0x121360, `sceMcChdir` 0x120b78 split off `mceStorePwd`; `sceMcSeek` / `mceReadFixAlign` /
   `mceSignalSema` and FS2's read-fast part 0x121470..0x121b80 by reading) + `sceSifSetRpcQueue`
   0x13ac68 / `sceSifRegisterRpc` 0x13ad00 (batch 010's open pair) + `memcpy` 0x125228
   (architecture.md "Key functions and globals", SDK). Doubts: the `mce*` internals are names by
   reading (Sony's own unknown); `sceMcReadFast` comes from the thread string, `sceMcWriteFast`
   (rpc 0x1b) by analogy only; 0x121b80 (error-code mapper, called only by the game's memory-card
   layer 0x2ac710..) left unnamed: libmc or game code?
   **Batch 021 done (2026-11-02, 23 names)**: libpad 0x11e440..0x11fb70 one to one with FF3's
   list (`../ReiPan/ff3_functions.txt` 0x283a28..0x285158: same sizes and order, FF3 merged
   PortOpen + PortClose; 14 names FF2 symbols, 6 FF3 names checked by reading) + three internals
   by reading (architecture.md "Key functions and globals", SDK). Doubts: `_scePadCmdHandler`,
   `_scePadSendActData`, `_scePadSetCallback` are made-up names; `rxalign.py` only reads RoR's
   list, so this alignment was done by hand (a `--ref` option for FF3's list would make the next
   one a command).
   **Next batches, in this order**: libc near strlen / strcpy (0x1252d8 memmove?; 0x106c00 / 0x106b80 = MSL's refcounted
   string rep); SDK functions from their `PsIIlib` / assert strings; rx (`rxfile.c`); rena
   (`rena_*.cpp`). Thunks 0x100218 (`_exit`) and 0x100270 keep Ghidra's thunk names (renaming a
   thunk may rename its target);
   (c) the first island ~0x2f3a40..0x2f4d20 (9 Ghidra functions, nothing references 0x2f3a40):
   **kernel patches the SDK installs** (batch 014: `InitAlarm` copies 0x2f4860 + 0x740 to kernel
   0x80076000; `InitExecPS2` / `InitTLBFunctions` / `InitSystemCallTableAddress` likely copy the
   rest): library code never run on the EE side, name it after those installers;
   (d) `libmatch.py` once the reference ELFs are here (items 3-4).
3. **The user (2026-11-02, in progress): Rule of Rose in Ghidra.** The user is loading RoR into
   Ghidra so its names can be re-applied there and compared with FS2 (BinDiff crashes on PS2
   programs, so not that). **2026-11-02: `SLUS_214.48` is in YamiPan's Ghidra project** (closed;
   `/mcp/instance_info` lists it): with 7.0.0 one server serves both once it is open, and
   `get_function_hash` gives opcode hashes for both programs (a cheap first compare). Once RoR answers or its
   ELF `SLUS_214.48` sits next to `../RosePan/ror_functions.txt`: **verify batch 005's rx names word
   for word** (libmatch.py's masked compare, RoR as the reference; a mismatch -> back to reading) and
   extend the match to the rest of rx and to the SDK 3.0 libraries both games link.
4. **The user: reference ELFs for libmatch.py.** The sibling checkouts on this machine have the
   function lists (`../ReiPan/ff3_functions.txt`, `../RosePan/ror_functions.txt`) but not the ELFs
   (`../ReiPan/SLES_538.25`, `../MioPan/SLES_523.84` with `symbols.txt` / `functions.txt`,
   `../RosePan/SLUS_214.48`); `libmatch.py` skips a reference whose ELF is missing (0 matches,
   2026-10-31). Copy them next to their lists (never committed) to match FS2's SDK 3.0 libraries.
5. **The user: a host toolchain (2026-11-02: MSYS2 installed, g++ only).** `C:\msys64\ucrt64\bin` has
   g++ / gcc 16.2.0 (Rev4) but no cmake, ninja or gdb, and it is not on PATH; set `YAMIPAN_MINGW_BIN`
   to it once `pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
   mingw-w64-ucrt-x86_64-gdb` has run. Earlier (2026-10-31): no MinGW g++, ninja or cmake on this machine (RosePan's paths,
   CLion's MinGW and Python's ninja, do not exist here; 2026-10-31). `build.py` / `layoutcheck.py` take
   them from PATH or `YAMIPAN_MINGW_BIN` / `YAMIPAN_NINJA`; bootstrap 6-7 need them.
6. ~~Install the GhidraMCP 7.0.0 built for Ghidra 12.2~~ **Done 2026-11-02** (the user installed
   the build from `../ghidra-mcp`, build 20261102): decompile, function at an address, tags and save
   all answer. Ghidra's function tags hold only 0x162250's provenance tag so far; the other 760 in
   `data/tags.json` are mirrored when a batch next tags them (or by a one-off loop over the file).
7. Fill architecture.md as each step learns something.

## Decisions

- 2026-11-02 (the user): **four rules for all code** (SKILL.md "The user's rules"): (1) no GS
  emulator, not even for textures (MioPan's `miopan_gs.cpp` reimplements the GS for textures: do not
  repeat it; decode assets at their source so the GS path is not needed); (2) name repeated values and
  state / flag bits as enums, `#define`s or `constexpr` while decompiling; (3) **arbitrary aspect ratio
  is the port's #1 feature**; (4) reuse MioPan / ReiPan / RosePan abstractions instead of
  reimplementing the basics.

- 2026-11-02 (the user): **source code first.** Stop seeding library names; reconstruct game code
  and name as each TU needs it ("Next up" item 0). `tus.py` comes over before the build exists, in a
  source-only mode; supersedes the 2026-10-31 decision to hold it for bootstrap 7.

- 2026-11-02 (the user): **bethington's GhidraMCP 7.0.0 replaces LaurieWired's 2.0.0** (the user
  installed it; both are `Extensions\GhidraMCP`, so it took 2.0.0's place) and **the tools were ported
  to it** ("Port tools to 7.0.0"): `ghidra_mcp.py` finds the one server per process by the open program
  and project on 8089..8104 and passes `program=`; `yamipan.py` / `ghidra.py` / `fetch.py` use its
  endpoints (ghidra.md); saving and function creation now work from the tools; the `ghidra` MCP server
  is the plugin's own bridge from `../ghidra-mcp` (`ghidra_bridge.py`). Supersedes the 2026-10-31
  decisions on 2.0.0 and on 5.14.2 beside it.
- 2026-10-31 (session decision, bootstrap 3; **superseded 2026-11-02 for `tus.py`**, which came over
  in source-only mode; `autopilot.py` still waits): **`tus.py` (and `autopilot.py`,
  which needs it) are carried over at bootstrap 7, not 3**: the routine switches to the TU loop as soon
  as `tools/reverse/tus.py` exists, and with no TU map, platform layer or build that loop would claim
  nothing useful. `status.py` / `tuid.py` import it lazily.
- 2026-10-31 (session decision, bootstrap 3): **provenance tags live in `data/tags.json`** (address ->
  tag, committed), mirrored to Ghidra's function tags only while 5.14.2 answers: 2.0.0 has no tags, and
  the repo record survives a Ghidra crash. `fetch.py` reads it; `name_ctx.py` / `libmatch.py` write it
  through `yamipan.set_tag`.

The user's standing rules, carried from the earlier ports (SKILL.md "The user's rules" says how they
apply here):

- 2026-10-31 (the user): **scheduled runs every 20 minutes**: the desktop app's task `yamipan-next-tu`
  (cron `*/20 * * * *`, prompt = `tools/reverse/routine.md`) advances the port by one step per run --
  a bootstrap step until `tus.py` exists, then one TU. `tools/reverse/preflight.py` gates each run
  (stop switch `work/STOP`, run lock `work/run.lock`, clean tree, work branch, Ghidra up), so a run
  still going makes the next one stop at once.
- 2026-10-31 (the user): work happens on the work branch **`fs2-port`** (on origin; `main` stays at
  the Ghidra switch). **Pushing each run's commit to origin/fs2-port is approved**; never force.
- 2026-10-31 (the user): the annotation tag is **`FS2`** (`// FS2 0x...`).
- 2026-10-31 (the user): GhidraMCP 2.0.0's gaps are covered by **bethington's GhidraMCP 5.14.2
  installed beside it** (the user installs it). The carried-over tools keep RosePan's 5.14.2 calls for
  what 2.0.0 lacks -- `save_program`, `create_function`, `disassemble_bytes`, the function-tag
  endpoints, `get_bulk_function_hashes` -- through a 5.14.2 client on its own ports (8089..8104,
  `program=` in the query string, as before 2026-10-31: `git show 8536a1f:tools/reverse/ghidra_mcp.py`);
  everything else and Claude's MCP tools go through 2.0.0.
- 2026-10-31 (the user): Ghidra 12.2 with **LaurieWired's GhidraMCP 2.0.0** replaces Ghidra 12.1.2
  with bethington's 5.14.2, on a new machine. YamiPan's project is
  `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr` (program `SCES_538.51`); the game's
  files moved to `C:\Users\willi\Documents\Decomp\Forbidden Siren 2`. The tools, the bridge and
  `.mcp.json` were switched the same day (ghidra.md).
- 2026-10-30 (the user): YamiPan ports **Forbidden Siren 2 (Europe, `SCES_538.51`)**. The game's
  data and ELF are in `C:\Users\willi\Documents\Decomp\Forbidden Siren 2` (since 2026-10-31):
  - `SCES_538.51`;
  - `ROM.000`..`ROM.007`, `ROMD.008`..`ROMD.010` and `SPK.ROM`;
  - `MODULES\`, `IOPCONF.CNF` and `SYSTEM.CNF`.

  The tools read the ELF and the data from there; nothing is copied into the repo.
- 2026-10-30 (the user): YamiPan is the next PS2 port; its skill carries ReiPan's and RosePan's
  learning. First step: Ghidra working with several instances (done the same day, ghidra.md).
- 2026-10-30 (session decision; updated 2026-10-31): one Ghidra project and process per game. With
  GhidraMCP 2.0.0 a backend is found by the program active in its window (`/health` on 8080..8095,
  `ghidra_mcp.py`), not by port, and every request checks it first.
- 2026-10-28 (the user, RosePan): the PC port displays correctly at any aspect ratio, read from one
  renderer-owned source; never a fixed 4:3 / 640x448 assumption in new host draw code.
- 2026-10-22 (the user, RosePan): drawing never goes through the emulated GS; the game's draw decisions
  are reconstructed and the packet-writing step calls a native host draw module per subsystem.
- 2026-10-15 (the user, ReiPan): methods, not free functions -- a function taking the object in a0 is a
  member of its class.
- ReiPan / RosePan (the user): the picture is the test; a drawing TU is `done` only when the screenshot
  matches the game's own assets; complete code nothing can show yet is `verify`.
- ReiPan / RosePan (the user): automated porting runs at effort xhigh or above; sessions never start
  other sessions, agents or scheduled tasks, and never edit `autopilot.py` or the routine.

## Open questions

- **(2026-11-02, bootstrap 5) TU keys and guessed cuts.** rx / SDPR2 keys end in `.c` because FS2's
  own strings say `rxfile.c` / `filehost.c`; RosePan reconstructed the same rx as `rx/*.cpp`. If the
  reconstruction reuses RosePan's C++ as is, rename the key with `tus.py define` (the extent stays).
  Guessed cuts to settle at their TUs: `rx/filecd.c` / `rx/filehdd.c` as own files, the six
  `rx/rxsndstr*.c` layers as six files, SDPR2's A/B and C/D cuts, every `game/*.cpp` name and both
  ends of `game/thread.cpp`, `game/perfmeter.cpp`, `game/gfxtarget.cpp`; `sound/sound.cpp`
  0x144080..0x148db0 is likely several files. Unread runs left as game code, not library:
  architecture.md "Address map" (`Heap` 0x1037c0..0x104c60 above all: MSL or game?).

- ~~Which game; engine kinship with FF2 / FF3 / RoR?~~ Settled 2026-10-31 (bootstrap 1): none (but the
  rx library is RoR's: 2026-11-02, batch 005); SCE's
  own rx runtime and rena renderer, built with Metrowerks CodeWarrior 2.4.1.01.
- **CodeWarrior, not GCC** (bootstrap 1, 2026-10-31). Consequences for bootstrap 3-4, to settle there:
  `sections.py` / `gcc2.py` have nothing to read (stripped, no linkonce sections); `rtti.py` needs a
  CodeWarrior variant (vtable `{type_info*, offset, fn...}` 4-byte entries, game classes with null
  type_info, vptr after the data in at least one class: architecture.md); `libmatch.py` can match
  Sony's prebuilt SDK 3.0 libraries (FF3 / RoR references) but not MSL C++ or CodeWarrior's runtime
  (no reference built with it); reconstruction.md needs a CodeWarrior ABI section (calling convention
  is the EABI / o32-like one GCC uses on the EE? 64-bit `long`? struct return and member-pointer
  layout?) before the first class TU. The host build stays MinGW / MSVC either way.
- A symbolised sister build? Forbidden Siren 1, the Japanese / US FS2 releases or a demo disc might
  keep `.mdebug` or a map; none is on this machine. Ask the user before bootstrap 4 if one exists.
- Two EE code islands outside .text: ~0x2f3a40..~0x2f4d20 (after the VU microcode) and
  ~0x310a00..~0x310f80. The second is settled (2026-10-31): static initialisers, table 0x310fb0 run by
  0x100280 (architecture.md). The first is still open (nothing references 0x2f3a40; kernel-patch or
  IOP-reboot code?).
- Multiple vptrs in one object, every secondary table with this_offset 0 (architecture.md "Classes"):
  embedded members or bases? Settle at the first TU of the +0xf4 family (ctor 0x171a30).
- ~~Can 5.14.2 sit beside 2.0.0?~~ Settled 2026-11-02: no (same extension name); bethington's 7.0.0
  replaced 2.0.0 and the tools moved to it (Decisions).
- **`tools/reverse/routine.md` step 4** still says "GhidraMCP 2.0.0 never saves ... save through the
  GhidraMCP 5.14.2 plugin's save_program": the tools now save through 7.0.0 themselves. The routine
  is the user's to edit (sessions never touch it); a run that saw `name_ctx.py` save need not report
  "Ctrl+S".
- RosePan's and ReiPan's own Ghidra tools (`rosepan.py`, `reipan.py`, pinned to 8089 and 5.14.2) do
  not run on this machine: their checkouts are read-only references here (cloned 2026-10-31).
- Parallel sessions on one game (several worktrees, one Ghidra): reads are fine, naming batches race
  (ghidra.md "Working with several"). Decide whether only one session per game names at a time, or the
  ledger learns to merge.

## Done

- 2026-11-02: TU 1 `game/gamesystem.cpp` (scheduled run): all 17 functions reconstructed into
  `src/game/gamesystem.cpp` / `.h` with minimal headers for the callee TUs, naming batch 022 (18
  names, saved by the tools), source-only check green (callcheck clean, layout lint OK); status
  `verify` (not built). Next: the next TU `tus.py next` picks.

- 2026-11-02: "Next up" 0(b) (scheduled run): `tus.py` carried over from RosePan with a source-only
  mode (no build: `check` = callcheck + layout lint, `finish` refuses `done`, records "not built");
  README, SKILL.md, bootstrap.md updated, status.md regenerated. Next: 0(c), the first TU
  (`game/gamesystem.cpp`).

- 2026-11-02: bootstrap 4, part 16 (scheduled run): naming batch 016 (139 names: libkernl's front,
  klib.s by slot from RoR, glue / cache / intr / thread / deci2 / tty / kprintf / sifcmd head from
  RoR + FF3), 75 function starts created, saved by the tools. Next: the other SDK 3.0 libraries
  (libgraph, libdma, libpad, libmc, libcdvd) by `rxalign.py`.

- 2026-11-02: bootstrap 4, part 14 (scheduled run): naming batch 013 (93 names: the game's sound
  layer by reading, SDK iopheap from RoR; 3 sink classes in `data/class_names.json`), saved by the
  tools. Next: `_InitSys`'s nine callees, then the SDK libraries by `rxalign.py`.

- 2026-11-02: bootstrap 4, part 11 (scheduled run): the code-pointer pass, new `codeptrs.py`
  (39 starts created, saved by the tools) and naming batch 010 (37 names: SIF cmd / rpc + fileio
  head from RoR by size alignment, `_root`, `rxSysNoMemFunc`, the rx memory hooks). Next: SDPR2,
  then rxMpeg.

- 2026-11-02: bootstrap 4, part 10 (scheduled run): 4(a), 1,023 missing function starts created in
  Ghidra (saved by the tools: no Ctrl+S needed) and naming batch 009 (113 names: the merged rx
  starts, the main loop's callbacks, the pad update slots); `yamipan.create_function` retries a
  call that answered without creating. Next: the code-pointer pass, then SDPR2, rxMpeg.

- 2026-11-02: bootstrap 4, part 9 (scheduled run): naming batch 008 (114 names: rxsndut +
  rxsndstr by reading, `strcmp`; 33 merged starts listed). Ghidra had unsaved changes (Ctrl+S).
  Next: SDPR2, then rxMpeg.

- 2026-11-02: bootstrap 4, part 8 (scheduled run): naming batch 007 (204 rx names: rxsnd from
  RosePan and rxthread's VBlank tail, which FS2 links before rxsnd; 28 merged starts listed).
  Ghidra had unsaved changes (Ctrl+S). Next: rxsndut + rxsndstr, SDPR2 (by reading).

- 2026-11-02: bootstrap 4, part 7 (scheduled run): naming batch 006 (61 rx names: the gaps and
  rxfile's tail from RosePan, 7 by reading, 5 fixes of batch 005), `rxalign.py` span partition and
  tie-breaking, 45 merged rx starts listed. Ghidra had unsaved changes (Ctrl+S). Next: rxsnd.

- 2026-11-02: bootstrap 4, part 6 (scheduled run): naming batch 005 (253 names: main's teardown,
  libmc / libpad / MSL bits, the SPK.ROM hooks, and 231 rx names from RosePan via the new
  `rxalign.py`). Ghidra had unsaved changes (Ctrl+S). Next: the rest of rx ("Next up").

- 2026-10-31: bootstrap 4, part 5 (scheduled run): naming batch 004 (52 names: `GameSystem` and
  its members, rena `Vif1PktManage`, libvifpk, clocks, debug meter, `rxPrintf`) and
  `data/class_names.json` (6 vtables). Ghidra had unsaved changes (Ctrl+S). Next: bootstrap 4(b)
  batch 005, main's teardown ("Next up").

- 2026-10-31: bootstrap 4, part 4 (scheduled run): naming batch 003 (34 names: libpad / libgraph /
  libdma / SIF, pad and sound systems, rx init and `spkRomLoad`, `gsClearVram`,
  `gfxInitRenderTargets`, options and system data). Ghidra had unsaved changes (Ctrl+S). Next:
  bootstrap 4(b) batch 004, the global object's class ("Next up").

- 2026-10-31: bootstrap 4, part 3 (scheduled run): naming batch 002 (26 names: the 8-slot thread
  scheduler and frame pacing, `gameInit`, heap init / teardown, libgraph / libkernl helpers,
  `_malloc_r` / `_free_r`). 0x12db00 needs a function first. Ghidra had unsaved changes (Ctrl+S).
  Next: bootstrap 4(b) batch 003 ("Next up").

- 2026-10-31: bootstrap 4, part 2 (scheduled run): naming batch 001 by reading (27 names: crt0 /
  libc / libkernl, CodeWarrior + MSL runtime, `main`, the heap), `data/context_names.json` and
  `data/tags.json` started. 4(a) still waits for 5.14.2. Ghidra had unsaved changes (Ctrl+S).
  Next: bootstrap 4(b) batch 002 ("Next up").

- 2026-10-31: bootstrap 4, part 1 (scheduled run): name snapshot, CodeWarrior `rtti.py` (490
  vtables, `--missing`), static-initialiser table found, 910 missing function starts listed. Next:
  bootstrap 4 (a)-(d) in "Next up".

- 2026-10-31: bootstrap 3, the tools (scheduled run): RosePan's tools carried over and renamed
  (`yamipan.py` on `ghidra_mcp.py`, a `Ghidra5` client for 5.14.2, `data/tags.json`, `data/layout.json`
  image sections for the stripped ELF, `ghidra.py find` over the ELF, `mipsdis.py` fallback for
  disassembly); `refresh.py` green against Ghidra (6,393 functions, `yamipan_functions.txt`,
  `status.md`). `tus.py` / `autopilot.py` wait for bootstrap 7. Next: bootstrap 4 (name seeding).

- 2026-10-31: bootstrap 1, the binary identified (scheduled run): stripped CodeWarrior 2.4.1.01 ELF,
  SDK 3.0 libraries, rx / rena middleware, gp 0x322870, `main` 0x12d910, address map with the VU
  microcode at 0x2ec900 (architecture.md). Next: bootstrap 3 (tools from RosePan).
- 2026-10-31: scheduled runs -- the work branch `fs2-port` (pushed), `tools/reverse/routine.md`,
  `tools/reverse/preflight.py`, the desktop app's task `yamipan-next-tu` every 20 minutes.
- 2026-10-31: the Ghidra layer on GhidraMCP 2.0.0 -- `ghidra_mcp.py` (finds the window by its active
  program via `/health` on 8080..8095, checks it before every request, parsed `data` and
  `GhidraRequestError`), `ghidra_bridge.py` (the wheel's server around that check, plus
  `list_instances` / `connect_instance`), `.mcp.json` (the venv beside Ghidra 12.2),
  `data/ghidra.json`. Tested live: every endpoint, a stdio MCP session (32 tools), the check refusing
  a write when the program was not active.
- 2026-10-30: multi-instance Ghidra -- `tools/reverse/ghidra_mcp.py` (discovery + client + CLI),
  `tools/reverse/ghidra_bridge.py` (the MCP bridge, instance-aware), `.mcp.json`; tested with two live
  Ghidras (the second bound 8090 by itself).
