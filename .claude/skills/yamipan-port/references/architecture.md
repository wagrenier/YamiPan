# Architecture -- what is known about the game

Established facts only, each with its address (unconfirmed guesses go to open-items.md). Replace what a
new fact supersedes; no contradictions. Empty until bootstrap (bootstrap.md) names the game; the
sections are the ones RosePan's grew into.

## Binary and toolchain

Serial, region, build date, entry, gp (`.reginfo`), compiler family and ABI, symbols kept (sections,
`.mdebug`), Sony library versions, IOP modules, VU microcode sections.

- `SCES_538.51`, Forbidden Siren 2, Europe, ELF dated 2006-05-17. MD5
  `d3334a6ed5cd1a6537ab248d58d747cb`, SHA-256
  `cb877026b9c795eb6970a72df973f1d33d9a9e2baa044f83efe4143b7b470b2f` (Ghidra's `program_info`,
  2026-10-31).
- Ghidra (emotionengine-reloaded, `r5900:LE:32:default`): image base 0x100000, entry 0x100008, the
  loaded image one block `SECTION4` 0x100000..0x31ad7f; auto-analysis found 6,393 functions, no
  imports, one export (`entry`).
- **Compiler: Metrowerks CodeWarrior**, not GCC: `.comment` reads `MW MIPS C Compiler (2.4.1.01)` /
  `PlayStation2` (bootstrap step 1, 2026-10-31). C++ with exceptions and Metrowerks' MSL C++ library
  (`std::exception` / `logic_error` / `length_error` / `out_of_range`, MSL's exception-matching type
  strings `!std::exception!!std::logic_error!!...` at 0x2fe540, `vector::insert length error`,
  `tree::insert length error`, `basic_string::reserve length_error` repeated per template
  instantiation). So neither of reconstruction.md's GCC ABIs applies as is (open-items.md).
- **Stripped**: 8 section headers, `.symtab` / `.strtab` empty, no `.mdebug`, no linkonce sections;
  the whole image is one unnamed PROGBITS section at 0x100000 (file 0x80, 0x21ad80 bytes). No mangled
  names anywhere in the image (no `_Z`, no `__<n><class>`).
- Program headers: one LOAD 0x100000, filesz 0x21ad80, memsz 0x4a5600 (bss to 0x5a5600); an empty
  LOAD at 0x5a5600. `.reginfo`: gp **0x322870**, gprmask 0xf7fffffe. Entry 0x100008 clears
  0x31ad80..0x5a5600, SetupThread / SetupHeap (syscalls 0x3c / 0x3d, stack 0x10000, argc/argv at
  0x31af80), FlushCache, EI, then `main` 0x12d910 and the exit call 0x123538.
- **Sony libraries: SDK 3.0** (`PsIIlib<name><ver>` strings): libgraph 3000 0x2f1080, libdma 3000,
  libipu 3000, libcdvd 3000, libpad 3020, libmc 3030, libkernl 3000, libscf 3000; IOP replacement
  `ioprp300.img`. Sony's libc/stdio semaphores (`SceStdio*Sema`, `SceKernelLibc`) are present.
- **IOP modules** (`MODULES\`, `IOPCONF.CNF`): IOPRP300.IMG, SIO2MAN, PADMAN, MCMAN, MCSERV, LIBSD,
  SDRDRV and the game's own **RXSNDDRV.IRX** (the rx sound driver). The ELF also names cdvdfsv,
  cdvdman, fileio, sifcmd, usbd. `SYSTEM.CNF`: `VER = 1.00`, `VMODE = PAL`.
- **Middleware: "rx"** (SCE's in-house runtime; `$RX_SCE_LIBRARY_VERSION:4` at 0x2f088d): rxsys
  (IOP module binding), rxfile (`rxfile.c` 0x3016c0, `filehost.c` 0x301908; disc / host / HDD devices,
  a request-queue thread), rxsnd / rxsndstr / rxsndut (XVAB banks, XSEQ, BGM / SE / PCM / ADPCM
  streams, `RXWS` / XWS stream files), rxthread -- **the rx Rule of Rose links** (batch 005, "Naming
  sources"). **Renderer "rena"**, C++: `rena_2d_texture_tim2.cpp`
  0x30fdd0, `rena_pkt_vif1pktmanage.cpp` 0x310640, `rena draw` 0x303bd8. No game-engine kinship with FF2,
  FF3 or RoR (other developer, other compiler), but the rx library is RoR's (RosePan names and
  reconstructs it); the rest comes from this binary only.
- Strings are few (7,813 printable runs of 4+, dense only in 0x2f0000..0x31aaa4); only 2 `.c` and
  2 `.cpp` file names. Game-side strings are mostly action / event names (`GettingOnCar`,
  `NumLockOpen`, `GunAttack`, `QuickTurn`, `DemoExecuter` 0x303d30, ...).
- **Vtables** (CodeWarrior layout, 4-byte entries): `{type_info*, this-offset 0, fn...}`, e.g. 0x311000
  `{0x2fe688, 0, 0x107220, 0x1073d0}`; game classes have a null type_info (0x311020
  `{0, 0, 0x12dc60}`), so RTTI is emitted only for the library's exception classes. `main` stores
  0x311030 then 0x311020 into a member object's vptr at object+0x120 (object at +4, store at +0x124):
  the vptr is **not at offset 0** there, it follows the data (as GCC 2 places it).

## Address map

| range | contents |
|---|---|
| 0x100000..0x100008 | header words before `entry` |
| 0x100008..~0x2ec8e0 | .text: crt0, game, rx, rena, MSL, Sony SDK (7,010 `jr ra`; Ghidra 6,381 functions) |
| 0x2ec900..~0x2f0880 | VU microcode (lower/upper pairs, `0x8000033c` / `0x000002ff` NOPs; ~16 KB, VU1-sized) |
| ~0x2f0880..0x2f3a40 | data: library version strings, SDK tables |
| ~0x2f3a40..~0x2f4d20 | an EE code island (Ghidra functions 0x2f3bd0..0x2f4ce8); what it is: open |
| ~0x2f4d20..0x31ad80 | .data / .rodata: strings to 0x31aaa4, vtables from ~0x311000, function tables (0x31a000) |
| ~0x310a00..~0x310f80 | static initialisers (9; table 0x310fb0..0x310fd4 run by 0x100280: "Static initialisers") |
| 0x310fe0..~0x31a820 | vtables (490, `rtti.py`: "Classes") |
| 0x31ad80..0x5a5600 | .bss (cleared by `entry`); gp 0x322870 |

The ELF names none of these (one unnamed section), so `data/layout.json` "image_sections" carries the
table above as named regions for the tools (`elf32.Elf` uses them for the game ELF only, 2026-10-31):
`.text` 0x100000..0x2ec900, `.vutext` ..0x2f0880, `.data` ..0x2f3a40, `.text` (island) ..0x2f4d20,
`.data` ..0x310a00, `.text` (island) ..0x310f80, `.data` ..0x31ad80, `.bss` ..0x5a5600. The `~`
bounds are guesses: refine them there when a region is pinned (no `.rodata` / `.sdata` split known
yet; gp-relative small data straddles 0x31ad80).

**Library ranges** (`data/layout.json` `lib_ranges`, bootstrap 5, 2026-11-02; replaced on PC, never a
TU; `export.py` lists each under its label's library): crt0 + CodeWarrior entry 0x100000..0x100328,
MSL C++ runtime 0x104c60..0x1076e8, libgraph / libdma / libdev 0x1076e8..0x10ac60, libvifpk
0x10ac60..0x10c218 (its leaf run after 0x10acc8 unread: libvu0?), libmpeg 0x10c218..0x116978, libipu
..0x116fb8, libcdvd 0x11c6b0..0x11e440, libpad ..0x11fb70, libmc ..0x121b80, libc (newlib)
0x121ef0..0x12c9b8, libkernl 0x1366c0..0x142dd0 (front, sifcmd / sifrpc, fileio, iopheap, tail) and
the kernel-patch island 0x2f3a40..0x2f4d20: 912 functions. **Unread runs between them stay game code
until read** (not library by default): `Heap` + its neighbours 0x1037c0..0x104c60 (MSL pool or the
game's?), 0x116fb8..0x11c6b0 (soft-float / math runtime shapes 0x119b70..0x11af08, then the
~40-caller cluster 0x11b800..), 0x121b80 (libmc error mapper?), 0x12c9b8..0x12cd20, the VU0 math
0x135a90..0x1366c0, 0x142dd0..0x144080.

First features pass over it (`refresh.py`, 2026-10-31): 6,393 functions, 25,668 calls, 871 tail
jumps, 2,446 string references, 3,478 float constants; `export.py` anchors 6 functions to 2 source
files from the 4 file-name strings.

## Naming sources

| source | count | tool | tag |
|---|---|---|---|
| ELF symbols | 0 (stripped) | -- | -- |
| Ghidra auto-analysis | 100 (SDK syscalls, thunks) | emotionengine-reloaded | -- |
| vtables (classes) | 490 tables, 4 named by RTTI | rtti.py (CodeWarrior) | -- (class map, not names yet) |
| missing function starts | 910 (904 vtable slots, 6 static initialisers) | rtti.py --missing | **created 2026-11-02** (bootstrap 4a) |
| by reading, batch 001 (2026-10-31) | 27: crt0 / libc / libkernl, CodeWarrior + MSL runtime, `main`, heap | name_ctx.py | lib-context 24, game-inferred 3 |
| by reading, batches 002-004 (2026-10-31) | 26 + 34 + 52: threads, gameInit's subsystems, `GameSystem` and its members, rena `Vif1PktManage` | name_ctx.py | ledger 139 |
| class names | 6 (`data/class_names.json`) | rtti.py | -- |
| batch 005 (2026-11-02) | 253: main's teardown (22 by reading) + **231 rx names from RosePan** (rxsys 89, rxthread 29, rxfile 113) | rxalign.py + name_ctx.py | lib-context; ledger 386 |
| batch 006 (2026-11-02) | 61 rx names: the gaps (span partition), rxfile's tail (thread, queue, device map), 7 by reading, 5 fixes of batch 005 | rxalign.py + name_ctx.py | lib-context; ledger 443 |
| batch 007 (2026-11-02) | 204 rx names: rxsnd 0x159bc0..0x162178 (190 aligned, 5 by reading, 3 FS2-only RPC wrappers) + rxthread's VBlank tail 0x159928..0x159bc0 (6) | rxalign.py + name_ctx.py | lib-context; ledger 647 |
| batch 008 (2026-11-02) | 114: rxsndut + rxsndstr 0x162178..0x166b50 by reading (113, every Ghidra function there) + `strcmp` 0x128238 | name_ctx.py | lib-context; ledger 761 |
| rx starts Ghidra merged | 45 + 28 (batch 007) ("rx from RosePan"), + 33 rxsndstr (batch 008, "by reading") | rxalign.py --missing; batch 008 by a jr-ra / tail-j scan | created 2026-11-02 |
| bootstrap 4(a), batch 009 (2026-11-02) | **1,023 function starts created** (all 1,016 of `data/missing_functions.json` + 7 code pointers passed as arguments; Ghidra 6,395 -> 7,418 functions); 113 names: the 106 merged rx starts with their batch 005-008 names, `gameMainThread`, `gameFrameReady`, `gameThreadSwitchHook`, `threadVSyncHandler`, `dmacBusErrorHandler`, `GamePad::update`, `MaskedGamePad::update` | ghidra create_function loop + name_ctx.py | lib-context 106, game-inferred 7; ledger 874 |
| code pointers, batch 010 (2026-11-02) | **40 starts created** (39 code pointers found by `codeptrs.py` + `sceSifSetSysCmdBuffer`; Ghidra 7,458); 37 names: the SIF cmd / rpc library and fileio head from RoR (32), `_root`, `rxSysNoMemFunc`, the three rx memory hooks | codeptrs.py --create, rxalign.py + name_ctx.py | lib-context 34, game-inferred 3; ledger 911 |
| libmpeg / libipu, batch 012 (2026-11-02) | **15 starts created** (small libmpeg getters / setters Ghidra had merged; Ghidra 7,483); 46 names: SDK 3.0 libmpeg 0x10c218..0x116978 and libipu 0x116978..0x116fb8 from RoR (37 aligned, 8 merged starts, `_sceMpegSliceA0` by its message) | rxalign.py --lib + name_ctx.py | lib-context; ledger 1,065 |
| libkernl tail, batches 014-015 (2026-11-02) | **14 starts created** (syscall stubs and small helpers Ghidra had merged; Ghidra 7,498); 111 names: SDK 3.0 libkernl 0x13ef80..0x142dd0 (iopheap tail, loadfile, TLB, initsys, libosd, exit, timer, alarm, reboot notify) from RoR (77 aligned one to one), 14 RoR placeholders from FF3 (FF2 symbols), 18 by reading; `InitThread` / `topThread` | rxalign.py --lib + name_ctx.py | lib-context; ledger 1,269 |
| libkernl front, batch 016 (2026-11-02) | **75 starts created** (71 klib.s syscall stubs nobody calls, 4 merged glue / sifcmd stubs; Ghidra 7,573); 139 names: klib.s 0x1366c0..0x136f60 by slot from RoR, glue / cache / intr / thread / deci2 / tty / kprintf 0x136f60..0x139940 from RoR + FF3 (FF2 symbols), 15 by reading | rxalign.py --lib, a slot table + name_ctx.py | lib-context; ledger 1,408 |
| fileio, batch 017 (2026-11-02) | 38 names: SDK 3.0 fileio (filestub.c) 0x13b7e8..0x13ed90 from RoR (35 aligned one to one), `_fs_version` / `sceLseek64` / `get_iob` from FF3 (FF2 symbols), `_sceFsCallRpc` / `_sceFsCountOpenIob` by reading | rxalign.py --lib + name_ctx.py | lib-context; ledger 1,446 |

Expected sources (bootstrap step 4): Sony SDK 3.0 libraries by match (libmatch.py against FF3 / RoR,
both SDK 3.0); MSL C++ / CodeWarrior runtime only by reading (no reference game built with it); rx and
rena from their messages; game code from its strings and callers. **rx from RosePan** (batch 005): FS2
links the same prebuilt rx library as Rule of Rose -- same functions, same order, same sizes, down to
`rxThFindCallback`'s delay-slot bug (always -1) -- so `rxalign.py` aligns size sequences against
`../RosePan/ror_functions.txt` and RosePan's `src/rx/*.cpp` is the reference (likely reusable) for
the rx TUs. Not yet verified word for word (RoR's ELF / Ghidra program is not on this machine).
Batch 006 (2026-11-02) finished rxsys 0x100350..0x1037b0 + 0x153108..0x153518, rxfile
0x153518..0x159928 and rxthread 0x166b50..0x1681c0: every Ghidra function there has RoR's name.
Differences: **FS2's rx is a slightly newer build** whose error messages carry `__FILE__` /
`__LINE__` (`"rxfile(%s,%d) : failed to open file '%s' %d %d %X,%d %d"` 0x301680, `rxfile.c`), so
`rxFileOnCreate` / `rxFileOnGetSize`, the host driver (`rxFileHostGetSize`..`Write`, each +0x10) and
`rxFileCdFindDir` (+0x18) are longer than RoR's; `rxSysPrintf` is the same size. Ghidra had merged
45 small rx functions (setters reached only by pointer or not at all, thread entry points) into
their predecessors; created 2026-11-02 (bootstrap 4a, named in batch 009). After rxthread's `rxThDmaSync`
0x168150 FS2 diverges from RoR (RoR's `rxThVBlank*` / `rxThSync`; FS2's 0x1681c0.. differ in size,
not rxsnd: the `SDPR2` messages, below).
**Batch 007 (2026-11-02): rxsnd and rxthread's tail.** FS2 links rx in another order than RoR
(RoR: rxsnd, rxsys, rxfile, rxthread; FS2: rxsys 0x100350.., ..., rxsys tail + rxfile
0x153108..0x159928, **rxthread's VBlank tail 0x159928..0x159bc0** (RoR 0x24b1f8..0x24b48c:
`rxThVBlankHandler` on INTC 2 counting fields, `rxThVBlankInit` / `Exit`, `rxThReadHSync` (RCNT0),
`rxThWaitVBlank`, `rxThGetField` merged into it, `rxThSync`), **rxsnd 0x159bc0..0x162178** (RoR
0x237ce8..0x23ff58, 222 functions: 190 aligned, 27 merged starts), then FS2-only code, rxthread
0x166b50..0x1681c0). rxsnd's differences: three FS2-only RPC wrappers nothing calls
(`rxSndXvabRpc52` 0x15c7e8, `rxSndXvabSetParam` 0x15c890 = command 0x53 {id, a, b}, the XVAB twin of
`rxSndXseqSetParam`'s 0x63; `rxSndXseqRpc62` 0x15d040), and `rxSndAdpcmGetPosition` 0x1617c0
converts the position through 0x119be8 where RoR returns it; `rxSndBgmSetParam6` /
`rxSndSeSetParamC` / `rxSndPcmSetVolume` differ in size only (read: same code).
**Batch 008 (2026-11-02): rxsndut + rxsndstr 0x162178..0x166b50** (not in RoR; named by reading
in rx style, `rxSndUt*` / `rxSndStr*`; messages 0x3022e0..0x302738). Five layers, each with its own
static slot table and init / exit / update called from `rxSndUtInit` 0x162178 / `rxSndUtExit`
0x1621f0 / `rxSndUtUpdate` 0x162220 (debug flags 0x2f5f10: bit 1 error prints, 2 BG-thread warning,
4 registration print):
- **XWS bank** 0x1622f8..0x1634b8 (`rxSndStrBank*`, 256 slots x 0xa8 at 0x514780): an `RXWS` file
  (version >= 0x200) of 0x10-byte-header chunks `FORM` (variable-length request records: +0 format id,
  +1 length, +2 flags (bit 0 loop), +4 / +5 volume, +9 channels 1 / 2, +10 rate?, +0x10 offset,
  +0x14 size, +0x18 loop offset), `FTXT` (request names), `MARK` / `BRCH` (per-request marks and
  branches: +0 position, +4, +8 type, +0xa / +0xb loop count, +0xc name), `TRCK` (per-request tracks:
  +1 channels, +4 / +6 volume, +8 name), `BODY`; the body is in the file (type 0), a separate file
  (1, with the device and search dir remembered) or IOP memory (2).
- **Stream core** 0x1634b8..0x164398 (64 streams x 0x9c at 0x51ef80): `rxSndStrPlayEx` picks the
  request's registered format (8 slots x 0x28 at 0x52168c, `rxSndStrRegisterFormat`; function table
  {init, exit, update, open, close, getHandle, pauseRead, syncRead}), setters mark dirty bits +0x1c
  (1 pause, 2 attr, 4 volume, 8 pitch, 0x10 a float nobody reads, 0x20 track volumes) that the format
  update applies; mark / branch callbacks +0x54 / +0x58 run with interrupts off; master volume
  0x521688. `rxSndStrStop` / `rxSndStrStopAsync` / `rxSndStrStopAll` are the names its own checks print.
- **PCM format** 0x164398..0x164c38 (table 0x2f5f18, format 1 "PCM stream") and **ADPCM format**
  0x164c38..0x1656e8 (0x2f5f38, format 0 "ADPCM stream", multi-track interleave): feed rxsnd's
  `rxSndPcm*` / `rxSndAdpcm*` from a source.
- **Stream source** 0x1656e8..0x1660e0 (`rxSndStrStream*`, 64 x 0xc at 0x5226e0): type 0 an rxFile
  ring (`rxSysStreamOpenRing`), type 1 a memory stream.
- **Memory stream** 0x1660e0..0x166b50 (`rxSndStrMem*`, 64 x 0x20 at 0x523a00): the rxFile ring API
  over memory, sorted branch markers and tags in node pools 0x524200 / 0x525a04; IOP bodies are read
  through `rxSndIopRead` into 0x522a00 at most 0x1000 at a time.
Ghidra merged 33 of its small functions into their predecessors (getters after a `jr ra`, a tail `j`
then a new prologue); created and named 2026-11-02 (batch 009). Since then `rxalign.py` pairs
every rx range one to one (0 gaps, 0 starts lacking).
**SDPR2 0x1681c0..0x171300 (batch 011, 2026-11-02, 108 names by reading, `sdpr2*`)**: FS2-only
middleware (not in RoR) that plays panned sound effects over rxSndSe. Its message set (`LimitRange`,
`LimitRangeF`, `RoundRange`, `ERR OutOfRange`, `ERR Call Undefine Function`) exists **four times**
(0x3027b8, 0x302920, 0x302a88, 0x302bf0): four source files, each with its own copy -- the TU
boundaries: **A** core + public API 0x1681c0..~0x16c870 (uses 0x3027b8..), **B** the two-voice type
~0x16c870..0x16e498 (0x302920..), **C** the one-voice type + pan math 0x16e498..0x170088 (0x302a88..),
**D** the driver 0x170088..0x171300 (0x302bf0..); the exact cuts between A/B and C/D (functions
without messages) are guesses. Every message prints only while `sdpr2GetDebug` (0x537c48) is 1.
Layers:
- **Globals**: surround 0x537c40 (0..1; 0 forces both phase signs to +1), pair mode 0x537c44
  (0..5: second-voice pitch factor 0x2f5fa0[mode]/4096 = 3520..3856), debug 0x537c48; **64 handle
  slots** 0x537140 x 0x2c {+0 type table, +4 / +8 voice ids (-1 free), +0xc flip, +0x10 / +0x14 /
  +0x18 angles, +0x1c volume, +0x20 pitch (0x1000 = 1.0), +0x24 spread, +0x28 state}; type registry
  0x2f5f80[8].
- **API** (`sdpr2Play*` / `Stop*` / `Set*` / `Get*` / `Update`): checks the type or handle, then calls
  the type's function table slot: +0 PlayPan, +4 Play (volume 0x80, angle 0, spread 0x80), +8
  PlayPanEx, +0xc PlayEx (an index into the SE parameter table 0x2f7a58), +0x10 Stop, +0x14 StopAll,
  +0x18 Pause, +0x1c SetPitch, +0x20 SetPitchF, +0x24 SetAngle, +0x28 SetVolume, +0x2c SetSpread, +0x30
  SetAttr, +0x34 GetLevels, +0x38 GetStatus, +0x3c Update, +0x40..+0x48 type extensions (null in both
  types). `sdpr2Init` 0x1683a8 is called by `soundInit`, `sdpr2Update` 0x16b698 by `soundUpdate`,
  `sdpr2StopAll` by `soundShutdown`; game callers sit at 0x144250..0x148130.
- **Types**, tables filled in code: registry 0 = two-voice "pair" (`sdpr2Pair*`, table 0x537c50,
  config copy 0x537ca0; voices at angle and angle + 180, the second detuned by the pair mode),
  registry 1 = one-voice (`sdpr2Single*`, 0x537cc0, config 0x537d10). Pan: `sdpr2CalcPanVolumes`
  0x16fa78 reads gain tables 0x2f7a48 / 0x2f7a4c (720 entries) and gives negative gains as
  phase sign -1 (SPU2 phase inversion: rear sounds); the pair scales by spread tables 0x2f7a50 /
  0x2f7a54.
- **Driver**: `sdpr2Drv*` call the table at 0x537d30 (+0 Play .. +0x24 GetStatus, +0x28..+0x30 unused);
  the only driver is the SE driver `sdpr2SeDrv*` (table 0x2f7a68, `rxSndSe*`), config 8 words at
  0x537d38 (defaults from 0x537d58 {1, 1, -1, -1, 0..}). Ghidra had merged 10 starts (the two type
  exits, eight SE-driver slots); created 2026-11-02.
The six ranges (FS2 lo hi, RoR lo hi) for `rxalign.py`: `100350 1037b0 23ff80 2433e0`,
`153108 153518 2433e8 2437f8`, `153518 159928 2437f8 249b88`, `159928 159bc0 24b1f8 24b48c`,
`159928 162178 237ce8 23ff58` (rxsnd, as batch 007 ran it: its first six FS2 functions are the
VBlank tail and stay unpaired), `166b50 1681c0 249b88 24b1f8`.

## Program flow

Entry -> main -> the main loop; the boot sequence the smoke pad walks.

- `entry` 0x100008 -> `main` 0x12d910 (argc, argv) -> exit call 0x123538. `main` (batch 002):
  `gameInit` 0x12d620(argv[0]) (heap, sceGsSyncV waits, RNG seed, `new` of the 0x540-byte global
  object stored at gp-0x7aec, ctor 0x12d0d0, AddDmacHandler(15, `dmacBusErrorHandler` 0x12dc20: BEIS, write-1-clears D_STAT bit 15, sets
  gp-0x7ae0)); argc/argv to gp-0x7ae8 /
  -0x7ae4; `threadSysInit`(`gameFrameReady` 0x12db70, 2); `threadSetSwitchHook`(`gameThreadSwitchHook` 0x12dab0, obj+0x4a8: a perf-meter mark
  per thread through 0x133610, names 0x2f3810 `Th_Ctrl`.., colours 0x2f3830 by slot % 6);
  **`threadRunMain`(`gameMainThread` 0x12db00)**, which blocks until the game ends; then
  `threadSysShutdown` 0x130e50, then the teardown (batch 005): `memcardLibEnd` 0x2acec0 (thunk;
  `sceMcEnd` 0x11fe08), 0x151280 (empty), `soundShutdown` 0x148540, deletes the global object
  (members at +0x510, +0x4a8, +0x46c, +0x2f4, +0x17c, +4; vptr stores above), then
  `gfxReleaseRenderTargets` 0x12e3a0, `padSysShutdown` 0x130070 (`scePadPortClose` 0x11ea80 per open
  port), `rxTerm` 0x152750 (unhooks SPK.ROM, frees its index, `rxSysTerm` 0x101e98),
  `shutdownGlobalHeap` 0x1045a0, `__destroy_global_chain`.
- **The main loop** is `gameMainThread` 0x12db00 (thread slot 0): starts slot 1 on 0x231bb0, then
  forever: 0x151270 (empty), `frameSyncWait` (returns the vsyncs this frame took),
  `GameSystem::flipFrame` 0x12cdf0(obj, vsyncs) ("The global object" below), `padSysUpdate` 0x12ffa0, `soundUpdate` 0x148500,
  `threadYield`(1), `gsEndFrameSync` 0x12cd20 (sceGsSyncPath with a GS / DMA reset on timeout). The
  2 passed to `threadSysInit` is the frame interval: 30 fps unless the frameReady callback `gameFrameReady`
  0x12db70 says otherwise (on the frameSync record: ready when requested and count >= interval;
  calls 0x12e0e0(ready, ...) either way).
- **`gameInit`'s subsystems** (batch 003): `rxInit` 0x1527a0 (SCE's rx runtime: thread priorities
  relative to the caller, boot path from argv[0], devices, `spkRomLoad` 0x152870 = all of `SPK.ROM`
  into the heap, magic `SLPK`, its entry table relocated), `padSysInit` 0x130130 (libpad, two
  0x180-byte port records at 0x4aed00, 16 input listeners at 0x4af000 called through vptr+0x120 slot
  0 each frame), `gsClearVram` 0x12d770, `gfxInitRenderTargets` 0x12e3f0 (256x256 and 512x512
  offscreen draw envs, VRAM managers at 0x31ad9c / 0x31ada0 / 0x31ad98), `soundInit` 0x1486e0(isPal)
  (SDPR2 driver, PCM / ADPCM streams, 50 / 60 Hz tick), `rcnt1Start` 0x12f260 (RCNT1 on H-blank),
  `getLocalDateTime` 0x23de20 (RTC -> local BCD date, seeds the RNG), `systemDataInit` 0x23bb50
  (persistent state 0x598b30, options 0x599000 via `getGameOptions` 0x23baf0 / 
  `GameOptions::setDefaults` 0x23be70), `GameSystem::GameSystem` 0x12d0d0, 0x151290 (empty),
  `memcardInit` 0x2a9c80 (`memcardLibInit` 0x2acf00: `sceMcInit` 0x11fb70 once, flag gp-0x79e0).
- **rx boot** (batch 005, names from RosePan): `rxInit` 0x1527a0 is the game's rx user layer (not in
  RoR's rx; it sits between `rxSysTermUser` and `rxSysThreadCallback`): `rxThSetPrioIndex`,
  `rxSysSetBootPath` 0x102138, `rxSysInit` 0x101288, `rxSysSetMemFunc0..3` 0x100350.., `rxThSetInterval`(4),
  `rxFileCdSetLayerMode`(0) 0x157d70, `rxSysInitFileDevice`(1), `spkRomLoad`, `rxSysSetDeviceParam`.
  **SPK.ROM** is a name index over the ROM archives: `spkRomLoad` reads it whole (`rxSysStreamOpen`,
  `rxFileSeek` / `rxFileRead` / `rxFileClose`), relocates its 16-byte entries `{name, archive name,
  offset, size | bit 31 = DVD layer 1}` (header `SLPK`, +8 count, +0xc string base, +0x10 relocated
  flag) and installs three hooks: `spkLocateFile` 0x1529d0 as the CD locate hook
  (`rxFileCdSetFindFunc` 0x157c40, RoR's rx word for word; batch 005 read it as a FS2-only
  `rxFileCdSetLocateHook`; the previous hook is kept in gp-0x7a88 for
  `ROM.*` / `ROMD.*` themselves), `spkLoadFile` 0x152e20 (`rxFileSetLoadHook` 0x155700) and
  `spkGetFileSize` 0x152c90 (`rxFileSetSizeHook` 0x155748). A name is binary-searched case-folded
  (`toupper` 0x12a010) and resolved to its archive's lsn + offset >> 11, switching the DVD layer
  (`ROMD.008`..`010` are on layer 1). The host port's file layer can do the same lookup over the
  disc files.
- Named (batch 001): crt0 `entry` -> `_InitSys` 0x140e58 (libkernl, nine init calls, named in
  batches 014-015: `supplement_crt0` (the SceKernelLibc / SceKernelLibcEh semaphores),
  `InitSystemCallTableAddress`, `InitAlarm`, `InitTimer(2)`, `StartTimerSystemTime`, `InitThread`,
  `InitExecPS2`, `InitTLBFunctions`, `sceSifSetRebootNotifyBuffer`) -> `FlushCache`
  -> `main` 0x12d910 -> `exit` 0x123538 (newlib: `_impure_ptr` 0x2f3398 `->_atexit` +0x148, then
  `__cleanup` +0x3c) -> `_exit` 0x100218 (crt0: `j Exit`) -> `Exit` 0x141258 -> `_Exit` syscall.
  0x100220 (`addiu v1,zero,0x23; syscall`) is crt0's `_root`, the thread entry SetupThread gets in t0
  (ExitThread; a function since batch 010).
- **rx memory hooks** (batch 010): `rxInit` passes `rxSysSetMemFunc0` / `1` `gameRxMemAlloc` 0x1530b0
  (`Heap::alloc` on the global heap 0x329970, align 0x40), `rxSysSetMemFunc2` `gameRxMemRealloc`
  0x153090 and `rxSysSetMemFunc3` `gameRxMemFree` 0x153080 (both pass the caller's ra in v0, a heap
  debug tag); a 0 argument installs `rxSysNoMemFunc` 0x100328 ("rxSys:No Memory Allocate Func !").
  On PC these four hooks are the host allocator.
- `main` opens with `__init_cpp` 0x100280 -> `__call_static_initializers` 0x105280 (the table, then
  0x106930 over an empty range 0x31a830..0x31a830: returns 0) and ends with `__destroy_global_chain`
  0x1051f0 (thunk 0x100270). Before that, 0x1045a0 tears down the global heap 0x329970 (DeleteSema,
  then `_free_r` of its block).

- **The global object: `GameSystem`** (batch 004, 2026-10-31; class name provisional), 0x540 bytes,
  `new`ed by `gameInit` into gp-0x7aec, ctor 0x12d0d0, deleted by `main` (member dtors in reverse):

  | offset | member | class (vtable) | ctor / dtor |
  |---|---|---|---|
  | +0 | debug meter mode (0 off, 1 detail) | int | -- |
  | +4 | pad, port 0 | `GamePad` (0x311020) | `Pad::Pad` 0x12fcf0 / `Pad::~Pad` 0x12fa90 |
  | +0x17c | pad, port 1 | `GamePad` (0x311020) | same |
  | +0x2f4 | pad, port 1 | `MaskedGamePad` (0x311030) | same |
  | +0x46c | packet manager (2 x 0xc0000 buffers 0x32e280 / 0x3ee280, 14 lists) | `GamePacketManager` (0x311040) : rena `Vif1PktManage` (0x31a808) | 0x12dee0 / 0x12de80 |
  | +0x4a8 | debug CPU / heap meter (stripped drawing) | `PerfMeter` : `RcntTimer` | 0x1336e0 / 0x133630 |
  | +0x510 | the root game clock (12 ticks per 2-vsync frame) | `MasterClock` : `Clock` | 0x135390 / 0x135330 |

  - **Pads** (0x178 bytes, vptr at **+0x120**, after the data): `{+0 input frame read, +4 port, +8
    listening, +0xc terminal mode, +0x10 previous / +0x14 held / +0x18 pressed / +0x1c released
    (libpad words inverted: L2 0x1, R2 0x2, L1 0x4, R1 0x8, ... select 0x100, R3 0x400), +0x24 analog
    bytes, +0x28..+0x34 stick floats, +0x38 25 pressure bytes, +0x54 25 x {7, 9, 0}}`; `GamePad` adds
    normalised sticks / lengths / angles +0x124..+0x150 (`GamePad::computeSticks` 0x12d2c0), a
    `ButtonMap` +0x154 (`ButtonMap::setDefaults` 0x23bd00) and +0x174 = the options' button map
    (getGameOptions()+0x24). Each vtable has **one slot, the per-frame update** that `padSysUpdate`
    calls through the 16 listeners 0x4af000 (`Pad::registerListener` 0x12fed0): `Pad` 0x130060 (thunk to
    `Pad::update` 0x12f300), `GamePad` 0x12dc60 (update + computeSticks), `MaskedGamePad` 0x12dcb0
    (the same, then `Pad::clear` 0x12f960 while L1+L2 are held). The slot functions were created and named at
    bootstrap 4a (batch 009; 0x130060 stays a thunk). **Reconstructed 2026-11-02 in
    `src/game/gamesystem.cpp` (TU 1, batch 022)**: the hierarchy is `MaskedGamePad : GamePad : Pad`
    (the destructors store 0x311030, then 0x311020, then call `Pad::~Pad`); Pad's own vtable is
    0x311050, its EE size 0x124 (data to +0x11c, vptr +0x120: ctor clears +0x10..+0x23, sets the
    analog bytes +0x24..+0x27 to 0x80, `{7, 9, 0}` x 25 at +0x54). `GamePad` (0x178) is
    `{+0x124 dir[4] (sticks squared to a circle, y up), +0x134 angle[4] (atan2(y, x), atan2(-x, y)
    per stick), +0x144 length[4] (each stick's twice), +0x154 ButtonMap (8 ints, defaults 6 5 4 7 3 1 2
    0), +0x174 options map}`; its constructor is inline (three copies in `GameSystem::GameSystem`), and
    0x12d5a0 / 0x12d540 are the out-of-line default ctor (`Pad::Pad(bool)` 0x12fb10 = port 0) and
    destructor. `GamePad::read` 0x12d4f0 has the slot's body word for word (non-virtual caller
    0x239d50). GameSystem: port 0, port 1, port 1 masked; R3 pressed on port 1 dumps the meter groups.
  - **The pad layer, `src/game/pad.cpp` (TU 2, batch 023, 2026-11-02; 0x12f290..0x130a20)**:
    `PadPort` x2 at bss 0x4aed00 (0x180 each: +0 scePadPortOpen's 256-byte DMA buffer, +0x100 this
    frame's report, +0x120 last frame's, +0x140 terminal mode, +0x144 detection phase, +0x148 act
    align[6], +0x150 act direct[6], +0x156 motor requests[2], +0x158 one-shot flag, +0x159 opened,
    +0x15c port, +0x17f read ok), the 16 listeners at bss 0x4af000 (`Pad::registerListener`
    0x12fed0; the member that tracks +8 is `Pad::setListening` 0x12fa30, inline in the ctors / dtor),
    `gPadSysOpen` 0x31adb0 and the pad frame `gPadFrame` 0x31adb4 (gp-0x7ac0 / -0x7abc; nothing
    outside the TU touches any of them). `PadPort::read` 0x130230 is a detection state machine on
    libpad (phases 0/1 detect, 0x28..0x2a digital -> analog, 0x46..0x51 lock analog, pressure mode,
    act align, 99 read): a DualShock 2 ends as **terminal mode 8** (the game's code, pressure mode
    entered), a DualShock 1 as 7, no motors 9. **`Pad::update` reads buttons, sticks and pressure
    only for mode 8**: any other pad is "connected" with nothing held (the host libpad must report
    a DualShock 2 with pressure mode). Stick bytes are libpad's order **right x, right y, left x,
    left y** (`m_stick[0..1]` = right: GamePad's dir / angle / length comments were swapped and are
    fixed); dead zone 0x58..0xa7, (b - edge) / 88; each direction is a virtual button bit
    (`Pad::readSticks` 0x12f6b0, table 0x2f3870: 16..19 left stick up / right / down / left, 20..23
    right stick) with pressure |v| * 255; libpad's 12 pressure bytes go to their buttons through
    table 0x2f3890; held buttons without one read pressure 1. `Pad::updateRepeat` 0x12f600 fills
    +0x20 (pressed, or held for 7 frames then every 2). `Pad::setVibration` 0x12f290(motor, value)
    (mode 8 only; small motor 0 on / off, large 1 0..255) is cleared after each send (one-shot), so
    callers (0x199130) must repeat it every frame.
  - **Frame flip and GS recovery** (batch 022): the frame-ready callback `gameFrameReady` calls
    `gfxFlipDisplay` 0x12e0e0(ready, kickPacket, putDrawEnv) every vsync: on a ready frame it writes
    PMODE / SMODE2 / DISPFB1/2 / DISPLAY1/2 / BGCOLOR from the 0x540-byte display record 0x4ae280[gp-0x7acc]
    (resetting the GS on a mode change), sends the record's 0x3a-qword packet (+0x180) and puts its
    draw env (+0x80) as the flags allow: `gGfxKickDisplayPacket` 0x31a894 (sdata 1; 0 for one frame
    after a GS timeout reset in `GameSystem::endFrameSync`, the renamed `gsEndFrameSync`, a member
    that ignores `this`), `gGfxPutDrawEnv` 0x31a890 (sdata 1), `gGfxFlipNoPackets` 0x31ad94 (both off).
    `gDmacBusError` 0x31ad90 is set by `dmacBusErrorHandler` and makes the next end of frame reset
    GS path / graph / DMA.
  - **Scene controller** `gameSceneThread` 0x231bb0 (thread slot 1): three init calls, then forever
    a scene from the 0x18-byte table 0x2fabd4 (index gp-0x7a34; Heap::alloc'ed stack) in slot 2,
    yields until it exits. Thread slot names 0x2f3810: `Th Ctrl`, `Th Main`, `Th 02`, `Th Game`,
    `Th 04`, `Th Extra`, `Th 06`, `Th Effect`.
  - **RNG and date** (batch 022): `gameInit` seeds newlib `srand` 0x126f78 and the game's
    `rndInit` 0x104860 (97-word table 0x3299c0, LCG x * 0x5d588b65 + 1) with second + minute + hour +
    day + month of `getLocalDateTime`'s 8-byte BCD date `{flags 0x10 (0x11 clock error), second,
    minute, hour, day, month, u16 year 0x20yy}`. `atan2f` 0x119b88 -> fdlibm `__ieee754_atan2f`
    0x1173e0 (MSL math in the run after libipu). Empty hooks `debugSysInit` / `Update` / `Shutdown`
    0x151290 / 0x151270 / 0x151280 and the meter's `PerfMeter::addBar` 0x133610 (value, ABGR colour,
    group, label), `drawBars` 0x133600, `drawDetail` 0x132f20 are retail-stripped.
  - **rena `Vif1PktManage`** (0x30 bytes, vptr at +0x2c): `{+0 write ptr, +4 buffer base, +8 chain
    start, +0xc open list, +0x10 buffer index, +0x14 list vector (count +0x18, data +0x1c), +0x20
    buffer vector {base, size} (count +0x24, data +0x28)}`; lists are 0x34-byte `Vif1PktList`s
    (sceVif1Packet at +0, id +0x20, head +0x24, tail +0x28, vptr +0x30). Per frame: `Open`(id) /
    `Close` while drawing, `Terminate` links the non-empty lists **in id order** into one DMA chain
    with an end tag, `Kick` sends it on VIF1 (CHCR 0x1c5), `Reset` flips to the other buffer. So the
    draw layer order is the list id (0..13; `GameSystem::flipFrame` puts a GS FINISH on 13): the port's
    host draw module keys its layers on it. libvifpk: `sceVif1PkReset` 0x10ac60,
    `sceVif1PkCloseDmaTag` 0x10ac70, `sceVif1PkNext` 0x10ad20.
  - **Clocks**: `Clock` (0x1c) `{+0 state (1 paused), +4 time, +8 delta, +0xc time2, +0x10 scale
    0x100 (8.8), +0x14 parent, +0x18 children PtrList}`, `Clock::advance` 0x1354a0 scales and recurses,
    `Clock::createChild` 0x135210 (deleted with `Clock::~Clock` 0x1351b0 by many systems: 0x151550,
    0x18b330, 0x19bbe0, 0x20aba0, 0x25bc70); `MasterClock` (0x30) adds ticks per frame +0x20, vsync
    total +0x24, interval +0x28, variable-rate flag +0x2c, `MasterClock::update` 0x1352d0(vsyncs).
    `PtrList` `{u16 capacity, u16 count, void **data}`: `add` 0x135860, `next` 0x1357c0.
  - **Debug meter** (`src/game/perfmeter.cpp`, 0x132f20..0x133800, written 2026-11-02):
    `RcntTimer(tickRate 15625, fieldRate 50, fieldsPerFrame 2)` 0x12f200 (usPerTick = 1e6 / tickRate at
    +0x10; the first one sets T1_MODE 0x83, a gp byte -0x7ac4 remembers it) / reset 0x12f190;
    `PerfMeter` beginFrame 0x133620, collectStats 0x133350, resetStats 0x133590, writeCsv 0x132f90
    (`host:la_%d_%d.csv`, min / max / mean x100 per cell), dumpGroups 0x132f30 (its print compiled out);
    its bar / draw calls `addBar` 0x133610 (returns 0, also called by 0x198970 / 0x199400 with group 4,
    the game loop's stage marks, labels 0x303bd8..0x303cb0), `drawBars` 0x133600, `drawDetail` 0x132f20
    are empty in this retail build, so the bar list stays empty; nothing allocates the stat tables
    (+0x5c cells = 0); `m_recording` +0x40 is cleared by 0x17f7c0 / 0x17f890 before writeCsv, no
    setter found yet: in retail the meter records nothing. Heap marks: `Heap::setMark` 0x1037c0, `Heap::markedUsage` 0x103870,
    `Heap::realloc` 0x1038a0.
- rx's debug print is `rxSysPrintf` 0x102328 (was `rxPrintf`; 1 KB format buffer, hook 0x2f0880,
  scePrintf or a 4 KB host log 0x31c580); rena's messages are Shift-JIS.
- **Sound teardown** (batch 005): `soundShutdown` 0x148540 undoes `soundInit` (removes the rx thread
  callback 0x1488f0, deletes the 0x4c-byte sound settings object 0x31addc built by 0x148130, 0x159ca8
  driver term, resets the SPU2 allocator); `spuMemReserve` 0x148a70(size, addr) takes a fixed block
  from the 32-node SPU2 allocator 0x4d08d0 (free span 0x5040..0x1f9000; 0x7000 at 0x1d9000 is
  reserved at init).
- **The game's sound layer 0x144080..0x148d60** (batch 013, 2026-11-02, by reading): every rx / SDPR2
  call goes under one of three semaphores made by `soundInit`: sound 0x31a8ac (gp-0x7fc4; also taken by
  `soundUpdate` and the rx thread callback `soundThreadLock` 0x1488f0), IOP alloc 0x31a8b0, SPU alloc
  0x31a8b4. rx's memory hooks are the game's: IOP `soundIopAllocLow` / `High` / `soundIopFree` 0x148d60 /
  0x148d10 / 0x148cd0 on `sceSifAllocSysMemory` / `sceSifFreeSysMemory` (SDK iopheap 0x13ed90..0x13ef80,
  RoR's sizes), SPU `spuMemAllocLow` / `High` / `spuMemFree` 0x148a20 / 0x1489d0 / 0x148920 on
  `spuMemAlloc` 0x148b60(size, fromTop) (nodes 0x14 {size, addr, end, prev, next}, 64-byte units).
  Classes (no vtable unless said):
  - `SoundStream` 0x144090..0x145d70 (0x60 bytes, ctor 0x145d70, dtor 0x145be0): one rxSndStr stream
    over a bank: {+0 IOP body, +4 its size, +8 not-owned, +0xc body kind 1 file / 2 memory / 3 IOP,
    +0x10 state 0 stopped / 1 playing / 2 paused / 3 stop pending / 4 ended, +0x14 / +0x18 / +0x1c
    stream buffer size / address / owned, +0x20 bank, +0x24 stream, +0x28 request, +0x2c tracks, +0x30
    start offset, +0x34 SPU ring (0x4000 from the top), +0x38 / +0x3c volume L / R, +0x40 fade level
    (0x80), +0x44 / +0x48 / +0x4c / +0x50 / +0x54 fade from / to / frames / counter / end action
    (1 stop, 2 pause), +0x58 channels, +0x5c pan state}. Volume sent = L|R * fade >> 7. Pan: radians
    negated, wrapped, to degrees -> `sdpr2CalcPanDeg`. Game users 0x25b8e0.., 0x290000..0x2931b0,
    0x2b0d60...
  - `SoundSink` (vtable 0x311180, vptr +0xc) with `SoundPcmSink` 0x311140 / `SoundAdpcmSink` 0x3111c0:
    {+0 handle}, slots +8 dtor, +0xc setVolume, +0x10 open, +0x14 close, +0x18 getStatus, +0x1c getFree,
    +0x20 write, +0x24 end, +0x28 / +0x2c set / getUserData, +0x30 addCallback, +0x34 setAttr (PCM:
    empty 0x144080), +0x38 muteIfEnded. `SoundFeeder` 0x145de0..0x1463a0 {+0 state, +4 source (vptr
    +0x10), +8 sink, +0xc fed, +0x10 / +0x14 volume} pumps a source into a sink each frame: the movie
    player's audio (callers 0x1faa90..0x1fc3a0). The five `return 0` functions 0x1461c0..0x146200 are
    slots of vtable 0x3112c0 (class at 0x152590), not of these.
  - `SoundSeBank` 0x146ee0..0x147a80 {+0 state 0 / 1 loading / 2 loaded / 3 freeing, +4 rxSndXvab id,
    +8 IOP, +0xc SPU}: load / poll / unload of an XVAB, plain SE calls (`rxSndSe*`) and panned ones
    (`sdpr2*`; `playPan` picks the pair type 0 or the single type 1).
  - `SoundConfig` 0x147be0..0x148130 (0x4c bytes at 0x31addc): volumes master / BGM / SE / PCM / ADPCM
    +0..+0x10, output mode +0x14 (0 mono?, 1 stereo, 2 surround: `rxSndSetOutputMode` + `sdpr2SetSurround`,
    flags +0x48 / +0x49), reverb per core +0x18..+0x2c / +0x30..+0x44 (`setEffect`, caller 0x2921d0).
  Float -> int angles go through 0x11ba00 / 0x11c180 / 0x11c4e8 (float to double, double divide, double
  to int: CodeWarrior's soft-double runtime, unnamed).

## Modules

The game's own source files (from its strings), what each does.

**The TU map (bootstrap 5, 2026-11-02)**: FS2 names only four of its files (`rxfile.c`,
`filehost.c`, `rena_2d_texture_tim2.cpp` in a message, `rena_pkt_vif1pktmanage.cpp`), so the map is
mostly **pinned ranges** in `data/tus.json` (27 TUs, each with its `why`), from what the naming batches
proved: rx in FS2's link order (`rx/rxsys.c` in two ranges, `rx/rxfile.c` in two around the three
device drivers `rx/filehost.c` (its string) / `rx/filecd.c` / `rx/filehdd.c` (by analogy),
`rx/rxthread.c` in two, `rx/rxsnd.c`, and FS2's own `rx/rxsndut.c` + six `rx/rxsndstr*.c` layers),
SDPR2's four files by its four message copies (`sdpr2/sdpr2.c`, `sdpr2pair.c`, `sdpr2single.c`,
`sdpr2drv.c`), and the boot's game code (`game/gamesystem.cpp`, `gamepacket.cpp`, `gfxtarget.cpp`,
`rcnttimer.cpp`, `pad.cpp`, `thread.cpp`, `perfmeter.cpp`, `spkrom.cpp`, `sound/sound.cpp`). rx and
SDPR2 keys end in `.c` after FS2's own `rxfile.c` / `filehost.c`; game code is C++ (`.cpp`, like
rena's). Every game file name is a guess (no strings): sessions rename or re-cut with `tus.py
define`. `export.py` now also takes a source path that leads a message (`name.cpp: ...`) and class
anchors put `Vif1PktManage`'s members under `rena_pkt_vif1pktmanage.cpp` (0x2eaff0..; rena sits at
the end of .text, ~0x2e2000..0x2ec8e0). Result: 1,031 of 6,670 game functions in 29 files, the rest
`(unassigned)` (0x171300.. onward is almost all unread game code).

## Classes

Name, vtable, vptr offset, size, file. `python rtti.py` (CodeWarrior variant, bootstrap 4, 2026-10-31)
finds **490 vtables** in .data 0x310fe0..~0x31a820: 475 installed by code (`lui/addiu` + `sw`), 15 only
by shape (marked; those in 0x31a0e0..0x31a7a0 may be other function tables). Names: 4 from RTTI
(`std::exception` 0x310fe0, `std::length_error` 0x310ff0, `std::out_of_range` 0x311000,
`std::logic_error` 0x311010: MSL), the rest `Vt_<addr>` until named in `data/class_names.json`.

- Layout confirmed: `{type_info*, this_offset, fn...}`; the vptr points at the table start, slot i at
  vptr+8+4i (0x131dd4: `lw t9,0x98(s1); lw t9,0x48(t9); jalr` = slot 16 of the table at +0x98).
  type_info is `{const char *name, bases*}`, bases a list `{type_info*, offset}` ended by 0
  (0x2fe628 logic_error -> 0x2fe618 `{0x2fe400 exception, 0}`).
- Tables are often 4-byte aligned, not 8 (0x311314, 0x3133f4), packed with zero padding.
- **One object, several vptrs**: ctor 0x171a30 installs 0x3112f0 at +0, 0x311314 at +0x1c, 0x31132c
  at +0xf4, 0x31153c at +0x204; every secondary table has this_offset 0 (embedded member objects with
  their own vtables, or CodeWarrior records no offset there: open-items.md).
- **The big hierarchy**: ~40 tables of 130-190 slots, all with the vptr at +0xf4 (0x31132c,
  0x3115fc, 0x31189c, ... 0x3141ec): the game's main polymorphic family (characters / actors?).
- Slot 0 looks like the destructor: 0x311060 slot 0 = 0x131d80, which installs 0x311060 at +0x98 and
  later 0x3110b0 (derived-then-base, a destructor's order).

## Static initialisers

- 0x100280 (crt0 side) runs the table **0x310fb0..0x310fd4**: 9 initialisers 0x310a00, 0x310a20,
  0x310a70, 0x310ac0, 0x310b10, 0x310b60, 0x310ec0, 0x310f10, 0x310f60 -- the second code island is
  CodeWarrior's static-initialiser code. Ghidra has functions for all 9 since 2026-11-02 (6 created at bootstrap 4a).
- 0x310a00 tail-calls **0x104c60(0x537d80, 0x1772f0, 0, 0x38, 0x87)**: array, ctor, dtor, element
  size, count -- MSL's `__construct_array` (named, batch 001).

## Key functions and globals

- **Allocation**: one global heap object at 0x329970 (`{sema id, block, ..., +0x10 head, +0x14
  tail, +0x18 count}`; blocks have a 0x10-byte header before the user pointer). `Heap::alloc`
  0x103c50 (heap, size, align) / `Heap::free` 0x103aa0 wrap 0x103cd0 / 0x103b10 in WaitSema /
  SignalSema when the sema id >= 0. `operator_new[]` 0x1002f0 and `operator_new` 0x100310 pass align
  16; `operator_delete[]` 0x1002b0, `operator_delete` 0x1002c0; 0x1002d0 passes a caller's
  alignment (an aligned overload, unnamed). Which of the two plain-size allocators is `new[]` follows
  from MSL's string rep 0x106b80 (char buffer -> 0x1002b0, int refcount -> 0x1002c0) and the same
  order for new: verify at the first TU that allocates. Class name `Heap` is provisional (could be
  MSL's pool allocator).
- **CodeWarrior / MSL runtime** (all lib-context): `__construct_array` 0x104c60,
  `__register_global_object` 0x105250 (obj, dtor, 12-byte entry) and `__destroy_global_chain`
  0x1051f0 on the chain head 0x329b48 (dtor called with -1), `__throw` 0x106830 (MSL exception-match
  type string, object, dtor), `std::terminate` 0x1051a0 through the handler word 0x2f1060 (default
  `dthandler` 0x1051d0 -> `abort` 0x121ef0). Deleting destructors take `(this, short flag)` and free
  `this` when flag > 0; -1 means destroy only.
- **MSL exception classes**: `std::exception` (vtable 0x310fe0, dtor 0x104ee0, `what` 0x104f30),
  `std::logic_error` (0x311010, dtor 0x1070f0, `what` 0x1073d0, message string at +4),
  `std::length_error` (0x310ff0, dtor 0x107180), `std::out_of_range` (0x311000, dtor 0x107220).
  0x104f30 and 0x1073d0 are among the missing function starts.
- **Thread system** (batch 002, game code 0x130a30..0x131930): a cooperative scheduler over **8
  thread slots** at 0x4c7840, 0x24 bytes each: `{+0 tid, +4 state (0 free, 1 waiting N ticks, 2 start
  pending; bit 7 masked), +8 ticks, +0xc arg, +0x10 cleanup fn, +0x14 result, +0x18 stack size?,
  +0x1c stack base (canary 0x572a8b4c), +0x20 own-stack flag}`; default stacks per slot in the table
  0x2f38c0 (8 bytes each). gp-0x7fd8 is the current slot. `threadScheduler` 0x131290 (prio 0x40)
  wakes on the vblank callback `threadVSyncHandler` 0x131840 (signals semaphore gp-0x7fcc) (via `sceGsSyncVCallback`) and steps slots round-robin;
  `threadSlotCreate` 0x130c90, `threadYield` 0x130bc0(ticks) (canary check), `threadRunMain`
  0x130e00, `threadSetSwitchHook` 0x130a30 (post-switch; gp-0x7aa4 / -0x7a9c is a pre-switch hook
  nothing set yet). Frame pacing: `frameSyncInit` 0x131870 / `frameSyncThread` 0x131930 (prio 0x10,
  record 0x4c8160 `{+0 requested, +4 vsync count, +8 interval}`) / `frameSyncWait` 0x131260.
  `jobWorkerThread` 0x131720 (prio 0x50) runs a linked job list 0x4c8b38..0x4c8b3c (records
  `{busy, ?, fn, arg, prev, next}`; a job returning non-zero is re-queued). On PC this whole layer is
  a host replacement candidate (one OS thread or fibers), decided at the platform-layer step.
- **SDK** named so far: libkernl `StartThread` 0x137c20, `DIntr` 0x140ba0 / `EIntr` 0x140bf0,
  `DisableIntc` 0x1375c0 / `EnableIntc` 0x137628, `scePrintf` 0x1397c8; libgraph `sceGsGetGParam`
  0x107878 (block 0x2f1090), `sceGsSyncV` 0x108278, `sceGsSyncPath` 0x108310, `sceGsSyncVCallback`
  0x109270, `sceGsResetGraph` 0x1076e8, `sceGsResetPath` 0x107888, `sceGsSetDefDrawEnv` 0x107ea0,
  `sceGsSetDefClear` 0x108088; libdma `sceDmaGetChan` 0x109468
  (table 0x2f10e0), `sceDmaReset` 0x109490, `sceDmaSync` 0x109a38, `sceDmaSend` 0x109798.
  **libgraph / libdma / libdev 0x1076e8..0x10ac60 (batch 018, `rxalign.py 1076e8 10ac60 258f78
  25d4c0`, 64 of 67 size matches; 7 merged starts created)**: libgraph in FS2's member order
  `sceGsResetGraph`, `sceGsGetGParam`, `sceGsResetPath`, `checkModelVersion` 0x1078f0,
  `_GetGsDxDyOffset`, `sceGsSetDefDispEnv` 0x1079a0 (PMODE 0x66 / SMODE2 / DISPFB / DISPLAY from
  (psm, w, h, dx, dy); not a DBuff setup as noted before), `sceGsPutDispEnv` 0x107d18,
  `sceGszbufaddr` 0x107dd8 (read), `sceGsSetDefDrawEnv`, `sceGsSetDefClear`, `sceGsPutDrawEnv`
  0x108190 (GIF ch.2 send, read), `sceGsSyncV`, `sceGsSyncPath`, `sceGsSetDefTexEnv` 0x108628
  (12 args -> TEX0_1 / TEX1_1 / CLAMP_1, read; not in RoR), `sceGsSetDefLoadImage` ..
  `sceGsExecStoreImage` 0x108be0, `sceGsSyncVCallback`, `WaitDma` / `WatchDma` / `CheckAddress` /
  `memclr` 0x109430; FS2 lacks RoR's `sceGsSetDefDBuff` / `sceGsSwapDBuff` / `sceGsSetHalfOffset`
  in this run (the game builds its own double buffer). libdma `sceDmaGetChan` .. `sceDmaRestart`
  0x109b40 in RoR's order (0x109570, 0x224, is RoR's unnamed `FUN_0025b070`: read it). libdev
  `sceDevVu0CheckBusy` 0x109b58 .. `sceDevVif1GetCnd` 0x10a9c8 (0x294 against RoR's 0x234, read;
  no `sceDevVif1GetFifo` after it), then libvifpk 0x10ac60. RoR's libdev `FUN_` slots stay unnamed
  here too (0x109b88, 0x109ff0..0x10a060, 0x10a0c0..0x10a400, 0x10a570..0x10a648, 0x10a8a0..
  0x10a978: Vu1 / Vif0 / Vif1 Pause / Continue / Put / GetFifo and kin, read them at the TU).
  **libcdvd 0x11c6b0..0x11e440 (batch 019, `rxalign.py 11b800 11e440 24b400 24d220`, 29 of 29 size
  matches; `_sceCdSC` 0x11d430 split off `_sceCd_scmd_prechk`, as in RoR: a leaf switch on
  0x8001.. after the prechk's `jr ra`, no callers; FF3's list keeps them merged)**: eecdvd.c in RoR's
  order `sceCdCallback` 0x11c6b0, `_sceCd_cd_callback`, `_Cdvd_cbLoop`, `_sceCd_cd_read_intr`,
  `cmd_sem_init`, `cdvd_exit`, `sceCdPOffCallback`, `_sceCd_Poff_Intr`, `PowerOffCB`,
  `sceCdLayerSearchFile` 0x11ccb8, `sceCdSearchFile`, `_sceCd_ncmd_prechk` 0x11cfd8 /
  `sceCdNcmdDiskReady` 0x11d130 (FF3's FF2-symbol names for RoR's placeholders), `sceCdSync`,
  `sceCdSyncS`, `_sceCd_scmd_prechk` 0x11d2d8, `_sceCdSC`, `sceCdInit` 0x11d4e0,
  `sceCdDiskReady_old`, `sceCdDiskReady`, `sceCdRead` 0x11dc00, `_sceCdGetDiskType` 0x11dde0 (read:
  S-command RPC 3, -1 on error; Sony's name unknown), `sceCdGetDiskType`, `sceCdGetError`,
  `sceCdStatus`, 0x11dff0 (unnamed: S-command RPC 0x25 with two words, no callers),
  `sceCdReadDvdDualInfo`, `sceCdMmode`, `sceCdChgSys` 0x11e268 (its error string), `sceCdReadClock`,
  `sceCdTrayReq` 0x11e350; then libpad `scePadInit` 0x11e440. S-command RPCs go through client
  0x2f2f00, send buffer 0x2f29c0, receive buffer 0x2f2580 (read uncached), semaphore 0x2f13ac; the
  RPC numbers follow ps2sdk's CD_SCMD order (1 ReadClock, 4 GetError, 5 TrayReq, 0xc Status, 0x27
  DvdDualInfo). Before it, 0x11b800..0x11c6b0 (18 unnamed functions, a small call cluster, 0x11ba00
  / 0x11bef8 / 0x11c3e0 called ~40 times each) is not libcdvd: read it at its TU. **libmc
  0x11fb70..0x121b80 (batch 020, `rxalign.py 11fb70 121470 24d220 24eb28`, 20 of 20 size matches;
  FF3's FF2-symbol list agrees)**: a newer libmc than RoR's (`sceMcInit` 0x298 vs 0x228; RoR's
  0x124 "sceMcEnd" is FF2's `sceMcChangeThreadPriority`, not linked in FS2; FS2's own `sceMcEnd`
  0x11fe08 is 0xac). `sceMcInit` 0x11fb70, `sceMcEnd`, `mceSignalSema` 0x11feb8 (the end
  callback of every libmc rpc: iSignalSema 0x2f2fa0), `sceMcGetSlotMax`, `sceMcOpen`, `sceMcMkdir`,
  `sceMcClose`, `sceMcSeek` 0x120268 (rpc 4) + `mceReadFixAlign` 0x120348 (sceMcRead's end callback;
  FF2 / FF3 / RoR merge the two into a 0x190 `sceMcSeek`), `sceMcRead`, `sceMcWrite`, `sceMcSync`,
  `mceGetInfoApdx`, `sceMcGetInfo`, `sceMcUdCheckNewCard`, `sceMcGetDir`, `mceStorePwd` 0x120ae8 /
  `sceMcChdir` 0x120b78 (split), `sceMcFormat`, `sceMcDelete`, `sceMcFlush`, `sceMcSetFileInfo`,
  `sceMcRename`, `sceMcUnformat`, `sceMcGetEntSpace` 0x121360; then FS2-only **read fast**
  0x121470..0x121b80 (by reading): `mceReadFastThread` (thread 'sceMc_read_fast_thread', started by
  `sceMcInit` through `mceStartReadFastThread` 0x1214d8, rpc server sid 0x80000400 =
  `mceReadFastRpcServer` 0x1218b0), page ECC `mceCalcEcc` 0x121560 / `mceCorrectEcc` /
  `mceCorrectPage` / `mceReadFastCopy`, `sceMcReadFast` 0x121970 (rpc 0x16), `sceMcWriteFast`
  0x121a80 (rpc 0x1b, name by analogy). libmc's rpc client 0x32a340, send buffer 0x32a3c0, reply
  0x32b900, call semaphore 0x2f2f9c, last command 0x2f2f98. 0x121b80 (0x370, unnamed) maps (command,
  libmc result) to 0x8108xxxx / 0x8109xxxx error codes, called only by the game's memory-card layer
  0x2ac710..0x2acbb0: libmc tail or game code, read it there. **SIF cmd
  / rpc + fileio head 0x139bc0..0x13b7e4 (batch 010, RoR's libkernl in the same order and sizes,
  `rxalign.py 139bc0 13b600 260508 262300`)**: `sceSifExitCmd` .. `_sceSifCmdIntrHdlr` 0x139ea8,
  `sceSifInitRpc` 0x13a0a0 (registers `_request_end` / `_request_bind` / `_request_call` 0x13a998 /
  `_request_rdata` for SIF cmds 0x80000008 / 9 / a / c through `sceSifAddCmdHandler` 0x139c28),
  `sceSifBindRpc` 0x13a848 / `sceSifCallRpc` 0x13aa28 (packet pool 0x4cb380), the server side
  `sceSifRemoveRpc` .. `sceSifRpcLoop` 0x13b118, then fileio `_sceFsIobSemaMK`, `new_iob`,
  `_sceFs_Rcv_Intr` 0x13b430. Unnamed there: 0x13ac68 (0x94) + 0x13ad00 (0xd0), where RoR has one
  `sceSifSetRpcQueue` (0x168): likely `sceSifRegisterRpc` + `sceSifSetRpcQueue` (RoR's Ghidra merged
  them; read them). **fileio (filestub.c) 0x13b1c8..0x13ed90 (batch 017, `rxalign.py 13b7e8 13ed90
  262130 2656d8`, 37 of 37 one to one; every entry point's `SceStdio*Sema` string agrees)**:
  `_sceFsCountOpenIob` 0x13b1c8 and `_sceFsCallRpc` 0x13b2d0 (names by reading: `sceSifCallRpc`
  retried with 1..127 ms back-off, 100 tries), `get_iob` 0x13b3c0 (iob table 0x4cc900, 32 x 16
  bytes), `_sceFsSemInit` 0x13b7e8, `_sceFsWaitS`, `scePowerOffHandler`, `_sceFs_Poff_Intr`,
  `sceFsInit` 0x13b968 (SIF cmds 0x80000011 / 0x80000013, binds rpc 0x80000001 into client
  0x4ccb00, rpc 0xff -> version word 0x4ccb28, init flag 0x2f39fc), `_fs_version` 0x13bb70 (FF3 /
  FF2), `sceFsReset` 0x13bc00, then `sceOpen` 0x13be68 .. `sceReadlink` 0x13ebb0 in RoR's order
  with `sceLseek64` 0x13e4d0 (FF3 / FF2). Unnamed: 0x13bc38 (rpc 0xfe: a size, multiple of 16 KB
  below 1 MB, and a count 1..32) and 0x13bd78 (rpc 0xfd, one word), both refusing with -16 while
  files are open; no callers, unnamed in RoR and FF3 too. **libkernl tail 0x13ef80..0x142dd0 (batches
  014-015, `rxalign.py 13ef80 142dd0 2658c8 269710`, 99 of 100 size matches)**: iopheap tail and
  loadfile (`sceSifLoadIopHeap` 0x13efa0 .. `sceSifRebootIop` 0x140210), TLB (`SetTLBHandler`
  0x140320, `InitTLBFunctions` 0x140470, syscall stubs 0x55..0x59 at 0x140538..0x1405e8,
  `InitTLB` 0x1405f8, `_kTLBException` 0x140840), initsys (`supplement_crt0` 0x140c08,
  `InitSystemCallTableAddress` 0x140d48), libosd (`PatchIsNeeded`, `InitExecPS2` 0x140f80), exit
  (`SetArg`, `ExecPS2` 0x1411a0, `LoadExecPS2`, `ExecOSD`; 0x141198 is an 8-byte `j InitTLB` stub
  every exit.c entry calls, left as Ghidra's thunk), timer (`InitTimer` 0x1414b0 installs
  `cbTimerHandler` 0x141a50 on INTC 11 = TIM2; `SetT2_COUNT` / `_MODE` / `_COMP`; the
  `*TimerCounter` / `*TimerSystemTime` API to 0x142748 `TimerFreq2BusClock`), alarm (`InitAlarm`
  0x141338 copies 0x740 bytes of 0x2f4860 to kernel 0x80076000 and 0x28 of 0x2f4fa0 to 0x82000:
  **the first island is kernel code the game installs**, item (c); `SetTimerAlarm` 0x142830), and
  reboot notify to 0x142dd0. Each file has its own syscall 0x5a `Copy` stub + `kCopy` (named
  `Copy_<file>` / `kCopy_<file>` after RoR). `RFU086_WaitEvnetFlag` 0x140558 is the plugin's name
  for the syscall 0x56 stub `SetTLBEntry` 0x140568 calls (kept); `InitThread` 0x1378b8 /
  `topThread` 0x1377e0 open RoR's libthread run (iEnableDmac.. before, iWakeupThread.. after).
  **libkernl front 0x1366c0..0x139bc0 (batch 016; `rxalign.py 136f60 139bc0 25dd60 260508`, 54 of
  55 size matches)**, in link order: **klib.s** 0x1366c0..0x136f60, 138 syscall stubs of 0x10
  (`addiu v1,N; syscall; jr ra; nop`), syscalls 0x0..0x82 with their negative `i` twins, slot for
  slot RoR's 0x25d4c0..0x25dd60 (every slot named in both agrees); Ghidra only had the called
  ones, the other 71 were created and take RoR's names (RoR merged 0x55..0x59, 0x5b, 0x74: named
  after ps2sdk, `RFU085_iClearEventFlag` .. `RFU091_GetEntryAddress`, `SetSyscall`; the TLB file
  0x140538 reuses 0x55..0x59 for its own stubs). Before it, 0x135a90..0x1366bc is VU0 macro-mode
  math (lqc2 / sqc2 and COP2 macro ops; not libkernl: rena's or the game's, unnamed). **glue.c**
  `sceResetttyinit` 0x136f60, `VSync`, `VSync2`, `write` / `read` (tty), `open`, two -1 stubs
  0x1371c8 / 0x1371d0 (unnamed: close / ioctl?), `lseek`, `sbrk` 0x1371e0, `isatty`, `fstat`,
  `getpid`, `kill`, `stat`, `unlink`; **cache** (FS2-only, not in RoR or FF3) `_SyncDCache` /
  `SyncDCache` 0x1373d8 / `_InvalidDCache` / `InvalidDCache` 0x137518 (index walk of the 4 KB
  D-cache, ps2sdk names); **intr.c** `QueryIntrContext` 0x1375b0, `Disable` / `EnableIntc`,
  `Disable` / `EnableDmac`, the four `i` variants; **thread.c** `topThread`, `InitThread`,
  `iWakeupThread`, `iRotateThreadReadyQueue`, `iSuspendThread`, `DelayThread` 0x137b58,
  `StartThread`, `_DelayThreadHandler`; **deci2.c** `sceDeci2Open` 0x137dc0 .. `kputs` 0x137fd0
  (RoR links it elsewhere); **tty.c** `QueueInit` .. `sceTtyHandler` 0x1380a0, `sceTtyWrite`,
  `sceTtyRead`, `sceTtyInit` 0x138480; **kprintf.c** `kputchar`, `deci2Putchar`, `serialPutchar`,
  `ftoi`, `printfloat`, the formatter `_kvprintf` 0x1388c8 (putc callback, context, max length,
  fmt, va_list; name by reading), its callbacks `bufferPutchar` 0x1395e0 / `deci2Output` /
  `serialOutput`, `kprintfSub`, `sceVsnprintf` 0x1396d8, `sceSnprintf`, `deci2Vprintf` (no callers),
  `scePrintf` 0x1397c8 (deci2), `kprintf` 0x139848 (serial; topThread's internal error, the TTY
  errors, InitTLB32MB); then sifcmd.c `_set_sreg`, `_change_addr`, `sceSifGetSreg`,
  `sceSifSetSreg` 0x139910, `sceSifGetDataTable` 0x139930, `sceSifInitCmd` 0x139940 (FF3's FF2
  symbol). glue's `open` / `stat` / `unlink` / `sbrk` call 0x123528 (errno setter?). **libpad
  0x11e440..0x11fb70 (batch 021, one to one with FF3's 0x283a28..0x285158 by size and order, 14 of
  the names FF2 symbols)**: `scePadInit`, `_scePadCmdHandler` 0x11e590 (SIF cmd 0x80000019, calls
  the callback in 0x2f2f50), `scePadInit2`, `scePadEnd`, `_scePadSendActData` 0x11e780 (SIF DMA of
  the 0x20-byte actuator packet), `scePadPortOpen` 0x11e898, `scePadPortClose` 0x11ea80 (FF3's
  list merged these two), `scePadGetDmaStr`, `scePadRead` 0x11ece8, `scePadGetState`,
  `scePadSetReqState`, `scePadGetReqState`, `scePadInfoAct`, `scePadInfoMode`, `scePadSetMainMode`,
  `scePadSetActDirect`, `scePadSetActAlign`, `scePadSetButtonInfo`, `scePadInfoPressMode`,
  `scePadEnterPressMode` 0x11f6f8, `scePadSetVrefParam`, `scePadGetPortMax`, `scePadGetSlotMax`,
  `scePadGetModVersion`, `scePadSetWarningLevel`, `_scePadSetCallback` 0x11fa18; then libmc. Port
  table 0x32a090 + port * 0x70 + slot * 0x1c (+0 DMA buffer, +4 actuator packet, +8 IOP address,
  +0xc DMA id, +0x10 open); RPC client 0x32a040, buffer 0x32a280 (word 0 the command). The three
  `_scePad*` names are made up (Sony's unknown). `scePadExitPressMode` is not linked. Likely
  `sceCdReadClock` 0x11e290 and `sceScfGetLocalTimefromRTC` 0x264790 (batch 003). rx (names
  from RosePan since batch 005, which renamed batch 003's guesses): `rxThSetPrioIndex` 0x168010,
  `rxThSetInterval` 0x167fc0, `rxThFindCallback` 0x1671e0 / `rxThAddCallback` 0x167220 /
  `rxThRemoveCallback` 0x1672a8, `rxSysSetBootPath` 0x102138, `rxSysInit` 0x101288. libmc
  `sceMcInit` 0x11fb70 / `sceMcEnd` 0x11fe08 (all of libmc: batch 020 above); sifrpc
  `sceSifSetRpcQueue` 0x13ac68 / `sceSifRegisterRpc` 0x13ad00 (batch 020); libc `memcpy` 0x125228
  (batch 020; 0x1252d8 after it likely memmove); MSL `strstr` 0x128a70, `toupper` 0x12a010. Heap setup: `initGlobalHeap` 0x104600 (`_malloc_r` 0x124950 a block for 0x104500 to
  manage) / `shutdownGlobalHeap` 0x1045a0 (`_free_r` 0x123b18).
- **libmpeg / libipu (batch 012, RoR's SDK 3.0 libraries, same sizes; `rxalign.py` with `--lib`)**: FS2 links the archive members in another order than RoR: the MPEG-2 decoder members first, 0x10c218..0x1112f8 (`_motionComp0` 0x10ccf0, `_getRef0`, `_doMC`, `_pictureData0`, `_slice0` 0x10d828, `_motionVectors`, `_flushBuf` 0x10e7f0, `_nextBit` 0x10ead8 (83 callers), `_sceMpegSliceA0` 0x10fea8, the header parsers `_groupOfPicturesHeader` .. `_pictureDisplayExtension` 0x111130; RoR 0x2541b0..0x258f78, after mpeg.c), then mpeg.c / csc / pack / init 0x1112f8..0x116978 (`sceMpegCreate` 0x1112f8, `_getpic`, `sceMpegInit` 0x111ea0 (calls `sceIpuInit`), `sceMpegDelete` .. `sceMpegGetDecodeMode`, `sceMpegSetDefaultPtsGap` 0x112370, `_decPicture`, `_alalc*`, `_lastFrame`, `_ch3dmaCSC` 0x113750 / `_ch4dma` (two code pointers of batch 010), `_id2type`, the PSS demuxer 0x113d40 / 0x1143e8, `sceMpegDemuxPss` 0x116570, `sceMpegAddStrCallback` 0x116590, `_ErrMessage` 0x116768 ('[MPEG ERROR]%s'), `_Error` 0x1168e0; RoR 0x24eb28..0x2541b0), then libipu 0x116978..0x116fb8 (`sceIpuStopDMA` 0x116a68, `sceIpuRestartDMA`, `sceIpuSync`, `sceIpuInit` 0x116d80; RoR 0x269948). Unnamed in RoR too: 0x10c218 (0x244, no RoR pair: before `FUN_002541b0`'s counterpart 0x10c460), 0x111228 (RoR 0x24ea58), the tiny accessors 0x1123a0..0x112400 (mp->sys +0xec, +0xe0, +0xe4, +0xc8 twice, +0xfc swap, +0x100: likely `sceMpegDispWidth` / `Height` / `CenterOff*` and kin; read them at the TU) and the other `FUN_` pairs of the alignment.
- **The game's movie player is FS2's own, not RoR's rxMpeg** (batch 012): RoR's `RxMpeg` / `rxMpeg*` (snd/rxmpeg.cpp, RoR game code) has no counterpart by size (`rxalign.py 1fa800 1fd000 1810f4 1830fc`: 3 chance pairs). FS2's player is a C++ class at ~0x1fa880..~0x1fcbf0 (vtables 0x315f50 / 0x315f80 without type_info): 0x1fc7b0 initialises it (CreateSema, `sceMpegInit` once behind the flag 0x31ae28, 4 x volume 0x80), 0x1fb9d0 opens a stream (rx stream ring 0x152400, `sceMpegCreate`, `sceMpegAddStrCallback` x4, `sceMpegAddCallback`), 0x1fb630 gets pictures (`sceMpegGetPicture`), 0x1fab50 is the decode thread side (WaitSema, `sceMpegIsEnd`); 0x1fbe00 / 0x1fbe20 wrap `sceIpuRestartDMA` / `sceIpuStopDMA` (the libmpeg stop / restart DMA callbacks). Callers: 0x25bd30, 0x25e260 (the movie scenes: `movieEU/ev*.d2b`). Name it at its TU.
- **libc** (Sony's newlib): `strlen` 0x128498, `strcpy` 0x128380 (R5900 quadword scans), `raise`
  0x127a68 / `_raise_r` 0x1278f8, `_impure_ptr` 0x2f3398.

## Disc and data files

Archive formats, file tables, where the data root starts.

## PC platform layer

What was taken from RosePan, what was dropped, the host draw modules and their users, the platform
contract hooks.
