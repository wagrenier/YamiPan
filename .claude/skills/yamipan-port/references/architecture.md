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
MSL C++ runtime 0x104b40..0x1076e8 (from `__destroy_new_array`, batch 031), libgraph / libdma / libdev 0x1076e8..0x10ac60, libvifpk
0x10ac60..0x10c218 (its leaf run after 0x10acc8 unread: libvu0?), libmpeg 0x10c218..0x116978, libipu
..0x116fb8, newlib libm (float) 0x116fb8..0x119be8, libgcc (libgcc2 + fp-bit soft-float) ..0x11c6b0,
libcdvd 0x11c6b0..0x11e440, libpad ..0x11fb70, libmc ..0x121b80, libc (newlib)
0x121ef0..0x12c9b8, libkernl 0x1366c0..0x142dd0 (front, sifcmd / sifrpc, fileio, iopheap, tail),
**libscf 0x2640e8..0x2647d8** (batch 027: linked between game objects, in the middle of the game's
0x171300.. run) and the kernel-patch island 0x2f3a40..0x2f4d20: 931 functions. **Unread runs between them stay game code
until read** (not library by default): 0x121b80 (libmc error mapper?), 0x12c9b8..0x12cd20, the VU0 math
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
| libm + libgcc, batch 025 (2026-11-02) | 51 names by reading (TU `unassigned@00116fb8` resolved as library): **newlib libm, float fdlibm** 0x116fb8..0x119be8 in newlib's member order (`__ieee754_acosf` / `atan2f` / `fmodf` / `logf` / `powf` / `rem_pio2f` / `sqrtf`, `__kernel_cosf` / `rem_pio2f` / `sinf` / `tanf`, double `isinf` / `isnan`, `atanf` `copysignf` `cosf` `fabsf` `floorf` `scalbnf` `sinf` `tanf`, the `_IEEE_LIBM` wrappers `acosf` `atan2f` `fmodf` `logf` `powf` 0x119b70..0x119be8: no `sqrtf` wrapper, the game uses the FPU's sqrt.s); **libgcc** 0x119be8..0x11c6b0: libgcc2 `__muldi3` `__divdi3` `__moddi3` `__udivdi3` `__umoddi3` (`__clz_tab` 0x2ffa00), `__floatdidf` `__floatdisf` `__fixunsdfdi` `__fixdfdi` `__fixunssfdi`, then fp-bit: single-float `__pack_f` / `__unpack_f` / `__make_fp` without NaN / Inf / denormals (the R5900 FPU's format), full double `__pack_d` / `__unpack_d` / `_fpadd_parts` / `__adddf3` `__subdf3` `__muldf3` `__divdf3` `__fpcmp_parts_d` `__cmpdf2` (the only compare entry) `__floatsidf` `__fixdfsi` `__make_dp` `__truncdfsf2` `__fixunsdfsi`, plus `__extendsfdf2` 0x11ba00 | name_ctx.py | lib-context; ledger +51 |

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
  - **Scene controller** (`src/game/scene.cpp`, `gameSceneThread` 0x231bb0, thread slot 1, written
    2026-11-03): `loadInitPack` 0x290840 (`dataEU/packinitEU.pak`, pack object at 0x31ae60),
    `loadGamePack` 0x2afd00 (was `runStartupSequence`; 0x2afd50 in slot 3: `packgameEU.pak` parsed
    into `PackTables` by 0x2bcf10, no screens: `game/stageresult.cpp` in "Modules"), `overlayThreadStart` 0x267780 (slot 6, 0x267830:
    draws the 0x98-byte overlay objects gp-0x7a28 / gp-0x7a20 into packet list 0xb; the second is the
    loading indicator kept alive by 0x267950, called from the pack loader's wait loop), then forever
    `loadMenuPack` 0x2c3e10 (`packmenuEU.pak` at 0x31ae98 unless loaded), the scene from the
    0x18-byte `SceneDef` table **0x2fabd0** `{name, func, cleanup, arg, stackSize, stack}` (index
    gp-0x7a34, never written) in slot 2 until it exits, `overlayReleaseScreen` 0x267a00,
    `gfxSetDrawEnvClear` 0x12ea80(`gfxGetCurrentDrawEnv` 0x12e080 = display record +0x80, 1, 0,0,0,0).
    Retail's one scene: "Menu (Pack File)" `menuSceneThread` 0x2b5740 -> the menu flow 0x2b5770(1).
    Pack objects (0xf8): ctor 0x131f00(name, ?), load-and-wait 0x131d70 (job 0x132a40), deleting dtor
    vtable slot 2 at +0x98; 0x231660..0x231bb0 parse the `pak` image. Thread slot names 0x2f3810: `Th Ctrl`, `Th Main`, `Th 02`, `Th Game`,
    `Th 04`, `Th Extra`, `Th 06`, `Th Effect`.
  - **RNG and date** (batch 022): `gameInit` seeds newlib `srand` 0x126f78 and the game's
    `rndInit` 0x104860 (97-word table 0x3299c0, LCG x * 0x5d588b65 + 1) with second + minute + hour +
    day + month of `getLocalDateTime`'s 8-byte BCD date `{flags 0x10 (0x11 clock error), second,
    minute, hour, day, month, u16 year 0x20yy}`. `atan2f` 0x119b88 -> fdlibm `__ieee754_atan2f`
    0x1173e0 (newlib libm, batch 025). Empty hooks `debugSysInit` / `Update` / `Shutdown`
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

**`game/sysdata.cpp` 0x23af30..0x23c020 (TU 5, 2026-11-02, batch 026)**: one 0x920-byte block
`gSystemData` at 0x598b30 = `SaveData` (+0, 0x4bc: 69 stage flag bytes +0x69 holding clear bits
1 / 2 / 4 / 8 / 16 / 32 per (kind 0..2, variant 0..1) from table 0x307350, best records
`record[3][2][30]` +0xd8 (u_int; fresh = `stageDefaultRecord` 0x296d00: kind 0 = 3599, kinds 1 / 2
from tables 0x309ab0 / 0x309ba0 [stage][variant]; 123456 = "no record" for index -1 / -2; stage 29
keeps the highest, every other stage the lowest), medal bytes `medals[3][2][30]` +0x3b4
(`StageResult::getPerfectBits` 0x2b00d0: one bit per zero counter of the result 0x599700), a
once-only byte +0x468 seeded from the memcard module's gp-0x79f0) + `StageSelect` (+0x4bc =
0x598fec, the play record: stage 1..29 / 80..94, kind, variant; `getStageSelect` 0x23bb10 has 153
callers) + `GameOptions` (+0x4d0 = 0x599000, 0x50 bytes: +0 sound output 0 mono / 1 stereo / 2
surround (`SoundConfig::setOutputMode` 0x147ee0 on `gSoundConfig` gp-0x7a94), +0xa / +0xc screen
offset (`gfxSetDisplayOffset` 0x12ead0 on `gfxGetCurrentDisplay` 0x12e050), +0x18 an index into
the 19-step table 0x2fac20 (28..176, default 9: brightness?), `ButtonMap` +0x24) + `FlagTable`
(+0x520 = 0x599050, 256 ints, corrected by TU 7: `clear`'s loop counts ints to 0x100 and the save copies 0x400 bytes; `countListed` counts 29 listed entries, byte list 0x2fac00).
`SystemData::recordStageResult` 0x23b630 is the after-stage update. The **language** is sdata
0x31aad0 (default 5 = EN), codes J US KR CN Ct EN DE FR ES IT (0x2fac40); `getRegionCode` "EU",
`getFileSuffix` "" / "_e". `randUnit` 0x104720 is the game's float RNG (97-entry shuffle over an
LCG).

**`game/datetime.cpp` 0x23de20..0x23e740 (TU 6, 2026-11-02, batch 027)**: `getLocalDateTime`
reads `sceCdReadClock` (sceCdCLOCK, BCD, Japan time), shifts it with libscf's
`sceScfGetLocalTimefromRTC` (`_scfAdjustMinutes` by timezone + 60 * summer time - 540, wrapping hours /
days / months / years) and hands an 8-byte `DateTime` (byte +0: status bits 0-3, 1 = clock error;
format bits 4-7, 1 = BCD; then second, minute, hour, day, month, u_short four-digit year) to
`DateTime::setFormat` 0x23dfa0 (BCD <-> binary, unrolled: 0x798 bytes). Both callers (`gameInit`
for the srand / rndInit seed, the memcard code 0x2ac3f0) ask for binary (format 0). A failed read
gives 2000-01-01 hour 0x0c (not BCD 0x12; decodes to 12 anyway). The 1900 century branch tests
the BCD year against decimal 99 (0x63) and is dead. **libscf** 0x2640e8..0x2647d8: the
`sceScfGet*` readers (Language, Aspect, Spdif, TimeZone, DateFormat, SummerTime, TimeNotation) of
GetOsdConfigParam (bit 0 spdif, 1-2 aspect, 4 old Japanese / English, 13-15 config version, 16-20
language, 21-31 timezone minutes) and GetOsdConfigParam2 byte 0 (bit 4 summer time, 5 time
notation, 6-7 date format), each returning a static default 0x2fb380..0x2fb387 on a DTL-T10000
(`_scfIsT10K`: rom0:ROMVER [4] == 'T', buffer 0x2fb388); month lengths at 0x308840.

**`game/memcard.cpp` 0x2a9c70..0x2acff0 (TU 7, 2026-11-02, batches 028-030, all 55 written, `verify`)**: three
layers in one file (its sbss bytes 0x31ae80 / 84 / 88 / 90 = gp-0x79f0..-0x79e0 are one block).
`McDevice` (0x14: port, slot, result, sync result, last libmc function) wraps one sceMc request
per call and syncs it inline (`wait` 0 polls: sceMcSync mode 1); a rejected request or a negative
sync result becomes `mcErrorCode(func, result) & 0xffff` (0x121b80, alone between libmc and libc:
0x8108xxxx codes; GetInfo -1 -> 0x9003, -2 -> 0x2f; GetDir -4 -> 2, -6 -> 5). Read records
function 0x16, write 6. `SaveFile` (0xe40, the sceMcTblGetDir alignment pads 0xe0c) is one save:
the 0xc00 data file = `SystemData` (0x920) + `SaveExtra` 0x104 (`getSaveExtra` 0x29efd0 ->
0x5995e0, unread) + `SaveStamp` +0xa28 (file index, second..month, `randNext` u16; the module keeps
the loaded one at 0x31ae88, -1 none, only when GameOptions +0x4d is set) + 0x1c8 zero + magic
0x20060126 +0xbf8 + byte sum of +0..+0xa30 at +0xbfc; then the 8-entry directory listing +0xc00,
status +0xe00 (-1 unknown, 0 ok, 1 no dir, 2 GetDir error 5, 3 broken, 4 empty), found bits +0xe04
(icon.sys 0x3c4, icon1.ico 0x10eb8, data 0xc00) and the index. Four saves:
/BESCES-5385100..03/{BESCES-53851xx, icon.sys, icon1.ico}, title "Forbidden Siren 2 File0n" in
full-width SJIS (0x30a9d0..), wildcard BESCES-53851* for index -1. `Memcard : McDevice` (0x90,
port 0 slot 0) is the menus' manager: `start*` set a request (1 card info, 2 dirs, 3 scan, 4 check
all, 5 load, 6 / 7 save, 8 none, 9 format) when idle, `update` polls and runs its step function
(step < 0 ends; 0..2 the card-info sub-step, 10 / 20 / 30 / 40 + phase per file, 50 after the
last), the outcome +0x68 (1 ok, 2 unusable, 3 no PS2 card, 4 failed, 5 unformatted) and progress
+0x6c are for the menus 0x2b5770 / 0x2b6810 / 0x2b85b0 / 0x2b86e0. Save resources (icon1.ico, an
icon.sys template) come from `getSaveResource` 0x2c3d70 (an object at 0x31ae98, unread).
The requests: **save** (6 / 7) = card info (no PS2 card -> 3, unformatted -> 5), a blocking
`mkdir` of BESCES-53851n (its error never reaches the step: update's next sync sees nothing
running), then data / icon1.ico / icon.sys each open (SCE_CREAT | SCE_WRONLY) -> write -> flush ->
close at steps 20.. / 30.. / 40.. with fixed progress marks (0.2 / 0.25, 0.5 / 0.55, 0.8 / 0.85,
+0.05 per flush / close); a failed close only sets outcome 4 and the next file goes on (50 then
overwrites it with 1). **load** (5) and **check all** (4) read the directories, then read a
complete save's 0xc00 bytes (SCE_RDONLY) and judge it (`SaveFile::judge`, inline: all three files
+ magic + sum -> 0, else 3; none of the files -> 4); check all's outcome is 1 when any save
directory exists, else 2, its progress `0.3f + 0.2 * file` in double. Check all's open loop
returns the starting file's step even when it opened a later file, so that later save is read
into the earlier SaveFile (the ROM's slip). **scan** (3, at the title): room to save = 80 free
clusters (`memcardRequiredKB`) or a complete own save, else outcome 2; then GetDir (no table,
maxent -1) over the **first game's ten save directories** (0x2fb760: BISCPS-15053, BASCUS-97355,
BESCES-51920 / 52327..52330, BKSCKA-20019, BCSCCS-40013, BISCAJ-30003, each `*`); the first that
lists an entry sets gp-0x79f0 (`memcardGetState`), which seeds `SaveData::unlock468`: **the
first-game save bonus**. `buildIconSys` 0x2ab7c0: "PS2D", icon1.ico for all three views, TransRate
/ BgColor / lights / ambient (+0xc..+0xc0) from the template, title from 0x2fb7e0 with OffsLF 0x12
(line 1 "Forbidden", 9 SJIS letters). The four `operator=` at 0x2ac170..0x2ac3f0 are the implicit
copy assignments of FlagTable / GameOptions / StageSelect / SaveData (memberwise: `SaveData +0xae`
is 0x27 bytes, then padding).

**`game/progress.cpp` 0x2955e0..0x296c10 + 0x296d00..0x296d60 (TU 11, 2026-11-03, batch 033, all
24 written, `verify`)**: the **navigator table**'s unlock logic and `stageDefaultRecord`. The navi
menu (0x29a700..~0x2a0000, unread; it uses "2dEU/navi%s.d2b" at 0x29f804 and owns `getSaveExtra`
0x29efd0 -> 0x5995e0, its state saved with the system data; 0x29ef70 sets that block's three ints
and gp-0x79f4) passes a `NaviTable`: a 0x10-byte header (count +8), then 0x70-byte `NaviNode`s:
kind +0 (1 = single part), part[2] +4, grid x / y floats +8 / +0xc, `cond[3][4]` {type, arg} +0x10
(set 0 open, 1 first goal, 2 second goal), `trigger[2]` +0x28 {flag, part1Only, target,
secondGoal}, pending links `[3][4]` +0x30 {flag, target, secondGoal, -}, 8 unread bytes +0x60,
neighbour `link[4]` shorts +0x68. Condition types 0..14 (`NaviCondType` in `progress.h`): stage
cleared in variant 0 / 1 (stage flags & 0x15 / 0x2a), archive n (0 = all), all archives but 98, all
records, own pending links all fired (skippable), never, other nodes' open / goal states
(recursive), `SystemData +0x4c0` (`select.kind`) != 0, stages 1..29 cleared by kind 2 in each
variant; anything else (unused slots are -1) holds. Flags are `FlagTable` (SystemData +0x520)
indices: a node's goal is "done" ignoring its own pending links and "open" only when they all
fired. **SaveData +0x4..+0x68 is `archive[101]`** (index 0 unused; `SaveData::hasArchive` 0x296a00,
id 0 = all 100, the archive file is "2dEU/archive%s.d2b"). 0x296c10..0x296d00 (a VU0 lerp step,
two 16-byte copies, a 14-word clear; 12-18 callers each, none in this file) are out-of-line inline
copies of other classes, not in this TU.

**`game/boxgrid.cpp` 0x2647e0..0x2676e0 (TU 12, 2026-11-03, batch 034, all 29 written or mapped
to the host `<vector>`, `verify`)**: a **2D lookup grid over oriented boxes**. A `BoxSet` is loaded
data: count +4, bounding rectangle min / max vec4 +0x20 / +0x30 (xy used), then `count` boxes of
0x40 from +0x40, each the matrix taking world points into the box's unit cube (`boxContainsPoint`
0x2362a0: VU0 transform + vclip against w). `BoxGrid` (0x18 on the EE: set +0, MSL
`vector<vector<vector<const Box*>>>` +4, float cells-per-unit x / y +0x10 / +0x14) lists per cell
the boxes whose xy footprint touches it (`cellTouchesBox` inverts the matrix, tests the 8 corners,
cell corners in the 6 face quads, cell edges against the face edges, eps 0.0001); `find` tests only
the point's cell. The stage loader 0x19fcd0 builds one with size 8 through 0x190900 (deleting the
old one) at +0x3c of the object behind `*(gp-0x7a7c) +0x1c`; 0x1908d0 asks `contains` (callers
0x179d30, 0x18e280, 0x1e3e80, 0x2d0550, 0x2d0750). What the boxes mean (areas, no-go volumes) is
not read yet. **MSL vector layout: {capacity +0, size +4, data +8}**, 0xc bytes; CodeWarrior emits
each instance (insert, resize, erase, assign, range init, copy ctor, begin / end, placement new
0x2673f0) out of line after the file's code, plus linkonce `std::max` / `min` / `fabs` / `swap`
0x2651e0..0x265250 (also called from 0x2db540).

**`game/stageresult.cpp` 0x2afc20..0x2b0d50 (TU 13, 2026-11-03, batch 036, all 32 written or mapped
to the host `<map>` / `<vector>`, `verify`)**: **`StageResult`** at bss **0x599700** (`getStageResult`
0x2b0580; static init 0x310f60 = the inline ctor (layout +0xb8 / mode +0xbc cleared) + `begin(0)` +
`__register_global_object(.., ~StageResult 0x2b0cf0, 0x5996e8)`; it is the **last** of the nine
initialisers, so this object file links after every other file-scope object). EE 0xc0: four counters
+0 (`[0]` continues: `addContinue` from the game-over / pause menus' checkpoint answer; the medal bits
of `getPerfectBits` are one per counter at 0: [0] 8, [1] 1, [2] 4, [3] 2), an "attempt" block of 16
ints +0x10 (`restart` clears it; 0x17f7c0 reads +0x4c), a "stage" block +0x50..+0x90 and +0x90
(`begin` only), **two clocks `StageClock` {state, value, step, target} at +0x94 / +0xa4** (step
added each tick; a rising clock stops at -1, a falling one at 0; the stage scripts 0x1a8ca0..0x1a9000
set and test them, state 0 or bits 0x14), paused +0xb4 (the stage loop sets it around its menus),
hold +0xb5 (stops a countdown), the clock's `UiLayout` +0xb8, **mode +0xbc** (0 none: no record;
1 count-up from 0; 2 countdown from 7500, cues `SYS_COUNT1` every 25 ticks from 250 and `SYS_COUNT2`
at 0 through the stage object's sound, `*(gp-0x7a7c)+0x24` 0x1d6a80). The stage loop **0x17f160**
picks mode 2 on stage 29, else 1 when `StageSelect` +0x10 is set, else 0, then calls `update` each
frame with packet list 10 (tick, then the layout's advance / update / draw unless paused). `tick` also
counts **`SaveData` +0 = play time** (stops at -1). `beatsRecord` 0x2aff80 is `recordStageResult`'s
comparison (clock2 higher on stage 29, clock lower elsewhere). The clock faces are **language-pack
items 4 (count-up) / 5 (countdown)** (`getLangPackItem` 0x290630: the pack 0x31ae68 that 0x290670
loads from `dataEU/packinit<lang>.pak`; `getInitPackItem` 0x2907c0 is the same on packinitEU.pak at
0x31ae60; items are pack vtable slot 6). The same file: **`loadGamePack`** 0x2afd00 (was
`runStartupSequence`: no screens, it runs `loadGamePackTables` 0x2afd80 in slot 3: `PackFile`
0xf8 on `dataEU/packgameEU.pak`, `PackTables` 0x80 = eight MSL maps (ListMap int -> vector of 0x10
{4 bytes, string} / NameMap string -> 4 bytes, alternating), `PackTables::parse` 0x2bcf10 reads
columns ラベル / ctype / SEtype, then both are freed: where the parsed data goes is not read yet) and
`getInitItemForLanguage` 0x2afc20 / `getInitItem21` 0x2afcd0 (init items 0x15..0x1a by language).
**MSL map: {tree root +4, ...} 0x10 bytes; nodes {left +0, right +4, .., value +0xc}; MSL string
0xc {bit 0 of +0: heap buffer at +8}** (0x297fb0); CodeWarrior emits the map / vector instances
between the file's last function and its static destructor.

**`game/screen.cpp` 0x2676e0..0x2681e0 (TU 14, 2026-11-03, batch 037, all 18 written, `verify`; the
file name is the port's)**: the 2D screens around the game. **Overlay thread** slot 6
(`ThreadSlot_Overlay`; `overlayThreadStart` / `overlayThread` 0x267830 / cleanup 0x2677e0): every
frame draws, into **packet list 11** (`PacketList_Overlay`), the **BG movie screen** gp-0x7a28 (a
`UiLayout` on the init pack's file `2dEU/BGMOVIEEU.d2b` via `getInitPackData` 0x290800 = init pack
`FileLoader::getData`; `overlayShowScreen` 0x267a30 shows it, `overlayReleaseScreen` deletes it;
callers the menu flow 0x2b5770.., 0x20ad70.., 0x2954f0) and the **loading indicator** gp-0x7a20
(language-pack **item 0**; `overlayShowLoading` 0x267950 asks for it one frame at a time, the
thunks 0x231d20 / 0x231d30 for `FileLoader::waitLoaded` 0x1320f0 when its showLoading argument is
set; deleted on the first frame nobody asked). **Modal runner** `runLayout` 0x267ae0(item, arg,
cancelable, baseDir): a stack `UiLayout` (+0x38 answer, +0x3c done, +0x60 base dir by
`setBaseDir` 0x245f10) drawn into **packet list 12** (`PacketList_Screen`) until done, or -1 on
**cross** (`Pad::m_pressed` bit `PadBit_Cross`) when cancelable; `runLayoutFile` 0x267c00 loads the
file first with a one-file **`FileLoader`** (PackFile's base, vtable 0x3110b0, EE 0x9c: slot 2
`addFile`, 5 `getData`, 7 `getSize`, 14 done, 15 `waitLoaded`, 16 release; `startLoad` 0x132280
= `jobAdd(loadJob 0x1321f0)`, 0 while no job slot is free). Screens on it: the stage's **e / h
screens** `runStageScreenE` 0x2676e0 / `runStageScreenH` 0x267730 (`2dEU/s%02dm%1de<lang>.d2b` and
`...h<e|n|h><lang>.d2b` per difficulty = `StageSelect::kind` 0 e / 1 n / else h; none for stage 1
variant 0; the stage loop 0x17f160 / 0x17fa70), the **archive** `runArchiveScreen` 0x267f80
(`2dEU/archive<lang>.d2b`, base dir "2dEU/": answers bit 0 = layout answered 1, bit 1 = the
`SaveData` changed) and the **options** `runOptionScreen` 0x268050 (`2dEU/option<lang>.d2b`;
afterwards `memcardClearStamp` unless `GameOptions::_4d`, `applySound` when `soundMode` changed;
answers 1 when the options or `StageSelect::kind` changed). **Dialogs** in slot 4
(`ThreadSlot_Dialog`), so the stage loop keeps drawing and polls: **game over** (language items 2,
or 3 while `StageResult::hasAttemptFlag`; result gp-0x7c84 sdata -1: 0 restart, 1 quit, 2 continue,
as 0x17f890 acts on them) and a **notice** (item 1, gp-0x7c80; 0x17f7c0 / 0x2b5dc0 when 0x290080
answers < 0). `gfxGetTexArea` 0x12f160 = gp-0x7ad0, the `TexArea` (0x814, `TexArea::TexArea`
0x143270(0x3700, 0x3fbe, 14, 0x20), built by `gfxInitRenderTargets`): GS texture pages per packet
list; layout draws take `getList(list)` 0x142dd0. Next file: 0x2681e0..0x268310 is a **48-bit LCG**
(0x5deece66d: java.util.Random's constants; seed 0x268290, ctor 0x2682e0, next-int-below 0x2681e0;
callers 0x195d40 / 0x199f00 / 0x19c180), then 0x268310.. (vtables 0x315ab0 / 0x316c30).

**`game/uilayout.cpp` 0x243390..0x246230 (TU 15, 2026-11-03, batch 038, all 22 written, `verify`;
file and class names the port's)**: the **2D layout player**. A `UiLayout` (EE 0x98) plays an
**S2D file** (magic `"S2D\0"`, the `.d2b` files and pack items; `S2dData::relocate` 0x24d820 turns
its offsets into pointers in place once, flag at +8): +0xc start sequence, +0x10 / +0x14 the
**variables** (0x20: name[0x10], value, min, max, wrap), +0x18 / +0x1c objects, +0x2c **anim sets**
(count, then 0x10-byte entries {actor id, priority, anim, play flags}: next entry), +0x34
**widget defs** (0x30, id at +8), +0x3c **sequence defs** {priority, command
count, -, commands}, +0x40 / +0x44 the **string table**. A sequence runs a **script of 0x24-byte
commands** {trigger 0 frame / 1 start / 2 stop, op, skip count, condition {kind, flags (bit 0 all
keys, bit 1 negate, 0xf00 compare op 0 == / 0x100 > / 0x200 >= / 0x300 < / 0x400 <= / 0x500 &),
a, b}, arg0, arg1}: **30 opcodes** (`UiLayout::runCommands` 0x243f40, `S2dOp` in uilayout.h) and
**17 condition kinds** (`testCondition` 0x245060, `S2dCondKind`), the pad read from
`gGameSystem->m_pad0` (pressed / repeat / held words; the d-pad always from repeat as bits
0x2000000..0x10000000; decide / cancel = `ButtonMap::defaultButton(0 / 1)` as 0x20000000 /
0x40000000; 0x80000000 = any key). Ops 25..30 pack anim set lo16(arg0) + var hi16(arg1), value
lo16(arg1), sequence hi16(arg0). Op 15 **ends the layout** (`m_result` +0x38, `m_done` +0x3c).
A frame: `advance` (sequences' `UiSequence::advance` 0x246db0) / `update` (frame commands in
priority order, a pass that starts / stops a sequence begins again) / `draw` (`UiDrawer` 0xe0 at
+0x20: begin(packet, texList) / setMode(+0x30) / per sequence `UiSequence::draw` 0x246400 / end).
**`UiSubLayout`** (+0x00..+0x1c, UiLayout's base): one **nested layout** opened by op 21 from
`m_baseDir` + a file (a **`FileJob`** 0x132cc0, 0x28 bytes: dir + path then path through the rx
file hooks, states 1 queued / 2 loading / 3 done / 0x80 not found / 0x81 no memory) or, for
`@name` inside a stage (`inStage` 0x262e60), the stage pack's item at once (`getStagePackData`
0x262df0); it updates and draws on top, its result goes to +4 when it closes. Sounds: four slots,
names +0x64, 64-bit handles +0x78 (`playSound` 0x25ff40: the stage's StageSound by name in a stage,
else the menu bank; **`stopSound(-1)` does nothing**: its all-slots branch is dead). The sequence
side is `game/uisequence.cpp` (next entry); the track factory and its classes are
`game/uitrack.cpp` (after it), the S2D file's code `game/s2ddata.cpp`; the matrix and drawer
`game/uidrawer.cpp` 0x24dfa0..0x252840; still unassigned: the widget classes 0x25b..0x25e (vptr +0x1c,
`UiWidget::create` 0x25e260, its type table 0x2facf0: 0x2a names) and the game hooks
0x25ff40..0x2640e8 (sounds, game values / state names 0x599490).

**`game/uisequence.cpp` 0x246230..0x249550 (TU 16, 2026-11-03, batches 039-040, all 29 written,
`verify`; names the port's)**: a layout's **running sequence** and what it plays. S2D side
(`game/s2ddata.h`): **objects** (+0x18 / +0x1c: {track count, name, track record pointers}; a record
starts {kind, length}), **actor defs** (+0x24: {count, -, entries {object, loop}}), **anim set
entries** {actor id, **priority**, anim, **play flags**} (the flags are `UiTrack::setPlay`'s: 1
forward / 2 backward / 4 pause / 8 rewind / 0x10 to the end / **0x80 stop**; an entry with anim -1
and 0x80 removes the actor, and AnimSetDone does not wait on 0x80 entries), widget defs {name,
type name, id, priority, params}. Runtime: `UiSequence` (0x70) holds **actors** (`UiActor` 0x18:
data, def, id, anim count / array, priority) and **widgets** (base `UiWidget` Vt_00316580, 0x20:
sequence, data, id, priority, def, layout, arg, vptr +0x1c; slots dtor / draw / advance /
refresh(arg changed) / getStatus); an actor plays one **`UiAnim`** (0x10: object, object index,
tracks) per def entry; an anim's **tracks** (`UiTrack` Vt_00316560, 0x14: record, play state 0
stopped / 1 fwd / 2 back / 3 paused, frame, loop byte, vptr +0x10; slots dtor / draw(drawer) /
apply(xform, parent, colour, state) / **getAnchor**: the first vertex xy of a shape record, where
`UiWidget::create` places widgets; read as getText in TU 16, renamed in TU 17). **Keyed kinds 9..14** have a frame length (the
only ones `UiTrack::step` 0x24d200 moves and `isDone` waits on); selectors in anim set entries /
script ops: an object index, or -2 every keyed anim, -3 kinds 9/12, -4 10/13, -5 11/14 (kinds
11/14 apply to the **offset colour**), -1 (startAnim: every unkeyed anim forward, or all stopped).
**Draw** (`UiSequence::draw` 0x246400): actors and widgets sorted by priority and **merged**
(before each actor the widgets at or below its priority); per actor, from identity matrices
(seq +0x38 / +0x50, 2D affine 6 floats, `UiMatrix` 0x24e580..0x24e8a0), colour 1 and offset 0,
each anim applies itself (kind **0x1c** resets them and makes them the base of the next actors,
kind **0x25** makes them the layout's +0x40 / +0x50); the colour is multiplied by base and layout
colour, the offset added; the drawer gets matrix (stack +0x1c, top +0x7c; `updateMatrix` 0x24ea30
applies the screen matrix of mode +0xc0 from 0x2fac90 / 0x2facc0 unless **pixel mode** +0xdc
and no rotation), colour +0x80, flag +0xb4, then each playing track draws; a **non-zero offset
colour redraws the actor in the additive pass** (`setAddPass` 0x24fc20: blend 2, colour +0x98).
**Advance** 0x246db0: the same merged walk (widgets sorted only), tracks stepped, widgets advanced.

**`game/uitrack.cpp` 0x249550..0x24d530 (TU 17, 2026-11-03, batches 041-042; 97 of 109 written,
`partial`; names the port's)**: the **track factory** `UiAnim::init` and one class per S2D record
kind (`UiTrackKind` in uisequence.h, classes and records in `game/uitrack.h`), vtables
0x316540..0x316a60 in file order, then `UiAnim::findTrack` 0x24d1a0, `UiTrack::step` / `isDone` /
`setPlay`, and the inline virtuals emitted last (`UiWidget::refresh` / `getStatus` 0x24d400 /
0x24d410, the shapes' `getAnchor` 0x24d420..0x24d520). Kinds: **shapes** 0 line strip / 1 sprite /
2 triangles (record mode 0 list / 1 fan / 2 strip) with vertex colours, 0x1d / 0x1e / 0x1f the same
in one record colour (PRIM 0x4a / 0x4e / 0x4b..0x4d = gouraud + ABE, line AA1 from flag bit 0);
**textured** 5 sprite / 7 triangles / 8 strip mesh, 0x21 / 0x23 / 0x24 one-colour, 0x27 triangles
with the texture repeating (`setTexRepeat`: CLAMP_1; PRIM 0x15b..0x15e: + TME + FST; each owns a **`GsTexture`** 0x231360 (0x60,
vptr +0x44; "Zbuf" / "Draw" / "View" image magics are render targets) of S2D image `+0xc`, TEX1_1
from `+0x10`); **6 / 0x22** a sprite of a **TimM sheet** (`timSheetRelocate` 0x25fe10, picked by
name through 0x261d00); **3 text** (`UiFont` 0x25fac0: glyph table + font texture, spacing; text from
fMsg message files or a copied wide string, setters 0x24c6e0..0x24ca00); **4 scissor**; drawer
state **0xf..0x14** `setDrawPreset(0..5)` (0x250130: gfx calls 0x230980..0x231090), **0x1b** additive
blend, **0x2a** dest-alpha blend (`setBlend` 0x24ffa0: ALPHA_1 0x44 / 0x48 / 0x58 / 0x54), **0x28 /
0x29** `copyTarget` (a GS local copy of a 512x512 buffer, block 0x800 -> 0x1800 / back: game/uidrawer.cpp); **apply** kinds 9 / 0xa / 0xb
a matrix / colour multiply / offset add **per frame**, 0xc / 0xd / 0xe the same **by keys** {frame,
value} interpolated (matrices through `UiMatrix::interpolate` 0x24dfa0), 0x1c / 0x25 colour reset,
0x26 the drawer flag, 0x2b offset translate (+0x14, set by widgets), 0x15..0x1a pin / rotate to
the combined matrix (0x16 / 0x18 / 0x1a bind "rCharactorPostion": the stage camera, gStageScene
+0x78). S2dData +0x48 / +0x4c: image count and entries {data, name} (`getImage` 0x24d530 /
`getImageEntry` 0x24d570, game/s2ddata.cpp); the tracks keep the image's **name** as its key. **Rotate / pin** (0x24ad20..0x24b1b0):
world = parent * xform (`UiMatrix::multiply` 0x24e7c0); 0x19 / 0x1a: xform = R(wrap(world angle
(`angle` 0x24e360, atan2 of the x axis) + own (+ target))), parent identity, `coordMode` Pixel;
0x15..0x18 (0x16 / 0x18 first move by the target's position / turn by its angle): record +8 mode 0
xform = scale(record +0xc) and parent = translate(world origin), else xform = `setScaleRotTrans`
0x24e3d0 (scale, world angle, origin) with the origin moved to the parent. UiMatrix also: `rotate`
0x24e5d0 (R * this), `setScale` 0x24e5b0, `uiTransformPoint` 0x24e4f0 (x16 for GS fixed point).
**Sheet sprites** 6 / 0x22: `findSprite` 0x261d00 (the record's +0x14 name, layout hooks) answers a
`TimSheetSprite` {+2 image, +4 u v, +8 w h}; `UiDrawer::setImage` 0x24f580 binds the sheet's
image (+0x24 table, 8-byte {?, image}) through a temporary GsTexture, then PRIM 0x15e and
`drawSheetFlat` 0x250750 / `drawSheetGouraud` 0x2509b0 (uv 0..1 in the sprite rect). **Text** 3:
`UiDrawer::drawText` 0x24eba0 (font, message, record +0x1c box, the track's alignment) when the message is
>= 0 and +0x24 visible; setters 0x24c6e0..0x24ca00 (widgets 0x25b8e0..0x25ce30, `UiWidget::create`):
a borrowed or new[] copy of a 16-bit string (`wstrlen` 0x2d4650 / `wstrcpy` 0x2d4510, message
10000), or an **fMsg** file's message by index or name (`fMsgRelocate` 0x25fbd0 like
`timSheetRelocate`, `fMsgFind` 0x25fb30: entries {name, text} at +0x30, count +0x10).

**`game/s2ddata.cpp` 0x24d530..0x24dfa0 (TU 18, 2026-11-03, batch 044, all 9 written, `verify`;
names the port's)**: the **S2D file's own code**: `getImage` / `getImageEntry` (no lower bound
check), `findSequence` 0x24d5a0 / `findObject` 0x24d640 / `findVarIndex` 0x24d6e0 (-1 none) /
`findVar` 0x24d770 (null none) by `strcmp`, `getVar` 0x24d810 (unchecked), `getString` 0x24df60
(null out of range) and **`relocate` 0x24d820**. The **header (0x50)**: "S2D\0", +4 a version-like
word (785 / 800 / 817 seen), +8 relocated, +0xc start sequence, then **{count, table} pairs** at
+0x10 vars, +0x18 objects, +0x20 actor defs, +0x28 anim sets, +0x30 widget defs, +0x38 sequence
defs, +0x40 strings, +0x48 images. In the file **a table field is an offset from the file's start,
a table entry an offset from its own table's start** (the track records' offsets are from the
**objects** table), and **every name is a strings index**, relocated to a pointer (null out of
range): object +4, actor def +4, anim set +4, sequence def +8, widget def +0 / +4, image +4, the
textured track kinds' +8 (5..8, 0x21..0x24, 0x27), kind 6's sprite name +0x14 (**not** kind 0x22's:
the ROM leaves it an index; no S2D on the disc has a 0x22 record), 0x16 / 0x18 target +0x14, 0x1a
target +0xc. Vars and widget defs are arrays in place, images 8-byte {data, name}. On the host every
relocated pointer is an **`S2dRef<T>`** (s2ddata.h): a 32-bit offset from the field itself, so the
file keeps its EE layout (all records asserted) and relocates in place. Checked on the disc: 1,764
S2D files found uncompressed in ROM.000..ROMD.010 relocate with every pointer inside the file;
names read like `pMASKOFF` / `APS44D1EN` (objects), `sFADEIN` / `slanguage_sel` (sequences),
`prgDebug` (a widget type), `language_sel_1.tm2` (TIM2 images; image 0 is often
`_dmy.file.name_`, magic "YNAM"), sheet sprites `rItem00` / `rWeaponL00`, pin target
`rCharactorPostion`.

**`game/uidrawer.cpp` 0x24dfa0..0x252840 (TU 19, 2026-11-03, batches 045-047; all 50 written,
`verify`; names the port's)**: `UiMatrix` (2D affine rows (a b tx) (c d ty); `concat` = m * this,
`rotate` = R * this) and **`UiDrawer`** (0xe0), the layouts' **GIF packet writer**. Fields: +0 packet
(the packet list drawn into), +4 the TexArea list, +8 DIRECT open, +9 vertex run open, +0xc its
format, +0x10 texture TBP, +0x14 / +0x18 texture size (the UV scale), +0x1c matrix stack [4], +0x7c
top, +0x80 colour, +0x90 preset, +0x94 blend, +0x98 add colour, +0xa8 add pass, +0xac / +0xb0 the
preset / blend saved by it, **+0xb4 fixed colour** (UiDrawState::drawerFlag, kind 0x26: the drawer
colour alone, the tracks' own colours ignored), +0xb8 translate, +0xc0 mode, +0xc4 the draw matrix,
+0xdc pixel mode. Packets: state is an **A+D run** (tag 0x3076e0), PRIM a REGLIST {PRIM} (0x307640)
or {PRIM, RGBAQ} (0x307650), vertices a REGLIST of **format** 0..7 (0x307660 + 0x10 f: bit 0 RGBAQ,
bit 1 UV, bit 2 in threes XYZF3 XYZF3 XYZ2); each vertex XYZ2 = (m_matrix * xy) x16 clamped
+-32752, + 0x8000 (the GS centre), Z 0xff0fffff; UV = uv x size x16 (FST); RGBAQ bytes x256 (alpha
x128), clamped (`UiVec4::toRgbaq` 0x24f350). **The add pass** clears TME and forces ABE in every
PRIM and draws the add colour, blend 2 (Cs * Ad + Cd). **Screen matrices** (setMode 0 / 1, 0x2fac90
/ 0x2facc0): **4:3 and 16:9**: pairs {scale(k, 1), scale(1/k, 1)} with k = 4/3 / 16/9;
`updateMatrix` draws through scale(1/k, 1) (layout x is in display-square units, narrowed to the
frame), pixel mode keeps only the translation, or with a rotation turns in the widened space and
narrows a / b by 1/k after. **Textures** (`setTexture` 0x24f7a0): `TexAreaList::place` 0x143510
(a GS block per texture id per packet list; 1 = newly placed), `GsTexture::upload` 0x2e1c50 then
`putTex0` 0x231200 (TEX0_1), `getSize` 0x2311c0. **Presets** (`setDrawPreset`, gfx functions
0x230980..0x231090 named `gfxUiPreset*`): 0 colour + alpha written, 1 / 2 alpha only (FBMSK
0x00ffffff), 3..5 RGB only (FBMSK 0xff000000), 6 back to the draw env's FRAME / ZBUF / SCISSOR
(`end`); each resets the blend to 0. **`setTexRepeat`** 0x24fef0 writes CLAMP_1 (reg 8; read as
TEST_1 before, hazards.md), **`copyTarget`** 0x24e8a0 TRXPOS / TRXREG 512x512 / BITBLTBUF / TRXDIR
2 (a local copy 0x800 -> 0x1800, or back). The draw env packet (`GfxDrawEnv`, gfxtarget.h): GIF tag,
then sceGsDrawEnv1 at +0x10. libvifpk named here: `sceVif1PkCnt` 0x10acc8, `OpenGifTag` 0x10b058,
`CloseGifTag` 0x10b070, `Align` 0x10b130, `AddCode` 0x10b1b0, `AddGsData` 0x10b328, `AddGsAD`
0x10b350, `OpenDirectCode` 0x10b568, `CloseDirectCode` 0x10b5c0. On the host the drawer keeps its
decisions and calls **`yamipan/rendering/yamipan_drawui.h`** ("PC platform layer").
**`interpolate`** 0x24dfa0 decomposes both matrices (origin, axis lengths, the x axis's atan2),
blends scale / angle (the short way, wrapped to [-pi, pi], compared as doubles) / origin by t, and
rebuilds scale then rotate; a mirrored `from` (negative determinant) flips x and takes atan2(y, -x)
for both ends (ROM: `neg.s f13`), so a mirrored *and rotated* `to` does not round-trip at t = 1 (a
ROM quirk, kept). **`setScissor`** 0x250520 (rect through m_matrix) / **`setScissorPixels`** 0x2503c0
(window pixels): rounded (+0.5), x0 y0 >= -256, x1 y1 <= 255, SCISSOR_1 = each + 0x100 (the 512x512
target's centre). **Sheet draws** 0x250750 / 0x2509b0: UV = (sprite w x uv + sprite u) x16 (FST).
**Strip meshes** `drawTexStripFlat` 0x251350 / `drawTexStripGouraud` 0x2519a0 are UiDrawer members
(f12 a **float width**, record +0x14; were read as free functions (mode, drawer)): vertices are
{xy} / {rgba, xy} (no uv: UV comes from the constant uvRect {0.5, 0, 0.5, 1}); width 0 = a plain
strip at uvRect's first uv; else **a ribbon**: the points are pairs (point, cross-direction point),
each pair emits point +- (q - p) x k, k = (width / 2) / |first pair| (through the matrix), uv
first / second, xy clamped +-2047 px. **`drawText`** 0x24eba0 (a1 font, a2 message (unread), a3 the
record's **`UiTextBox`** +0x1c {rgba, +0x10 topLeft, +0x28 bottomRight}, f12 / f13 **alignment**
0..1 = UiTextTrack +0x1c / +0x20): text pointer odd -> 0; TEX1_1 = 0, PRIM 0x15e (sprite, gouraud,
TME, ABE, FST; add pass 0x14e) + RGBAQ (box colour x drawer colour, or add / fixed colour); the box
corners go through m_matrix, the glyphs do not (texel size); block height = lines x lineHeight -
spacingY x (lines - 1), y = alignY x room + top; each line x = alignX x (box width - lineWidth) +
left; per glyph a 2-vertex SPRITE (UV / XYZ2), advance w / 16 - spacingX; '
' adds lineHeight -
spacingY; control chars skipped. Answers the glyphs drawn. **`UiFont`** +0xc / +0x10 are the
**spacing** (pixels off each advance / line; were "scale"; record S2dTextRec +0x14). **`FontData`**
(game/fontdata.h, the glyph table, S2D image `glyphImage`): +8 count, +0xe line height (1/16 px),
+0x10 entries of 0x10 sorted by u16 code, glyph rect u v w h at +8 (1/16 texel), the entry after
the last = the missing-character glyph; `nextGlyph` 0x1fe4d0 (binary search), `countLines` 0x1fe150,
`lineWidth` 0x1fe2b0 (spacing + sum (w / 16 - spacing)).

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

- **Allocation: `game/heap.cpp` 0x1037c0..0x104b40** (TU 8, batch 031; game code, not MSL: one
  object with the RNG, its data together in sdata 0x31a880..0x31a88c, bss 0x329970 / 0x3299c0, sbss
  0x31ad80). `Heap` (EE 0x44): `{+0 sema id (<0: no locking), +4 base, +8 end, +0xc size, +0x10 head
  / +0x14 tail block (address order), +0x18 count, +0x1c four usage marks (highest data end since
  setMark), +0x2c peak count, +0x30 unread, +0x34 u8 mode, +0x38 / +0x3c gap list, +0x40 min gap
  0x40}`. Block header 0x10 right below the user pointer `{data, size | bit 31 gap-recorded, prev,
  next}`; the data starts `align * ((align + 0xf) / align)` past the aligned end of the block before.
  Free space after a block >= 0x40 gets a record `{0x7381a9bc, block, prev, next}` written into it
  (at the data end rounded to 16) and joins the gap list. Mode 0 (default): a recorded gap, then the
  space before the head, then the blocks from the tail down; mode 1 (callers flip +0x34 around one
  alloc, 0x298e1c / 0x299190): after the tail, then a gap. `Heap::alloc` / `free` / `realloc` lock
  and call `allocBlock` 0x103cd0 / `freeBlock` 0x103b10; realloc grows in place up to the next block,
  else moves with align 0x10. `initGlobalHeap(0)` takes 0x2000000 - (image end 0x5a5600 + stack
  0x10000 + 0x2000) - 0x20 from newlib's malloc (sdata words 0x31a880 / 884 / 888; a fixed heap
  size when 888 >= 0). On the host the headers hold 8-byte pointers: `headerRoom` and `blockOf` use
  `sizeof(HeapBlock)` (0x20) where the ROM has 0x10. `operator_new[]` 0x1002f0 and `operator_new`
  0x100310 pass align 16; `operator_delete[]` 0x1002b0, `operator_delete` 0x1002c0; 0x1002d0 passes a
  caller's alignment (an aligned overload, unnamed). Which of the two plain-size allocators is
  `new[]` follows from MSL's string rep 0x106b80 (char buffer -> 0x1002b0, int refcount -> 0x1002c0)
  and the same order for new: verify at the first TU that allocates.
- **RNG** (same file): 97-word table 0x3299c0 filled from the LCG `x * 0x5d588b65 + 1` (state sdata
  0x31a88c, starts at 1); a draw returns `table[i] >> 1` for the index i = previous entry % 97
  (unsigned) and refills that entry. `randNext` 0x1046b0 and `randNext2` 0x1047f0 are byte-identical;
  `randUnit` 0x104720 divides by 2^31 and draws again at 1.0; `rndInit(skip)` 0x104860 refills the
  table and throws away `skip` draws (gameInit's clock sum: the argument is not a seed).
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
- **Thread system** (`game/thread.cpp` 0x130a20..0x1319f0, reconstructed 2026-11-02; batches 002, 032):
  a cooperative scheduler over **8 slots** at 0x4c7840 (`ThreadSlotRecord`, EE 0x24: `{+0 tid, +4 state
  (0 free, 1 waiting, 2 start pending; bit 7 masked), +8 ticks, +0xc arg, +0x10 cleanup(arg), +0x14
  message (threadYield's result, `threadSwapMessage`), +0x18 stack size, +0x1c stack (canary 0x572a8b4c
  at the bottom; a mismatch hangs in sceGsSyncV), +0x20 own-stack byte}`); default stacks in the table
  0x2f38c0 (`{size, base}`, nine rows: slots 0..7 and the job worker, all inside bss 0x4af040..0x4c7840).
  Slot threads run at prio 0x30, the scheduler `threadScheduler` 0x131290 (stack 0x4c81b0, 0x800) at
  0x40: it does **not** wait for the vblank; it loops over the slots (current slot sdata 0x31a898),
  StartThread on a pending slot, a waiting slot's ticks counted down once per visit, WakeupThread at 0,
  then sleeps until the slot yields (`threadYield` 0x130bc0) or ends (`threadExit` 0x130a90). A detour
  stack 0x4c8170 (`{count, 4 x {resume, target, state}}`) and a pre-switch hook 0x31adcc are read but
  never set; `threadSetSwitchHook` 0x130a30 sets the post-switch one (0x31add0 / arg 0x31add8). Wrapping
  past slot 7 signals `threadRunMain`'s semaphore 0x31adb8 only when the byte 0x31adbc is set (never in
  the ELF). `threadSlotCreate` 0x130c90 (func, slot, arg, cleanup, stackSize, stack), `threadSlotKill`
  0x130b30, `threadSlotState` 0x130a70. **Frame sync**: `frameSyncInit` 0x131870 / `frameSyncThread`
  0x131930 (prio 0x10, stack 0x4c7960) waits on the vsync semaphore 0x31a8a4 that `threadVSyncHandler`
  0x131840 (the sceGsSyncVCallback, installed by the scheduler) keeps at one count, then asks the
  frame-ready callback with `FrameSync` 0x4c8160 `{+0 requested, +4 vsync count, +8 interval}`; a ready
  frame signals 0x31a89c for `frameSyncWait` 0x131260 (returns the count). **Jobs**: `jobAdd` 0x131620
  (fn, arg) -> id (counter 0x31a8a8, 1..0x6fffffff) takes one of 16 `JobRecord`s 0x4c89b0 (0x18: `{busy,
  id, fn, arg, prev, next}`, list head 0x4c8b38 / tail 0x4c8b3c); `jobWorkerThread` 0x131720 (prio 0x50,
  thread id 0x4c8b30) spins while the list is empty, runs the head, re-queues it at the tail when it
  returns nonzero; `jobCancel` 0x131480 (0 not found, 1 waiting, 2 running: the worker is recreated).
  Callers of the jobs: the next module (unassigned@001319f0: 0x131a30, 0x131d80, 0x132000, ..).
  On PC this layer needs a host run model (open-items.md "Open questions", thread system).
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
  0x32b900, call semaphore 0x2f2f9c, last command 0x2f2f98. `mcErrorCode` 0x121b80..0x121ef0 (TU
  `sdk/mcerror.cpp`, 2026-11-02, `verify`) maps (libmc command, result) to SCE_ERROR_ENCODE(0x01080000,
  e): e an EE newlib errno (ENOENT / EIO / EBADF / EACCES / EEXIST / ENOSPC / ENOTEMPTY 90 / ...),
  0x2f for NoFormat, Sony 0x8001.. / 0x9003 (ChangedCard); the command matters only for FullDevice,
  NoEntry, DeniedPermit and NotEmpty, and a full card on a read returns a bare 5 (no prefix). It knows
  every libmc command plus 0x16 / 0x1b (read / write fast) and 0x20 (all errors -> 0), so it is read
  as libmc's own encoder (link position: after the read-fast tail, before libc's `abort` 0x121ef0);
  the game's `McDevice` 0x2ac630..0x2ace30 is its only caller and keeps the low 16 bits. **SIF cmd
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

- **`src/yamipan/rendering/yamipan_drawui.h`** (TU 19, 2026-11-03; declarations only until bootstrap
  6): the 2D layouts' draw module, user `game/uidrawer.cpp`. Keyed by the packet the game drew into
  (= the packet list = the layer); `YamiPan_DrawUiReg` (A+D: ALPHA_1, CLAMP_1, TEX1_1, SCISSOR_1,
  the BITBLTBUF..TRXDIR set of a local copy, done on TRXDIR between host render targets),
  `YamiPan_DrawUiPrim` (PRIM, optional shared RGBAQ), `YamiPan_DrawUiTexture` (a GsTexture owner,
  decoded at its source; `setImage`'s owner is deleted right after the call), `YamiPan_DrawUiBegin`
  (vertex format) / `YamiPan_DrawUiVertex` (RGBAQ bytes, UV / XY 12.4, Z). The gfx presets
  (`gfxUiPreset*`) still write their own packets until their TU is written.
