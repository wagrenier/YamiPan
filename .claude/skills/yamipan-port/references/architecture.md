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
- **IOP modules** (`MODULES\`, `IOPCONF.CNF`; "The IOP side" has all of it): IOPRP300.IMG, SIO2MAN,
  PADMAN, MCMAN, MCSERV, LIBSD and the game's own **RXSNDDRV.IRX** (the rx sound driver, RoR's, revised);
  SDRDRV.IRX is on the disc and unloaded. The ELF also names cdvdfsv, cdvdman, fileio, sifcmd, usbd.
  `SYSTEM.CNF`: `VER = 1.00`, `VMODE = PAL`.
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
| 0x310a00..0x310f80 | static initialisers (9 `__sinit_*`, each in its owner's TU; table 0x310fb0..0x310fd4 run by 0x100280: "Static initialisers") |
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
0x121ef0..0x12cce0 (its stdio tail to 0x12cce0 read at TU unassigned@0012c9b8, batch 199), libkernl 0x1366c0..0x142dd0 (front, sifcmd / sifrpc, fileio, iopheap, tail),
**libscf 0x2640e8..0x2647d8** (batch 027: linked between game objects, in the middle of the game's
0x171300.. run) and the kernel-patch island 0x2f3a40..0x2f4d20: 931 functions. **Unread runs between them stay game code
until read** (not library by default): 0x121b80 (libmc error mapper?), the VU0 math
0x135990..0x1366c0 (game code, TU 42: before it `game/mathutil.cpp` / `game/clock.cpp`), 0x142dd0..0x144080.

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
**TU `rx/rxsnd.c` (2026-11-03, all 229 written, `verify`)** corrects and completes that: the three
size differences (and `rxSndAdpcmGetPosition`'s) were **four FS2-only functions hidden in their
tails**, created and named (batch 069): `rxSndBgmGetActiveList` 0x15dc60, `rxSndSeGetActiveList`
0x15ea40, `rxSndPcmGetActiveList` 0x15fa80, `rxSndAdpcmGetActiveList` 0x161820 (walk the family's
active list, write each id into an optional int array, return the count; no callers). The other
**215 shared functions are RoR's word for word** (both ELFs compared, immediates of jumps, `lui`
and gp-relative words masked; loads / stores off other bases were not compared), so `src/rx/rxsnd.c`
is RosePan's `rxsnd.cpp` carried over to C. FS2's real code differences: `rxSndAdpcmGetPosition`
returns position x channels (`__muldi3` 0x119be8); `rxSndAdpcmUpdate` builds the 64-bit position
as hi:lo (`lwu`/`lwu`/`dsll32`/`or`; RoR ORs the two words); `rxSndQueueBegin` flushes at
+0x58 / +0x40 of margin (RoR +0xc / +0: its end marker ran past the 0x1000 queue). Data: **bss is
RoR's moved by -0x2b7bc0** (s_config 0x4fdf00 .. s_queueReply 0x513780; s_xvab 0x501c00, s_xseq
0x503400, s_bgm 0x504000, s_adpcm 0x508808, RPC client 0x512740); the data words one by one:
s_bufferBank 0x2f5b60, pitch tables 0x2f5b68 / 0x2f5cf0, BG-call hook 0x2f5e80, IOP hooks
0x2f5e88 / 8c / 90, SPU hooks 0x2f5e98 / 9c / a0 (0x2f5e94 unused), ADPCM end block 0x2f5ec0,
s_rpcSema / s_queuePos / s_queueReplyPos 0x2f5f00 / 04 / 08.
**TU `rx/rxsys.c` (2026-11-03, all 126 written, `verify`)**: **all 126 rxsys functions are RoR's
word for word** (`rxcmp.py '^rxSys'`: same sizes, jump targets / `lui` / gp offsets masked), so
`src/rx/rxsys.c` is RosePan's `rxsys.cpp` carried to C with no FS2 difference. `rxcmp.py --data`
pairs the addresses identical code forms: rxsys's **data is RoR's +0x14eb0** (s_printHook 0x2f0880,
s_libVersion 0x2f0888, s_productNumber "SCPS-00000" 0x2f08b0, s_bootDevice 0x2f08d4, s_iopImage
"ioprp300.img" 0x2f0b00, the module lists 0x2f0c08 / 20 / 38, semaphores 0x2f0c44 / 48, g_rxSysMemFunc
0x2f0d3c..0x2f0d48, the console font 0x2f0d60..0x2f1060: words and font bytes identical), its **bss is
RoR's -0x4b1240** (s_saveName 0x31b100, s_modules 0x31b140, s_deviceReady 0x31b540, s_confBuf
0x31b580, s_ttyBuf 0x31c580, s_ttyClient 0x31d580, the console 0x31d5c0..0x32996c), and **rxfile's
slot pools are RoR's -0x30a030** (rxFileSlots 0x4d0b80, rxFileTagPool 0x4d3f80, rxFileMarkerPool
0x4d4384, rxFileSegmentPool 0x4d4988, free heads 0x4d4380 / 0x4d4984 / 0x4d5988). The System Monitor
is 0x102fe0..0x1037a8; `_exit` (the instructions it patches) is 0x100218; newlib's ctype table is
0x3000c1. FS2's callers: **`rxInit` 0x1527a0** (rxThSetPrioIndex, rxSysSetBootPath, rxSysInit, memory
hooks `gameRxMemAlloc` 0x1530b0 twice / `gameRxMemRealloc` 0x153090 / `gameRxMemFree` 0x153080 over
the heap at 0x329970, rxThSetInterval(4), rxFileCdSetLayerMode(0), rxSysInitFileDevice(1),
spkRomLoad, rxSysSetDeviceParam(0x80000)) and `rxTerm` 0x152750 (rxSysTerm). Nothing found reads
g_rxSysMemFunc besides its setters, and rxSysSetDebugHandlers / rxSysSetIopImage / rxSysBindRpc /
rxSysHexDump have no caller in FS2. newlib `atoi` 0x121f38 -> `strtol` 0x129d80 -> `_strtol_r`
0x129b48 (batch 070).
**TU `rx/rxfile.c` (2026-11-03, all 108 written, `verify`)**: 106 of 108 rxfile core functions are
RoR's word for word; `rxFileOnCreate` 0x154518 / `rxFileOnGetSize` 0x1545a0 print
"rxfile(%s,%d) : failed to open file '%s' %d %d %X,%d %d" (fmt 0x301680, "rxfile.c" 0x3016c0,
lines 1018 / 1041, then device, flags, ring, ring size, result) where RoR prints the path only.
Data: s_verbose 0x2f5aa0 (= 1; RoR +0x198f0), an unread zero word 0x2f5aa4, filehost's data
0x2f5aa8.., filecd 0x2f5ab0.., filehdd 0x2f5b10 / 14, then the core's second part: device map
0x2f5b18 {0,1,2,3}, fallback devices 0x2f5b28 {0,1,2,-1}, error hook 0x2f5b38, device threads
0x2f5b40. Bss: drivers 0x4d5990 (HOST / CDROM 0x4d59b4 / HDD 0x4d59fc), host root 0x4d5a20, CD
cache 0x4d5b40.. (RoR -0x30a040 from here), HDD root 0x4e7d80, s_contextIndex 0x4e7e80, contexts
0x4e7e84, device info 0x4e7e9c (0x128c each), file thread 0x4ec8d0, its queues 0x4fc8e0. The
three drivers sit between the two ranges in text, data and bss alike. The game's own
`rxFileCallLoadHook` 0x152730 / `rxFileCallSizeHook` 0x152740 are `j` thunks to 0x155690 /
0x1556d0 (FileJob::run calls them).
**TU `rx/filehost.c` (2026-11-07, 12 functions, `verify`)**: RoR's host0: driver except the
open-failure message, "rxfile(%s,%d) : failed to open file '%s' %d" (fmt 0x3018d8, "filehost.c"
0x301908, lines 125 / 157 / 188 / 234 for GetSize / Create / Read / Write, then the path and the
fd; Create passes no fd but t0 still holds it, 0x156250). s_hostRootDefault 0x2f5aa8 -> "host0:"
0x3018d0, s_hostFlags 0x2f5aac (bit 0 busy). Unlike filehdd it has no I/O-error fallback.
**TU `rx/rxthread.c` (2026-11-03, all 53 written, `verify`)**: `rxcmp.py '^rxTh'` finds all 53
RoR's word for word (`rxThWorkerMain`'s one flagged word is an unmasked `addiu` of a data address),
so `src/rx/rxthread.c` is RosePan's `rxthread.cpp` carried to C in two pieces: the VBlank tail
0x159928..0x159bc0 and the core 0x166b50..0x1681c0. Data (RoR +0x19cf8 for the core, +0x198cc for
the tail): s_initCount 0x2f5f58, s_basePriority 0x2f5f5c, call queue write / read 0x2f5f60 / 64,
s_request 0x2f5f68, s_interval 0x2f5f6c (= 4), s_prioIndex 0x2f5f70 {0,2,4}; vblank handler id
0x2f5b50 (= -1), field 0x2f5b54, users 0x2f5b58, vblank count 0x2f5b5c. Bss (RoR -0x2e1500 for the
core): semaphores yield / dispatch / request 0x526a40 / 44 / 48, main / worker / idle thread ids
0x526a4c / 50 / 54, alarm 0x526a58, yield tick 0x526a5c, yielding 0x526a60, thread count 0x526a64,
current slot 0x526a68, slots 0x526a70 (8 x 0x10), call queue 0x526af0 (32 x 0x10), request args
0x526cf0, callbacks 0x526d00, worker stack 0x526d40 (0x10000), idle stack 0x536d40 (0x400); the
tail's last-vblank / HSYNC carry 0x4fdef0 / f4 (just before rxsnd's s_config 0x4fdf00). The game's
**0x151f70 / 0x151f80 are `j` thunks to `rxThEnableDispatch` / `rxThDisableDispatch`** with the
same Ghidra names (MoviePlayer::receiveAudio calls 0x151f70), like the rxFile hook thunks.
**Batch 008 (2026-11-02): rxsndut + rxsndstr 0x162178..0x166b50** (not in RoR; named by reading
in rx style, `rxSndUt*` / `rxSndStr*`; messages 0x3022e0..0x302738). Five layers, each with its own
static slot table and init / exit / update called from `rxSndUtInit` 0x162178 / `rxSndUtExit`
0x1621f0 / `rxSndUtUpdate` 0x162220 (debug flags 0x2f5f10: bit 1 error prints, 2 BG-thread warning,
4 registration print; all three set at boot, the value 7; `sound.cpp` clears them). **`src/rx/rxsndut.c`
written 2026-11-03** (all 6, `verify`): the file's data are the flags and the two format tables
0x2f5f18 (PCM, id 1) / 0x2f5f38 (ADPCM, id 0), 0x20 each, registered in that order after the layers'
inits (memory, stream, bank, core); `rxSndUtInit` also clears **rxsnd's** debug bit 4 (the SPU heap
alloc / free trace, `RXSND_DEBUG_SPUHEAP`); exit runs stream, bank, core, memory; update memory,
stream, bank, core. `rxSndUtSetDebugFlags` answers the old flags. Its BG-call check is the same code as
rxsnd's `rxSndCheckBgCall` (same message, "rxsndut") on its own flags. The layers:
- **XWS bank** 0x1622f8..0x1634b8 (`rxSndStrBank*`, 256 slots x 0xa8 at 0x514780): an `RXWS` file
  (version >= 0x200) of 0x10-byte-header chunks `FORM` (variable-length request records: +0 format id,
  +1 length, +2 flags (bit 0 loop), +4 / +5 volume, +9 channels 1 / 2, +10 rate?, +0x10 offset,
  +0x14 size, +0x18 loop offset), `FTXT` (request names), `MARK` / `BRCH` (per-request marks and
  branches: +0 position, +4, +8 type, +0xa / +0xb loop count, +0xc name), `TRCK` (per-request tracks:
  +1 channels, +4 / +6 volume, +8 name), `BODY`; the body is in the file (type 0), a separate file
  (1, with the device and search dir remembered) or IOP memory (2).
  Written 2026-11-03 (src/rx/rxsndstrbank.c, file records in rxsndstrbank.h): slot +0 used, +1 the
  open's first argument (unread), +2 body type (0xff never attached), +4 header, +8..+0x18 the
  FORM / FTXT / MARK / BRCH / TRCK chunk data, +0x1c body addr, +0x20 size, +0x24 device, +0x28 dir
  [0x40], +0x68 name [0x40]. Tags are compared as little-endian words of the strings. Indexes are
  {count, offsets from the chunk data}: FTXT one; MARK / BRCH one at +4 of per-request ones (the
  records); TRCK one of per-request lists whose own index is at +4. Request rate +0xa is a u_short.
  `rxSndStrBankFindTrack` returns 0 (not -1) for an unknown bank or request; `rxSndStrFormGet` is a
  twin of `rxSndStrBankGetRequest` nobody calls; OpenBody's "never attached" print is dead (it
  compares the u_char type with -1). On the host the body address is a `uintptr_t` (EE pointer or
  IOP address), which `rxSndStrStreamOpenMemory` takes with its iop flag.
- **Stream core** 0x1634b8..0x164398 (64 streams x 0x9c at 0x51ef80): `rxSndStrPlayEx` picks the
  request's registered format (8 slots x 0x28 at 0x52168c, `rxSndStrRegisterFormat`; function table
  {init, exit, update, open, close, getHandle, pauseRead, syncRead}), setters mark dirty bits +0x1c
  (1 pause, 2 attr, 4 volume, 8 pitch, 0x10 a float nobody reads, 0x20 track volumes) that the format
  update applies; mark / branch callbacks +0x54 / +0x58 run with interrupts off; master volume
  0x521688. `rxSndStrStop` / `rxSndStrStopAsync` / `rxSndStrStopAll` are the names its own checks print.
  Written 2026-11-03 (src/rx/rxsndstr.c, `RxSndStrSlot` in rxsndstr.h): `rxSndStrPlayEx` takes 10
  arguments (bank, request, start, buf, bufSize, iopAddr +0x3c, iopKB +0x40, spuAddr +0x44, spuKB? +0x48
  on the stack, keepOnEnd +0x1b on the stack); `rxSndStrPlayAt` is it under another name,
  `rxSndStrPlay` the same from 0. `SoundStream::play` passes (.., 0, 4, ring, 4, 0). `rxSndStrUpdate`
  stops a stream whose status has bit 4 (played out) unless keepOnEnd. Free slots are pushed 0..63, so
  slot 63 is taken first.
- **PCM format** 0x164398..0x164c38 (table 0x2f5f18, format 1 "PCM stream") and **ADPCM format**
  0x164c38..0x1656e8 (0x2f5f38, format 0 "ADPCM stream", multi-track interleave): feed rxsnd's
  `rxSndPcm*` / `rxSndAdpcm*` from a source.
  **ADPCM written 2026-11-06** (src/rx/rxsndstradpcm.c, `RxSndStrAdpcmStream` in its header, 64 x 0x20
  at 0x521ed8, active 0x5226d8, free 0x5226dc). Open: a request with tracks opens mode channels |
  0x1000000, else 1 channel mode 0, 2 channels mode 2, anything else fails. The block is 16 bytes per
  channel, the read ring bufSize / (channels * 0x20) * block * 2. Marks become the source's tags (value:
  the mark's index), branches its markers (id: the low byte of the branch's position, jump -1 when param
  is -1, user: the index), and one marker more is the end: at offset + size, jumping to offset +
  loopOffset for a looping request, -1 otherwise. Start is rounded down to a block and clamped to the
  size, and the slot keeps it. **So the mark and branch callbacks get an index, not a record**
  (`RxSndStrMarkCallback` was typed wrong). Request +3 is rxSndAdpcmOpenEx's last argument (-1 with
  flags bit 1), +4 / +5 signed volumes multiplied into the slot's, +6 u_short: non-zero ORs 1 into the
  attribute. A slot's status mirrors rxSndAdpcm's 2 playing and 8 paused every frame; 4 ended is sticky.
  **ROM quirk kept:** a failed open closes the stream's source and rxSndAdpcm stream even if this open
  never set them, so a stale id can close a stream in use. **PC:** rxSndAdpcm's user data is an int, so
  it holds the format stream's id, not the slot pointer. No IOP answers, so the block "the driver's
  silent side" reports every open stream through rxSndAdpcmUpdate, from the request's length; it replaces
  the 2026-11-04 bridge. It goes with the audio milestone, with ADPCM_SILENT_BUFFER (open's buffers:
  `rxSndIopAlloc` fails with no IOP heap, so 0 cannot be passed down).
  **PCM written 2026-11-07** (src/rx/rxsndstrpcm.c, `RxSndStrPcmStream` in its header, 64 x 0x1c at
  0x5217d0, active 0x521ed0, free 0x521ed4; +0x18 the attribute byte is set at open and never read).
  Open: 1 channel mode 0, 2 channels mode 2, anything else fails (no tracks); the read ring is the slot's
  bufSize as is; marks, branches and the end marker as ADPCM; start clamped to the size, not aligned;
  `rxSndPcmOpen(mode, slot iopAddr +0x3c, slot iopKB +0x40)` -- no rate. Update applies only dirty bits
  1 (pause) and 4 (volume, the same formula), clears all of dirty, mirrors 2 / 8, 4 sticky, and writes
  whatever is readable without block rounding. The same PC changes as ADPCM (stream id as user data,
  PCM_SILENT_BUFFER, a silent driver reply through rxSndPcmUpdate, 16-bit samples assumed).
- **Stream sources** 0x1656e8..0x1660e0 (src/rx/rxsndstrstream.c, written 2026-11-06; 64 x 0xc at
  0x5226e0, active 0x5229e0, free 0x5229e4; EE layout next, id +4, open +5, type +6, handle +8): the ids
  a format reads through. Type 0 is an rxFile ring (`rxSysStreamOpenRing`, threshold bufSize / 2 & ~0xf),
  type 1 a memory stream (rxsndstrmem). Free sources are pushed 0..63, so source 63 is taken first.
  Seek also unpauses the file. Pause and Sync do nothing on a memory stream, IsEnd on a ring is "ring
  empty and rxFileGetState bit 8 (position at the size)". **ROM quirk kept:** a failed open leaves the
  open byte set on a free source. **Seen, YAMIPAN_TELEPORT=d0101, temporary logs, removed:** source 63
  opens streamEU/demo/EV01_01_e.xwb, reads it through the ring to its end by about 23 s, closes when the
  silent voice's timer ends at about 58 s, then streamEU/amb/st01amb1_e.xwb opens and keeps running.
  **The silent driver gives no backpressure**, so a stream's bytes run ahead of its voice: up to 7936
  bytes a call go to rxSndAdpcmWrite and nothing plays them. The reading ends long before the voice, and
  this is not a stall. The audio milestone replaces it. Memory sources still reach rxsndstrmem's stubs.
- **Stream source** 0x1656e8..0x1660e0 (`rxSndStrStream*`, 64 x 0xc at 0x5226e0): type 0 an rxFile
  ring (`rxSysStreamOpenRing`), type 1 a memory stream.
- **Memory stream** 0x1660e0..0x166b50 (src/rx/rxsndstrmem.c, written 2026-11-06; `rxSndStrMem*`, 64 x
  0x20 at 0x523a00; EE layout open +4, end +5, iop +6, addr +8, pos +0xc, size +0x10, tags +0x14, markers
  +0x18, arg +0x1c, +0 untouched): the rxFile ring API over memory. 256 markers at 0x524200 (free 0x525a00),
  256 tags at 0x525a04 (free 0x526a04), rxFile's own records, sorted by pos. IOP bodies are read through
  `rxSndIopRead` into 0x522a00, at most 0x1000 a read, one buffer for all streams. Ids start at 1: stream
  0 is never given out. A read stops at the next marker past the position, disabled or not; Consume then
  runs the first active marker exactly at the new position that stops (max 0: every pass, else the first
  max passes; no callback or callback answering 1: jump, -1 or past the end to the end; another answer:
  next marker, position re-read). **ROM quirk kept:** Seek never clears the end flag, so an ended stream
  stays ended and Consume does nothing. **Reached only from** `SePlayer::openStreamBank` (stage SE tables
  and the menu pack, banks with isStream), always with toIop true, through SoundStream::playVolume /
  playPan. Set*Enable 0x166828 / 0x166a10 have no caller in the ROM. **Proved offline:** strmemcmp.py,
  ROM against host on random op scripts, 22,805 ops, 498 callbacks, 0 differences. **Not seen in a
  run:** temporary logs, removed: no stream bank opened and no memory stream opened in 60 to 70 s of
  YAMIPAN_TELEPORT=d0101, 5 or d0401.
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
- **The pair** (`src/sdpr2/sdpr2pair.c`, 2026-11-07): voice[0] main (angle, main spread table),
  voice[1] back (angle + 180, back table); spread 0 = both at cos 45 (no direction). `sdpr2PairTurn`
  0x16c870 never turns more than 90 degrees: a bigger step turns 180 less the other way and toggles
  slot +0xc flip, which swaps the voices' tables (voice[0] angle / back table, voice[1] angle + 180 /
  main table). +0x10 is the last turn's angle (not moved while spread is 0; GetLevels reads it).
  Detune: data 0x2f5fa0 = {3520, 3616, 3520, 3616, 3808, 3856} (/4096) by pair mode; the play sets
  voice[1]'s pitch to it and +0x28 state = 2 (modes 0..1) or 1, `sdpr2PairUpdate` counts it down and
  on the 1 -> 0 step puts voice[1] back on the slot's pitch; `SetPitch` / `SetPitchF` detune voice[1]
  while state > 0. Every voice call goes to both voices and fails only when both do. **ROM quirk:
  `sdpr2PairPause` 0x16d3a8 calls `sdpr2DrvStop` (jal 0x1703f0) with `on` as the mode, not
  `sdpr2DrvPause`, and keeps the slot** (kept).
- **Driver**: `sdpr2Drv*` call the table at 0x537d30 (+0 Play .. +0x24 GetStatus, +0x28..+0x30 unused);
  the only driver is the SE driver `sdpr2SeDrv*` (table 0x2f7a68, `rxSndSe*`), config 8 words at
  0x537d38 (defaults from 0x537d58 {1, 1, -1, -1, 0..}). Ghidra had merged 10 starts (the two type
  exits, eight SE-driver slots); created 2026-11-02.
- **sdpr2drv.c as written (2026-11-07, all 27 + 3, `verify`)**: three more starts Ghidra had
  swallowed, created and named (batch 203): `sdpr2DrvRegister` 0x1700d0 (store the table at
  0x537d30 when null, else -1) and `sdpr2DrvUnregister` 0x1700f8 (inside `sdpr2DrvInit`'s body),
  `sdpr2SeDrvUninstall` 0x1712e0 (no caller). `sdpr2DrvInit` = `SDPR2_TRY(sdpr2SeDrvInstall(config))`.
  `sdpr2SeDrvInstall`: config null -> {keep 1, pause arg 1, -1, -1, 0, 0, 0, 0}, else words 0..3
  and **word 4 copied into 4..7** (ROM quirk), into 0x537d58; `sdpr2DrvSetConfig` copies those to
  0x537d38 (what `GetConfig` hands the types), then registers 0x2f7a68. The game's config
  (`sound.cpp`) is {0, 0, -1, -1, 0..}: keep 0, pause arg 0. Every `sdpr2Drv*` call is
  `SDPR2_TRY(check(table)); SDPR2_TRY(check(slot)); call` (message 0x302d30 twice on failure);
  Stop..SetParam answer 0 after the call, Play / PlayEx / Ext* the slot's answer; `GetStatus` checks
  quietly (-1). SE driver: volumes = gain x phase sign; **note bit 16 (0x10000) is a flag**,
  stripped, then `rxSndSeSetAttr(voice, 1)`; preset -> `rxSndSeSetParamB` of {1, 2, 0, 0} (data
  0x2f7a58); int pitch x 1/4096; status: not open (bit 0) 0, paused (bit 3) 2, else ended (bit 2)
  0 / 1. The driver table is a `Sdpr2Driver` (layout: host, 0x34 on the EE).
- **sdpr2.c as written (TU 40, 2026-11-03)**: every API call is the same shape -- check the type
  (0..7) or handle (0..63), limit / wrap the arguments (xvab 0..0xffff, prog 0..0x7f, note
  0..0x1ffff, volume / spread 0..0x80, angle wrapped into 0..359, pitch 0..0x2000 or 0.0..2.0f,
  mode / on / attr 0..1, preset 0..3), check the table and its slot, call it. The play calls get
  the slot index back from the type and store the type in `slot.type`; the getters
  (`sdpr2GetPitch` / `PitchF` / `Angle` / `Volume` / `Spread`) read slot +0x20 / +0x20 / +0x14 /
  +0x1c / +0x24, but only when the type has the matching **setter** slot. `sdpr2GetStatus` and
  `sdpr2Update` / `sdpr2StopAll` check quietly (no messages). `sdpr2SetSurround` /
  `sdpr2SetPairMode` report out-of-range values but store them unlimited; `sdpr2SetDebug` limits.
  The checks are a macro evaluated twice (`if (CHECK(x) == -1) return CHECK(x);`): failed checks
  print twice and a failed `sdpr2DrvInit` / `PairInit` / `SingleInit` runs a second time
  (`src/sdpr2/sdpr2check.h`). `sdpr2PairInit` / `sdpr2SingleInit` take no argument.
- **sdpr2single.c as written (2026-11-07, all 24, `verify`)**: data 0x2f5fb8..0x2f7a58 is this
  file's (after the pair's mode table 0x2f5fa0): `sdpr2SinData` 0x2f5fb8 / `sdpr2CosData` 0x2f6af8
  (720 half degrees, 2048 = 1.0, **-0x1000 = a zero with phase sign -1**: sin at 180, cos at 90),
  `sdpr2SpreadMainData` 0x2f7638 / `sdpr2SpreadBackData` 0x2f7840 (129 by spread: 1448 = cos 45 up
  to 2048 / down to 0), a zero word, then the four **pointer globals** 0x2f7a48..54 (cos = left,
  sin = right, spread main, spread back; the pair reads them too, plus 0x2f7638 / 0x2f7840
  directly). **Angles: 0 ahead, clockwise, 90 right.** The pan is constant power read at
  (angle + 90) over **two turns**: `sdpr2UnwrapAngle` 0x16fb68 moves 0..359 to the nearest of
  0..719 next to the previous pan angle, so the phase sign does not jump crossing the back
  (`slot.panAngle` +0x18 keeps it; `sdpr2CalcPanDeg`'s state is the same thing for `sound.cpp`).
  Gains come out volume x |table| / 2048 (0..0x80) plus a sign; without surround both signs are
  +1. `sdpr2CalcLevels` 0x16fbd0: five speakers **[0] centre 0, [1] left 330, [2] right 30, [3]
  rear left 240, [4] rear right 120**; the two around deg get cos / sin of the sector (cos table
  0..180), times the spread gain (which 0 = main, 1 = the pair's back voice at deg + 180). The
  single type ignores spread (stores 0x80), plays voice[0] only (voice[1] = -1), and its
  `Update` frees slots whose driver status is 0, stopping them first (mode 1) only when config
  word 0 (0x537d10) is 1 (the game passes 0). `GetStatus` checks quietly; the others print.
  Sdpr2Slot +0x10 is now `levelAngle`, +0x18 `panAngle`.
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
  0 each frame), `gsClearVram` 0x12d770, `gfxInitRenderTargets` 0x12e3f0 (the two display
  records, 256x256 progressive and 512x512 interlaced, not offscreen targets; the GS allocators at
  0x31ad9c / 0x31ada0 / 0x31ad98: `game/gfxtarget.cpp` in "Modules"), `soundInit` 0x1486e0(isPal)
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
    0x151290 / 0x151270 / 0x151280 (`game/debugsys.cpp` 0x151270..0x1512a0, bare `jr ra`, between modelpose and
    demoevent; written 2026-11-07) and the meter's `PerfMeter::addBar` 0x133610 (value, ABGR colour,
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
    **Written 2026-11-03 (TU 30, `src/rena/rena_pkt_vif1pktmanage.cpp`, 0x2eaff0..0x2ec110, `verify`;
    the MSL vector instances 0x2eb7d0..0x2ec110 are the host `<vector>`'s).** The sceVif1Packet is
    the SDK's `{pCurrent +0, pBase +4, pDmaTag +8, pVifcode +0xc, ..}` (0x20; `sceVif1PkReset` =
    pCurrent = pBase, pDmaTag = 0; `Next(pkt, addr, opt)` closes the open tag and writes
    `{0x20000000 | opt, addr & 0x9fffffff}`, pDmaTag = it; `CloseDmaTag` pads to a qword and fixes
    the open tag's QWC). `Reset` takes the next buffer of the ring (index +0x10 from -1), resets each
    list (head / tail 0) and puts a "next" tag (0x20000000, ADDR patched later) at the buffer start;
    `Open(id)` (when not already open) closes the open list, sets the list's pCurrent to the write
    cursor and makes it the list's head or links the list's tail tag to it, so **a list opened
    several times per frame is one chain of segments**; `Close` puts a "next" tag after the
    segment (= the tail) and moves the cursor past it; `Terminate` = Close + chain start = buffer
    start, the start tag -> each non-empty list's head, its tail -> the next one, the last -> an
    "end" tag (0x70000000) at the cursor, FlushCache(0); `Kick` only when a chain exists (DMAC_CTRL
    1, STAT 2 under DIntr, VIF1 QWC 0 / TADR / CHCR 0x1c5). `AddBuffer` answers the new index,
    `AddList` 0 or -1 with "このID(%d)はすでに登録されています。" -- whose %d gets the found list's
    address, not the id (a ROM slip). PC: tag ADDR = byte offset in the frame buffer, Kick ->
    `YamiPan_DrawUiKick` (open-items.md "Decisions" 2026-11-03).
  - **`GamePacketManager`** (`src/game/gamepacket.cpp`, 0x12dd30..0x12dfa0, written 2026-11-03; EE
    0x3c, vtable 0x311040 with only the destructor): `Vif1PktManage` + `{+0x30 open list id (-1 none),
    +0x34 skip-next-kick byte}`. Every call writes the D-cache back first (`FlushCache(0)`).
    `openList`(id) 0x12de00 closes the open list unless it is `id` -- also when none is open, so rena's
    `Close` must tolerate a closed list -- and returns `Open`'s packet; `closeList` 0x12dd30 is `Close`
    + `IsOverflow` (result unread); `endFrame` 0x12ddc0 = `Terminate`; `kickAndSwap` 0x12dd70 = `Kick`
    unless +0x34, then `Reset`. The ctor adds both buffers, lists 0..13, `Reset`, `Terminate`.
    `IsOverflow` 0x2eb030 = buffer size < (open list's cursor or the write ptr) - base. No load or
    store of GameSystem +0x4a0 (= +0x34) exists in the ELF: whoever sets the skip does it through a
    pointer, if at all.
  - **Clocks** (`src/game/clock.cpp`, 0x1350d0..0x135990, written 2026-11-03, TU 42): `ClockNode` /
    `Clock` (EE 0x1c) `{+0 state (ClockState: 1 paused), +4 time, +8 delta, +0xc time2, +0x10 scale
    0x100 (8.8), +0x14 parent, +0x18 PtrList *children (made on the first child, grown by 16)}`;
    `Clock::advance` 0x1354a0 adds ticks * scale >> 8 (0 paused) and recurses, `Clock::create(parent)`
    0x1350d0 / `createChild` 0x135210 (the same body), `setPaused` 0x135190, `getSeconds` 0x135420 /
    `getDeltaSeconds` 0x1353d0 (/ 300). Destructors: `ClockNode::~ClockNode` 0x135550 is the body
    (detach children, free the list, leave the parent's list), `Clock::~Clock` 0x1351b0 and
    `MasterClock::~MasterClock` 0x135330 only call it (Metrowerks: one dtor per class, (this, short
    flags), deleting when flags > 0) -- the ROM's detach stops after the first child.
    `MasterClock` (EE 0x30) adds +0x1c byte (last ctor arg, unread), ticks per frame +0x20, vsync
    total +0x24, interval +0x28, variable-rate flag +0x2c; `update` 0x1352d0(vsyncs) advances by
    ticks per frame, or ticks * vsyncs / interval (unsigned) with the flag.
    `PtrList` `{u16 capacity, u16 count, void **data}` (null = free slot): `add` 0x135860 (gGlobalHeap
    alloc align 0x10 / realloc), `next` 0x1357c0 (null also when `after` is no longer in the list).
  - **Debug meter** (`src/game/perfmeter.cpp`, 0x132f20..0x133800, written 2026-11-02):
    `RcntTimer(tickRate 15625, fieldRate 50, fieldsPerFrame 2)` 0x12f200 (usPerTick = 1e6 / tickRate at
    +0x10; the first one sets T1_MODE 0x83, a gp byte -0x7ac4 remembers it) / reset 0x12f190
    (`src/game/rcnttimer.cpp`, EE size 0x20, written 2026-11-07). A stopwatch nobody reads: the only
    T1_COUNT reads in the ELF are in reset and the constructor (their inlined reset), the only T1_MODE
    writes in the constructor and `rcnt1Start`, and no retail code reads +0x00 or +0x08 back;
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

**`game/mathutil.cpp` 0x133800..0x1350d0 (TU 42, 2026-11-03, batch 071)**: the game's math
helpers, all free functions: `copyVec4`, `matrixToRotYXZ` 0x133830 (Euler of Rz Rx Ry) /
`matrixToRotZYX` 0x1338f0 (of Rx Ry Rz: inverts sceVu0RotMatrix), `normalizeAngle` 0x1339c0 and its
twin `wrapAngle` 0x134a20 (one turn), `colorToFloat` (/ 255) / `colorToFloatGs` (/ 127.5),
`approachAnglesXZ` 0x133af0, `lerpVec4` 0x133be0, `approachVec3` 0x133c40 (step = rate * squared
distance, capped), `getBounds` 0x133d20, `subAngles` / `addAngles` / `addAngle` (fmodf wrap; 76
callers), and **the game's own Cody & Waite libm**: `asinAcos(x, mode)` 0x1344a0, `atan2Cw(y, x)`
0x1346a0, `atan2Octant(y, x)` 0x134940 (the usual atan2: 66 callers), `sinCos(x, mode)` 0x134a90
(sin 0 / cos 1; 38 callers), tables 0x300b90..0x300e48, errno EDOM through newlib's `__errno`
0x123528. Newlib's float libm 0x116fb8 is linked beside it. The VU0 math after the clocks, 0x135990..0x1366c0, is game/vu0math.cpp ("The VU0 math" below).

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
between the file's last function and its static destructor. **packtables.cpp's 43 instances are named (batch 244, ledger in the file):** a map is {size +0, root +4, compare byte +8, leftmost +0xc} (a set: leftmost +8), end() is map + 4; nodes 0x1c (NameMap, ListMap) / 0x10 (IntSet) with {left, right, parent | colour bit 0, value +0xc}; `insertNode` aborts on size == -1, then rebalances with 0x25b590; a vector is {capacity +0, size +4, data +8} (EntryList 0x10 elements, StringVector 0xc), growth doubles the capacity.

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
(a GS block per texture id per packet list; 1 = newly placed), `Tim2Texture::upload` 0x2e1c50 then
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

**`game/gsutil.cpp` 0x230980..0x231410 (TU 20, 2026-11-03, batch 048, all 12 written, `verify`;
cut out of `unassigned@00171300`; the file name is the port's)**: the UI **draw presets** and
**`GsTexture`**. Presets: an A+D run from a table, then FRAME_1 (FBMSK replaced) and ZBUF_1 (ZMSK
set) from the current draw env, each in its own GIF tag (tag 0x307190). Tables 0x3071a0..0x3072f0
of {u_int reg, pad, u_long value}: **Normal** (6) TEST_1 0x30000 (ZTE, ZTST always, no alpha
test), ALPHA_1 0x44 (Cs - Cd) * As + Cd, CLAMP_1 0 (repeat), TEX1_1 0x61 (LCM 1, bilinear),
COLCLAMP 1, CLAMP_1 5 again (clamp: the second write wins), FBMSK 0; **AlphaOnly1** TEST 0x30003
(alpha test always), **AlphaOnly2** TEST 0x3080b (alpha >= 0x80), both FBMSK 0x00ffffff;
**RgbOnly3** TEST 0x37001 (ATST never, AFAIL RGB_ONLY, DATE on DATM 0: where the frame's alpha bit
is 0), **RgbOnly4** 0x3f001 (DATM 1: where it is 1), **RgbOnly5** TEST 0x30000 with ALPHA_1
0x8000000054 ((Cs - Cd) * Ad + Cd), all three FBMSK 0xff000000; **DrawEnv** the Normal run, then the
env's own FRAME_1 / ZBUF_1 / SCISSOR_1. **GsTexture** (0x60, vtable 0x3164e8 at +0x44) derives from
rena's **`Tim2Texture`** (rena_2d_texture_tim2.cpp 0x2e1b70..0x2e2970, EE 0x48, vtable 0x31a7c0 at
+0x44; ctor 0x2e2920, deleting dtor 0x2e2810, `load` 0x2e26c0 ('T' check, **one Tim2Texture per mip
level of the file's first picture**, the top level owns the rest; all FS2's TIM2s seen have one level),
`setupMip` 0x2e22d0, `putTex0` 0x2e1b70, `upload` 0x2e1c50; written 2026-11-03,
`src/rena/rena_2d_texture_tim2.h` has the fields). It reads the file in place and does not own it: +0x40
is the **TIM2 file itself, not a copy** (+0x28 pixels, +0x38 CLUT point into it); +0x3c, a second CLUT
on the global heap sent 4 blocks after the first, has no writer in anything read so far. PC: `upload` is
empty on purpose. **Sony's sample TIM2 library** is `rena/tim2.cpp` 0x2e97a8..0x2eaeb0, all twenty written
(2026-11-07, batches 232 and 235): `Tim2CheckFileHeaer` (the sample's spelling; "TIM2" 1, "CLT2" 2, version 3 or 4
with format id 0/1), `Tim2GetPictureHeader`, `Tim2GetMipMapPictureSize`, `Tim2GetUserSpace` /
`Tim2GetUserData` / `Tim2GetComment` (the "eXt\0" header; header sizes by mip count at 0x310360,
"00PPP```"), `Tim2GetImage`, `Tim2GetClut`, `Tim2GetClutColor` (CSM1 block swap, 0xAABBGGRR), then
`Tim2SetClutColor` 0x2e9d90, `Tim2GetTexel` 0x2ea010, `Tim2SetTexel` 0x2ea188 (the sample's 4-bit nibble
slip kept), `Tim2GetTextureColor` 0x2ea380, the GS loaders `Tim2LoadPicture` 0x2ea490 / `Tim2LoadImage`
0x2ea4b8 / `Tim2LoadClut` 0x2ea610 (an **older cut with no tbp / cbp arguments**: the addresses come from
the picture's GsTex0 and GsMiptbp1 / 2; LoadClut clears CSM / CSA in the header), `Tim2TakeSnapshot`
0x2ea8d8 and the statics `Tim2LoadTexture` 0x2eac98 (psm jump table 0x310430, slices of 0x7ffc0 bytes),
`Tim2CalcBufWidth` 0x2eae00 (table 0x310520), `Tim2GetLog2` 0x2eae70. **Nothing in the ROM calls
0x2e9d90.. except Tim2GetTextureColor -> Tim2GetTexel / Tim2GetClutColor**: the loaders and the
snapshot are dead sample code; on the host they keep their decisions and send nothing (no GS), the
snapshot writes its header and black rows. The sample's `Tim2IsClut2` and `Tim2GetMipMapHeader` are not
linked. Disc
samples (ROM.000 / 001, about 5,500 headers): 4-bit with RGB32 CLUT (most), 4-bit with RGB16, 8-bit with
RGB32, direct 32-bit; none with a linear-order CLUT or a second mip level. Its ctor then `readTarget` 0x231290: an image whose **TIM2 header reserved word
(+0xc) is `Zbuf` / `Draw` / `View`** stands for a 512x512 render target: +0x50 = 1, +0x48 TEX0
(TBW 8, TW / TH 9, TCC 1) at block **0x1800 PSMCT32 / 0x800 PSMCT32 / 0 PSMCT16**, +0x54 / +0x58 =
512. `getSize` 0x2311c0 answers the target's 512 or the base's shorts; `putTex0` 0x231200 (cursor,
block, reg) writes CNT 2 / FLUSH + DIRECT 2 / A+D tag / the target's TEX0 (block ignored) or calls
the base's. Callers: UiDrawer, the textured tracks, UiFont, FUN_00288c00 and the 0x2c5550..0x2cc600
group. On the host the presets go to `YamiPan_DrawUiReg`; the draw module must sample its own copy
of the named target when a GsTexture has m_isTarget (no GS memory).

**`game/uiwidget.cpp` 0x25b8e0..0x25fa50 (TU 33, 2026-11-03, batch 066; cut out of
`unassigned@00252840`; all 56 written, `verify`; class names follow the type strings, the file
name is the port's)**: the S2D **script widgets**. `UiWidget::create` 0x25e260 looks
`S2dWidgetDef::type` up in the **class table 0x2facf0** (42 {id, name}: `prgRot` .. `prgDebug`,
`PrgType` in uiwidget.h) and either builds a widget into the sequence's next slot (counted) or acts
once (slot left empty). `S2dWidgetDef::param[8]` (+0x10) are floats, also where they hold strings /
objects / images indices. Classes (each a `UiWidget`, vptr +0x1c, vtable {dtor, draw, advance,
refresh, getStatus}): **PrgRot** Vt_00316c10 (0x84; prgRot / prgRotRev start angle 0 / pi) rotates /
zooms / pans the sequence's parent matrix (+0x50) -- the map: L1 / R1 or right stick sides turn
pi/64 a frame, right stick up / down zooms 0.6..1.8 (0.02 x tilt), left stick pans 8 px, L1+R1
recentre (on the player when the map shows them), Triangle faces the player's heading; four axis
**UiTween**s (+0x38: angle / scale / x / y) for the scripts' ordered moves (prgRotOrder* on this
sequence's widget, prgRotCallOrder* on another sequence's; *Spd: frames = distance / speed);
draw concatenates the scale **twice** (sic) and keeps matrix / centre for **PrgRotCall**
Vt_00316bf0 (0x24); **PrgRotEnd** Vt_00316bd0 resets to pixel mode. **PrgTrans** Vt_00316b10 (0x28,
translate +0x20) / **PrgTransMsg** Vt_00316af0 (0x44) scrolls a text track by whole lines with pad
Up / Down (the S2D's rMsgUpLimit / rMsgDownLimit / rMsgUseScroll ask it; was `UiScrollWidget`) /
**PrgTransEnd** Vt_00316ad0. Messages: **PrgMsg** Vt_00316b70 and **PrgMsgReference** Vt_00316b30
are built, run once and **deleted in create**; prgMsg replaces `<MC0nC>` / `<MC0nK>` (table
0x2fae40) by memory-card save n's cleared stages (`testStageFlags` 0x15 / 0x2a: any kind, variant
0 / 1) / listed flags; **PrgMsgAniX** Vt_00316b50 slides a text's x alignment; prgMsgOnOff sets a
text track's visible byte. **PrgReturnFlag** / **PrgReturnValue** Vt_00316bb0 / Vt_00316b90 set the
layout's result (+0x38) and done (+0x3c). **PrgMovie** Vt_00316ab0 (0x34) drives FS2's
**MoviePlayer** (`game/movieplayer.cpp`, below; ctor (512, 512, 2 audio channels), `drawFrame`
uploads at tbp 0x800 at 256 + p1 / p2; the audio channel by `GameOptions::_04`); **PrgSndStream** Vt_00316a90 (0x38) a `SoundStream` with its own child clock;
both show **fsm subtitle files** (`fsmRelocate` 0x28fd30: magic "fsm\0", tracks of 0xc at +0x40
{name, count, ranges {start, end, text}}; `fsmFindText` 0x28fb80 by seconds: movie frame / 25, or
`Clock::getSeconds` 0x135420 = m_time / 300). prgScaleNavi / prgScaleNaviScroll / prgMcCard are other
TUs' classes (ctors 0x29e4f0 / 0x29ae30 / 0x2b9d60). After the code: **UiTween** 0x25f5b0..0x25f990
({from, to, length, frame, delay, hold, curve} 0x1c, curve 1 = t squared; start 64 callers, value 29),
`UiShowFlags` 0x25f9c0 (17 bytes, one caller 0x19c180), `UiFont::countLines` 0x25fa10. Before it,
0x252840..0x25b8e0 is another file (vector / tree heavy, 0x253420 has 'そんな馬鹿な', its MSL
instances incl. `mslTreeIncrement` 0x25a850 after it), still `unassigned@00252840`.

**`game/movieplayer.cpp` 0x1faa30..0x1fcb40** (TU 34, batches 067-068): **MoviePlayer** (EE 0x110, no
vtable): the `sceMpeg` at +0 (getFrame = its frameCount), state +0x48 (`MovieState`: 1 open, 2 primed,
4 ready, 8 playing, 0x10 closing, 0x20 file end, 0x40 drained, 0x80 over, 0x8000 abort -- no writer
here), width / height / audio channels +0x4c..+0x54, work area +0x58 size / +0x5c / +0x60 owned, a data
ring's size / memory +0x64 / +0x68 (no writer in this file: the DATA stream is registered only when
set), sema +0x70, `sceIpuDmaEnv` +0x74, file ring +0x98, volumes [4][2] +0x9c, the carve +0xbc..+0xe4
(file ring w*h*4 in whole macroblocks, video ring 0x30000, RGB32 picture, sceMpegCreate work w*h*9/2 +
0x2800, 0x20000 audio per channel, each 128-aligned; `workSize` = their sum + 0x80), video ring +0xe8,
audio rings +0xec[4], data ring +0xfc, SoundFeeders +0x100[4]. One file ring (`FileStreamRing`, primed
at 3/4) feeds `sceMpegDemuxPssRing` 0x116190 (libmpeg) in 0x4000 steps; callbacks: M2V -> video ring
(under the sema), PCM and ADPCM per channel -> that channel's `MovieAudioRing` (the first 0x28 bytes are
the SDK's SShd / SSbd header: type < 0x10 PCM sink else ADPCM, mono / stereo / stereo-16-interleaved,
the ring ends at body size - 0x4000), DATA -> data ring, Nodata -> `feedIpu` (toIPU DMA by hand,
<= 0x1000 bytes, the end code 4 x 000001b7 at 0x2fa440 when the file is spent), Stop / RestartDMA ->
sceIpuStop / RestartDMA, Error / Background -> 1. `decodeFrame` steps open -> primed -> ready (first
`sceMpegGetPicture`) and, once `startAudio` set playing, one picture per call; drained -> waits for the
feeders (states 0 / 3 / 4 count as done) -> over. `uploadFrame` sends each 16x16 macroblock by
`sceVif1PkRefLoadImage` 0x10b380 (libvifpk), **columns outer**, then a TEXFLUSH; `prime` draws the
first one into tbp 0x700 through packet list 13. The rings: **StreamRing** (vtable 0x3112c0, vptr
+0x10 after {size, threshold (7/8 by default), data, owned}; `game/streamring.cpp`, next paragraph;
slots filled / contiguous / freeSpace / write / readPtr / readPos / consume / poll / endPos, the base
ones return 0), **FileStreamRing** (0x311290, +0x14 rx stream ring, +0x1c file size = endPos, +0x20
consumed; open 0x1521c0 = rxFileCallSizeHook + rxSysStreamOpenRing, table 0x2f5a80[8]), **MovieRing** (0x315f50, 0x34: write / read positions, filled, totals, +0x2c the video
bytes in flight to the IPU, +0x30 endPos) and **MovieAudioRing** (0x315f80, 0x68: channel, player,
header). The movie widget passes 2 audio channels and mutes one of them by `GameOptions::_04`.

**`game/streamring.cpp` 0x151f70..0x152750** (TU 41, 2026-11-03, no new names): **StreamRing** /
**FileStreamRing**, cut out of `unassigned@00148db0` between Vt_00311260's last slot 0x151f60 and rxTerm
(game/spkrom.cpp). Both constructors inline one buffer set-up (release, take mem or `Heap::alloc(size,
0x40)` from gGlobalHeap and mark it owned, **zero it**, threshold < 0 -> size * 7 / 8). FileStreamRing
starts paused (+0x18 = 1) with stream -1; every slot answers -1 (readPtr null) while no stream is open;
`contiguous` = min(filled, size - rxFileGetRingReadPos), floored at 0; `consume` = rxFileRingConsume and
adds to +0x20, answering size; `poll` 0, `write` empty (the rx file thread fills the ring). `open(name)`:
size hook first (< 0 -> -1, no stream), then `rxSysStreamOpenRing(name, data, size, threshold)` and
`rxFilePause(id, 0)`; the open ids go to the table data 0x2f5a80[8] (-1 free) and a count sbss gp-0x7a90
(the count rises even when the table is full); nothing reads either. The destructor frees the ring
**before** `rxFileClose` (kept). The file holds four rx jump thunks (0x151f70 rxThEnableDispatch,
0x151f80 rxThDisableDispatch, 0x152730 rxFileCallLoadHook, 0x152740 rxFileCallSizeHook): the port calls
the rx functions. StreamRing's base slots filled / freeSpace / write / readPos (0x1530d0..0x153100) lie in
game/spkrom.cpp's range (unwritten), contiguous / readPtr / consume / poll / endPos in sound/sound.cpp.

**`game/uigame.cpp` 0x25fa50..0x263b90 (TU 21, 2026-11-03, batches 049-052; cut out of
`unassigned@00252840`; the file name is the port's; all 32 written, `verify`)**: the S2D UI's
**game side**. Below it `game/uiwidget.cpp` 0x25b8e0..0x25fa50 (the widget classes and
`UiTween`: previous entry); above it libscf. In it: **`UiFont`** (0x14: glyphs, owned GsTexture, text, spacing 2 / 0);
the two resource files the S2D images can be besides a TIM2, both relocated in place once (magic,
+4 relocated byte, then name / text / picture tables whose entries are offsets from a header base):
**fMsg** message files (+0x10 count, +0x14 / +0x18 / +0x1c name count / table / base, +0x20 / +0x24
/ +0x28 the same for the 16-bit texts, +0x30 entries {name index, text index}, negative none;
`fMsgFind` 0x25fb30, `fMsgRelocate` 0x25fbd0) and **TimM** sprite sheets (+0x10 sprite count, names
as fMsg, +0x20 / +0x24 pictures {name rel +0x28, TIM2 rel +0x2c}, +0x30 sprites of 0x10 {name,
picture, u, v, w, h}; `timSheetFind` 0x25fd60, `timSheetRelocate` 0x25fe10): the item / action
icons ("LW_NIGHTSTICK" on S_OBJ_011.TM2). Names compare with libc **`strcasecmp`** 0x127ea0. On the
disc: 241 TimM and 2,620 fMsg files in ROM.000..ROMD.010 (raw), all relocated by the port's
S2dRef code with every pointer inside the file. The **layout hooks**: `playSound` / `stopSound`
(4 slots: outside a stage the effect player **`getSePlayer`** 0x2c4140 (sbss 0x31aea0; +0 the
effect table: +0xc count, sorted name index +0x428 / +0x42c, entries 0x30 at +0x430) with
`SePlayer::play` 0x292ac0 (volume 0x80 / 0x80, 64-bit handle {voice, id, bank byte}) / `stop`
0x292db0; inside one the stage's `StageSound::playNamedAlways` 0x1d6a70 / `stopNamed` 0x1d6a90);
`readGameValue` / `writeGameValue` / `testGameValue` (packed argument: byte 0 says which of bytes
1..3 are variable indices) over **`getGameValue`** 0x2633f0 / **`setGameValue`** 0x262e70, kind 13
the save's stage flags inline. The tracks' target **"rCharactorPostion"** is the stage's player
(`gStageScene` +0x78: +0x150 / +0x154 x / z, +0x168 yaw): rotate-to takes `normalizeAngle`
0x1339c0 (pi - yaw), pin-rot (x, -z) / 10 too; outside a stage 6000 / 5000 and ~pi / 2.
**Game states** (`UiLayout::testGameState` 0x2604a0, second run, batch 050): 41 names in a bss
table 0x599490 (count 0x31abe8) that the TU's static initializer **0x310b60** fills from sdata string
pointers 0x31aae8..: r16_9 (`GameOptions` +0x20 **wideScreen**), rF01..rF09 / rFDF (the player's
floor, `StageScene::getFloor` 0x19aea0, 0 while the map hides the player), rI00..rI07 (inventory
kinds > n), rW00 / rW01 (player +0x238 / +0x23c weapons), rDunknown / rDunlock / rDblocked / rDlock /
rDNone (`getDoorState` 0x2626f0: stage object by name through `StageObjectList::find1` 0x193ce0 on
gStageScene +0x10, kind (vtable +0x1f4) 2, +0x2a8 4 / 3 / 1-2 / 0), rCheckPoint
(`StageResult::hasAttemptFlag`), rHint (options +0x48 **hints**), rMapP (options +0x44 **mapMode**: 1
always, 0 never, else player +0x234 -> +0x240 -> ** -> +0x214), rMsgUpLimit / rMsgDownLimit /
rMsgUseScroll (a scroll widget's +0x28 / +0x34: 0x25c310..0x25c360), rInGame, rProgramStatusNone /
00..03 (the widget's getStatus + 1), rMapKey0 / 1 (`NaviTable::getTriggerState` of the play record's
stage / variant vs the argument k0 / k1 / k2; `getNaviTable` 0x2955d0 = init pack item 1),
rConnectedPad (pad 0 connected and terminal mode 8). **Sheet sprites** (`findSprite`): key-config
sprites 0x2fafc0 (16 x 0x14 {name, slot, prefix, suffix, _}: rKcDecide..rKcJack -> the options'
ButtonMap, rKcLDecide..rKcLJack -> the menu's draft 0x599470, `getButtonMapDraft` 0x263a70, loaded /
applied by 0x263b10 / 0x263a80) name button icons 0x2fb100 (AI_L2 AI_R2 AI_L1 AI_R1 AI_A AI_O AI_X
AI_H); item sprites 0x2faed0 (12: rItem00..07 slots 0..7, rWeaponL00 / 01 "_W" and rWeaponS00 / 01
slots 8 / 9) print "<prefix><held name><suffix>" from **`scanItems`** 0x2628a0 (player +0x22c list
of {handle, next}; objects of kind 0x15 skipped; merged by name +0x20, count +0x20c) /
**`scanWeapons`** 0x262a90 (firearm = item def kind 2..6, ammo +0x458 / +0x454). Item defs
`gItemDefs` 0x574c10 (160 x 0x20: name[16], +0x10 kind (>= 7 items, below weapons), +0x19 > 1 shows
the count, +0x1e max). Outside a stage the sprites preview the sheet: item slot n = the sheet's
(n + 1)-th item, weapon slots cycle by the counter 0x31ae44 that 0x262840 steps.
**Game values** (third run, batch 051): `setGameValue(kind, value, i, j)` / `getGameValue(kind, i,
j, expected)`, 40 kinds through jump tables 0x3086e0 / 0x308780 (Ghidra does not recover them: read
the asm; `GameValueKind` in uigame.h names all 40). Nearly all are SystemData fields reached through
`getSaveData` 0x23bb40 (options +0x4d0 sound / screen offset / brightness / language / wideScreen /
mapMode / hints and the unnamed bytes as bools, archives, stage flags, variant3ac, the flag table,
medals[1] / [2], the play record, unlock468, both button maps); writing the screen offset calls
`gfxSetDisplayOffset(gfxGetCurrentDisplay(), x, y)`. The flag table is reached with a **null-checked
+0x520 adjust**, the code shape of a base-class conversion: SystemData may derive from FlagTable (the
other parts are plain members). Archive ids >= 150 read stage (id - 150)'s flags (2 unless set, or
always 2 with unlock468); writes to archive 64 also draw variant3ac. Kinds 21 / 22 / 30 / 31 read the
navi menu (`getNaviMenu` 0x29efe0, sbss gp-0x79f8: +0x2a8 ints, +0x584 bits, +0x5e0 / +0x5f0 bytes).
Kind 25 answers 3 / 0x11 / 0xff by index, else 0xfe (unread). Kinds 36 / 37 are **pending clock
amounts**: `StageResult` clock2 (stage 29's score: amounts 0x2fae60, 0..100000, 9) / clock (0x2fae90,
0..4500, 7; counts in sdata 0x31abd8 / 0x31abdc): an index arms (state 1, target = amount), 253
cancels (|= 2), 254 adds (|= 8, value += target, target 0, |= 0x10), 255 marks (|= 4); read with one of
those as the expected value it answers it when the bit is set, else the armed amount's index
(`StageClockState` in stageresult.h). Kind 29 is the play record's +0x10 (count-up clock), 1 on stage
29. The player hooks the tracks inline exist out of line for the map widgets 0x25d8a0 / 0x25e080:
`getPlayerMapPos` 0x2625c0, `mapShowsPlayer` 0x262640, `getPlayerHeading` 0x2627e0;
`stepWeaponPreview` 0x262840 (UiWidget::create) steps the preview counter outside a stage, in one
calls `PlayerMotion::refresh` 0x1e66c0 on the player's +0x224 (guess: the motion set by weapon);
`isHighestWinsStage` 0x261cf0.
**Text variables** (fourth run, batch 052): a text widget (0x25cab0) expands its string through
**`UiWidget::expandTextVariable`** 0x260bc0 (a0 the widget, unused): answers 1 when the 256-byte buffer
now holds 16-bit text (the widget then `setTextAt`), 0 when it holds a message name or nothing (then
`setMessageByName` on its fMsg; the widget asks first without the file, then with it). Names from
table 0x2fb120 (24 x 0x14, the sprite-slot record: rItem00..07 suffix "_C" = caption, rItem00_I..
= the item's own message, rWeaponL / S 00 / 01 slots 8 / 9, L / S the large / small layout) print
"<prefix><held name><suffix>"; outside a stage the fMsg's own entries preview them (item slot n = its
(n + 1)-th item-def-named entry, weapons cycling by 0x31ae44). Anything else goes to
**`formatTextValue`** 0x2612c0: 28 names 0x2fb300 (`UiTextValue` in uigame.cpp): rItem00_N.. counts,
rWeaponL00_N / S00_N (weapon 0) and L01 / S01 (weapon 1) ammo "%d/%d", rPurpose (**`getPurposeText`**
0x195d50, a 16-bit text with \n / \N breaks; outside a stage 0x2faeb0, Japanese "sub-goal: sample"),
rLnBestTimeD1 / D2 (the navi menu's stage +0x46c, record variant 0 / 1 of the play record's kind),
rBestTime / rBestScore (the play record's record), rMcTotalTime0..3 (the memory-card menu's
**`Memcard`** at sbss 0x31ae94 (gp-0x79dc, owned by `PrgMcCard`): `m_files[n]` play time as
h:m:s), rOptScrOfsX / Y and rBrightness (- 9) signed "+%d", rPlayTime / rPlayScore (`StageResult`
clock / clock2 value), rBonusTime / rBonusScore (their pending target, "+%ds" / "+%dp"). Times are
**25 ticks a second**: minutes:seconds:hundredths (t / 1500, t / 25 % 60, t % 25 * 4); stage 29 shows
numbers. Every value is sprintf'd as chars and widened in place by **`wstrFromAscii`** 0x2d4440
(copies src first when dst == src); outside a stage counts / ammo / times show samples ("999",
"999/999", "12:34:56", "12:34:<slot>"); the navi sample is left narrow (a ROM slip).
The class after it, 0x263b90..0x2640e8, is **`game/stagealert.cpp`** (its own TU; next paragraph).

**The stage's alarm: `game/stagealert.cpp` 0x263b90..0x2640e8 (2026-11-06, batch 185, `verify`, 8 of 8 functions; the file name is the port's).** `StageAlert`, scene +0xf4, a stage's only, EE 0x1c: state +0, frames in it +4, count +8, this frame's request +0xc, who started it +0x10, a `StageFade` +0x14, +0x18 cleared and never read. A script asks **every frame** (natives 0xc7, 0xc8, `show`; a stage script such as ALERT01 does); `update`, in `StageScene::update` when no event runs, starts the pad's rumble ALERT on the first request and goes Idle -> (Start, passed through) -> Running, counts frames there, and on the first frame with no request stops ALERT and goes Ending. `show` does nothing with `GameOptions::alertMode` (+0x10, was `_10`) at `GameAlert_Off`; 1 is flash (a foe's, `CharacterBrain`: `flash`), 2, the default, is `StageParty::warnAttacker`. A request for anyone but the one shown does not displace it. **ROM slip kept:** leaving Running, the count is converted with count / delta * 30 where count * delta * 30 was meant, so one frame run at 25 a second is already 750, past Ending's 30, and Ending lasts exactly one update. The picture: `flash` and `cry` run the fade to 0.25 in 0.01 s from 0.25 with colour 0x80 (half red in RGBA, alpha 0x20 of 0x80: a quarter of the way to it), and `draw` only fills while it is Running, so the veil lasts one or two frames. `cry` (native 0x13e) adds `playQuestionVoice` and the rumble CAUTION. 0x263b90 is empty: `beforeFileEvent`, which `StageFileEvent::queueFiles` calls first. `tools/reverse/alertcmp.py`: 40 scripts, 61,078 ops, ALERT started 1,054 times, 0 differences; it catches the two bugs `--plant` puts in. **Seen, temporary builds, removed:** stage 2, flash forced 30 frames on and 30 off: work shots show the whole frame under a dark red veil against the same view clean; `show` forced for 120 frames at 25 a second: state 2, ALERT playing, count 119, then 89,250 (119 / 0.04 * 30 + 0.5), rumble off, idle. No forced flash on a wide window, but the fill is `gsFillRect` as the scene's fade. Unforced, no pad, 55 s each: d2303 and d0401 call `show` from their own scripts, stage 9 does not; no `flash` or `cry` in those runs; no crash in any of them.

**`game/wstring.cpp` 0x2d4150..0x2d46a0 (TU 22, 2026-11-03, batch 053, all 5 written, `verify`; cut
out of `unassigned@002b0d50`; the file name is the port's)**: the game's **16-bit strings** (the UI
text is u_short units ending in 0). `wstrcmp` 0x2d4150, `wstrstr` 0x2d42a0, `wstrFromAscii`
0x2d4440 (each byte widened), `wstrcpy` 0x2d4510, `wstrlen` 0x2d4650; wstrlen is inlined into
wstrcmp / wstrstr. Every one answers 0 / null for a null or **odd** pointer, except that a null
source leaves the copies' dst untouched and answers dst. **The copies answer the end of what they
wrote (the 0 unit), not dst**; dst == src goes through a new[] copy. **`wstrcmp` compares only
wstrlen(a) units** (b's length is counted and dropped): b starting with a answers 0 (callers
0x209d90 / 0x1cb3e0 test == 0, so a prefix matches). `wstrstr`: first match, an empty needle answers
hay (caller 0x25ce30). Before it a FontData-derived class (ctor 0x2d4110, vtable 0x319ff0 at +0x10,
dtor 0x2d40a0; 0x2d3e20 / 0x2d4060 use FontData::nextGlyph); after it 0x2d46a0 (a lookup through
+0xc / +0x40 tables) and the flag pair 0x2d4780 / 0x2d4790 (sbss 0x31af14 = 0 / 1).

**`game/gamepaths.cpp` 0x28ff60..0x2908d0 (TU 23, 2026-11-03, batches 054-055, all 14 written, `verify`;
cut out of `unassigned@002681e0`; the file name is the port's)**: the **stage's file paths** and the
**resident init / language packs**. Paths from a `StageSelect` (stage, kind, variant): the digit is
**1, or 2 for any variant but 0** (not variant + 1), the letter **e / n / h for kind 0 / 1 / else**
(`StageKind` Easy / Normal / Hard in sysdata.h, read from the letters). `stageMoviePath` 0x28ff60:
event 0 `movieEU/mv<stage><lang>.d2b`, else the **event movie table 0x2fb520** (13 x {stage,
variant, branch 0 = any, path function}, count sdata 0x31ac2c) whose entries are
`eventMoviePath<id>` 0x2ec160..0x2ec520 (`movieEU/ev<id><lang>.d2b`, buffer 0x5a5460; the ids are
the stage's own: ev1404 / ev1405 are stage 14 variant 1 branches 1 / 2); `findStageMovie` 0x290080
the same search (caller 0x17f7c0 passes `StageResult::attempt[15]` and opens the notice dialog on
-1). `stageScriptPath` 0x290100 `script/s<stage>/c<n><k>/export/s<stage>c<n><k>.slt`,
`stagePackPath` 0x2901e0 `data<region>/pks<stage>c<n><region><getFileSuffix(GameOptions::_04)>.enc`
(both write the caller's buffer, answer 0 / 1). Screen layouts in static 0x20-byte buffers
0x599540..0x5995e0: `stageScreenPathH` / `E` (none for stage 1 variant 0), `stageScreenPath`
0x290460 (`...m<n><lang>.d2b`, or `...m<n>b<lang>.d2b` with alt on stages 22 / 27; caller
0x20ad70), `stageScreenPathMp` 0x290590 (`...m<n>mp<lang>.d2b`; callers 0x1a3a60, 0x20b010). The
packs: `loadInitPack` 0x290840 (dataEU/packinitEU.pak, sbss 0x31ae60 + loaded byte 0x31ae64) and
`loadLangPack` 0x290670 (dataEU/packinit<DE|FR|ES|IT|EN>.pak by `getLanguage` 6..9, else EN; 0x31ae68
/ 0x31ae6c; caller the menu flow 0x2b5770); getters `getInitPackData` (slot 5, by name),
`getInitPackItem` / `getLangPackItem` (slot 6, by id), `getInitPackItemSize` 0x290780 (slot 8).
**PackFile's slots 5..8 are thunks** 0x131d30 / 0x131d20 / 0x131d10 / 0x131d00 into the parsed pack
at +0xc4 (0x2314a0 / 0x231480 / 0x231450 / 0x231430 -> 0x231770 by name (strstr over the entries'
names, recursing into nested packs at +0x28) / 0x231860 by id (the id's high half walks nested
packs)); an item entry is 0x10 at +0x20: data +8, size +0xc. Around it: sound-table code
0x28fa90..0x28ff54 (called from 0x290930..0x290b10) and the SoundStream / SePlayer wrappers from
0x2908d0, so this may be the middle of one larger file.

**`game/fontdata.cpp` 0x1fcb40..0x1fe6b0 (TU 24, 2026-11-03, batch 056, all 31 written, `verify`;
cut out of `unassigned@00171300` between the movie player (last dtor 0x1fcad0) and an unread small
run at 0x1fe6b0; the file name is the port's)**: **fonts**. A font file starts with the magic
**"knj\0"** (0x305b38); `FontData` +8 glyph count, **+0xc cell width**, +0xe line height (both 1/16
px), 0x10-byte entries from +0x10 sorted by code (+0) with the glyph rect {u, v, w, h} at +8 (1/16
texel), the entry after the last the fallback (binary search, a do-while). Wide text (u_short) looks
codes up as they are; **byte text** (`*Mb`: `countLinesMb` 0x1fe1e0, `lineWidthFixedMb` 0x1fe410,
`nextGlyphMb` 0x1fe5b0) takes a byte < 0x20 signed (control or >= 0x80) and the next as **b0 << 8 |
b1**, any other byte as **b << 8** (ASCII 'A' is code 0x4100). `lineWidthFixed` 0x1fe370 is
lineWidth in int 1/16 px. **Printers**: base `TextPrinter` (vtable 0x315fb0, dtor only; EE fields
+0 RGBA 0x80808080, +4 the sceVif1Packet, +8 / +0xa origin in pixels, +0xc flags 1 centre x on each
line / 2 centre y on half the first line, **vptr at +0x10**) and **`FontPrinter`** (vtable 0x315ff0,
EE 0x40; ctor 0x1fe0c0 (x, y), deleting dtor 0x1fe020): +0x14 font, +0x18 image (set = texture
owned), +0x1c `Tim2Texture`, +0x20 GS block (negative: **`gfxGetVramBlock(3)`** 0x12f170, the
table 0x2f3850 {0, 0x800, 0x1800, 0x2800, 0x2800, 0x3700}), +0x24 glyph count, +0x28 / +0x2c the
last print's start, +0x30 / +0x34 its end (1/16 px), +0x38 spacing 0x20, +0x3c line spacing.
Slots: 0 dtor, 1 `nextGlyph` (byte), 2 `setDrawState` (packet, bilinear: ALPHA_1 0x44, TEXA
0x8000008000, TEX1_1, TEST_1 0x3000d, TEX0_1 via putTex0, TEXFLUSH), 3 `uploadTexture`, 4 `print`
(x, y, fmt, ...: newlib **`vsprintf`** 0x12cab8 into 520 bytes, then one sprite per glyph as REGLIST
words PRIM 0x156 / RGBAQ (Q 1.0) / UV / XYZ2 (Z 0xff0fffff) / UV / XYZ2 (Z 0xffffffff) into a run the
caller opened), 5 `printScaled`, 6 `measureLines`, 7 `textWidth`, 8 / 9 `cellWidth` / `lineHeight`,
10 `countLines`. `setFont` 0x1fd260 (**`strncmp`** 0x1286a8 on the magic), `setTexture` 0x1fd0d0
(unowned), `setImage` 0x1fd160 (owns a new Tim2Texture), `releaseTexture` 0x1fd210. Users: the
**wide printer** 0x2d3700..0x2d4150 (vtable 0x319ff0, subclass: ctor 0x2d4110 calls FontPrinter's
at (0x700, 0x700); shares slots 2 / 3 / 8 / 9) made by 0x2b1010 (setFont + setImage), and 0x19fcd0
(setFont + setTexture on gp-0x7a7c +0xbc's printer). **Vtable 0x315fc0** (`SpritePrinter`, 7 slots:
print / printScaled and five constant answers) has no RTTI and no store: never constructed, and its
prints fetch glyphs through their own slot 1 (= printScaled), so they only fit a FontPrinter object.

**`game/gfxtarget.cpp` 0x12dfa0..0x12f190 (TU 25, 2026-11-03, batch 057 with 13 names, all 15
written, `verify`; re-pinned from 0x12dfa0..0x12ea60: the display records and gp-0x7ad8..-0x7ac8 are
used only up to `gfxGetVramBlock`, `RcntTimer` starts at 0x12f190)**: the **display**. Two **display
records** `GfxDisplay` (0x540, bss 0x4ae280): **0 = 512x512 interlaced field mode, 1 = 256x256
progressive frame mode** (`gfxSelectDisplay` 0x12ea70 stores the request gp-0x7ac8; 0x2c4320 selects
1 on entry and 0 on exit; `gfxFlipDisplay` switches gp-0x7acc on the next ready frame with
sceGsResetGraph (0, inter, PAL, ff) inside a cleared vsync callback). Record layout: +0
`sceGsDispEnv` (display buffer PSMCT16 at block 0, FBP forced 0), +0x28 DISPFB1 = DISPFB2 with DBY 1,
+0x30 DISPLAY1 = DISPLAY2 with DH - 1 (with PMODE 0x8023 = EN1 + EN2 blended at ALP 0x80: **the
interlace flicker filter**; record 1 PMODE 0x62, circuit 2 only), +0x38 / +0x3a size, +0x3c / +0x40
`gfxSetDisplayOffset`'s x / y (DX = x * (0xa00 / width) + 0x290 PAL / 0x27c NTSC, DY = y + 0x48 /
0x32), **+0x80 `GfxDrawEnv`** (0xf0: GIF tag NLOOP 14 / 8, `sceGsDrawEnv1` PSMCT32 size x size, ZTEST
GEQUAL, Z32, FRAME_1 FBP 0x40 = block 0x800, ZBUF_1 ZBP 0xc0 = block 0x1800, then `sceGsClear` from
0x700 (record 1: 0x780); clear RGBA at +0x130), **+0x180 `GfxCopyPacket`** (0x3a0, built by
`gfxBuildCopyPacket` 0x12ec10): 57 A+D writes that draw the draw buffer as a PSMCT32 texture (FBP /
FBW / PSM from the draw env's FRAME_1, TW / TH from its scissor, TCC 0, bilinear) into the display
buffer (dithered: DIMX 0x5150626247407373, TEXA 0x80/0x80, PRIM 0x156, RGBAQ 0x80808080, ALPHA Cs *
FIX 0x80) as **64-texel sprites** (UV half-texel inset; record 1 reads 32 lines fewer: 224), unused
of the 10 strip slots NOP (reg 0x7f). gfxFlipDisplay sends it on DMA channel 2 (`sceDmaSendN`, 0x3a
qwords), puts the draw env, calls **`TexArea::resetLists`** 0x142df0 and turns the clear back on
black. Getters: `gfxGetCurrentDisplay` 0x12e050, `gfxGetCurrentDrawEnv` 0x12e080 and **its twin
`gfxGetDrawEnv` 0x12e0b0** (same body, 17 3D callers), `gfxGetCurrentCopyPacket` 0x12e020,
`gfxSetClearColor` 0x12dfa0 (gfxSetDrawEnvClear on the current env). **GS allocators**:
`GsBlockMap` (0x80c, vtable 0x311120: {first, last, 0x4000-bit map (1 = in use), vptr +0x808}; slot
0 `mark` 0x12ea60 is an inline virtual emitted here, a tail jump to `setRange` 0x1438c0; ctor
0x143f50 / non-virtual dtor 0x143f00) at gp-0x7ad4 with View / Draw / Zbuf / 0x2800..0x3700 marked;
**`TexArea` derives from it** (vtable 0x311100 overrides all three slots) at gp-0x7ad0, blocks
0x3700..0x3fbe, 14 lists x 0x20 slots; `GsRegionAlloc` (0x1838, vtable 0x31a7b0, ctor 0x2e14d0(base
0x2800, size 0xf00), deleting dtor slot 0 0x2e1440) at gp-0x7ad8, handed to **`drawSysInit`**
0x2d7cf0(0x700, 0x700, 512, 512, alloc) (unread: about nine draw-system objects). Release deletes the
region allocator and the block map, never the TexArea.

**`rena/rena_2d_texture_vram.cpp` 0x2e0840..0x2e1700 (2026-11-07, batch 198, all 11 written, `verify`;
the file name is a guess)**: **`GsRegionAlloc`** = {+0 base block, +4 size, +8 used list, +0x618 free
list, +0xc28 the first region, +0xc2c the texture last bound, +0xc30 128 `GsRegion` records, +0x1830
spare records (linked through +4), vptr +0x1834}. Both lists are one inlined template, a list of
pointers over a fixed pool of 128 nodes {count, head, tail, spare, nodes of {next, prev, value}}.
**`GsRegion`** (0x18): {+0 lower neighbour, +4 higher neighbour, +8 first block, +0xc blocks, +0x10
pinned, +0x12 state 0 free / 1 to send / 2 sent / 3 released, +0x14 the Tim2Texture}. `assign`
0x2e0a10(tex, pinned) -> `alloc` 0x2e10c0 = best fit (`findFree` 0x2e0a90, smallest size >= wanted),
else free the oldest unpinned used region and retry (hangs if all are pinned), split the rest into a
new free region above; `freeRegion` 0x2e0b80 merges free neighbours and frees released ones first;
`GsRegion::release` 0x2e0840 (Tim2Texture's dtor) only marks it released. `upload` 0x2e09b0 sends a
to-send region's pixels; `bind` 0x2e0850(cursor, tex, flags) writes TEX0_1 (1), TEX0_2 (2) or a VIF
UNPACK of {TEX0, TEX0 with CBP + 4 = the second CLUT} to VU1 0x26 (4). rena_draw 0x2e7240 does
assign(tex, 1), upload, bind, then unpins; it picks the mip with `Tim2Texture::getLodScale` 0x2e16c0
(the float at +8 of the TIM2 file, the header's reserved bytes) and `getMip` 0x2e16d0.

**`game/gsblockmap.cpp` 0x142dd0..0x144080 (TU 31, 2026-11-03, batch 062 with 8 names, all 14
written, `verify`)**: the GS allocators, the whole run between the SIF library and
`sound/sound.cpp`. **`GsBlockMap`** slots: 0 `mark` (0x12ea60, in gfxtarget), 1 **`alloc`**
0x143fd0(count, fromTop, pageAligned) = `findFree` 0x143640 then mark in use, 2 **`free`**
0x144040(block, count). `findFree`: count 0 -> -1; **runs of 1..3 blocks are looked for inside one
32-bit word** (every word touching [first, last), the lowest bit position that fits) unless
pageAligned; longer runs (forced) are whole free words inside [first, last); fromTop scans words
downwards. **`setRange` 0x1438c0**: under 0x200 blocks bit by bit; else head / whole words / tail,
where **the ROM counts head and tail bits as (block & 0x7ff) >> 6, not block & 31** -- right only
by coincidence for the game's own ranges (0x800-aligned, 0x2800..0x3700, 0x3700..0x3fbe): kept.
**`TexArea`** (Vt_00311100, +0x80c list count, +0x810 list table on the global heap): `mark`
0x142e70 / `alloc` 0x1430f0 / `free` 0x142fa0 do the base's work (**alloc always from the top,
bit-level: its arguments are ignored**) and then an inline clip: every list's end page drops to the
first page with a block in use (slots ending past it emptied), so **long-lived allocations come from
the area's top and the per-frame texture lists use the pages below them**. **`TexAreaList`** (0x14,
one per packet list, `TexArea::getList` 0x142dd0): first page (first + 31) / 32, end page, next
page, slot count, slots (0xc: {key, first page, end page}, empty {0, -1, -1}). **`place`
0x143510(list, &tbp, Tim2Texture)**: keyed by the texture's TIM2 file (+0x40): already there ->
TBP0, answers 0; else pages = (`Tim2Texture::getVramBlocks` 0x2e1c30 = (+0x20 + +0x30 + 31) & ~31,
image and CLUT blocks) / 32 from next page, wrapping to the first page when the rest is short,
**-0x20 and 1 when it does not fit at all**; `insert` 0x143450 empties the overlapped slots and
takes the first empty one (slot 0 when none). `resetLists` (every frame, gfxFlipDisplay) empties
the slots but keeps next page, so placement goes round robin across frames. PC: nothing on the host
uploads into GS memory (the no-GS-memory rule), so the drawer keeps its `TexAreaList *` only to
pass along; `UiDrawer::m_texList`, `UiLayout::draw` and `UiDrawer::begin` now take the type.

**`game/stageloop.cpp` 0x17f150..0x17fce0 (TU 43, 2026-11-03, batch 073 with 45 names, all 9
written, `verify`; the file name is the port's)**: the **stage loop**. The menu flow **0x2b60b0**
builds the stage's loader (a `PackFile` on `stagePackPath` with a 0x1800000 heap buffer, or a bare
`FileLoader`), hands it to **`setStageLoader`** 0x17fc90 (sbss gp-0x7a78; deletes the old one), and
starts **`stageThread`** 0x17f160 in **ThreadSlot_Game (3)** with the stage script's path (0x5997c0,
`stageScriptPath`), then waits for the slot and sets the loader to 0. `stageThread`: clock mode
(`StageResult::begin`: Countdown on stage 29, CountUp when `StageSelect::_10`, else None),
`GameSystem::beginStage` 0x12cce0 (heap mark, meter reset), a **`StageSound`** (0x314, sbss
gp-0x7a6c: SE player, four streams, 0x40 effect slots; one per stage, handed to every scene),
**`stageLoad`** 0x17fb90 (deletes and rebuilds **`StageFiles`** 0x160 over the loader, sbss
gp-0x7a74 = `getStageFiles`, and the **`StageScene`** 0x130 = `gStageScene` gp-0x7a7c; then
`loadCommon` (spotlight / shadow_00), `loadList` (../data/listfile/s%02dlist.lst) or the empty
sub-scene list, `load(script)`, `setupCameras`, `StageObjectList::rebalance`), the **e screen and,
with hints on, the h screen**, `beginPlay`, `StageParty::resetSaved` (scene +0x88), the frame copy's
**ALPHA FIX = the options' brightness** (`GfxCopyPacket` +0x114) and the meter on. **Per frame**:
`StageScene::update` 0x199400, `draw(getActiveCharacter())` 0x198970, `StageResult::update` into
packet list 10, then one of: **game over** (`isGameOver`: scene +0x118 and 0x2b1c50() == 5) ->
`stageGameOverMenu` 0x17f890; **cleared** (`isCleared`: +0x11c and 0x2b1620() == 2) ->
`stageCleared` 0x17f7c0 (notice dialog when `findStageMovie` < 0) and the loop ends; **a sub-scene
request** (scene +4) -> the scene parks in gp-0x7a70 and a `StageScene(true, null)` over the same
files plays (`enterSubScene` with the parent's +0xec event slot 3), back when `isSubSceneDone` (+0
set, +8 null: sound unmuted); **start** (`wantsPauseMenu`, pad 0 bit 0x800) -> `stagePauseMenu`
0x17f970 (thread 0x20ad20 in slot 4: 0 resume, 1 restart, 2 continue, 3 quit, 4 navi jump);
**select** (`wantsMapMenu`, bit 0x100) -> the map menu 0x20afe0 / 0x20afa0 (thread 0x20b010). Every
menu is bracketed the same way (inline in the ROM): `RumbleList::removeUnlooped` on scene +0x30,
`StageResult::paused`, meter off, ALPHA FIX 0x80, the scene clock (+0x60) paused; menus poll with
`threadYield(1)` + `StageScene::drawPaused` 0x198fc0. A restart / continue runs **`stageRestart`**
0x17fa70: party ids saved (0x598b20), `StageSound::stopAll` and wait `isStopped`, `stageLoad`, party
restored, the e screen only while `!StageResult::hasAttemptFlag`. **How the stage ended goes to
`StageSelect::exitKind`** (+0xc, was `_0c`; `StageExit`: -1 none, 0 game over, 1 quit, 2 navi, 3
cleared), written through `getSaveData()` + 0x4c8. The thread ends deleting files, scene and sound,
`StageResult::releaseLayout`, `threadExit`. `stageAnswerZero` 0x17f7b0 (answers 0) is only a word
of the handler tables 0x318038 / 0x318058 (the 0x2887a0.. classes). 0x17fce0 (answers 25) is slot
108 of the bullet class vtable 0x312cdc, so the next file starts there.

**`game/stagesound.cpp` 0x1d3dc0..0x1d7c50 (TU 44, 2026-11-03, batches 074-077, all 45 written, `verify`; the file name is the port's)**: the **stage's sound**, `StageSound`
(0x314, `game/stagesound.h`, `layout: host`): +8 an **`SePlayer`** (0x1298: +0 the SE table, +4 16
`SoundSeBank`, +0x44 16 `SoundStream`, **+0x84 the master volume** 0..0x80 with its fade
+0x88..+0x94 = `fadeMaster(frames, from, to)` 0x292360; handles with the **top nibble set are
paused**: `pause` 0x291d80 stops the voice and answers handle | 1 << 60, `resume` 0x291c00 plays the
entry again, `isPlaying` 0x292f60 counts a paused handle as playing; `playUnscaled` 0x292830 =
`play` with the voice record's +0x26 flag 0, not under the master volume), bytes +0xc / +0xd (set
by the update 0x1d5580, unread) and **+0xe the attr** a paused source gets, **four 0x3c stream
channels** from +0x10 (`StageStreamChannel` {`SoundTrack` *, cue +4, next cue +0x1c (6 words:
source, def, owner, variant, +0x10, fade-in frames at 30 fps scaled 25 / 30), volume float +0x34,
state +0x38: 0 idle, 1 starting, 2 playing, 3 pausing, 4 paused, 5 stopping}; **0 / 1 the music**
(pause / resume fade them 128 <-> 64 x volume over 10 frames), 2 / 3 the cue streams the stage
objects start through 0x1d4c10 / 0x1d4d30 (0x2a6380.. callers)), **0x40 effect slots** from +0x100
({kind, pointer}: 2 = `StageEffect` 0x18 {entry, 64-bit handle +8, stopped +0x10} from
`playNamed`, 1 = a positional `StageSoundSource` (vtable 0x315a20 over 0x315a80, ctor 0x1d6d10,
dtor 0x1d3e90; +0x100 handle, +0x108 entry, +0x115 stopped)), +0x300 the free-slot search start
(does not wrap; the update resets it), +0x304 the highest slot used, **+0x30c the turn-down state**
(0 normal, 1 turning down: master to 0 over 12 frames, 2 down: the update 0x1d5e40 then pauses the
**resumable** effects (`SeEntry` +0x1d) through `setEffectsPaused(true)`, 3 turning up: master to
0x80 over 12 frames, effects resumed, the update then goes 0) and **+0x310 why** (1 paused, 2 muted).
While turned down `playNamed` plays only resumable effects, started paused; `playNamedAlways`
(UiLayout::playSound) always plays, unscaled. `SoundTrack` (0x90, `game/seplayer.cpp`) is a `SoundStream` with a name (default "snd" 0x309778), an fsm subtitle file (+0x7c) and an
**StDT** stream table (+0x60, relocated once by `setTable` 0x290d60); 0x19d9e0 hands the four
streams the stage's files (`setSubtitles` / `setStreamName` / `setStreamTable`). **The update**
`StageSound::update` 0x1d5580 (StageScene::update / drawPaused, 0x287f20 while settling; a1 the
listener = the active character, a2 playerFlag from StagePlayer 0x214f60, a3 sceneFlag from
StageScene 0x19b900, kept in +0xc / +0xd; +0xe the sound attr `StageScene::getSoundAttr` 0x19ae10 at
the listener, a Kind9 listener's from its +0x300 object's "bn_head" bone via `BoneNode::find`
0x177270) runs `updateSlots` 0x1d5e40 (SePlayer::update, frees ended slots, +0x300 / +0x304
recomputed, the turn-down settles when the master fade (+0x90 -1) is done, then every positional
source: pan / distance from `stageSoundLocateFacing` 0x1d7a30, ratio = distance / `SeEntry::range`
+0x10, beyond 1 stopped unless resumable (then clamped), `SeEntry::placed` +0x1c sounds through
`SePlayer::playPlaced` 0x292520 / `setPlacement` 0x2931b0, the alternate variant `+0x2e` entries on
when the flag the source follows (+0x114: scene, else player) is set, a change restarts it) and
`updateChannels` 0x1d56a0 (channel state follows the SoundTrack; music starts the next cue with
`playEntry` 0x291370 and a fade of fadeFrames * 25 / 30; cue streams 2 / 3 are placed at their source
bone with `playPlaced` 0x290fd0 / `setPlacement` 0x291770, occluded by `StageScene::raycast` 0x198170
(mask 0xe42600f, flags 0x200, a face without flag 0x400 blocks: ratio 1.1, a thin one doubles the
distance unless a second ray, flags 0x600, hits too); a player-flag change stops a cue and restarts it
from the position stopped at; playing / paused channels pause (fade 12, SoundFadeEnd_Pause) when
turned down -- music only when muted -- and resume unless `StageFade::isFullInMode2` 0x1512a0 of scene
+0xfc). Pan = -(angle to the sound - listener facing), facing = atan2Octant(matrix row 1 x, y); a
Kind11 listener whose +0x300 object's record (bss 0x539b10 [difficulty][m_itemDef], 0xfc each) has
kind 0xf divides the distance by clamp(1 - (pi/4)/|pan|, 0.4, 1) * 2.5. **The cues** (batch 077,
all 45 written, `verify`): the stage objects (0x2a6380..0x2a6830) queue a cue on channel 2 / 3 with
`playCue` 0x1d4d30 (the channel from `pickCueChannel` 0x1d4e70: a free channel scores 100, a busy
one whose cue's `SeEntry::priority` +0x2c is >= the new one's 200 for the same owner / 50 for
another, else 0; then rejected -0x7ffd beyond range from the owner's bn_head, -0x7ffb behind a
wall or a thin wall past half the range, -0x7ffc no channel, -0x8000 no StDT table) or `setCue`
0x1d4c10 (a given channel): next cue = {head bone, entry, owner, variant (< 0 random abs/randNext,
> 0 `cycle++ % variants`), 0, 0}, what plays fades out over 10 frames to stop (state 5) and the
update starts the queued cue. `stopCue` 0x1d47e0 / `stopCuesOf` 0x1d4920 (4 frames) / `hasCue`
0x1d4a30 / `isCuePlaying` 0x1d4aa0 test the queued cue, else the playing one; `findCue` 0x1d5140
searches channel 2's StDT table (`SoundTrack` +0x60 `StreamTable`: count +0x10, 0x30 SeEntry
records from +0x40, linear strcmp). `getCueSubtitle` 0x1d46e0 (StageScene::update) answers the
fsm text (`SoundTrack::getSubtitle` 0x290980, keyed by `SeEntry` +0x8 via `hasSubtitles`
0x290930) of the last cue channel playing a subtitled entry. **Music**: `playMusic` 0x1d5390
(channel, name, fade-in frames at 30 fps) queues the entry from that channel's table,
`stopMusic` 0x1d52d0 fades out (frames * 25 / 30), `setMusicVolume` 0x1d5210 sets channel 0's
stream volume (percent * 128 / 100, `SoundTrack::setVolume` 0x291a30 scales by the entry volume)
and its fade scale +0x34. **Positional sources**: `addSource` 0x1d6d10 (`addSourceNamed` 0x1d6bd0
by SE-table name) new's a 0x140 `StageSoundSource` -- a **scene node** (`game/stagenode.h`: node
base vtable 0x315ae0 over 0x314150 / 0x314120, ctor 0x1d8540, deleting dtor 0x1865c0; +4 parent,
+8 child list {head, count} of 0xc links {item cell, next, prev} via `StageNodeList::pushBack`
0x1842c0 / `erase` 0x184370; vtable 0x315a80 adds a `NodeTransform` +0x40 (ctor 0x1d7f20: dirty
byte, position +0x10, rotation +0x20, matrices +0x40 / +0x80 = the node's +0xc0),
`setPosition` 0x1d7ee0) with handle -1, entry, 1.0f +0x10c, variant (`(cycle + 1) % variants`,
stored), +0x114 = the name starts "CAR_" (follows the scene flag), hung under the given parent
node (without one the update rebuilds its matrix); `findSource` 0x1d5510, `setSourcePitch`
0x1d73b0 (`SePlayer::setPitch` 0x293070(f12 pitch, handle)). `setSeTable` 0x1d70e0 (0x19d9e0)
releases every slot, takes the stage's SEDT file (+0, `SePlayer::setTable` 0x2921d0: magic SEDT,
version >= 1.2, relocated once) and pairs each X_J / X_D entry with X through `altOffset` +0x2e
(base +d, alternate -d). 0x1d3d20 before the file (a {records of 0x1c, short count} name lookup,
eight callers elsewhere) is unplaced; 0x1d7c50.. after it is the node family's (inline virtuals
of vtables 0x315a50 / 0x315a80, the transform 0x1d7ee0 / 0x1d7f20, the node base ctor 0x1d8540,
math helpers to ~0x1d85c0).

**`game/seplayer.cpp` 0x2908d0..0x2937e0 (TU 45, 2026-11-03, batch 078, all 35 written, `verify`;
header `game/seplayer.h`)**: the two sound players over sound/sound.cpp. **SEDT** (the effect
table, `SePlayer::setTable` 0x2921d0): "SEDT", +4 float version (>= 1.2), +0xc entry count,
+0x10 16 bank records of 0x40 (+0x30 byte: a stream bank), +0x410 / +0x418 the two reverb cores
{mode, float depth} handed to `SoundConfig::setEffect` (core 0 / 1, depth both sides), +0x420
relocated, +0x424 the string base, +0x428 / +0x42c the sorted name index {index, name}, entries
(0x30 `SeEntry`) from +0x430. **StDT** (a SoundTrack's stream table, `SoundTrack::setTable`
0x290d60): "StDT", +4 relocated (`stdtIsRelocated` 0x1a0570), +0x10 entries (`stdtEntryCount`
0x1a0560), +0x14 / +0x18 banks of 0x10 {name, body file (both from string base +0x1c), _, the
XWS header (+0xc, not relocated here)}, +0x20 / +0x24 / +0x28 a string table (an entry's +8 is
its index, relocated to the string: its fsm subtitle key), +0x2c the names' base, +0x30 / +0x34
a name index, entries from +0x40. Both relocate once in place (host: `S2dRef`). **SePlayer**
(0x1298): +0 table, +4 16 `SoundSeBank *` (`loadBank` 0x292140: new on an XVAB, waited), +0x44 16
`SoundStream *` (`openStreamBank` 0x292090: a bank body in EE memory copied to the IOP), +0x84
master volume 0..0x80, +0x88..+0x94 its fade {from, to, frames (-1 idle), frame}, then three
arrays of 0x40 `SeVoice` (0x18: {scale -1 free / 0 unscaled / 1 under the master, level, pan,
ratio, bank, placed}) at +0x98 (voice-bank voices by voice), +0x698 (pan voices by sdpr2 handle),
+0xc98 (stream banks by bank). **The handle** (64-bit): bits 0..31 the voice / pan handle /
stream request, 32..47 the entry index (`SeEntry::id`), 48..55 the bank, 56..59 placed, 60..63
paused (`pause` stops a voice and answers the handle with 0x1000000000000000; `resume` plays the
entry again, placed ones at pan 0, ratio 0). `play` 0x292ac0 / `playUnscaled` 0x292830: left /
right x entry volume / 0x80, x master / 0x80 unless unscaled, random pitch 1 +- spread
(`randUnit`); a stream bank plays its request = the entry's note from 0. `playPlaced` 0x292520:
volume from `seDistanceVolume` 0x283df0 (ratio, the entry's falloff: 0 linear 1 - r, 1 cos(r pi/2),
2 1 - sin(r pi/2); 0 beyond 1) x volume, `SoundSeBank::playPan` (ratio as spread). `update`
0x293440 steps the master fade and, only while one runs, re-levels every record (freeing ended
voices), then updates the 16 stream banks. **SoundTrack** (0x90 over SoundStream's 0x60): +0x60
StDT table, +0x64 the playing entry, +0x68 placed, +0x6c / +0x70 the request's channels / rate,
+0x74 **second half** (the request holds two takes; StageSound's player flag picks the second:
start offset + half the size rounded to channels x 16, the very start of it + 2 blocks; positions
reported minus the half), +0x78 the request size, +0x7c the fsm file (`setSubtitles` 0x290b10,
magic "fsm\0"), +0x80 its track (`fsmFindTrack` 0x28fc80 by the entry's key, strcasecmp; -1
none), +0x84 the name ("snd" by default: the body files' path prefix, "%s%s" name + file into
+0x88), +0x8c started (the position passed 0, so 0 again reads as ended). Seconds played =
position x 28 / 16 / channels / rate (ADPCM blocks); `getSubtitle` 0x290980 answers
`fsmFindText` at that time.

**A character's frame (2026-11-05):** `StageScene::update` runs `pairs->actionStart` -> `StageCharacter::sideHook0` 0x17a860 (slot data 0x31248c through the thunk 0x17e4a0: allows cleared, `PlayerMotion::beginFrame`, wants cleared; for kinds 1 and 9 `strafe` and `gunMove` follow isPlayer; a live character's clocks: flinch -10, unarmed MSP +10, endurance + `m_enduranceRegen` +0x5c8 a second, not in 0xa0, 0xa1; a kyo model's player carries P_URIEN 400 ahead and tells the script uryenchu), then `runStageScripts`, where natives may set wants (a door's 0x87..0x89), then `pairs->actionControl` -> `sideHook1` 0x17a810 (`CharacterInput::update`, the brain 0x1dd2f0, `PlayerMotion::controlStep`), then `stageUpdateCharacterMotions` 0x17df60 (`stepLower`, `stepUpper`, `stepFace`).

**`game/stagemenu.cpp` (2026-11-06, batch 183, all of it): the stage's own menu, Triangle by default (`PadAction_Menu`, button 2 of the map, the old `PadAction_Unused`).** Ranges 0x208d40..0x20acb0 and 0x22fea0..0x230980: CodeWarrior put `StageMenuFade::setLook` and `draw` far from the window. `StageMenu` (EE 0x18, vtable 0x3164b0 : 0x3164d0, vptr +8; slots 0x230820 dtor, 0x230400 update, 0x2303c0 draw): `state` +0 (0 closed, 1 opening, 2 open, 3 closing), `request` +4 (1 open, 2 close), `list` +0xc, `pending` +0x10 (the picked line's text), `pendingOn` +0x14 (one frame). It opens only with show flag 3 set and `actionAllowsMenu(playerAction())`; while open, the same test failing closes it. The focused menu takes Use (pick), Squat (close), Up / Down on `m_repeat`; Triangle closes unfocused too. **`StageMenuList` (EE 0xaa8):** `m_state` +0 (0..3), close step +4 (1 the picked line blinks, 2 all fade, 3 done), own-clock byte +8, clock +0xc (the only caller passes 0), printer +0x10, page +0x14, `m_pages[2][16]` +0x18 (`StageMenuLine`, 0x54), laid-out byte +0xa98, the plate's TIM2 +0xa9c, `Tim2Texture` +0xaa0, line count +0xaa4. **Scripts ask for a line every frame:** `addLine` marks it `asked` on the page being filled; `draw` copies 16 lines to the other page with `asked` cleared and flips, so a line nobody asks for again fades and frees itself. The text pointer is the line's identity (`wstrcmp`), the order sorts (exchange sort, free lines last), `enabled` is the script's second argument == 0. **Looks and colours:** six looks (0 gone, 1 closed, 2 grey, 3 grey half transparent, 4 orange, 5 orange half transparent), table 0x2fab40 [from][to] into 32 tracks at 0x2fa9a0 (count, colours, shows text), 3 frames from look 0 and without text, 6 from look 1, the picked line's blink at 0x2fab20. Rows: x -89, y 28.05 apart centred on the middle. **A plate:** a sprite of the TIM2's rect (1,1) 230 x 29 at scale 0.75 x 0.857, the picture is white with an alpha ramp to the right, so the colour comes from the track; the text is black, the track's alpha, at (x + 260, y + 256.5), scale 0.75, centred on y. `tools/reverse/menucmp.py` compares the window with the ROM. `clear` leaves `enabled` alone, in the ROM too.

**The rumble: `game/rumblelist.cpp` 0x2bc100..0x2bcd20 (2026-11-06, batch 163, `verify`; the file name is the port's).** Was `KeepList`. `RumbleList` (0xc: file +0, head +4, tail +8) at scene +0x30, a stage's only, plays named tracks side by side from init pack item 0x14. The file: magic "sv2S" +0, count +8, offsets from its start +0xc. Each entry: magic "fa2S" +0, name[0x10] +8, flags +0x18, three channels {kind, offset from the entry; 0 none} +0x1c, steps behind. A step is a varint length (7 bits a byte, low first, bit 7 more) then a value byte; length 0 ends the list. Flags: bit 0 loop (the same bit that `removeUnlooped` keeps; the old name's "keep bit"), bit 1 always (felt whoever the frame's source is). Channels: 0 the large motor 0..255, 1 the small motor 0 or 1, 2 events. Kind 0 holds a value until the next step, kind 1 pulses up to 4 values for the one frame a step starts. An event byte 1 makes `updateRumble` play the sound ALERT2. Tracks (23): ALERT, PETITJACK, CAUTION, DAMAGE1, DAMAGE2, ATTACKHIT1, ATTACKHIT2, BITE, GUNSHOOT, LNDH, DMGLND, KAKOSI, COLLISIONHIT, CARCLASH1..7, ELEVATOR1..3. Only ALERT has an event channel (flags 3, 6 frames on, 34 off, for ever), ELEVATOR2 is the only other with the loop flag, and DAMAGE1, DAMAGE2 and ELEVATOR3 lack the always flag, so they need the right source. A node (`RumbleTrack`, 0x5c) holds three `RumbleChannel`s (0x18) at +0x10 and finished at +0x58. `update` plays each, keeps the strongest of each motor from the source's tracks and the always ones, and drops a track the frame all three timers are 0. Time is in `frames` as `updateRumble` passes it. Other callers still stubs: `StageAlert` 0x263c10.. (add, isPlaying, removeNamed), the elevator 0x294560.. (its +0x2f8 as source), 0x28b470 (isPlaying), `StageCharacter::playerEvent`, 0x20e6e0.

**`game/stagescene.cpp` 0x1953f0..0x1a68b0 (TU 46, 2026-11-03, batches 079-086, `partial`; the file name and the one-file extent are the port's guesses)**: the **stage scene** (`StageScene` 0x130) and the **stage's files** (`StageFiles` 0x160) in one run with no source-path string: they share the path chooser `StageFiles::dataDir` 0x1a1ca0 and the string-map templates 0x1a4e80..0x1a6790, and the this-adjust thunks 0x1a67c0..0x1a68a0 end it (0x1a68b0 is the dtor of the next run's class, vtable 0x315690). The **scene's member map** (constructor 0x19c180, destructor 0x19bbe0: about 30 member objects, which only a stage or only a sub-scene builds) is the file comment of `src/game/stagescene.h`. Facts written: `getMission` 0x196870 = the scene's **+0xe8 mission state** (0x287900, 0x13d0 bytes) or the nearest parent's (+0x10c, `getParent` 0x196950), recursion unrolled eight levels; **`getPurposeText` 0x195d50 is a member** (formatTextValue passes gStageScene in a0): mission + 4. **The view stack** +0x34 (`StageViewStack`: capacity 8, top -1, entries of 0xc whose +0 is the object) gives StageSound its listener. **`drawPaused`**: a sub-scene only runs its **`DemoStream`** (+0x28: a `SoundStream` subclass, 0x80, unassigned@002b0d50, dir "streamEU/demo/", +0x74..+0x7c 0x80606060 / 0x100 / 0x1bc); a stage calls `StageSound::update(top of the view stack, player->getAltSoundFlag() 0x214f60, sceneFlag)` where sceneFlag = `StagePlayer::isMode1` 0x2152d0 (+0x400 set, +0x3c8 == 1), else with a view target (+0x100, its +0x3c0 character): target is the player and the listener not Kind11, or listener Kind11 and target == `StageParty::getCompanion` 0x1fea50. **`raycast`** walks group 1 (the Root objects, `StageObjectList` +0x98) skipping inactive ones (+0x34), takes those with +0x200 set and +0x37 clear, tests each mesh of +0x20c (count +0x208) with `StageCollision::raycast` 0x233810 (mask, flags, arg reordered) and keeps the face nearest `from` (`vecDistance` 0x135f20). **`StageFiles::load`** sets the loader's directory (`FileLoader` +0x14 is that pointer, +0x18 its copy: `FileLoader::setDir`, inline everywhere; stageLoad's "m_arg = 0" was the same) from `dataDir` (bg/ chara/ motion/ item/ -> ../data/debug/ or ../data/proof/ by the flags +0x124..+0x127, else ../data/), loads the script, then runs **`parseScript` 0x1a20f0 twice** (+0x128 = 1: each keyword only `addFile`s its path; load all; +0x128 = 0: each keyword takes `getItem(findIndex)` into its container). parseScript copies the item, splits it into lines (0x1a3500 on a line list 0x1a5a60 with vtable 0x311210), resets the containers (0x1a3dc0), skips empty and '#' lines, strtok-like 0x129a60 on " \t" (0x303d40) and strcasecmp against the keyword table 0x31a928.. (target 0x1a1b50, 0x1a1820, texture 0x1a1430, model 0x1a0e70, motion 0x1a0ae0, collision ("SSS\x1a" magic, 0x23ae40), particle, 0x1a0950, script (0x1d3a40), sndxv, sndseh 0x1a0580, a language-suffixed item (getInitItemForLanguage), areaclip, ...), all skipped while gStageScene is a sub-scene. **The script language** (read 2026-11-03, batch 081): lines of words split on " \t\r\n", `#` a comment, keywords in table 0x31a928 (target, end, texture, model, motion, mdl, collision, particle, map, script, rute, sndxv, sndseh, font, areaclip, bbx, lpt, sndsth, message), case-insensitive; an unknown keyword fails the load. Blocks: `target NAME` (table 0x31a974: script, camera, character, bg, bg_motion, particle, item, map, sound, effect; a second target before `end` fails) .. `end`, which in the second pass hands the lists the block filled to the target's handler and then empties them all (`reset`). Keywords only list and take files; the targets give them to the game: **script** needs two script files (the first's short +0xa counts `StageScene` +0x44 int slots), a rute (`RoutePlayer`, 0xc, vtable 0x316a78, at +0x4c), a font (from the init pack by language, `getInitItemForLanguage`, whatever path the line names) and at least one texture (the last one listed goes to the scene's printer), bbx optional (`StageNodeTree::setBoxes` 0x190900, a BoxGrid of size 8; a sub-scene's script block does only this); **character** makes a `StageCharacterModel` (0x1c) per model line, two motion lists each, filed under the model's name in the map +0x13c; **effect** needs the texture `siglamp` (`SigLamp` into `StageScene` +0x8c). File fix-ups after load: collision must start "SSS\x1a" and is relocated (0x23ae40), scripts are relocated with their second table (0x1d3a40). `splitLines` is Shift-JIS aware (lead 0x81..0x9f, 0xe0..0xfc). Containers: `StageList<T>` and `StringMap<T>` in `src/game/stagelist.h`, an unbalanced strcmp tree whose nodes are also chained in insertion order; the ROM has one instance per element type, 0x1a4960..0x1a6790. **Keyword handlers and the bg / particle targets** (read 2026-11-03, batch 082): `model PATH [NAME [NAME2]]` loads the model and the .tpk beside it (the path up to its first dot + .tpk; a `.tpk` is a pak of `<name>.tm2`), relocates it (`ModelFile`, magic at gp-0x7b48, `relocate` 0x2e2d20) and makes one Tim2Texture per name in the model's list (+0x3c, count +0x1e) unless the name is taken; `nothing` keeps the model list's place with a null; one name is the model's, of two the first is the alternate. `motion PATH` is a text + the .mpk beside it: each line's last `\\` word is an item of the pak, a `#` line a null entry (`readMotionList` 0x1a1df0). `map PATH` relocates the map (`StageMapFile::relocate` 0x186ee0, six section pointers). `sndseh PATH`: `fileWithSuffix` puts the options' suffix before the extension, the first pass loads the SEDT at once and lists each named bank's `file` (+0x10 + 0x40 * i) and, for a stream bank, `streamFile` (+0x18 in the record). `sndsth Stream_sNN_SE`: streamEU/Stream_sNN_SE<suffix>.sth, whose banks the first pass lists, and ../data/sound/msg/SCNN[_<language>].fsm. `message PATH_<language>` goes to the mission (`StageScene::setMessages` 0x196840, a stage only). A `script/` texture in the second pass is the init pack's item 21 named after the language path. The language inline: 0..3, 5..9 `getLanguageCode`, 4 "Ct", else empty. **The bg target** 0x19f680 walks models with collisions beside them: each node of the model, last first, is a new `ModelInstance` (0x2e5210, vtable 0x31a7f0) and an object named after the node: a stage `createMapObject` 0x1766c0 (the name picks the class: oGK99_01 / oDC99_01, oXXX_dp / _ds with a number, ...) then `setupMapObject` 0x1760c0, a door (kind 2) taking its index in **`gDoorRecords`** bss 0x537d80 (0x87 of 0x38, by name, +0x298); a sub-scene its own lighter classes (`_dp` / `_ds` 0x2c0 vtable 0x314ef0 with the record's +0x34 at +0x280; gates 0x2bbdb0; others 0x2bc040). Then the scene takes the model's name (+0x68), script (+0x38) and areaclip (+0x58, `AreaClip` 0x2dccf0, an 8 x 8 grid); the first lpt goes to +0x5c, its magic "lpt" turned to "LPT" as +8 becomes a pointer. **The particle target** 0x19f140 gives `StageNodeTree` 16 textures in insertion order from common01 on and the first two particle files (kind 0xf at +4, data at +0x10).

**The action lists (`game/scenepairs.cpp` 0x171300..0x171a30, 2026-11-04, batch 117).** `ScenePairTable`, StageScene +0x14, EE 0x2c: five doubly linked lists of 8 bytes (head, count; the ctor's "five pairs"), then a word nothing touches. A node is {box, next, prev}; the box is a 4-byte heap copy of the entry pointer. An entry is the object's side base (+0x18: flag, then vptr). `add` appends. `remove` takes the first hit in each list and only `~StageBullet` and `~StageLight` call it: every other entry stays until the table goes, and `~StageScene` deletes the objects first, which is safe because the table's dtor never reads an entry. Lists: 0 items and bullets, 1 map objects and an event's objects, 2 lights, 3 characters (`StageCharacter::setup`), 4 unused (`ScenePairListId`). The steps call side slots +8, +0xc, +0x10, +0x14 (`sideHook0..3`) for entries whose flag is set: `actionStart`, `actionControl`, `actionMovement`, `actionTerminate`.

**The character, built (`game/stagecharacter.cpp` 0x17b3e0..0x17dcb0 + 0x17dd10..0x17df60, 2026-11-04, batch 119).** `StageCharacter` (vtables 0x312240 / 0x312264 / 0x31227c, EE 0x800) is **also the scene's player**: one class in the ROM, so the host's `StagePlayer` stand-in is gone (`typedef StageCharacter StagePlayer`), which is why `gStageScene->player` and `partner` read real fields now. Build order: ctor (input 0x60 always; brain 0x9b0 only when the scene has a rute file, its +0 the character's ScriptRunner; a 16-byte heap vector at node +0x10; rotation order flag `m_xform.rotationXyz` = 1), then `StageFiles::placeCharacter` sets `m_itemDef`, position and rotation, then `setup`: name, `addCharacter`, `ScenePairList_Characters`, script entry by name, model, the sheet's kind to `applyCharacterModelFlags` and `CharacterHidePart`, `m_loop` = 0, `initAbilities`, `createBlock98`, PlayerMotion 0xc20 and `loadMotions` (the bones exist only from here), `setupBody(true)`, body size from the **pose's** name (`pose->name`, table 0x2f7ba0: _lh_ _th_ 0, _mh_ _ms_ 1, _sh_ _ss_ 2, _ch_ 3, default 1), CharacterGround 0x1c0, the sheet's two item columns (`findAll3`, walked from the last placed, `giveItem` until one takes), `m_parts` ("head" and the first part record), blood and effects, then `reset(0)`. **Effects:** blood name yamiblood for class bit 8, else otblood (kind 0xd) or BLOOD; class bit 8 also spawns yamismoke1 at bn_pelvis, yamismoke2 on stages 0x11..0x15, and for kinds 5 / 0xc koh at bn_spine_base; every character has a splash (bthamon for kind 0xb, else L_SPLASH) and F_HAMON at both feet (kinds 0, 1, 2, 4, 8, 9, 0xa, 0xe) or one on itself (5, 6, 0xc, 0xf). A body with no model data gets shiryotai at bn_head and shiryotai2 at bn_pelvis, bn_r_thigh, bn_r_calf (`setupBody`). `setupBody` also sets `m_lightProxy` = bn_pelvis, a **bone node, not an object** (the field was typed `StageObject *`, and only its `getWorldMatrix` is read), and copies shadow size and offset from `gMotionModels`. **`m_abilities`** (+0x3a4, 0x2c): all 1, three values from the sheet, `driving` 2 / 0 / else 1, stepLevel 0..4 (5 reads 4), then a clear list per kind; only the sheet's columns are understood. **Status** +0x594..+0x5b0 is a copy of the sheet; the ROM clamps each to 0..its own value, which changes nothing. `m_weapons` +0x238 / +0x23c is what the old `m_handNode` and `StagePlayer::weapon` both read. The lamp held is `m_lampItem`; the old `StageMapHolder` was that lamp: its +0x240 the `ItemLamp`, whose first spot's `m_switchedOn` is what the map shows as rMapP. The side hooks are thunks to +0x210.. : 0x17a860 start, 0x17a810 control (input 0x236b90, brain 0x1dd2f0, motion 0x1e3e80), 0x17a490 movement, 0x179d30 terminate: step 5, unwritten.

**The plain map object's frame (`game/mapobject.cpp`, same batch).** Side table 0x3115e4: `sideHook2` 0x1771b0 advances the model by dt/(1/30), calls `updateMatrix` on every node of `boneMap`, clears +0x210 (who touched it, written by the collision hook 0x1753f0, not ported). `sideHook0` 0x1756b0 ends a one-frame dirty: map objects rest with a clean transform, so `setPosition` 0x174aa0, `setRotation` 0x174a40 and `setYaw` 0x174a10 (slots +0x20c, +0x210, +0x214) set dirty and +0x21c when it was clean. Before this run the host object had the base's setters, so a script's move of a map object never rebuilt its matrix. `sideHook1` / `sideHook3` are empty. The door class (0x174940, side table 0x311314) shares `sideHook0` and the three setters and has its own 0x174180, 0x1741c0, 0x174390, all still unwritten.

**The stage's frame (`game/stagescene.cpp`, batch 086; running since 2026-11-04).** `StageScene::update` 0x199400, in the order of its timing marks (each label names the stretch that ends at it): `ScenePairTable::actionStart` (the table's five lists of action objects: slots +8, +0xc, +0x10, +0x14 are start, control, movement, terminate); "script": `runStageScripts` 0x19aba0 for a stage, or a sub-scene's own script, which ends when it stops running and the demo voice has stopped, or on start when `m_skippable`; then the game-over or clear sequence; "action(control)": `stageUpdateCharacterMotions`; "action(movement(&motion))"; "collision": `SceneRecordBuckets::collide`; "action(terminate)"; "calcmatrix(root)": `calcMatrices` over groups 1..8, each active root node `propagate` + slot +0x20, then the model's pose (`ModelPose::solve`, 3, -1, 0) and what hangs from its bones, then group 0, the cameras with `m_viewActive`, slot +0x25c + slot +0x20; a stage's owned objects (+0x30), party, +0x98 and +0x9c; "particle": `StageNodeTree::update` with range `getDrawDistance` * 1.2, `SceneBoxList::update`, `updateRumble`; StageSound (or DemoStream) and the cue subtitle into `ScenePool` slot 0; `SceneRender::update` / `updateLights`; a stage's pause state, and with no event the menu, the item list and the alert; the letterbox (`StageMission` +0xda, native 0x1f6: `SceneDirector` ramps the colour of the two bars from 0 to 0x80000000 over 15 frames and back, then is deleted; it was read as a blackout until game/gameoversequence.cpp, the fill is only the bars); `controlRequest` +0x84 (0 nothing, 1 back to the player, else a StageObject to take over, which needs the player's alternate flag, and becomes `controlled` +0x80, followed by the main camera); objects with `m_dead` +0x37 are deleted; "others": `updateVcms` (groups 1, 2, 3, 8), `drawSysTick` (seconds * 300), `frameCount` +0x64. `StageScene::draw` 0x198970: clear colour, `prepareDrawFlags`, the camera's matrices into `drawSysCurrent()` +0xc, the draw distance = camera +0x2cc + +0x2d0, `AreaClip::update`, `drawSysRender` ("rena draw"), `StageNodeTree::draw`, the menu program, then packet lists: 9 view filter (`gsFrameFilter`) and party for a stage, or for a kind 13 view the d0101 filter and `DemoCamera::drawOverlay`; 8 the model holder +0x12c; 9 ScreenFill; owned objects; 10 item list and menu (only with no event); 12 pool text (a stage whose printer has a texture) or subtitles; 11 alert, fade, director, or for a sub-scene the fade and the **letterbox**: black bars y 0..0x40 and 0x1c0..0x200 of the 512-tall frame. **Camera kinds** (`getKind` from the vtables): 7 StageCamera (map cameras, cam_dive), 9 `StageMainCamera` "camera" (0x3c0, vtable 0x311fa0), 10 `ViewCamera` camview / camview_zoom / camview_snipe (0x440, 0x3185b0), 11 the party's camjack (0x340, 0x311ac0), 13 `DemoCamera` camera_demo (0x340, 0x316ed0). The view stack +0x34 stays sorted by priority (main camera 0, demo camera 4); its top has `m_viewActive`. `setupCameras` 0x199f00 builds them, a spare bullet and the point light "flash" (both off), runs the bg script's entries (a sub-scene: all but the last, skipping names ending in m), binds bones, and takes the first two active characters as player and partner when no script did. The **view filter** table 0x31a8f8 gives passes / arg: the player on stage 6 and the partner on stage 12 {3, 1}, F_E24_SNPC2 / F_E21_SNPC1 {1, 1}, else none. `SceneArrayPair` +0x124 holds 3 bytes and one NodeTransform for every object but group 4's; `restoreFlags` walks the same order.

**The stage's script step, sound and list files (`game/stagescene.cpp`, batch 088; written, not yet run).** `runStageScripts` 0x19aba0, a stage with a script and no event (an event only polls: slot +0x10 of the vptr at +0x2c, 3 ends it): `SceneUiState::pollSoundSources`; each character of the mission's list +0x69c (count +0x6a0) steps (`beforeScripts` 0x21c7c0) or, inactive, may spawn (`canSpawn` / `spawn`); `StageMission::tickPending` (thunk 0x286ce0 of 0x286e30); with no game over the runners of the list +0x740 (+0x40 of each object) and the characters' brains; `afterScripts` 0x286c40, which starts the **pending event: mission +0xc8 is its name** (what `isBusy` tests: one byte, not a flag), else lets the fade go on when `getMenuMode` is 1; each character's `afterScripts` 0x21c310. `updateBuffers` / `updateBlock98` follow the object the top camera (kinds 9, 10, 11) links to: eight character blocks (shown while near a listed object within mission +0xb4) and the player's block, off while an owned object has a target or the link is not the player. `applySoundTarget` 0x19d9e0 runs once (`StageSound` +0x308): each SEDT becomes the table, its banks are made (`loadBank`, or `openStreamBank` with the streamFile), then the .sth, streamEU/, the .fsm, and **each StBank's header +0xc is pointed at the loaded file called name**; last `compact`. `applyBgMotionTarget` 0x19f230: a name is MODEL_dr_r (door type 2) / MODEL_dr_t (1), a target of `stageMotionTargetName` 0x175c00 (CAT, CROW, tonbi, oMT04_04, oZS09_04, oDC07_05, oMK21_09, oMT04_99), CAR (kind 3), Seven_Keys (kind 6), else one object by name; each takes the list through slot +0x270. `loadList` 0x1a3a60: init pack items 0x14 (KeepList +0), 7..0xc (`MenuProgSet`, all six or none), 0xd..0x10 and 5 (party), 0x11 item sheet, 0x12 menu, 0x13 pool; then the stage's screen pack, and without a pack loader ../data/listfile/sNNlist.lst, each word a file. Queries: a door's faces carry flag 0x4000000 and id +0x46 = 100 + (type - 1) * 180 + (number - 1) * 4 (`findDoorOfFace` 0x196b20, doors +0x240 / +0x244); `findFloorFace` 0x197280 keeps the highest face below the height (`heightAt` 0x2342c0 for a mesh with +0 clear, else a ray 10000 down), `getFloorAttr` 0x1970f0 its +0x44 from 50 above pos. Mission fields: +0x84 / +0x88 script properties 0x3e / 0x3f, +0x8c / +0x90[6] move scales (a script command sets them in percent), +0xa8 a script value, +0xac start time, +0xb4 the block range, +0xb8 / +0xbc what `saveObjectFlags` 0x1955e0 leaves for `restoreObjectFlags`. `requestSubScene` 0x196060 stores the **name** at scene +4 (so that field is a pointer); d1804 first clears a tree flag on the lamps ENV_PL03..06. `getMissionFrames` 0x196340 is (clock - start) * 30 rounded.

**The script runner's vtable and the object family's shape** (read 2026-11-03, batch 089). `ScriptRunner` (vtable 0x315850, 108 slots, vptr +0xb4) is an interface of **script commands**: the base answers each with a default, `defaultCommand` 0x1cf1e0 (0) or `unsupportedCommand` 0x1cf1b0 (logs the Shift-JIS "unsupported command" 0x304ab0 through the retail-empty 0x1a69a0, answers 0); vtable 0x315690 forwards most commands to a target from its slots +0x1b8 / +0x1bc. Inline defaults are emitted beside each vtable that needs them, so 0x19cf50..0x19d0b0 sit in this TU. **Slot 7 (+0x24) is the class's own name**: null in the base (0x1a7a10), `DemoExecuter` 0x19cf40 in vtable 0x314d30, so that is the real name of what a sub-scene runs (the stand-in was SubSceneObject); other classes' slot 7 is a cheap source of real names. **A StageObject has three vptrs because it has three bases**: +0, +0x18 (vptr +0x1c, four slots: thunks adjust -0x18) and the `ScriptRunner` at +0x40 (vptr +0xf4 = 0x40 + 0xb4; thunks adjust -0x40). The vtable block is contiguous and new virtuals follow its last sub-table, so `getKind`, slot 123 from the vptr at +0, is the entry rtti.py lists as slot 108 of the +0xf4 table: 0x1a45d0 (0x21, SubSceneDoorObject), 0x1a4880 (0x1a, StageMarker), 0x1a4950 (0x24, SubSceneProp). `StageScene::isInView` 0x19b820 is `targetInView` out of line, `playerOrTargetInView` 0x19b900 the scene flag, `viewLinkedObject` 0x19b9f0 the helper the frame also inlines; `stageHideObjects` 0x19c7e0 (reached from the mission's event start 0x288340 and 0x1fede0) takes groups 2..4 off and group 1 off the view. `SceneBufferSet::createFor` 0x2ce620 builds only for an owner named I_HATUEN.

**The map file** (the `map` keyword; `StageFiles::applyMapTarget` 0x19ddf0, batch 085): a 0x24-byte header of six section pointers and six shorts (+0x18..+0x22), the records right after it (`relocate` 0x186ee0 computes the pointers from the counts): **points** 0x2c {name, pos, rot}: `F_` / `player` -> `placeCharacter`, `P_` / `BA_` -> a mark (`P_SPW%02d` goes to `StageMission::setSpawnPoint` +0x750, other names to a `StageMarker`), the rest -> `placeItem`; **cameras** 0x30 {name, pos, aim}: `StageCamera` (450, 512, 512, 1600), looking at pos + aim; **spots** 0x34 {name, pos, rot, cone angle radians, radius}: ENV_CL / ENV_SL (`SpotLight`, once per name) and YAM_CL / YAM_SL (a stage's, off at start); **lights** 0x24 {name, pos, radius}: ENV_PL `PointLight`, else a `SphereVolume` (the ROM's YAM_PL test reads the spot section at the light's index); **boxes** 0x38 {name, pos, size w / d / h, rot}: ENV_CB and ENV_SB to the renderer's grids +0xdfc / +0xe00, ENV_MB_f%02d to `areas[0]` (the floor number `getFloor` answers), ENV_AB to `areas[1]`, a box named after an ENV_ light (8 characters, 10 with 'c' at [8]) to that light's list +0x278, SMOKE_A to `areas[2]`, REVERB_ON to `areas[3]` (`getSoundAttr`), any other a `BoxVolume`; **marks** 0x20 {name, pos}: marks without a rotation. A stage takes every section of every map except a plain ENV map (name[10] == 'd'); a sub-scene takes only an ENV map's lights and another map's boxes. **Items** (`placeItem`): the group map (+0x14c, filled by `applyItemTarget` with {model, motion list}) gives the model, `gItemDefs` the kind: 0xe `StageItemE`, below 7 `StageWeaponItem` (kind 0x15 by getKind then takes its `gWeaponDefs` index, bss 0x576010), 0xf with I_WANA a trap, with I_HATUEN / I_AKIKAN / I_PET / I_NUT a thrown item, else `StageItem`. **Characters** (`placeCharacter`): the last model line of the characters map, F_YRT with a random part ext_eyr01..03, `gStageObjRecords[kind]` by name (F_YRT / F_SRT by `name_Snn`) whose two item names (+0x18, +0x28) are placed first. **The running script** is sbss 0x31ae18 (`ScriptRunner::run` 0x1d27e0 sets it): `StageScene::isBusy` lets an event's own script go on. **Menus** open on pad 0's pressed start / select unless the fade is full, the stage is ending, the mission is busy (+0xc8) or `playerAction` is outside {0, 1, 9, 0x13, 0x14}; `UiShowFlags` [1] / [2] allow them.

**`game/areagrid.cpp` 0x283df0..0x2849c0 (batch 090, 11 functions, all written, `verify`).** `AreaGrid` (EE 0x240): +0 the list of every box, +8 64 cell lists (index x + y * 8), +0x210 / +0x220 the xy-z bounds, +0x230 / +0x234 the cell size, +0x238 a byte the ctor sets to 1 (no reader found). `addBox` only lists; `build` 0x2842f0 grows the bounds from each box's corners, divides them by 8 and enters each box in every cell its bounds touch, so a box added after `build` is in no cell. The grid owns its boxes (the dtor deletes them), so a box also held by a light's list +0x278 must not be in a grid. `contains` 0x284190 answers the **box**, not a flag; `collect` 0x283ff0 the boxes at a point. Cells cover x and y only: z decides in the bounds test and the box test. `AreaBox::contains` 0x284690 tests in box space through `invertRigidMatrix` 0x136500 and `applyMatrix` 0x136690 (the node's slot 3 matrix); `getWorldBounds` 0x284740 takes the eight corners through slot 4. Both slots answer +0xc0 (the transform's world matrix). `seDistanceVolume` 0x283df0 takes and returns unsigned (the u_int to float idiom at the top of the function) and sits here by link position only.

**`game/titlescreen.cpp` 0x231c90..0x231d40 (2026-11-03, 3 functions, all written, `verify`, batch 091; the file name is the port's).** `runTitleScreen` 0x231c90 runs `titleLayoutPath`'s layout through `runLayoutFile` (not cancelable, yield after) with arg 0 with the intro and 1 without, and passes on answers 1 and 2; anything else is 0 (`TitleAnswer`). The layout 2dEU/title<lang>.d2b holds, seen on screen: the "Sony Computer Entertainment Europe presents" card, then Load Game / New Game with a selection colour, "Select Menu Item", the copyright line, which slide in from the side, and a movie widget (presumably the background, black while `sdk/libmpeg.cpp` reads movies as ended). Any button press plays a menu sound (`UiLayout::playSound`, `getSePlayer` 0x2c4140), so the menu pack must exist before the title can answer. `loadingTick` 0x231d20 and `loadingBegin` 0x231d30 are two separate one-instruction functions, both `j overlayShowLoading` 0x267950: the waits (`FileLoader::waitLoaded`, `FileJob::wait`, PackJob::load's inlined copy) call Begin once before the loop and Tick each turn.

**`game/menupack.cpp` 0x2c3d30..0x2c42a0 (2026-11-03, 9 functions, all written, `verify`, batch 092; the file name is the port's).** The resident menu pack: `loadMenuPack` 0x2c3e10 opens dataEU/packmenuEU.pak (sbss 0x31ae98, flag 0x31ae9c) and `createMenuSePlayer` 0x2c3eb0 builds the outside-a-stage `SePlayer` (sbss 0x31aea0, flag 0x31aea4) from the pack's ../data/sound/se/seMenu_SE.seh, then `loadBank` or `openStreamBank` for each named bank; `unloadMenuPack` 0x2c3db0 deletes both. `getMenuPackItem` 0x2c3d70 / `getMenuPackItemSize` 0x2c3d30 (were getSaveResource / getSaveResourceAux) are slots +0x20 / +0x28 of the pack, nothing save-specific; the memcard asks for item 0 (icon1.ico) and 1 (the icon.sys template), and `Memcard` +0x78 is that item's **size** (was `m_iconAux`), which stepSave does not use (it writes kIconSize). `MessageTable` 0x2c4150..0x2c42a0 is the stage's script text lookup, here by position only: one word, the message file (magic 0x6d621a00, count +8, relocated flag +0xc, {key, text} pairs from +0x10, offsets from the file); `StageMission`'s first field points at one (0x287690 builds it, `setMessages` 0x286c30 and the lookup forwarder 0x286c20 go through it), and a script argument starting with '_' is replaced by its text (scriptPrepare 0x29f178). 0x2c42a0 is not in this file: it is slot 1 of Vt_00319f00 (`0x2cc8c0(); return 1`), an inline virtual of the class at 0x2ca870..0x2cc8c0.

**`game/kunitoris.cpp` 0x2c42a0..0x2ce0c0 (2026-11-07, batch 221 with 16 names; 9 of 93 written, `partial`; the file name is the port's).** Extra mode is **Kunitoris** ("= KUNITORIS =", a falling-piece minigame with its own pack dataEU/kuniEU.pak). `runExtraMode` 0x2c42c0 runs `kunitorisThread` 0x2c4320 in slot 3 and answers sbss 0x31aed4 (the save changed). The thread starts `kunitorisBackThread` 0x2cdb20 in slot 4 (each frame `gfxSetClearColor` from sdata 0x31accc (enable, initially 1) and sbss 0x31aeb0/b4/b8/bc (r g b a, bytes), `SePlayer::update`, and the six handles at bss 0x599800 set to -1 once they stop), switches `gfxSelectDisplay(1)` (0 on the way out), loads the pack (sbss 0x31aec0), its own `SePlayer` (0x31aec4, flag 0x31aec8; table item 0x29, bank 0 item 0x2a) and a `UiFont` (0x31aecc; glyphs 0x16, image 0x15, spacing 0 0). Pack items: 0..15 the level files (`KunitorisLevel`: width / height shorts +4 / +6, relocated byte +8, piece count +0xc, 64 refs +0x2010 and one +0x2114 relocated inline by the thread), 0x17 the how-to picture, 0x18 the title, 0x19 its blinking line. SE entries: 0 decide, 1 cancel, 6 cursor, 7 select BGM, 8 title BGM, 9 game over, 10 ending BGM (handle slots 0 clicks, 1 cursor, 5 music). Flow: `kunitorisTitle` 0x2c53c0 (scroll-in from 0x100, Start or Cancel; 600 idle frames show `kunitorisHowTo` 0x2c5550 for 300) -> `kunitorisLevelSelect` 0x2c5a70 (LEVEL 1..15, JAPAN, ENDING at 0x2fc340; 3 more lines per 3 clears, all 17 from 15 clears; text through `kunitorisPrintf` 0x2cd9a0, a vsprintf + wstrFromAscii + UiDrawer::drawText in a 256-pixel box) -> a `KunitorisGame` on the stack (EE 0x790, three class levels with vptrs 0x319f60 / 0x319fd0 / 0x319f80 at +0x18; ctor 0x2c8640 (best, level), `loadLevel` 0x2cd390 (cell 8.0, file), `start` 0x2cca40, `update` 0x2c64d0 (pad 0), `draw` 0x2c5ea0 into packet list 13; dtor inline): Select toggles +0x770 while lives (+0x80) last, Start shows the how-to as a pause, +0x778 cleared ends it, then 30 frames before Decide / Start. Score +0x5c beats `SaveData::_46c[level]` -> recorded; a first clear sets `_4ac[level]` and the 15th clear plays `kunitorisEnding` 0x2c4ee0(0) (the ENDING line plays it skippable). The ending: 60 intro frames (`KunitorisEnding::drawIntro` 0x2ca330, clear colour 0x118dc1 fading to black), then `KunitorisEnding::draw` 0x2c9de0 with `setTiles` 0x2ca790 (which writes uv rows into the data table 0x2fc2a0) for 27 tile sets. Every draw goes into `PacketList_Finish` (13) through a `UiDrawer` in pixel mode, centred coordinates (-128..128).

Kunitoris's objects (2026-11-07 second session, batches 222 / 223). Correction: +0x5c is the **hi score** (the ctor's a1, the level's best; `m_hiScore`), the score is +0x60; +0x6c is the player's cell (18, 18), not a view half. `KunitorisActor` (vtable 0x319f30, vptr +0x24; EE 0x28): pixel position +0/+4 (cell * 8), colour +8 (grey 0.5), a mover +0x18 (vptr +8, step slot 1, its position at +0), a fader +0x1c (vptr +0x10, its rgba at +0), the sprite +0x20; slots dtor 0x2cae40, update 0x2cc8c0 (steps both, deletes each when done), draw(drawer, pass) 0x2cdbd0 (empty). `KunitorisSprite` (0x319fc0 over 0x319f50, EE 0x28, ctor inline): quad uv 0..1 xy 0..16 at +4, GsTexture +0x24; draw 0x2cc9d0 (prim 0x15e, white). `KunitorisCursor` (0x319f00, EE 0x34, ctor 0x2cacc0: cell +0x28, lift +0x30, item 0x10) and `KunitorisBlock` (0x319f90, EE 0x50, ctor 0x2cc450: origin pointer +0x28, offset +0x2c, cell +0x34, lift +0x3c / rest +0x40, anchor +0x44, state +0x48 (0 rest, 1 a 14-frame hop of up to 6 cells, 2 sink placed from the anchor, 3 sink over 12 frames, 4 gone), timer +0x4c, item 0x11). Draw passes: < 0 (blocks only) a temporary sprite of item 0x13, 0 the shadow colour 0x2fc030 (0.01 x3, 0.8), > 0 translated by -lift * 8. `KunitorisActorList` 0x2cde50..0x2ce0b4: a doubly linked list whose nodes (0xc) hold the actor pointer in a 4-byte box. `KunitorisCells` (EE 0xc, ctor 0x2cb220 / dtor 0x2cb170 deleting the actors) and `KunitorisGroup` (EE 0x24, 48 at KunitorisGame +0x9c: cell +0xc (setCell 0x2c8070), step +0x14 (setStep 0x2c8080), addCell 0x2c8090). The ctor's per-level tables: lives 0x2fba00 (40..32), +0x84 0x2fba40 (210..120, 20 entries), +0x88 0x2fba90 (120..60), groups at the start 0x2fbad0 (4..10), +0x8c 0x2fbb10 (20..8); shapes {count, cells} 0x2fbe10 (27, levels 0..11) / 0x2fbef0 (20, levels 12..15; counts sdata 0x31acc0), picked from the first level + 8; a group enters at -8 or 39 outside the 32-cell view moving in. The lives bar (`drawLivesBar` 0x2c8510, item 0x12, one pixel a life at y -117..-101) and `drawBlinkBar` 0x2c83a0 (+0x764, colour 0x2fc010 or grey by bit 1 of +0x760). The board hierarchy: 0x319f60 (vptr +0x18; slots 0x2cdbe0 dtor, 0x2cdbf0) -> 0x319f70 (dtor 0x2cdc00) -> 0x319fd0 / 0x319f80 (KunitorisGame: both 0x2cdc90, 0x2cd280) and 0x319fe0 (the 0x38-byte piece: 0x2cddc0, 0x2cd520; FUN_002c81c0 calls slot 1 as draw(drawer, 1) for each piece, four times for the board's wrap, then the level's outline lines +0x2114 (count +0x2110 pairs) with prim 0x149 in 0x2fbfe0).

Kunitoris's ending pictures (2026-11-07 third session). `KunitorisEnding::draw` 0x2c9de0 and `drawIntro` 0x2ca330 share one body: four quads at data **0x2fc280** (quad 0 the banner (-75,75)-(149,107), quads 1..3 the tiles; `setTiles` narrows the tiles' v, so the earlier guess "0x2fc2a0 = the tile quads" is quads 1..3 of this table) shown once `m_timer` reaches {0,0,30,60} (intro, rodata 0x2fc300, stops after 300) or {0,0,15,30} (ending, 0x2fc320, stops after 120); pack items 0x22 + 0x23..0x25 (intro) or 0x22 + 0x26..0x28 (ending); then the scene's **pose** at (x-124, y+44) 64 x 64 from the 8-entry table **0x2fc220** ({count, steps {until, item}, loop}; scenes 0 / 2 / 6 loop; scene 4's steps in sdata 0x31acd0; entry 7 empty), back to scene 0 when the scene runs out. Poses are pack items 0x1a..0x21. **KunitorisGame +0x3c is a KunitorisEnding** (`m_ending`; the ctor's 0 / 0 / -1 / 0... are its frame, scene, timer, tiles) that `KunitorisGame::draw` 0x2c5ea0 shows with `drawIntro(0, 0)`.

**`game/uiworkarea.cpp` 0x243350..0x243390 (2026-11-03, 4 functions, all written, `verify`).** `UiTrack::draw` 0x243350 and `getAnchor` 0x243360 (the base slots: nothing, null), `setUiWorkArea` 0x243370 / `getUiWorkArea` 0x243380 over the word at sbss 0x31ae40 (gp-0x7a30). menuPlayMovie lends the stage pack's buffer through it for the opening movie, the movie widget hands it to `MoviePlayer::setWorkArea`; null means the player allocates. New Game's order on screen: title, runOptionScreen(1) (the Screen page), menuChooseKind (difficulty), saveExtraInit, the character / place / time card from menuPlayMovie's layout.

Kunitoris's board and its draw (2026-11-07 fourth session, batches 225..227). The board hierarchy, named: `KunitorisTinted` 0x319f60 (vptr +0x18, position +0 unset, colour +8 = grey 0x2fc000; slots `update` 0x2cdbe0 = true, `draw(drawer, pass)` 0x2cdbf0 empty; the earlier note called these dtors: there are no virtual dtors here) -> `KunitorisColorCycle` 0x319f70 (count +0x1c, colours +0x20, index +0x24, loop byte +0x28; `update` 0x2cdc00 steps the index, wrapping or holding on the last, and copies the colour to +8) -> `KunitorisPiece` 0x319fe0 (EE 0x38: its level record +0x2c, cell count +0x30, new[] of 0x20-byte cells +0x34; ctor 0x2cd8a0, non-virtual deleting dtor 0x2cd690, `update` 0x2cddc0 the same step again, `draw` 0x2cd520: pass 0 every cell white 0x2fbfc0, else lit cells in the cycle colour and unlit ones green 0x2fbfd0, each a 2-vertex sprite with prim 0x14e; `load` 0x2cd730: cell = {lit 1, record, x y, rect x*8..x*8+8}) and -> `KunitorisBoard` 0x319fd0 (level +0x2c, piece count +0x30, pieces +0x34, `m_highlight` +0x38 the flashing piece, -1 none, 0x100 every lit one; `update` 0x2cdc90 steps every piece's cycle, `draw` 0x2cd280 pieces then, for pass != 0, the level's border lines +0x2114 (count +0x2110 lines) prim 0x149 in 0x2fbfe0; `loadLevel` 0x2cd390 and `selectPiece` 0x2cca40 (was KunitorisGame::start) are board methods) -> KunitorisGame 0x319f80 (the same two slots). Level piece record: cell count +4, cells of 0x10 from +0x10 (x, y ints). Colour cycles at data **0x2fb9e0** ({count, colours}: 0 brown x1, 1 yellow x9 + brown x9 = the flash, 2 grey to black x6, 3 dark green x1; colours 0x2fb840..0x2fb9e0). `selectPiece(piece)`: the old choice back to cycle 0, then piece (negative: randNext2 % count, the first with lit cells round the board, -1 when none; 0x100: every lit piece) to cycle 1. `KunitorisGame::draw` 0x2c5ea0 (packet, texList pass through to UiDrawer::begin): `drawBoard` 0x2c81c0 (pixel mode, the pieces in pass 1 and the borders four times at -128 - 8 * scroll, scroll less the level's width / height: the level wraps), the blocks of m_cells[index ^ 1] then m_cells[index] in pass 0, the active groups' blocks in pass -1, the cursor's draw slot pass 0, the same in pass 1, then the map: the pieces at scale 0.125 from (64, 16), m_highlight lifted; identity; `m_ending.drawIntro(0, 0)`; = KUNITORIS = (8, 10), HISCORE:%d (8, 20), SCORE:%d (120, 20); the blink bar when m_highlight is 0x100, else the lives bar; GAME CLEAR! (96, 108) when cleared, else GAME OVER (104, 108) with no lives, PAUSE (120, 108) when +0x770 (`m_paused`, Select) is set. `KunitorisBlock::drawWithHighlight` 0x2cbc90 is the block draw slot's body again with the pass -1 sprite given (draw passes null).

Kunitoris's rules (2026-11-07 fifth session, batch 228): `KunitorisGame::update` 0x2c64d0 and its helpers. **How it plays**: the view (32 x 32 cells shown, 40 wide for wrapping) scrolls over the level's wrapping board with the d-pad (+0x75c the held direction 1 up 2 down 3 left 4 right, taken on a press, kept while held, moving on the pad's repeat word +0x20; the groups get minus the scroll so they stay on the board, `KunitorisGroup::scroll` 0x2c7c10). The player's blocks sit on the cursor's cell (KunitorisCells m_cells[+0x78], offsets from the cursor); groups (shapes, 48 slots) come in from a random side every +0x84 frames (at once when none is left; none from 45 on; shape from the first min(+0x77c, 27) of the table) and step every +0x8c frames (`step` 0x2c7dc0, wrap 0x2c7f00: 8 cells past one side of 40 back at the other). A group touching the player's blocks joins them (`absorb` 0x2c9170, from where its blocks were before their last move +0x1c); past 16 cells across they break off and sink in place (`breakOff` 0x2c9720, SE 12). Triangle turns the player's blocks a quarter about the cursor (`KunitorisBlock::turn` 0x2cb6f0: (x y) -> (-y x)), Square mirrors them (`flip` 0x2cb5e0); both slide over 3 frames with a `KunitorisSlide` mover (vtable 0x319fb0 over the base mover 0x319f20 {0x2c7bc0, 0x2cc9c0}: from +0xc, to +0x14, frame +0x1c, frames +0x20, origin cell pointer +0x24; ctor 0x2cc830, step 0x2cc740 a linear lerp). Cross shoots them as a group moving every frame in the held direction (up by default; `shoot` 0x2c9a80: blocks hop 15 frames, set state 2; the player keeps one block). Circle drops them on the board under the view (`KunitorisCells::drop` 0x2cb2e0 with the scroll +0x64 as anchor: offsets become board cells, the blocks sink 12 frames), switches +0x78 to the other set and starts again with one block; only when the previous drop has been scored (the other set's state < 2). When the dropped set is gone (state 5) `KunitorisBoard::clearUnder` 0x2c8b90 puts out the level cells under it (grid `short [64][64]` at level +0x10, [x][y], -1 none; only the flashing piece's cells count, any piece's in a fever): hits and misses. **KunitorisCells +8 is a state**: 0 empty, 1 active, 2 shot, 3 dropped, 4 sunk, 5 gone (the last block's update answered false). Lives (+0x80, max 80): one lost every +0x88 frames outside a fever, a drop costs 2 + 2 per miss, clearing the flashing piece gives 0/4/6/8/12/16/24/32/40 by hits (data 0x2fbf90, count sdata 0x31acc8), doubled when clean (no miss and every cell). Score: hits (x4 for a clear), doubled with no miss (unless 1), doubled again in a fever. A clean clear starts a **fever** (`selectPiece(0x100)`: every lit piece flashes, SE 11 as music): +0x76c (float) from 1 down by +0x768 (float, 1/325 a frame, 1/6500 more for each piece cleared in it; +0x774 counts them), +0x764 its bar (x 80), groups spawn and step twice as fast and no life drains; at 0 one random piece flashes again and SE 7 plays. The level is cleared when no piece has a lit cell (`KunitorisBoard::litPieces` 0x2cd120, +0x778). The pose scenes (m_ending.setScene): 1 more than one miss, 2 game over, 3 cheer (a fever, a 4..10-cell piece one in four), 4 a clean clear of 10+ cells, 5 a 20+ cell piece one in sixteen. Handles 0x599800: slot 0 the shot (SE 3), 1 the drop (SE 2), 2 turn / flip (SE 4 / 5), 3 the scroll (SE 6), 4 the break-off. The cursor gets `KunitorisActor::update` 0x2cc8c0 directly, not its slot. update answers false when paused, cleared or out of lives (the thread ignores it).

Kunitoris's last ten (2026-11-07 sixth session, batch 229; the TU is complete, 93 of 93, `verify`). The bases are real classes, not pure interfaces: `KunitorisDrawable` 0x319f50 { `~KunitorisDrawable` 0x2c7b70, `draw` 0x2cab50 empty } under KunitorisSprite 0x319fc0, and `KunitorisMover` 0x319f20 { `~KunitorisMover` 0x2c7bc0, `step` 0x2cc9c0 = true, a base mover never ends } under KunitorisSlide 0x319fb0. No fader class lives in 0x2c42a0..0x2ce0c0: KunitorisActor::update is the only code reading +0x1c. `kunitorisRandom` 0x2c81b0 is `j randNext2`, an out-of-line file-local wrapper only the inline spawnGroup (ctor and update) calls; `KunitorisGroup::copyPair` 0x2c7ee0 is step's out-of-line int[2] copy (m_moved = m_step). The vtables of KunitorisBlock 0x319f90 and KunitorisActor 0x319f30 point their update slot at thunks (0x2c7a30 `j 0x2cc220`, 0x2cdbc0 `j 0x2cc8c0`), not at the bodies; the host annotates both above the body.

**`game/prgmccard.cpp` 0x2b85a0..0x2b9ef0 (2026-11-03, 7 functions, all written, `verify`, batch 093; the file name is the port's).** `PrgMcCard` (0x94 bytes, Vt_00319520), the widget type "prgMcCard" that is the logic of 2dEU/mcBase%s.d2b, the load and save screens. It owns `gMenuMemcard` (sbss 0x31ae94: the ctor news it, the dtor deletes it) and talks to the layout only through 22 S2D variables in a row, from the one named by p0: rfMSG_FLAG, rfSAVEBAR, rfLOADBAR, rfSELECT, rfSELECT_CTRL, afSELECT_CTRL, rfYESNO, rfYESNO_CTRL, afYESNO_CTRL, SCN01..04_FLAG, SCN01..04_TYPE, s2sSELECT_CTRL_SEL, s2sSELECT_STATUS, s2sYESNO_CTRL_SEL, s2sYESNO_STATUS, rfBAR_CTRL (read from the file: `McVar`). The layout's arg picks the screen: 0 `stepLoad` 0x2b97b0, 1..5 (the `SaveDialogReason`s) `stepSave` 0x2b86e0, whose prompt is rfMSG_FLAG 1..4. rfMSG_FLAG selects one text image of the layout: all 0x22 values were cycled on screen, `McMessage` has each text. SCN0n_TYPE confirms two guesses: `StageKind` Easy / Normal / Hard are what the screen calls Difficulty, and +3 is "Autosave: ON", so **`GameOptions` +0x4d is `autoSave`** (was `_4d`): with it `stepSave` skips the list and rewrites the save of the module's stamp (`McSave_Auto*`), after re-reading it and comparing stamps; no stamp means McMessage_NoStamp, then the ordinary save. The ordinary save asks for the card's info every time one finishes while a question is up, so a pulled card lands on McMessage_NoCard from any of them. `SaveDialog_Extra`'s text names "Kunitoris" play data. `SaveFile::assignHead` 0x2b9710 is the implicit copy of the part under the sum, emitted here for stepSave. State 2020 has no way in; kept.

**`game/layoutpaths.cpp` 0x2ec160..0x2ec8e0 (2026-11-03, 24 functions, all written, `verify`; the file name is the port's).** One shape 24 times: `getSaveData` (answer ignored), `sprintf` of the name with `getLanguageCode(getLanguage())` into a 0x20 static buffer, answer the buffer. 13 event movies `movieEU/ev<id><lang>.d2b` (bss 0x5a55e0 down to 0x5a5460) and 11 layouts `2dEU/<name><lang>.d2b` (0x5a5440 down to 0x5a5300): navi, navi_tu, mcBase, option, memAlt, newGame, scelogo, topMenu, archive, help, title. Both tables run in file order with descending buffers, so they read as one generated list. The language codes are table 0x2fac40, EN by default in this build. `getDrawTarget` 0x2ec110 just before the range is not part of it: since 2026-11-07 its own file **`rena/rena_drawtarget.cpp` 0x2ec110..0x2ec160** (the port's name, `verify`): a function-local static `DrawTarget` at bss 0x5a52d0 (guard byte sbss 0x31af30 = gp-0x7940) whose first-use init writes 100, 100, 100 at +0x00..+0x08 and 1.0f at +0x10 (+0x0c untouched); drawSysRender stores the frame's draw env at +0x14, drawSysInit reads the size +0x18 / +0x1c, rena_draw's pass setup asks for it. Placed under rena/ by link order (the last function before game/layoutpaths.cpp).

StageScene ctor / dtor (second run, batch 080, 54 names): every member got a class name (guesses unless noted; `gamehooks.h` stand-ins) and the scene a host field map in `stagescene.h`. Facts: **+0xe4 is a 48-bit LCG** (`Rand48`, 0x2682e0 seeds the drand48 multiplier 0x5deece66d; `Rand48::next` 0x2681e0 = (x >> 17) % n, with a rejection rule: game/rand48.cpp, 2026-11-07, java.util.Random without the increment, n + 1 where Java has n - 1, n of 0x7fffffff never returns, every ROM caller passes 100 through Prop_Random100; gStageScene is a sub-scene during an event and its rng is null, so the ROM would run on address 0, the port's `StageScene::random` answers 0 there; `StageScene::random` 0x195d40); **+0xd0 `StageMenu`** is open while its +0 == 2 (`getOpenMenu` 0x1963d0); **the four area grids** +0xac..+0xb8 (`AreaGrid` 0x284640, 0x240 bytes; `contains` 0x284190 on +0xb8 is `getSoundAttr`; `addId` 0x284560 via `addSoundArea` 0x19ae30 / `addArea2` 0x19ae40); **`isGameOver` / `isCleared`** test the sequences +0x118 / +0x11c (`GameOverSequence::getState` 0x2b1c50 answers 5, `ClearSequence::getState` 0x2b1620 answers 2, both once the scene fade is full in mode 2); `beginPlay` = `currentMission()->markStart()` (mission +0xac = the scene clock seconds). **0x196960 / 0x196a40 are second copies of getMission / getParent** (`currentMission` / `parentScene`, 211 call sites). A sub-scene pushes its own draw-system record (`drawSysPush` 0x2d7c70 / `drawSysPop` 0x2d7c00, gp-0x7994 saved in gp-0x7990); a stage resets the per-surface tables (`resetSurfaceTables` 0x2cf510, stagecollision.cpp: for each of surfaceOfName's 44 surfaces the used byte at 0x599830 that useSurfaceOfName sets goes to 0, and the 32 step-sound cache slots at 0x599860 that StageCharacter::findStepSound fills go to 1, not looked up; the port keeps no cache). +0xbc is the wide-text printer (`WideFontPrinter` 0x2d4110, vtable 0x319ff0, at 0x700, 0x700); +0xf4 `StageAlert` 0x264070 / 0x264000. The ROM dtor asks a sub-scene's DemoStream isPlaying after deleting it (use after free; the port clears the pointer).

**`game/stageobjlist.cpp` 0x193900..0x1953f0 (TU 26, 2026-11-03, batch 058 with 45 names, all
written, `verify`)**: the **stage's object registry** `StageObjectList` (0xdc, `gStageScene` +0x10,
new in the stage scene ctor 0x19c180, deleted by its teardown 0x19bbe0). **Nine groups**, each a
name map (+0x00, 0x10 each) and a list in creation order (+0x90, 8 each); +0xd8 the last character
id. Who fills them: add0 0x177d20, add1 0x1760c0 ("Root"; doors are found here), **addCharacter**
0x17c700 (bones head / bn_l_foot, BLOOD; numbers obj +0x208 from 1 for `findCharacterById`), add3
0x183bf0 ("bn_pic"), **addBullet** from the bullet class 0x1803e0, add5 0x1861b0, add6 0x17ef10,
add7 0x17ecb0, add8 0x269bd0; only groups 4 and 5 have a remove. Each group's add / find / findAll
is its own function with the same body (`StageObjGroup` in stageobjlist.h). Lookups take (name,
index): the index-th object of that name, **a negative or too large index answers the last one**;
`findAny` 0x1939c0 tries the groups in order. The key a node holds is the object's own `m_name`
(+0x20). The containers are the game's own: **`StageObjArray`** (8 bytes: data, u_short count /
capacity; a gGlobalHeap block of capacity + 2 words, 64-aligned, items at data[1..count] and a 0
after the last; capacity 14, grows to 2 * capacity + 2) and **`StageNameMap`** (0x10: root, first,
last, count) of **`StageNameNode`** (0x14: array, key, left, right, insertion-order next): an
unbalanced strcmp tree until `rebalance` 0x194010 (from the stage load 0x17fb90) runs
`StageNameNode::balance` 0x194da0 on each root (rotates whenever the heights differ, even by one,
until the top is within one). `StageNameNode::find` 0x194ae0 is also called directly by 0x1ba970 /
0x1bac20 / 0x1c18b0 / 0x1d1150. The teardown deletes each listed object through its deleting dtor
(vtable +0x10, `StageObject::destroy` on the host).

**`game/fileloader.cpp` 0x1319f0..0x132f20 (TU 27, 2026-11-03, batch 059 with 45 names, all 53
written, `verify`)**: the game's file loading, every load on the job thread (`jobAdd`, game/thread.cpp).
**`FileJob`** (0x28): one file; `run` 0x132a40 asks `rxFileCallSizeHook` 0x152740 for m_dir + m_path
("%s%s", the static path 0x4c8b50), then m_path alone when that is missing; size 0 = done with no data,
< 0 = 0x80 failed; the data goes to gGlobalHeap (64-aligned) or into a caller's buffer at
m_bufferOffset (**negative: the file ends that far in, 16-aligned**; too small = 0x81), then
`rxFileCallLoadHook` 0x152730(path, data). States `FileJobState` 0 idle / 1 queued / 2 loading / 3 done /
0x80 / 0x81 / 0x82 (never set here) / 0x83 (the loaders' "nothing listed"); settled = anything but 1 / 2.
**`FileLoader`** (0x9c, vptr +0x98, vtable 0x3110b0, **18 slots**: dtor, isPack, addFile, findIndex,
getName, getData, getItem, getSize, getItemSize, unloadItem, unload, getState, getItemState, getCount,
isLoaded, waitLoaded, clear, compact): a linked list of 0x2c-byte entries (a FileJob + next) under the
path prefix +0x18 (0x80 chars, set outside this file); by-name lookups take **the first path containing
the name** (strstr). `startLoad` queues `loadJob` 0x1321f0, which runs every entry still idle / queued.
**`PackFile`** (0xf8, Vt_00311060, same 18 slots) **derives from FileLoader and `PackJob`** (+0x9c: a
FileJob, the `PakArchive` reader +0xc4 and the unpack buffer / variant); `PackFile::load` 0x131d70 is a
jump to `PackJob::load` 0x131a30 with a0 + 0x9c. The slots answer from the archive (addFile only finds;
isLoaded always; unload / waitLoaded / clear do nothing; compact reallocs the pack's block to its header
+0x1c). `setBuffer` 0x131eb0 (callers 0x2b5dc0 / 0x2b60b0: a 0x1800000 block and a variant 0 / 3 / 6 /
9 + a save field) makes the job read the packed file into the buffer's end; load then **SLDI-unpacks**
it to the start (`sldiDecompress` 0x2d0a10, **game/sldi.cpp**: header "SLDI", version 1, file size +8, unpacked size +0xc, records from +0x10, format in sldi.h; the disc holds 114 such packs, all 0x1800000 or smaller unpacked, largest 0x179bf80, all "pak" inside, found by a raw scan of ROM.000..ROMD.009, none in ROMD.010 / SPK.ROM) and
`PakArchive::open` 0x231520 keeps only the items whose u_short mask (entry +2) has bit `variant`.
The pak format is game/pakarchive.cpp (below). **`TextTokenizer`** (0x18, only user
0x1a3a60): whitespace-separated words into a buffer of fixed capacity.

**`game/pakarchive.cpp` 0x231410..0x231bb0 (2026-11-03, batch 060 with 7 names, all 17 written,
`verify`)**: the "pak\0" reader. `PakHeader` (0x20): +4 relocated flag, +8 name base, +0xc data
base, +0x10 count, +0x18 sub-pak count (the first entries whose data is a nested pak), +0x1c size
(after a variant pack: data base + kept bytes; PackFile::compact keeps it); then `PakEntry`s (0x10:
+0 u16 kind, read on entry 0 only, 4 = variant masks; +2 u16 variant mask, 0 kept in place; +4 name;
+8 data; +0xc size). `relocate` 0x231980 turns the offsets into pointers once (host: S2dRef), then
with a variant >= 0 and entry 0 of kind 4 drops the items lacking the variant bit (data null) and
packs the rest from the data start (an item's size is the gap to the next one's data, the last
one's its size field); it answers the variant, -1 for a bad magic, so **`PakArchive::open` with
variant -1 answers false** for a good pak (PackJob::load tests the count). Item ids are two u16
halves (`PakId`: index, level): `getEntry` 0x231860 answers entry[index] at level 0 and for any
other level descends into sub-pak 0 with the same id, ending null; `findIndex` 0x231660 (struct
returned by pointer) answers a sub-pak hit as that sub-pak's own index at level 0, so by id only
the top pak's items are reachable (by name, `findEntry` 0x231770, all are). Lookups by name are
strstr (the first entry whose name contains the key, own entries before the sub-paks').

**`game/vcmtable.cpp` 0x296d60..0x299d90 (TU 32, 2026-11-03, batches 063-065 with 59 names, all 9
game functions written, the 43 MSL instances mapped to the host STL, `verify`; the file name is the
port's, from the `VCM__` string 0x309c90)**: **`VcmTable`** (0x1c, `operator new` in the model holder
0x188b10, which keeps the rena model at +0 and the table at +4; per frame 0x187230 -> `update`, binding
0x187b40 -> `attach`, teardown 0x188690 -> the dtor). A rena model part (header +0x14, 0x18 each, count
u16 +0x2a) is **tagged** when its +8 -> +8 (geometry -> material, guesses) is non-null; its **tag** is
the material's name (+4) when it holds `__` tokens, else the first part named `VCM__...` (case
ignored) from the part up its group chain (`ModelInstance::findGroup` 0x2e4100: the record of model
+0x1c, 0x10 each, whose u16 children range +4 / +8 holds the index), else `VCM__` itself. Tokens are
the runs after each `__` **plus one byte** (`vcmSplitTokens` 0x297aa0; the last token's extra byte is
its NUL, so one-token tags compare right; `VCM__` alone gives the empty token). The ctor sorts the
distinct tokens (names owned, controllers null), makes one `VcmState` (8: token mask, fade bits, off
bits, alpha) per distinct tag (map order) and one link {part record, state} per tagged part.
`attach(obj)` binds the object whose inline name (+0x20) equals a token. `update`: per state, each
token in the mask gives its controller's weight (`VcmController::getWeight` 0x185af0; 0 when absent
or the byte +0x34 is clear): under 1 sets the next **compact** bit in fade, at 0 also in off, the
alpha is the last fading weight * 128; then every linked part record gets +0xa alpha, +0xb fade,
+0xc off (what the renderer does with them is unread). Tags on the disc (uncompressed): `vcm__ENV_P`,
`vcm__ENV_SL`, `vcm__ENV_SL01`, `vcm__ENV_SL42c1`: environment objects drive them. **The binary's MSL
`std::string` instances live here** (`~string` 0x297fb0, 129 callers; ctors 0x298030 / 0x2980b0 /
0x299320, `reserve` 0x2990c0, `replace` 0x298c70, `compare` 0x299060 / 0x299c20, `assign` 0x299d40;
short form: size in byte 0 >> 1, chars from +1; long: bit 0 of +0, size +4, data +8), and the tree
iterator ++ is `mslTreeIncrement` 0x25a850. Also named: newlib `strchr` 0x1280a8, `strncasecmp`
0x1285d0, `fprintf` 0x1239a0, `memcmp` 0x125190.

**`sound/sound.cpp` 0x144080..0x148db0 (TU 35, 2026-11-03, no new names: batch 013's held; all 98
written, `verify`; one file: the pinned range kept, the classes share the three semaphores and the
inlined SPU helpers, and CodeWarrior put the inline `SoundPcmSink::setAttr` at its head and
StreamRing's five empty base slots 0x1461c0..0x146200 beside the feeder)**: what "The game's sound
layer" (Program flow) lists, read in full. Details the reading added:
- Inlined everywhere: `spuMemFree` (every SPU ring release), `spuMemAllocHigh` (play's 0x4000 ring),
  `closeBank` (the open*, setBuffer, freeBuffer and dtor heads), `setVolume` / `applyVolume` (L or R
  * fade / 128, signed), `getPosition` and `soundUpdate` (in `stop(true)`'s poll loop: DelayThread
  4000, then rx / rxsndut / SDPR2 updates), the IOP body's release (+0..+8: a member with its own dtor,
  run after ~SoundStream's body). `stop(false)` with a read in flight leaves state 3 for `update`.
- `SoundFeeder::update`: stereo sinks (mode 1) take left + right blocks of the sink's interleave
  (+8) from the ring (right after left), as many as fit; mono / interleaved ones whole units of 0x40
  (mono: the interleave if larger), at most twice a frame; the sink is ended once the ring's endPos is
  fed, the feeder ends with the sink (status bit 4); status bit 2 makes it push its volume.
- Sink slot semantics: `open` closes first (the ADPCM sink then mutes itself), the PCM sink passes
  only (mode, a3, a4) to `rxSndPcmOpen`; `close` mutes, then closes.
- `SoundSeBank`: `poll` asks rx with wait 1, `getState` with 0; `unload(true)` retries the free 30
  times (threadYield 1 between) then frees the bank's IOP / SPU2 memory itself; the dtor stops all
  effects, unloads, and spins printing "終了待ち。。。%d" (0x3014e0) until the state is 0.
- `SoundConfig`: the reverb is two `SoundEffect` records (+0x18 / +0x30, 0x18 each: core, mode, depth
  L / R, feedback, delay); output mode 0 / 1 / 2 = stereo off+surround off / stereo / stereo+surround.
  ~SoundConfig (inline in soundShutdown) stops SDPR2, SE and BGM.
- SPU2 allocator 0x4d08d0 (0x2b0): head / tail sentinels, lo / hi words, 32 nodes; the head's end
  and the tail's start bound the free span; sizes round up to 0x40 (signed).
- SDPR2's config 0x2f5030 is 8 words {0, 0, -1, -1, 0, 0, 0, 0} (sdpr2Init copies it to the driver
  and both types).

**`game/menuscene.cpp` 0x2b5740..0x2b6b90 (2026-11-03, batch 083 with 32 names, all 14 written,
`verify`).** The scene table's only scene and the player's whole way outside a stage. `menuSceneThread`
-> `menuFlow(true)`, which never returns: `menuBootScreens` (2dEU/languageEU.d2b, then `menuCheckCard`:
memAlt%s.d2b over `Memcard::startScan`, arg 0 no card / 1 no room, until the scan is Ok or the layout
answers 1), scelogo%s.d2b, then forever: `loadMenuPack`, title (`runTitleScreen` 0x231c90, `TitleAnswer`;
unanswered: `runAttractDemo` 0x2b5aa0, which retail leaves as `return 1`, and the title again);
new game (a card scan for `SaveData::unlock468` in any good save, defaults, stage 1, first options,
`menuChooseKind` on newGame%s.d2b) or load (`menuLoadGame`, mcBase%s.d2b arg 0 over fresh defaults).
Then `menuTopMenu` (topMenu%s.d2b through `runMenuLayoutFile`: `TopMenuAnswer`) and
`menuPlayLoop`: navi screen (`runNaviFile` 0x295470: its answer is the node, `NaviAnswer` bits),
`menuPlayStage`, save dialogs. `menuPlayStage`: a single-part node only plays its movie and counts as
cleared; otherwise `unloadMenuPack`, the opening movie with the stage pack starting behind it on the
pack's own 24 MB buffer (lent to the movie widget by `setUiWorkArea`), `menuRunStage` (`stageThread`
in slot 3 on the script path at bss 0x5997c0; a message to slot 2 is passed on), the ending movie
(event 1, branch `StageResult::attempt[15]`), the notice when one played, `recordStageResult`,
`loadMenuPack`. The scene's "(Pack File)" is `menuFlow`'s argument, always 1: 0 would run the stage on a
bare FileLoader (loose files, no opening pack), retail code that never runs. Pack variants: 3 per
language (EN 0, DE 3, ES 6, FR 9, IT 12) + the record's kind. The memcard flag 0x31ae84 reads as "no save
to offer" (memcard.h). ROM oddities kept or marked: `menuPlayMovie` deletes its loader twice when the
movie file is empty (`PC:` guard); `menuPlayStage` copies the record over itself; `menuTopMenu`'s
archive save dialog passes no reason, a0 still holds the 3 of the compare. Doubts: the file name, all
screen names except the d2b files', `runAttractDemo`, `SaveDialogReason` (read from the callers only),
the size of the script path buffer (0x40 assumed). `runExtraMode` is game/kunitoris.cpp (below).

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
- **All 9 named and placed (2026-11-07, batch 236): CodeWarrior's `__sinit_<file>_cpp`, one per object
  file, in link order; the file names are the port's.** Each is joined to the port file that owns what
  it sets, as an `FS2` comment at the global (the host's compiler does the construction):
  0x310a00 `__sinit_packtables_cpp` (gDoorRecords, DoorRecord ctor 0x1772f0); 0x310a20
  `__sinit_stageloop_cpp`; 0x310a70 `__sinit_stagescene_cpp`; 0x310ac0 `__sinit_scriptrunner_cpp` (a
  ScriptRunner at bss 0x577a60, dtor registered at 0x577a50: **no code addresses it again**, so the port
  has no such global); 0x310b10 `__sinit_playermotion_cpp`; 0x310b60 `__sinit_uigame_cpp` (57 string
  pointers: 41 game-state names sdata 0x31aae8.. -> 0x599490..0x599530, 16 key-sprite names
  0x31ab98..0x31abd4 -> 0x2fafc0 every 0x14); 0x310ec0 `__sinit_snipecondition_cpp`; 0x310f10
  `__sinit_viewcamera_cpp`; 0x310f60 `__sinit_stageresult_cpp` (gStageResult, dtor 0x2b0cf0, entry
  0x5996e8). The port's file split is not the link order (playermotion.cpp holds code of many objects).
- **The zoom pair is a header's file statics:** five initialisers (0x310a20, a70, b10, ec0, f10) each
  compute `low = (float)pct * 256 / 100`, `span = 255 - 2 * low` from their own sdata int (30 in all
  five: 0x31a8d0, 0x31a8f4, 0x31a9d4, 0x31abf4, 0x31ac4c) into their own sbss pair (0x31adec, 0x31ae0c,
  0x31ae20, 0x31ae58, 0x31ae70). Read only by `PlayerMotion::setScope` (0x31ae20), `SnipeCondition::onApply`
  (0x31ae58) and `ViewCamera::setZoom` (0x31ae70); the stageloop and stagescene copies are dead. Host:
  `ViewCamera::kZoomPercent / kZoomLow / kZoomSpan` (viewcamera.h). **The EE's FPU rounds toward zero**:
  the ROM's values are 76.79999542 (0x42999999) and 101.40000916 (0x42caccce), the host's nearest
  rounding gives 0x4299999a / 0x42cacccd, one ulp apart; harmless for a zoom clamp, worth knowing where
  a run-time float initialiser feeds a comparison.

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
- **The game's movie player is FS2's own, not RoR's rxMpeg** (batch 012; written at TU 34, `game/movieplayer.cpp` in "Modules" supersedes the guesses here): RoR's `RxMpeg` / `rxMpeg*` (snd/rxmpeg.cpp, RoR game code) has no counterpart by size (`rxalign.py 1fa800 1fd000 1810f4 1830fc`: 3 chance pairs). FS2's player is a C++ class at ~0x1fa880..~0x1fcbf0 (vtables 0x315f50 / 0x315f80 without type_info): 0x1fc7b0 initialises it (CreateSema, `sceMpegInit` once behind the flag 0x31ae28, 4 x volume 0x80), 0x1fb9d0 opens a stream (rx stream ring 0x152400, `sceMpegCreate`, `sceMpegAddStrCallback` x4, `sceMpegAddCallback`), 0x1fb630 gets pictures (`sceMpegGetPicture`), 0x1fab50 is the decode thread side (WaitSema, `sceMpegIsEnd`); 0x1fbe00 / 0x1fbe20 wrap `sceIpuRestartDMA` / `sceIpuStopDMA` (the libmpeg stop / restart DMA callbacks). Callers: 0x25bd30, 0x25e260 (the movie scenes: `movieEU/ev*.d2b`). Name it at its TU.
- **libc** (Sony's newlib): `strlen` 0x128498, `strcpy` 0x128380 (R5900 quadword scans), `raise`
  0x127a68 / `_raise_r` 0x1278f8, `_impure_ptr` 0x2f3398. Stdio (batch 199, an old newlib: the cores
  take (fp, fmt, ap) and find the reent at fp->_data +0x54): `__sinit` 0x123910, `__sfvwrite`
  0x123fe0, `__smakebuf` 0x1245a0, `__swrite` 0x127d98, `vfprintf` 0x12a3a0, `__svfscanf` 0x12bd28,
  then the archive's last members `__sccl` 0x12c9b8, `vsprintf` 0x12cab8, `_vsprintf_r` 0x12cb10 (no
  caller), `_write_r` 0x12cb68 (the global errno word 0x5a55f8), `__swsetup` 0x12cbc8 ..0x12cce0.
  Right after it (8 bytes of padding) comes the game's first object, `game/gamesystem.cpp` from
  `GameSystem::beginStage` 0x12cce0 (stageThread: heap mark 1 = `HeapMark_Stage`, then
  `PerfMeter::resetStats` on +0x4a8).

## The IOP side (`MODULES\`, 2026-11-06, `tools/reverse/iop.py`)

- **It is all in `MODULES\`.** A raw scan of ROM.000..ROMD.010, SPK.ROM and the ELF finds no ELF and no
  ROMDIR image inside them (compressed packs were not opened; the game loads modules by name from
  `modules/`, `IOPCONF.CNF`). `python iop.py extract` unpacks the image to `work/iop/ioprp300/`, `python
  iop.py report` lists everything; none of it is ever committed.
- `IOPRP300.IMG` (278,305 bytes, built 2004-08-20) is a ROMDIR image (the IOP boot ROM's layout: RESET,
  ROMDIR, EXTINFO, then the files 16-aligned) holding **Sony's** 16 replacement modules and no game code:
  SYSMEM 2.03, LOADCORE 2.06, SIFCMD 2.08, SIFMAN 2.04, THREADMAN 2.03, IOMAN 2.03, MODLOAD 2.09, FILEIO
  2.12, CDVDMAN 2.26, CDVDFSV 2.26, LOADFILE 2.02, TIMEMANI 2.02, ROMDRV 2.01, EESYNC 2.02, SYSCLIB 2.03,
  STDIO 2.03. The extracted pieces rebuild the image byte for byte.
- Loose, loaded by `IOPCONF.CNF` in this order: sio2man 3.11, padman 4.22, mcman 2.30, mcserv 2.10, libsd
  3.03 (Sony's) and rxsnddrv 1.01. `SDRDRV.IRX` (sdr_driver 4.01) is on the disc, but neither
  `IOPCONF.CNF` nor the ELF names it: nothing loads it.
- **`RXSNDDRV.IRX` is the only game IOP code**, the server of the RPC `rx/rxsnd.c` calls: "Sound driver
  version 1.00", iopmod version 1.01, no symbols. Linked at 0: `.text` 0..0xd520, `.rodata` ..0xe190,
  `.data` ..0xe920, `.bss` ..0x3f210, gp 0x16910, entry `_start` 0. 40 imports (libsd 15, sysmem 5, sifcmd
  5, thevent 4, thbase 3, intrman 2, sifman 2, loadcore, stdio, sysclib, vblank), no exports. The
  dispatcher is 0x360.
- **It is RoR's driver, revised.** `python iop.py cmp` (table in `work/iop/rxsnddrv_cmp.txt`) compares the
  two with relocated fields and gp immediates masked: same version strings, the same 40 imports at the same
  indexes, the same 50 strings, `.rodata` and `.data` the same size; `.text` +0x430, `.bss` +0x6470 (RoR:
  `.text` ..0xd0f0, `.bss` ..0x38970, gp 0x164e0). 12,794 of RoR's 13,372 words align (95.7%). Of the 192
  functions of RosePan's `src/iop/rxsnddrv`, **152 are in FS2 word for word** (69% of the code by size), 38
  changed in place, 2 moved (RxSndDrvXvabGetUsers 0x2d14 -> 0x2e80, RxSndDrvPcmInit 0xb4a0 -> 0xcbd4: the
  same slot of `_start`'s init calls). **4 functions are FS2's own**: 0x2d9c, 0x3660, 0x4454, 0xd1dc,
  unread. Most other addresses moved by a growing delta: read FS2's address from the table, never from
  RosePan's `IOP 0x...` comments.
- **Not the game's compiler.** FS2's game code is CodeWarrior and RoR's GCC, but this module goes through
  neither game's build: 152 functions word for word, in the same order with the same register allocation,
  means one toolchain and one source tree. So every difference is a change in the source, never compiler
  noise, and RosePan's reconstruction carries over where the table says `=`.
- What changed, as read from the one-word differences (144 of the 256 differing runs; the field names are
  still to be proven): `RxSndDrvCommand` grew by 0x88 (the next function moved 0xb08 -> 0xb90, and its switch's branch offsets changed), voice code indexes a
  table by 16 where RoR has 8 (`sll 4` for `sll 3`), a voice stride 0x38 -> 0x3c with field offsets past
  +0x1c moved by 4, mix sizes 0x20 -> 0x2c and 0x40 -> 0x58, PCM offsets up by 0x18..0x1c (0x3c -> 0x54, 0x4c -> 0x68),
  which fits the larger `.bss`.
- Ghidra: `python iop.py elf <MODULES>/RXSNDDRV.IRX` writes `work/iop/elf/rxsnddrv.elf` (base 0, `_start`,
  `_gp` and 40 named import stubs as symbols). **Imported 2026-11-06 as program `rxsnddrv.elf`** in `CLaude
  Decomp`, open beside SCES_538.51 (read it with `YAMIPAN_PROGRAM=rxsnddrv.elf py dec.py <addr>`):
  `MIPS:LE:32:default`, 234 functions (40 import stubs and 194 others), all decompile with no bad
  instruction data in a headless import of the same file, whose function list this one matches address for
  address. The loader also adds read-only blocks for the PS2's RAM mirrors and BIOS (0x20000000 up, 66 MB
  of the program's reported size): placeholders, not the module. The 194 are 190 of the table's 192 addresses and the 4 new functions; the other two are
  `=` functions Ghidra did not split off (0x243c, 0xb770). `_start` reads FlushDcache, sceSdInit(0), sceSifInitRpc(0), the
  subsystem inits; 0x360 reads as RosePan's dispatcher. ghidra.md "Hazards" has the traps.
- **In the tree since 2026-11-07** (the user's call): RosePan's reconstruction is `src/iop/rxsnddrv/`, 14
  files, each function re-addressed to FS2 with RoR's address and the match beside it (`// IOP 0x00000b90
  (RoR 0x00000b08, word for word)`, or "N% of it aligned: FS2 differs, unread", or "moved in FS2").
  Addresses the map cannot place stay RoR's (`RoR IOP 0x...`), and so do the struct layouts. It compiles
  and links, but **nothing boots it**: its 40 imports are placeholders in `sdk/iop.cpp` that warn once if
  called (`thbase.h` and the other shims are macros onto `YamiPan_Iop*`, as in RosePan, since the EE
  kernel and Win32 have a CreateThread of their own), and `sdk/sif.cpp` still answers the EE's RPCs itself.
  Its `dma.cpp` now owns `YamiPan_RxSndNoteEeAddress`, which rx/rxsnd.c calls on each DMA: a 64-entry
  ring of EE blocks, harmless while nothing reads it.

## Disc and data files

Archive formats, file tables, where the data root starts.

## PC platform layer

The Pan layer, `src/yamipan/`, written fresh as classes on 2026-11-03 (`cpp-port.md` has the rule, the
map and the threads). What it keeps from RosePan is behaviour, not code. From RosePan's 32 K lines the
GS emulation (`rosepan_gs`), the replay half of the renderer and the i3d meshes are gone by the user's
rules or by being RoR's. From its 14 K line SDK only what FS2 calls came over.

- **Boot** (2026-11-03): `main` -> `Application::run`: SDL, `Window`, `gpu::Device`, `InputSystem`,
  `Overlay`, `Renderer`; the render thread starts, then `ee::Kernel::startMainThread` runs `gameMain` (the
  game's `main`: `gamesystem.cpp` builds with `main=gameMain`, so the file stays as the ROM has it) on the
  thread `YamiPanGame`. No data folder: the Game Data window opens and the game starts when one is
  chosen.
- **EE threads** (`ee::Kernel`): one runs at a time by priority; EE priorities are 0 (most urgent) to 127.
  `iChangeThreadPriority` from an alarm is how rx round-robins, so every priority change re-dispatches.
  A terminated thread unwinds with a C++ exception from its next kernel call. The EE main thread starts
  at priority 1 (a guess: rx's priorities are relative to it).
- **Interrupts** (`ee::InterruptController`): the field clock (50 / 59.94 Hz by `sceGsResetGraph`, running
  from boot), alarms, `DelayThread`, the timers. Handlers run only while the running thread is outside
  DIntr; a DIntr waits for a running handler.
- **Frame**: game threads fill `UiCanvas` layers keyed by packet list; `Vif1PktManage::Kick` ->
  `YamiPan_DrawUiKick` joins them into a `Frame` for the render thread's mailbox. The frame always fills
  the scene target, so the 512x512 frame's shape is `DisplayGeometry`'s alone.
- **Movies** (`media::MpegStream`, `media::MpegDecoder`, 2026-11-03): `sdk/libmpeg.cpp` forwards to one `MpegStream` (one movie at a time; `sceMpeg::sys` points at it). `demux` is the real PSS demultiplexer: packs, 0xE0 video, 0xBD private stream 1 (its first payload byte's low nibble is the audio channel, PCM and ADPCM callbacks alike, the game reads the type from SShd), a packet that wraps the ring is copied out, a refused packet stays and reaches the decoder once. Video goes to the game's ring (which only paces the demux: `YamiPanMpegVideoPending`) and to `MpegDecoder`, FFmpeg's avcodec + swscale, paced by the wall clock at the sequence header's rate and writing the IPU's layout (column-major 16 x 16 RGBA32 macroblocks, alpha 0x80) into `m_picture`. `sceMpegGetPicture` then calls the Nodata callback 8 times, clearing `REG_DMAC_4_IPU_TO_CHCR` first (no IPU: `feedIpu` would spin on the busy bit). `YamiPan_DrawUiMovie` -> `UiCanvas::drawMacroblocks` rebuilds an `Image` only when `frameCount` moves (one GPU texture per picture, not per game frame), for tbp 0x800 only: tbp 0x700 is `prime`'s buffer, which nothing shows. **Silent for now:** `kMovieHostHasSound` (game/movieplayer.cpp) keeps the feeders out, because with no IOP a sink's blocks never drain; `receiveAudio` drops a feederless channel and the movie ends on its video alone.
- **Textures** (`rendering::TextureStore`, 2026-11-03): `YamiPan_DrawUiTexture(list, owner)` -> `imageOf` ->
  `UiCanvas::bindImage`. The owner is a `Tim2Texture`: a `GsTexture` from the drawer, a plain `Tim2Texture`
  from the font (so `m_isTarget` is reached by `dynamic_cast`). The picture is decoded during the call to
  RGBA8 with alpha doubled (`min(a * 2, 0xff)`; a 16-bit colour's top bit gives 0xff or 0, 24-bit is
  opaque, TCC 0 is opaque) and the render thread makes the GPU texture. **The Image is TEX0's frame, not the file's picture** (2026-11-06): TW / TH round the size up to a power of two, S = 1 is the frame's far side and a repeat wraps there, so a 192 x 128 file is a 256 x 128 Image with its last column run on (hazards.md). The cache key is a hash of
  the size, picture header, pixels and CLUT, not the address (`texture_store.cpp` says why), least recently
  bound dropped past 256 MB. **What the disc holds** (every TIM2 in ROM.000..ROMD.010 + SPK.ROM by a raw
  scan): 21,751 4-bit CLUT32, 3,180 8-bit CLUT32 in CSM1 order, 26 32-bit direct, 2 4-bit CLUT16, no mip
  levels, no 16 / 24-bit direct, no linear CLUT. **The language screen** (2dEU/languageEU.d2b) binds six:
  the 512x64 row of names, black and opaque, drawn at frame y 224..288, and five 56..72 x 24 word tiles
  (grey ground, dark letters) for the highlight; their CLUT alpha is 0x7f, stored 0xfe.
- **Screenshots** are the scene before the overlay, 512 high at the composed aspect;
  `YAMIPAN_SHOT_WINDOW=1` takes the whole window with the overlay, `YAMIPAN_SHOW_MENU=1` opens the menu bar.
- **The disc** (`io::Disc`, 2026-11-03): FS2 boots from cdrom0: (`rxInit` -> device 1) and rx's driver reads
  ISO 9660 from raw sectors, which the game layer keeps as it is. So `sceCdRead` serves the data folder as
  a synthesized image: a descriptor at sector 16, a root-only path table at 18, the root directory at 19
  and the files from 27, sorted. `ROMD.*` are a second volume at `layer1Start` (the sum of layer 0's
  sectors), which `sceCdReadDvdDualInfo` reports, as on the dual-layer DVD. A file's sector is its offset
  on that image, so SPK.ROM's sector arithmetic holds. MODULES\ is not on the image: nothing reads it
  (`sceSifLoadModule` only counts). argv[0] is SYSTEM.CNF's BOOT2, `cdrom0:\SCES_538.51;1`.

- **`src/yamipan/rendering/yamipan_drawui.h`** (TU 19, 2026-11-03): the seam of the 2D layouts' draw
  module, `rendering::UiCanvas` behind it (`src/yamipan/seams.cpp`), user `game/uidrawer.cpp`. Keyed by the packet the game drew into
  (= the packet list = the layer); `YamiPan_DrawUiReg` (A+D: ALPHA_1, CLAMP_1, TEX1_1, SCISSOR_1,
  the BITBLTBUF..TRXDIR set of a local copy, done on TRXDIR between host render targets),
  `YamiPan_DrawUiPrim` (PRIM, optional shared RGBAQ), `YamiPan_DrawUiTexture` (a GsTexture owner,
  decoded at its source; `setImage`'s owner is deleted right after the call), `YamiPan_DrawUiBegin`
  (vertex format) / `YamiPan_DrawUiVertex` (RGBAQ bytes, UV / XY 12.4, Z). The gfx presets
  (`gfxUiPreset*`) still write their own packets until their TU is written.

## The object family (game/stageobject.cpp, 2026-11-04)

Every stage object is three bases in the ROM: the node (`StageNodeBase`, vptr +0, parent +4, children +8, blocks +0x10 / +0x14), the side base at +0x18 (a view flag, vptr +0x1c, four slots) and `ScriptRunner` at +0x40 (vptr +0xf4, 108 slots). **CodeWarrior keeps the three tables in one block and appends a class's new virtuals after the runner's**, which is why `rtti.py` shows 130 slots at +0xf4 and why calls through the first vptr read +0x1f4 and up: those slots are the object's own, not the runner's.

Node slots: 0 `setDirty` (+0x08), 1 `rebuildMatrix`, 2 dtor (+0x10), 3 `getMatrix` (+0x14), 4 `getWorldMatrix`, 5 `setDirtyTree`, 6 `updateMatrix` (+0x20, 0x1d8420 for all). Object slots: +0x1f4 `getKind` (empty in the base), +0x1f8 0x18a5b0, +0x1fc 0x189f40, +0x200..+0x208 `getPosX/Y/Z`, +0x20c `setPosition`, +0x210 `setRotation`, +0x214 `setYaw`, +0x218 `setModelValue`, +0x21c `getObjectName`, +0x220 `getSelf`, +0x224 `distanceTo`, +0x228 `distanceToPoint`, +0x22c `heightDifference`, +0x230 0x189ce0, +0x234 `playNamedHeard`, +0x238 `makeNoise`, +0x23c `noiseHook`, +0x240 `isShown`, +0x244 `setShown`, +0x248 `setTreeShown`; +0x24c.. differ by class.

Kinds (`getKind`): 0 character, 2 door, 7 camera, 8 vtable 0x311d30, 9 main camera, 10 view camera, 0xb 0x311ac0 (camjack), 0xd demo camera, 0xf light, 0x10 point, 0x11 spot, 0x13 item, 0x14 item E, 0x15 weapon, 0x16 trap, 0x17 thrown, 0x18 0x312f40, 0x19 bullet, 0x1a volume and marker, 0x1b sphere, 0x1c box, 0x1d 0x3149e0.

`StageObject` ends with the transform at +0x140 (world matrix +0x1c0). The serial at +0x3c is `(counter << 1) | 1`, so odd; `getHandle` answers it for characters and the pointer otherwise. The tree node (+0x38, EE 0x10): parent +0, children `StageList` +4, object +0xc; its walks inline four levels and use accessors 0x18b9a0..0x18b9d0 below that.

## The node transform (game/nodetransform.cpp, 2026-11-04)

0x1d7c50..0x1d85b0. **The conventions the 3D path rests on:** `applyMatrix` 0x136690 treats a matrix's rows as column vectors (rows 0..2 the axes, row 3 the position), and `multiplyMatrix` 0x136640 is a * b for those vectors (the sceVu0MulMatrix shape): world = parent world * local. Angles are radians, as `sinCosVec` 0x135c30's fold at pi and pi/2 shows; it answers in f1..f6, sin in the odd registers, and its one Taylor series expects -pi..pi. Two rotation orders, read term by term and checked numerically against the composed axis turns: `rotationMatrixZxy` 0x1359f0 (Rz Rx Ry, y applied first), the default, and `rotationMatrixXyz` 0x135b90 (Rx Ry Rz). 0x135a90 is a third order nothing here needs.

**`NodeTransform`** (EE 0xc0, exact layout now, +0x30 unused): the +2 byte `rotationXyz` picks the order (`StageCharacter`'s ctor sets it at 0x17df3c). The +1 byte `positionOnly` (set in 0x18e280 and 0x190220) makes the parent move the node without turning it: world = local rotation, position = parent * position, and with no parent the position is **dropped** (the ROM copies a local whose row 3 is still the origin). Otherwise local row 3 = position, w = 1, world = parent * local. `update` never clears `dirty`: the ctor sets it, and callers clear it through slots 0 / 5 (`setDirtyTree(false)` in stagelight.cpp is one, and so is the plain map object's ctor 0x1765e0, which doors reach through 0x174940: such an object's matrix comes from the file, `setModel` copies it into `world`, and left dirty it would be rebuilt from a zero position and rotation, at the origin). It uses the parent's slot +0x14 (`getMatrix`), `StageBoneNode` the parent's +0x18 (`getWorldMatrix`).

**Where stage 1 is** (read from the disc, `DATAEU/PKS01C1EU.ENC`, 2026-11-04). The map is `MK01.rm2`: `Root` has 351,082 rigid vertices and **no skinned mesh**, box x -5934..14798, y -4956..12381, z -256..3398, but the village itself is the upper right of that (x about 4000..14800, y about 3000..12400) and **the world origin is empty space**: a few stray pieces, nothing else. The other nodes place themselves by their node matrix, in the same world units; the only skinned one is `oMK01_01`, 3,952 vertices. `s01c1e.map`: `F_PC01` at (7286, 6064, 1138), z turn -0.48; `CAM_EV10` at (7236.6, 6105.9, 1207.1), aim (-0.25, 0.39, -0.89), which looks down at tatami, a picture book and bedding; `CAM_EV11` at (5061.4, 5617.2, 92.7). `placeMapCameras` already builds both as inactive `StageCamera`s. **`CAM_EV10` is not the opening's camera:** script entry `sd_MK01_10` switches to it with native 0x64 (0x1c8dc0: view priority 3, `pushView`) and back with 0x64 of `NULL`. The opening's view is `DemoCamera`, the bone `bn_top` of `Model_camera`. **The player comes from the script too:** `Root` calls native 0x70 (0x1c81a0) with `F_PC01`, which finds the character and calls `setPlayer`; `setupCameras` only does that in a scene with no script. Natives 0x64 and 0x70 are written (game/scriptrunner.cpp, below). Placed by hand, with no collision to pull it in, the main camera's follow offset from `F_PC01` shows a wall up close.

**Two more node classes**, both over `StageNodeBase`: `StageBoneNode` (vtable 0x315a50, EE 0xc0, an array from `StageModel::bindBones`): bare local +0x40 / world +0x80, setDirty empty. `StageMatrixNode` (vtable 0x315ab0, EE 0x1c): a pointer +0x18 to a matrix it does not own, the +0x80 of a motion's bone (`StageModel::setBaseMotion`), rebuild empty. Vtable 0x316c30 (EE 0x34) derives from it, built in 0x268830. `StageModel::boneMap` holds them, as `StageNodeBase *`.

**The walks**: `updateMatrix` slot +0x20 rebuilds the node, keeps `m_inverse` when there is one, then `updateChildMatrices` 0x1d8480 finishes a whole level (rebuild and inverse on each child) before going down. `propagate` 0x1d8260 belongs to the base, not to StageObject: it copies world row 3 to `m_lastPosition`. `setInverseTree` 0x1d7fc0 allocates or frees `m_inverse` down the tree (callers 0x175d40, 0x28e000, 0x295010, none written yet).

## The lights (game/stagelight.cpp, 2026-11-04)

`StageLight` (kind 0xf, abstract) with `SpotLight` (0x11, EE 0x300) and `PointLight` (0x10, EE 0x2c0), 0x184860..0x186ee0. The adjustor thunks just before it, 0x184610..0x184860, are the item and bullet classes': CodeWarrior closes each file with its thunks, so **a run of 8-byte thunks marks a file's end**. The lights' are 0x186cc0..0x186ed0, then `StageMapFile::relocate`.

A light has a fourth base, **`CollisionOwner`** at +0x200 (a flag at +0, vptr +0x204, vtable 0x3115a0: six slots, five empty): a stage's lights own a `CollisionBody` (+0x274, EE 0xe0, bucket 2, category 0x2000, a sphere of radius 20) in `gStageScene->records`. Slot 1 `onCollide`: a dead bullet (kind 0x19, `m_dead`) spawns the effect `lightbreak`, breaks the tree and disables the light. Slot 2 `syncBody` copies the world position to the body. Slot 4 answers the object, which is how the world compares two bodies. The same base sits at +0x240 in the items and the bullet ("The bullet and the items").

Table layout, from the first vptr: the five owner slots again after the +0x204 table's own (+0x27c..), then the `cmd*` setters, which are also **the script's commands**: `ScriptRunner` slots 16..33 and 86 are thunks to them (table in stagelight.h). So runner slot 22 (+0x60) and 24 (+0x68), the tree walks' two flags, are **switched on** (+0x214) and **broken** (+0x272) on a light. The side base's four slots are the action lists' entries (`ScenePairTable`, list 2 for lights): slot 2 is the light's frame (`sideHook2`), slot 3 a spot's map trim.

The frame: the colour track (`VectorTrack` +0x2ac, a keyed vector, ENV_ lights and mode 4; its w is the weight), the fade +0x23c towards +0x238 frames (10; -1 at the start jumps to the end), the effect on or off, the two `PartSwitch` (`bn_<name>_0, _1` lit and dark, `bn_<name>_f_0`; both show one part, hiding the rest through `CharacterPart::drawMode`), a temporary light's seconds (`m_keepFlags` +0x2a2, `setLifetime`). `isLit`: not broken, brightness not 0, on or fade outside {-1, 0}. `getWeight` = fade / frames (or the track's w) x brightness.

**`VcmTable`'s controllers are lights** (the stand-in `VcmController` is gone; `getWeight` reads light fields, and a model's VCM tokens are the names of lights). `SceneRender::updateLights` picks by `lightAt` (table +0x26c; the strongest of each ENV / other class) and, for characters only, `shadowAt` (+0x274), gated by `m_usable` +0x270, `m_bounds` +0x280 and `inBoxes`. +0x2b8 is an object: the shadow only counts while that is the player and the segment head to head is clear.

Effects: a spot named TORCH takes `dento1`, SigLamp `siglight`, both at the light turned (0, pi / 2, pi / 2); otherwise an effect named after the light; otherwise the lpt file's 0x40-byte records {name, effect, pos, rot}, matched with strcasecmp. The effect's object-tree node goes under the light's. Effects are `StageParticle` (EE 0x280, kind 0x1d, made by `StageNodeTree::spawn`, the name `StageEffect` being StageSound's).

## The stage script VM (game/scriptrunner.cpp, 2026-11-04)

**The file** (`StageScript`, relocate 0x1d3a40): header {table, table2, count, slotCount}, 0x1c-byte entries {name[0x10], code +0x10, slot names +0x14, size +0x18 with its top two bits in the bottom two, slotCount +0x1a}, table2 (slotCount * 0x10), then per entry its code and its slot names, then the **SSS block** {"SSS\x1a", first sub-block +4, count +0xc}. A code ends in a 0xc-byte trailer {word, 0xc, "SSS\x1a"}, and relocate writes the block's address over the word. Sub-blocks {magic, size, table offset (0x10), count +0xc, relocated +0xe}: **LBL** (each entry block + offset + 4, the name; the word before a name is its **cell**), **STR** (block + offset), **SST** (relocated, never read). **PC: every one of those is a 32-bit field, so the host keeps offsets** (a field's owner + the value) and the accessors add them; a cell holds a handle into a host table (`scriptLabelCell`).

**A runner** (`ScriptRunner`, vtable 0x315850, EE 0xb8): +0 parent (whose tables and locals it shares; nothing written sets it yet), +4 run depth, +8 labels, +0xc strings, +0x10 trailer, +0x14 locals (slotCount ints, gp-0x7a54 with none), +0x18 wait frames, +0x1c stack[16], +0x5c the pending math of each push, +0x9c sp, +0xa0 pc (null: from the top), +0xa4 target, +0xa8 entry (null: no script; this is what `StageScene::update` tests, not a separate flag), +0xac / +0xb0 what `scriptYield` keeps. `ScriptFrame` (vtable 0x315a08, EE 0xa8) is the same fields put aside; its callers are natives not read yet.

**The VM** (`execute` 0x1d1c40): an accumulator, a pending math op, words. 0 ends the pass (from the top next time, so an entry that does not exit runs again each frame it is run), 1 exit (`setEntry(0)`, slot +0x14), 2 wait acc 30ths, 3 the wait, 4 / 5 jumps counted in words from the operand, 6 target by label, 7 / 9 load / store a variable, 8 constant, 0xa push, 0xb pop and apply, 0xc..0x12 the math prefix of the next load (+ - * / & | ^), 0x13..0x18 pop and compare, 0x19 / 0x1a a local by name, 0x1b / 0x1c the parent's locals, 0x20 / 0x21 the grandparent's, 0x1d / 0x1e target by character index / handle, 0x1f skip one, 0x22 target raw, 0x23 player (or `controlled`), 0x24 partner, 0x25 self, 0x26 local, 0x28 / 0x2a property, 0x2c global, 0x30..0x35 compare with the operand. Variable words: bit 0x8000 a property of the target (a slot +0xc load, +0x10 store; the store also has a sub index in bits 16..23), 0x4000 `FlagTable`, 0x2000 a local, else a local below 0x40 and a global (`StageScene::scriptSlots`) from there. **The target lasts one word**: after every word but 6, 0x1d, 0x1e, 0x22..0x25 it is the runner again, so a script pushes its arguments, then names the target, then calls. The words 0xff...... are skipped, other top bytes than 0, 1, 2, 0xff stay on the word for ever (the VM yields there).

**Calls.** 0x01000000 | id: table 0x304230, index id - 0x19, ids 0x19..0x235, `const int *native(runner, int *acc, const int *code)`. Each native pops its own arguments (inline, `sp -= n`) and takes code words through 0x1d3200 (label, the first three refresh their cell), 0x1d32e0 (label as a name), 0x1d33c0 (label), 0x1d3410 (cell or label), 0x1d3470 (string). `sssnatives.py` reads every native's pops and words from the ROM into `src/game/scriptnatives.inc`; **a native not written yet still pops and skips**, and logs `[stub] scriptNative_<id>`. A waiting native calls `scriptYield` 0x1d3710: the VM keeps pc on the native's word and restores sp, so it runs again next frame with the same arguments (13 natives: 0x4f, 0x52, 0xaa, 0x142, 0x147, 0x149, 0x159, 0x15f..0x162, 0x191, 0x197). 0x02000000 | id: slot +8 `runCommand(id, code)`, the base answers code (characters: 0x1d8630, which uses `popArgs`).

**Label indices 0, 1, 2 are the player, the partner and self**, read from the scene, not the table; `scriptPrepare` 0x29f050 fills the other cells: an object by `findAny`, or a mission item text for a name starting @ or _. Values in registers are 32 bits: `scriptObjectOfValue` 0x29f500 takes an odd value as a handle (`StageObject::getHandle`) and an even one as an address, which **cannot work on the host**: a native that leaves an object in acc must leave its handle.

**Lights by name**: most light natives do not touch the target: they read its name and walk `findAll5(name)` (group 5), calling the same slot on each. Slots +0x48..+0x8c are stagelight.h's `cmd*`. A plain runner has no name (slot +0x24 null), so `setupCameras`' runner does nothing with a light until an opcode 6 names it first. Native 0x27 calls the target in its walk, not the namesake (kept).

## The lights of a stage (game/scenerender.cpp, 2026-11-04)

**Every number comes from a script.** `SceneRender` is 20 sets of 36 `VectorTrack`s (`SceneLightSet`; EE one block of 0x2d0 tracks, pointer tables +0x12c..+0xc1c): three directions + their angles + colours, ambient, the same for models with a second colour and ambient for ENV_SB boxes, fog (lanes near, far, near value, far value) and fog colour, four far fogs, clear colour (0..255), transition (lanes camera range, frames, frames in an SB box). Nothing fills them but the script natives **0x1b4..0x1c3** of table 0x304230 (id - 0x19; 541 natives, ids 0x19..0x235, in reverse address order there): `0x1c2` addSetKeys(set, time) makes one key in all 36 tracks, then 0x1b7 ambient(set, key, r, g, b), 0x1b8 light(set, i, key, rx, ry, rz, r, g, b), 0x1b9 modelAmbient, 0x1ba modelLight, 0x1b6 clearColor(set, key, r, g, b), 0x1bb fog(set, key, near, far, near value, far value, r, g, b), 0x1bc the first far fog (set, key, ...), 0x217 far fog (set, i, key, ...), 0x1bd / 0x1be / 0x1e1 transition lanes 0 / 1 / 2, 0x1df / 0x1e0 the ENV_SB ambient and colours, 0x1c3 setSetLoop. **Colours are integers / 127.5 (`colorToFloatGs`), so 255 is 2.0 and 127.5 is 1.0**: the lights do go above 1, which is half of why a map is dark with drawSysInit's defaults. Angles are degrees: a direction is (0, 1, 0) through Ry, Rx, Rz (`updateDirection` / `updateModelDirection`). A track needs two keys: `step` and `reset` only write `value` from count 2 up, so a one-key track stays as it was.

**rena's light records are a small class tree (batch 234, 2026-11-07).** Vtables: 0x315f00 `RenaObject` (dtor 0x1fa980 only; the DrawSys objects' dtors end on it too) -> 0x315f20 `RenaLightBase` (dtor 0x1fa9d0 only) -> 0x31a780 `RenaLight` (abstract: 0 at slots +0x14, +0x18, +0x20, +0x24) -> 0x31a750 `RenaPointLight` (SceneRender's base) and 0x31a720 `RenaSpotLight` (SceneRender +0x80). Slots: +8 deleting dtor, +0xc setActive, +0x10 isActive, +0x14 getKind (1 point, 2 spot: `RenaLightKind`), +0x18 the VU1 upload (0x2e05f0 point, 0x2e00f0 spot; the host's fillPointLight / fillSpotLight in rena/rena_draw.cpp), +0x1c getMatrix, +0x20 getAngle (the spot's +0x7c; a point answers -1), +0x24 getRadius (+0x70), +0x28 getColor and +0x2c getShade (three float pointers each). The point and spot dtors 0x1fa880 / 0x1fa900 and the two bases' are emitted beside SceneRender, so 0x1f6ac0..0x1faa30 and 0x2e00c0..0x2e0840 are all game/scenerender.cpp. Nothing found calls slots +0x14, +0x20..+0x2c yet; the names of the two bases are the port's.

**The tracks and the fades against the ROM (game/vectortrack.cpp, `tools/reverse/vectorcmp.py`).** VectorFade, VectorKey and VectorTrack are in their own game/vectortrack.h now. The tool played 60 random scripts on the ROM's own code beside the host's, every kind of track, 20,155 ops, 968,131 words compared as bits, 0 differ; it reached every instruction of the file except 14 words of `VectorKey::setTime`, its branches for a key with a next key, which its only caller `addKey` cannot take (the key is chained last): those were read by eye and agree. The order of the float work is the ROM's and it matters to the last bit: the Bezier's middle weights are `t * ((3 * u) * u)` and `t * ((3 * u) * t)`, the first key's control point goes through `divideVec4` (a multiply by 1/3, not a division per lane), and a point along a direction scales first and adds after. `VectorTrack`'s ctor in the ROM leaves `value` and `done` as the heap had them; the host zeroes them.

**The scripts on the disc (`sssall.py`, 2026-11-05):** 434 with an SSS block of version 0.4, which is what the scan looks for, so a script of another version would be missed; 142 of them outside the SLDI packs; 364 natives are called, 168 never. The SSS block's +8 / +0xa are that version, and `scriptHasLabelTexts` 0x29eff0 tests it for native 0x126. The most used: 0xf4 distance to x, y, z (251 scripts), 0xb9 no path to (236), 0xca last path length (236), 0x3c shout (232), 0x2f (165 scripts, 34,311 calls) gives a brain a script: `StageCharacter::scriptSetBrainEntry` 0x21b970 takes kinds 6, 5, 2, 1 into slots 2, 6, 4, 0 of the second script's entries. **Scene +0xdc is `SceneEffectList`** (game/sceneeffects.cpp): 16 entries {is rain, handle, effect}, natives 0x5b.. give scripts a handle to an effect; a name s??rain marks the stage's rain. **Runner slots read this run:** +0x94 the actor or null, +0xa8 script for a brain slot, +0xb0 who was heard, +0x124 crouching, +0x154 stage menu line.

**Stage 1's real values** (the script file s01 dumped from a New Game run; `sssdis.py` reads it; 42 entries, s01, s01_l and one per named light: CAR_pl, T_TORCH1_sl, yami_pl, ...): the entry **s01_l** keys the sets in straight lines, keys 0 and 1 of each set equal. Set 0: clear (0, 0, 0), **ambient (175, 180, 200) = 1.37, 1.41, 1.57**, all six directional colours and angles 0, model ambient (65, 68, 71) = 0.51, 0.53, 0.56, fog 1100..1500 at 230..0, colour 0, range 1600, fade 10 frames. Sets 1, 2 the same with model ambient (50, 54, 57), (40, 44, 47) and fog 1100 / 980. So on this stage **the map has only its ambient, about 1.4 to 1.6 blue-tinted**, not 0.1, and the fog closes at 1500. **Seen 2026-11-04:** `setupCameras` runs all 42 entries, the 18 `addSetKeys` come from s01_l, and with them `SceneRender` holds set 0, ambient 1.37255, 1.41176, 1.56863 and fog 1100..1500 in the running game.

**Why stage 1 is dark (measured 2026-11-04):** the map's vertex colours average 20 of 0x80 (0.16), so with ambient 1.37..1.57 and no directional light a vertex averages 27, 28, 31: a dark map by its own data, not a lost light. **Fog, VU1 0x22f..0x259:** VU address 24 (0x2e5a00; copied to 199) is (A, near value, far value, B * the scales 0x59b190, 0x59b194), and each vertex takes clamp(A + B / W, far value, near value), so a near value of 230 tints everything inside the fog near by 230 / 255 towards the fog colour. Disabled: (0, 255, 255, 0), no fog. It is `DrawSysFog::getFogValue` in microcode.

**The frame** (`update` 0x1f73c0, after the particles): every track but the angles steps; the set is the `m_number` of the ENV_CB box at the view (`viewLinkedObject` null: the top camera's position; else 200 ahead of `PlayerMotion::getLampMatrix`, ray 0xfc2600f, 10 off the wall), or `m_forcedSet` (`sceneRenderGroup(0)`, -1 here); a new set or `m_snap` starts all the `VectorFade`s over the new set's transition frames, at once when snap; the values go to `DrawSysLights` of the current record (that is what the map's models get, `renaSetLights`), the record's fog (far + the top camera's `m_range2`), `drawSysGetFog` 0..3, and every camera of group 0 gets `setRange` (lane 0 of the transition, default 1600). **`StageScene::getActiveCharacter` was the top view** (`views->topObject()`, a camera: it is tested for kind 9 and its +0x2d0 read), now `getTopView`: it does not answer null without a character, the ROM does not crash there and no PC guard is needed for it.

**Per model** (`updateLights` 0x1f8620, groups 1, 2, 3, 8): only models with `m_lit` (`markModelAsItem`: items; the writers of the script side unread). The map's Root model has it clear, so it takes the record's lights. Own set and ENV_SB flag (at `m_lightProxy`'s place if set), the walk of group 5: `lightAt` strongest ENV_ and strongest other, `shadowAt` for characters. Light 0 is the strongest other (a character: only the ENV_), when `m_lightsModels`; lights 1, 2 the strongest ENV_ and other. With a light: direction to it (light 0 z negated), colour `m_modelColor` / `m_color` * weight, ambient plus `m_modelAmbient` * weight; without: the set's model tracks. An item lying free gets its ambient scaled by 1.5 + 5 * (1 - mean)^2 when the mean is below 1, so it shows in the dark (`getModelAmbient`). `shadowDirection` eases 0.7 towards the last light that reaches (the ROM's "strongest" weight is never stored, so it is the last, not the strongest; kept).

**The scene's point and spot** (+0 base, +0x80 member, rena's records 0x2e0xxx; every lit model's `ModelInstance::spot`): of group 5, the highest `m_priority` (never -1) of each class, nearest wins a tie. `yami_pl` (priority 4) hangs under a character of class 8, `scr_demo_pl` / `demo_pl` priority 5. What holds a lamp gets priority 4 for it. The host renders neither record yet (VU1 0x545).

**What a character answers on the runner's slots (batch 171, 2026-11-06).** The tail of the character's object file, 0x17e470..0x17e860, is 63 adjustor thunks in the order of the tables 0x31227c (runner, -0x40) and 0x312264 (the four side hooks, -0x18): game/stagecharacter.cpp lists each one with its target. The list before them, 0x17e140..0x17e400, is `StageList<StageObject *>` (a second instance, with getTail and find, follows game/playermotion.cpp at 0x217700..0x217940; then StageCharacter::getMotionDoor 0x2179b0, m_motion->doorLink->door, and SightTable::canSeeThisFrame 0x2179c0, the sight-table line asked for this frame, inline in StageCharacter::canSeeThisFrame; 2026-11-07). Read against the host, four slots had no override and fell to the default:
- +0x3c `setPosition` 0x218500 (also slot +0x1d0): moves, `updateMatrix`, `PlayerMotion::reset(m_placeMotion)` (then 0), `CharacterGround::reset`, `onViewTop` on the top view when the view is linked, `CharacterBrain::stopAndRetry`, `m_placedThisFrame` = 1, which holds the brain for a frame. Natives 0x26, 0x47, 0x48, 0x144.
- +0x14 `onScriptExit` 0x219010: the brain's `resetScripts`.
- +0x140 `scriptPlayMotion` 0x21ac60: `setSpecial` when controlled and allowed, the brain's `playActionAndRetry` otherwise. Native 0x68.
- +0x160 `cmdIsLit`: `isLampOn`, the property Prop_SwitchedOn.
Slots +0xbc, +0xc0, +0xc4, +0xe4, +0x104, +0x134, +0x168 have no caller that a script reaches: natives 0x36, 0x37, 0x63, 0x6c, 0x84 are on no script of the disc and +0xe4's only site is the brain's own table. They are written as plain members. `tools/reverse/slotcheck.py` does this comparison.

**The volumes' tables, corrected (batch 171).** 0x3127f0 is `StageVolume`'s base table and 0x17eb20 its dtor; the box's table is 0x312580 and its dtor 0x17e860. The old name had them one class off. A box's four side hooks, 0x17e910..0x17e940, are empty.

## The runner table's defaults (0x171ad0..0x1723c0, 2026-11-07)

**What the run is:** inline functions CodeWarrior emitted into the door's object file, between its dtor and its first method: the bodies StageObject's runner table 0x3147ac leaves at their defaults (slots 3..107), the accessors `StageObject` inherits, and `DoorRecord`'s setters. So game/stagedoor.cpp owns the range and lists it; the bodies are in the headers. **Six shapes, and a shape is a type:** `jr ra` void; `jr ra / move v0,zero` a null or 0; `j defaultCommand` void or int; `jal defaultCommand; sltu v0,zero,v0` a **bool**; `jal defaultCommand; move v0,zero` a pointer (+0x190); `jal unsupportedCommand; move v0,zero` a pointer with the log (+0x98). `tools/reverse/defaultcmp.py` reads both sides. **Slots no class overrides:** 5, 6, 8, 9 (no native found; a scan of the offset alone cannot tell them from other classes' calls) and 54, 56, 60, 62, 69, 76, 77, 79 (no native; ScriptProxy forwards all but 54, which gives their argument counts). **The natives with a slot and no script:** 0x2e +0xac, 0x36 +0xbc, 0x37 +0xc0, 0x4b +0x110, 0x63 +0x134, 0x69 +0x144, 0x6c +0x104. Host natives usually ask `getScriptCharacter()` instead of a slot, which is the same on a non-character (null, so 0) and goes through a brain the same way, since `ScriptProxy` forwards that too.

**Item uses are bool:** +0x16c, +0x170, +0x174, +0x17c, in the ROM's natives `andi v0,0xff` into acc. The character's go by tail jump to `useItem` 0x218b50 (1 on one path, 0 after a `scriptYield` and on the others) and 0x218850; the brain's by 0x1da050, 0x1da0b0, 0x1da0c0, 0x1da110. +0xa8, +0xb8, +0x140, +0x154, +0x158 stay void: natives 0x2f, 0x35, 0x148, 0x7a, 0x80 do not read v0.

## Scripted events (game/demoevent.cpp, 2026-11-04)

**How an event starts.** A script native asks: 0x1f2 `pendingEvent(name)` (black at once; `StageMission::afterScripts` 0x286c40 starts it after the frame's scripts), 0x15f / 0x160 file event, waiting for its end, 0x161 / 0x162 and 0x155 script event. The waiting natives yield with `scriptYield` and take the state back from the runner's +0xac / +0xb0 (`ScriptRunner::takeYield`). `StageScene::runStageScripts` polls `event` first and runs nothing else of the stage while one exists; a finished one is deleted there.

**StageFileEvent** (vtable 0x318010): state 0 fades if `fadeSeconds` > 0, then `queueFiles` 0x287f20 mutes the sound, waits 24 frames and `isSettled`, and queues `demoEU/s<stage>/<name>.edp`, `<name><lang>.fsm`, for languages outside {1, 5..9} `.tm2` and `.cod`, and `streamEU/demo/EV<aa>_<bb>[A-E]<suffix>.xwh`. `takeFiles` 0x287db0 waits and calls `requestSubScene(name)`; `stageThread` then builds a second StageScene over the same files and `enterSubScene(pack)`. **StageSimpleEvent** (0x318030) plays the entry of its name of `scripts[0]` in a runner of its own, with the objects' flags saved and hidden.

**An .edp pack**: +0xa count, +0xc script's index (negative none), +0x10 pairs of (name offset, data offset). Its files: `<x>.mdl`, `<x>.mtn`, the script. `Model_camera.mdl` is the camera's skeleton (`setBaseMotion`, not a drawn model).

**How an event ends (d0101, measured 2026-11-04).** The sub-scene ends when its script has stopped **and** the voice has stopped (`StageScene::update`), or on start when skippable; `stageLoop` then deletes the sub-scene's scene and takes the parent back. d0101: cut d010100Cam01 at frame 0 with a fade from black (0x163, 15), d010101Cam02 at 929, fade to black at 1262 (0x164, 15), voice 51.02 s (48 kHz stereo, 2,798,720 bytes), end at 1276. The logic ran at 25 frames a second there. The parent is left at full black by native 0x1f2. **`StageMission::afterScripts`'s fade only runs on a stage's first frame** (`getMenuMode` answers 1 only while `inFrame` is clear), so it is not what ends the black after an event; `Root` does it: from the second frame on it jumps to 0x258c, and while slot 0x2014 is 0 calls native 0x4e fadeIn(15), waits 20 and sets it. `Root`'s 0x70 (0x1ae0) is the player: `findCharacter` + `setPlayer`, which links the main camera.

**The event's objects (`game/subsceneobject.cpp`, 2026-11-04, batch 112).** `SubSceneModelObject` (vtable 0x316c60, EE 0x240) stands on `StageObject`; +0x200 a lamp of four lights on bn_top (`SubSceneLamp`, built by slot +0x264 on first use), +0x208 the role (the ctor's argument: 1 map, 2 character, 3 item, 8 an actor with no model). The ctor clears the dirty tree and leaves only role 1 active and shown. On it: `SubSceneMapObject` kind 0x20 (0x319c90), `SubSceneGateObject` 0x22 (0x319a20), `SubSceneDoorObject` 0x21 (0x314ef0), `SubSceneCharacter` 0x23 (0x3197b0), `SubSceneItem` 0x25 (0x319540), `SubSceneProp` 0x24 (0x315420); the base alone is 0x1f. **Setup puts an object in the list's group 8; native 0x145 `demoBind(name, motion)` is what makes it live:** slot +0x24c `bindMotion` (a character takes `m_motionFile` of its stage self, the rest the pack's name.mdl: setBaseMotion, bindBones, placeBones, joins `ScenePairTable` list 1, setDirtyTree(true)), then the tree on and shown. A name in no list becomes the file's character or item, else a role 8 actor. The native only works before `camerasReady`, so an event binds in the bg script's entries that `setupCameras` runs. Script ids are the table index + 0x19 (sssdis.py): table index 0x12c is native 0x145. **Native 0x148 `demoMotion(name)` calls runner slot +0x140 on its target: on a `DemoCamera` that is the thunk 0x26d000 to `playCutNamed`, on the family 0x1a6860 to `playNamedMotion`.** Natives 0x163 / 0x164 fade back from / to a colour (the ROM's colour is only the second argument: asm 0x1b7070 overwrites its shifts). The frame is the side base's third hook (`sideHook2`, `StageModel::advance` at 1.2): nothing calls it until `ScenePairTable` exists.

**DemoCamera** (vtable 0x316ed0): each cut is a motion on that skeleton. `cameraStep` 0x26cb90: advance 1.2 frames (60 / 50? a guess), `bn_fovy` or `bn_length` gives the angle, `setScrZ(height / tan(angle))`; `bn_top`'s matrix with rows 2, 0, 1, 3 and y, z swapped (z negated) becomes the world matrix. So **the event's view is entirely bones.** A cut is started by native 0x159 0x1b7e70 (written): a runner's first one waits a frame, then `playCutNamed` on the top view through the thunk at +0xf4 slot +0x140, and `m_demoCut` + 1. Vtable +0x25c is `cameraStep` (0x178160 in StageMainCamera), +0x260 is `onLinkChanged` there and `playCut` here, +0x264 `playCutNamed`.

**Subtitles (2026-11-04, batch 116).** `DemoStream::drawSubtitles` 0x2b0d60 draws only in a sub-scene (`StageScene::draw`'s else, `PacketList_Screen`), when `GameOptions::subtitles` (+0x1c, default 1) is set: seconds = position x 28 / 16 / channels / rate, `fsmFindText(seconds, fsm, 0)`, centred on (0x100, 0x1bc) with colour 0x80606060; the last line stands there, more lines above it. `setFont` 0x2b1010 makes a `WideFontPrinter(0x700, 0x700)` on the event's font and image.

**An fsm file** (`FsmFile` in demoevent.cpp): "fsm\0", +4 relocated, +0x10 tracks, three tables at +0x14..+0x1c names, +0x20..+0x28 cue lists, +0x2c..+0x34 texts (count, table, base), +0x40 tracks of 0xc {name index or negative, cue count, cue list index}; a cue is 0xc {start, end seconds, text index}. **The host keeps file offsets where the ROM keeps pointers** (a 32-bit cell cannot hold one); the three readers are the only ones who look inside. d0101's single track has 5 cues, English: 10.73 "Open the door!", 22.93 "Don't let it get away!", 25.93 "It's outside!", 28.93 "Don't let it get away!", 41.60 "Daddy...? Kanae...?".

**`WideFontPrinter`** (vtable 0x319ff0, 0x2d3700..0x2d4150, in `game/fontdata.cpp`): `FontPrinter` for 16-bit text, no printf. Slots 4 and 7 are thunks at 0x208b70 and 0x208ba0, out of the range. Slot 5 calls slot 11. In the C++ the text arguments keep the base's `const char *`.

**`StageMission` notices:** `setNotice` / `setNoticeEnabled` fill the record at +0x688; `showNotice` 0x286cf0 formats it with `$1` into the next of four buffers and calls `PauseMenuState::addNotice` 0x207aa0 (11 entries; a level below 5, data 0x31ac18, is raised to 5; frames 0 is 0x7fffffff). `addTimedRecord` 0x286f30 scans only the first 12 - count records, so a table with used records at its end can answer 0 with free ones left.

**StageFade** (+0xfc): `start(seconds, target, from)`, from < 0 keeps the level; mode 1 running, 2 done. **Its rect starts at 0x700 and `gsFillRect` adds 0x700 again**: the far corner wraps past 0xffff to 0 and the sprite covers everything from 0 to 0xe00, which is how it fills the screen. `gsFillRect` keeps the wrapped words.

**StageMission** (0x13d0 on the EE): stagescene.h has the map. `setup` 0x2870f0 builds `scriptObjects` (Root first), `characters` by `m_id - 1`, the 16 class lists and the item lists. **Now that it runs, every object's script runs each frame**, reaching the stage's natives.

**Stage 1's opening is d0101, proven from the script**: the entry `Root` tests `menuMode == 1`, and with property 0x3c zero (0x21c8) goes to 0x2558: native 0xac `AMB_ST01_01`, native 0x1f2 label 0x4a, `d0101`. The other branch asks for `d0106` (label 0x48). Labels are the script's LBL block: offsets from its +0xc to names.

## The bullet and the items (game/stageitem.cpp, 2026-11-04)

0x17fce0..0x184860, 139 functions, whole file (`stageitem.h` holds every class; `gamehooks.h` lost them). A character's gear, one family: `StageItem` (kind 0x13) with `StageItemE` (0x14, the lamp), `StageWeaponItem` (0x15), `StageTrapItem` (0x16), `StageThrownItem` (0x17); and the bullet, two classes: `StageBulletBase` (0x18, ctor inline) and `StageBullet` (0x19, `new`ed by `StageWeaponItem::fire` 0x181f40 and once, off and with null arguments, by `setupCameras`).

**Slots from the first vptr.** Items: +0x24c `setupItem`, +0x250..+0x25c the side base's four again (0, 1 empty for all; 2 the frame: `advance` the motion; 3 the model's flag 0x400), +0x260 `canPick` (through the jump 0x183780). `StageItemE` adds +0x264 `setDark`, +0x268 `setSwitchedOn`, +0x26c `isSwitchedOn`. A class with a `CollisionOwner` has it at +0x240 and the owner's six slots at +0x26c.., then its own primaries again.

**`CollisionOwner` read in full here.** Slot 1 is `onCollide(contact, sweep, face, t, distance)`: a0, a1 = {own body, other body}, a2 the sweep vector, a3 the map face, f12 t along it, f13 the distance. The light's one-argument version was only what it read. Slot 3 is called with the body, after the frame's contacts. Slot 5 answers the object the body must not meet: a bullet's shooter.

**A bullet** is two bodies: bucket 2 capsule (shape 5, ends +0xa0 / +0xb0, radius +0xc0 = 8, category 0x80, mask 0x2002) and bucket 1 segment (shape 2, ends +0x60 / +0x70, category 0x100, mask 1, +0x44 / +0x48 its raycast mask and flags). `syncBody` puts the node's `m_lastPosition` (+0x10, a 0x10 heap block that `StageObject::propagate` fills; most nodes have none) and the position at the two ends, so each frame is a sweep. The segment's contact records the map hit (+0x26c hit, +0x270 point, +0x280 distance, +0x284 face). A face with 0x10000 lets it through (a door's always, others half the time) and `raycast` looks on with that flag added. The capsule's contact kills the bullet unless the map is nearer; a light sees `m_dead` then and breaks. `collisionHook3` on the capsule shows the map hit: effect by `face +0x4c` (0x1f GUN_SPLASH, 0x1d kyusyu on stage 0x1b only and nothing elsewhere, else GUNHIT), sound from `getSurfaceSound` 0x2cf0b0, then dead. Speed: 20000 units a second over the clock's rate. Range: `WeaponDef +0x18 / speed + 1` frames. The shooter's +0x5fc == 0 re-aims the bullet each frame at its +0x2a0 (the names `m_aimLock`, `m_aimPoint` are guesses), and a flying bullet sets the shooter's +0x688.

**`ItemPhysics`** (vtable 0x316510, a `CollisionOwner` at +0x240, EE 0x210; the name is the port's) is what thrown items and weapons have and traps do not: section "The items' physics".

**Items.** `gItemDefs` gained startCount +0x18, pickSound +0x1a, isBlade +0x1b, landSound +0x1c (the last two sounds are parsed by name into indices, `ItemDef::setLandSound` / `setPickSound`, from `PackTables::parse`); `gWeaponDefs` range +0x18, magazine +0x24. `ModelInstance` / `StageModelData` have flags at +0xa: 2 set on every item, 0x400 clear only while an item lies free with a dirty matrix. Pick point: the motion's `bn_pic`, turned (x, -z, y); weapons also `bn_point` and `bn_fire`, turned (x, -z, -y). The muzzle `bn_fire` is where `fire` puts `M_Flash` and the 0.1-second `flash` PointLight. G_NN38, R_MURATA and G_26NEN load round by round: no shell (`yakkyo`), RELOAD_* in place of MAGAZ_*.

**Thrown** (I_HATUEN, I_AKIKAN, I_PET, I_NUT): at rest and used, gone after 10 seconds. I_HATUEN is a flare: 20 seconds of `hatsuen` where it lies with SMOKE_ON, or `heyasmoke` once it rests inside an `areas[StageArea_Smoke]` box (+0x470 is 1 until asked). **Trap** (I_WANA): a sphere of radius 80 (bucket 2, category 0x200, mask 8), on only while open; the close and open motions are s_OBJ_098_CLOSE / OPEN; springs when the character is within 90 and the angle is under pi / 4. **The lamp** `ItemLamp` (EE 0x18, `StageItemE` +0x240): four lights of mode 1 under the item in both trees, `%s_sl` (`m_limitByMap`), `%s_sl2` (white, radius 200, cone 45), `%s_pl`, `%s_pl2` (not usable by `updateLights`), and the part `nl`. `SceneRender::update` asks `getLightMatrix`: the last point's matrix, pulled back 30 from a wall on the line from the holder's `bn_head`.

**The item waits: what "Throw Smoke Candle" and "Set Smoke Candle" run (2026-11-07, batch 192, game/playermotion.cpp).** The stage menu offers both lines on a character who carries I_HATUEN; the player's script F_PC<stage> then calls native 0x105 (`scriptUseSmoke`) or 0x106 (`scriptPutItem`), and from then on the same native runs each frame until it answers. Stages 17 and 20 start F_PC17 / F_PC20 with five flares; the default part of no other of stages 1..30 hands one out in its first 24 s (the other parts were not run). `StageCharacter::throwCarried` 0x218b50 (natives 0x104, 0x105, 0x15a, called `useItem` until this batch) and `putSmoke` 0x218850 are the same machine, kept in the running script's yield: low four bits the state, a frame above them, always under yield id 0x104, whichever native runs. Ask (0): findCarried, nothing carried answers false at once; a controlled character gets `wantThrow` 0x20b200 or `wants.putItem` directly, past `allows`, and the conditions read those; a brain gets `askThrow` 0x1db6f0 (route `throwAt` 0x22ab90, which does test `allows.throwItem`) or `askNothing` 0x1db6e0, always false. Asked (1): the motion, 0x9d or 0x12, on top of the history moves to Playing; otherwise, 30 frames after the stamp, Failed. Playing (2): on top, it waits for `kMotionEventEnding` (bit 2, `noteEnding`); another motion on top means the entry is looked for in the ring and it counts only if it reached noteEnding. Done (3) answers true, Failed (4) false, anything else yields. **Two slips of the ROM, kept:** `putSmoke` never stamps the frame, so Asked times out from frame 0 and past the mission's first second the motion must be on top at the next look (it is, when the condition allows it: `yield 1` then `yield 2` on stage 17); and a brain asked to put the smoke down yields for ever while it carries one. With an empty history or a motion gone from the ring the ROM reads the words at addresses 0 and 4; the host takes that as no entry. `PutItemCondition` needs `walkLink->wallFace` null: at a ledge with the Down prompt the menu still offers Set, the wait fails and nothing happens, as in the ROM's code. `tools/reverse/itemwaitcmp.py` compares both.

The list instances at the end (0x184260..0x184580) are `stagelist.h`'s 4-byte StageList, named `StageNodeList::` in Ghidra (dtor 0x184260, pushBack 0x1842c0, erase 0x184370, clear 0x1843c0, pushFront 0x1844a0, getTail 0x184530, Node::Node 0x184560, Node::unlink 0x184580; batch 245, 2026-11-09), the twin of the StageObject list at 0x17e140..; `CollisionBody`'s +0xd4 list uses the same instance. 0x184430 / 0x1845f0 are `CollisionBodyArray`'s dtor and `clear` (count 0, first slot null), which the bullet's dtor has inline. 0x183780, the items' +0x260 slot, is only `j 0x1838d0` (`StageItem::canPick_thunk`); PickupItemCondition calls 0x1838d0 directly. The file's empty and one-line slots are header inlines, listed in stageitem.cpp's "Written in headers" block: the TU counts 139 of 139.

## The draw system (game/drawsys.cpp, 2026-11-04)

A stack of records (gp-0x7994 = 0x31aedc the current one), each with its models, camera and lights. `drawSysRender` writes these packet lists: 0 set-up; 2 the far_ models (mask 2, fog 0); 4 far_1..3 (masks 4, 8, 0x10, fogs 1..3) at 1/20 scale; 5 a depth clear, then the main models (mask 1) with shadows and the lights' fog; 7, only with a view target, a depth clear and the target view (mask 0x20).

**Camera** slots: +0xc viewScreen (+0x90), +0x10 view (+0x50), +0x14 screen (+0x10), +0x18 setView, +0x1c setScreen. Screen matrix: `sceVu0ViewScreenMatrix(512, 1, 0.93333, 2048, 2048, 1, 16777215, 5, 200000)`; the draw distance defaults to 200000.

**Lights** (0x1c): ambient +4, lights[3] +8, fog +0x14. Defaults: (.4, dir 0 .2 .8), (.3, dir .1 0 0), (.2, dir .1 0 0), ambient .1, fog 500..2000 at 255..0. Fog is A + B/z, 255 at near and 0 at far.

**Depth clear**: 64-wide sprites at Z 0 with TEST_1 0x32001 (ZB_ONLY); the port keeps only the rect.

**Shadows**: depth is view-space y, below 0 becomes 100000, sorted ascending, at most 7. From model data bones[2].matrix; pitch clamped to 2.61799..2.96706. In stage 0x1b the shadow is the boss's own (F_E27_BOSS, bn_l_calf, size 400, 400, 1200, 1).

**The model (2026-11-04, second run).** `StageModelData`, `RenaModel` and `ModelInstance` were one class: what `ModelFile::createInstance` 0x2e5210 makes (0x100 bytes, vtable 0x31a7f0, set up by 0x2e4800) and `StageModel` +0 holds. gamehooks.h now has one `ModelInstance` plus the file's records in EE layout: `ModelFile` (+0x18 users, +0x1a nodes, +0x1c materials, +0x1e textures, +0x34 nodes, +0x38 material table, +0x3c texture names), `ModelFileNode` 0x30 (+0 matrix, +4 name, +0x10 file, +0x14 meshes, +0x24 bones, +0x26 surfaces, +0x28 texture slots, +0x2a meshes), `ModelMesh` 0x18 (+0 name, +8 parts: count, parts 0x30 each with materialCount +0x20 and 8-byte material refs +0x28, and a material +8 whose name tags the mesh for the VCM), `ModelMaterial` (+0 kind, +1 flags, +4 name, +8 texture animation, +0xc texture index). An instance's block holds one 0x50 `ModelSurface` per material of the node (+0x18 material; +0x20..+0x2c the two frames and blend, which WaveComposit materials set to their number, 0, 1), `+0x40` maps file material index to surface and `+0x44` lists the animated ones.

**drawSysAddModel** 0x2d5140 (StageModel::init): `ext_ghost` (no case) sets 0x100 and +0x34 / +0x38 = 191 / -52; `oXX27_11` sets 0x200; `oXX27_05` clears the shadow flag and sets 0x200; a name tag `_c` sets 0x200 (`parseNameTags` 0x2d54a0 reads the tags from the end); `far_`, `far_1..3` (no case) pick groups 2, 4, 8, 0x10, anything else 1; surfaces named `WaveComposit<N>` get N; textured materials whose texture is one of nine `tCM99_PLT*` plants (sorted table 0x2fd9c0) get material flag 0x20; last the model gets 0x400. `drawSysFindKind1Surface` 0x2d46a0 walks meshes, parts and materials for the first surface of kind 1; StageModel keeps it at +0xa8.

**The boss shadow texture** 0x2d5e10 (stage 0x1b, group 0x40, only while `drawSysBuildBossShadow` found F_E27_BOSS): a 256 square CT16 frame cleared white (gsFillRect 0x80, 0x60, 0x180, 0x160), the boss drawn with +8 = 0x87 and the 0x40 bytes at +0x80 zeroed (read as: drawn black; the readers of those fields are unread). View rows (1,0,0,0), (0,0,1,0), (0,-1,0,0), (-x, z + 800, -y, 1) from the shadow's origin; screen `sceVu0ViewScreenMatrix(256, 1, 1, 2048, 2048, 1, 0, 0, 1)` with row 1 = old row 2 and row 2 = -old row 1. Blur chain: down 256>128 (0x2d77d0), blur 128 (0x2d7540), down 128>64, blur 64, up 64>128 (0x2d72c0), blur 128, up 128>256. Each blit is three page-wide sprite passes at -1, 0, +1 texels, alpha 1, 1/2, 1/3 (0x30fa40 / 50 / 60), which is their mean; the source wraps (CLAMP_1 0), which is why the blur's v coordinates start at the size. 0x2d7030 and a T8H > T8 local copy make the result readable as 8 bits at sReservedBlock; the CLUT (0x2d6dc0, block +0x100) has alpha clamp((256 - i) / 4, 0, 128) in its top byte, so index 0 is 64 of 128. rena gets the TEX0 from template gp-0x7b70 through 0x2e8290 (bss 0x31af28). gp-0x7958 and gp-0x7954 are debug flags nothing writes (a sceVif1PkCall, a DISPFB2 view of the target). The host keeps the sizes, order, taps and ramp and hands the rest to `yamipan/rendering/yamipan_drawshadow.h`.

`sceVu0MulMatrix` and `sceVu0ViewScreenMatrix` were checked against the ROM (0x10b750, 0x10bf30); the signs of rotateMatrixX / Z come from the COP2 words.

## The model holder (game/stagemodel.cpp, 2026-11-04)

0x187230..0x188d60. `StageModel` (EE layout in the field comments of gamehooks.h) ties a rena instance to the scene tree. +0 the instance, +4 VcmTable, +8 pose kind, +0xc the pose, +0x10 `StageMatrixNode`s (one per pose bone), +0x14 `boneMap`, +0x24 / +0x28 `StageBoneNode`s, +0x2c five motion lists (0x10 each), +0x80 / +0x90 what the last advance turned and moved, +0xa8 the kind 1 surface, +0xb0 the frame's `MotionEvent` bits, +0x140 seven zeroed 0x40 records, +0x184 the base motion's file, +0x189 `m_mayGoDistant`.

**Both old stand-ins were one thing.** `BoneNode` (root at +0x14) and `ModelLink` (+0x18, next at +0x10) are one `BoneMap`: +0x14 root, +0x18 first (the insertion chain). The maps' code after 0x188d60 is that template twice, `MotionMap` and `BoneMap`, both **`LiteralMap`** (the node ctors 0x188fe0 / 0x189310 keep the key pointer, the node dtors 0x189000 / 0x189330 free nothing: the keys are the pose's bone names, the motion file's bytes): dtor 0x188d60 / 0x188ea0 (0x188d60 only as a pointer, to the array's ctor and dtor), insert 0x188df0 / 0x188f30, its recursion 0x1890d0 / 0x189380, find 0x189050 / 0x177270, **`balance`** 0x189700 / 0x1895c0, `height` 0x1898c0 / 0x189ad0. 0x189840 is the texture map's find (`StringMap<Tim2Texture *>`, which does copy its keys). All in stagelist.h, tags at the end of game/stagemodel.cpp, proven by literalmapcmp.py.

**Bones.** A `ModelInstance` (+0x14) has one `ModelBone` {record, matrix pointer} per bone; `ModelNodeBone +8` is the parent index. `setBaseMotion` points every bone the pose owns at the pose bone's +0x80 (`ModelPose::getBoneMatrix` 0x14f8c0, model bone to pose bone through +0x160) and files a `StageMatrixNode` under the bone's name. `bindBones` makes a `StageBoneNode` for each other bone: with no parent, local = instance +0x28 * bind (+0x24[i]), world = the bone's old matrix; else the node goes under the parent's node (or the pose's node of the parent's name) with local = inverse bind of the parent (+0x20 + parent * 0x40) * instance +0x28 * bind. Either way `bone.matrix` becomes the node's world. `placeBones(owner)` then hangs the parentless nodes under the owner, local = owner's inverse * world. `init` sets `instance +0x10`'s matrix (a ModelBone, called `rootBone`) to the owner's.

**Bone and motion files (game/modelpose.h has them as structs, read in place).** A bone file: +0 kind, +1 s8 bone count (over 0x40 refused), +2 flags, +0x10 name, +0x20 records of 0x40: name, parent name, position xyz, +0x2c the length to the next bone (four bytes on a sw bone: `swingParams`), +0x30 a quaternion. A motion file: +1 s8 track count, +2 flags (0x1000: playing starts at params[3]), +4 frames, +0x10 name, +0x20 four params, +0x30 tracks of 0x20: bone name, +0x10 offset of the keys from the record, +0x14 what a key holds (1 position 3 words, 2 quaternion 4, 4 euler 3, 8 event name 4, 0x20 one value, 0x40 none), +0x15 a start frame. A key is those words and last its length in frames, **negative on the track's last key**.

**A bone's slots** (`position` +0x100, `rotation` +0x180, seven of each): 0 the key behind, 1 ahead, 2 played, 3 final, 4 bind, 5 what the solvers read, 6 what a solver asks for. Each frame: `advance` fills 2, `solve(5)` builds matrices for the solvers, the last pass takes 2 (or blends to 5 and 6 by +0x174, +0x170) into 3 and `buildMatrix` makes +0x80 from it; +0xc0 is always slot 2, the pose with no solvers. A parentless bone gets its matrix from `solve` alone: world * the fixed turn `boneBasis` (rows 0, 2, -1, 3), which is why a bone's space has y and z swapped against the world's. With pose flag 0x40 the root carries the object: `stepRoot` adds the root's movement to the pose's position and turns it.

**The pose's solvers and swing (game/modelpose.cpp, all read 2026-11-04, none yet seen in a run).** Slots of a bone: From, To, Current (played), Final (what the model gets), Bind, Solve (what solvers bend), Target. `solveWeight` blends Current to Solve into Final; `settle` / `settleFixed` move it by the rate over a chain of bones. Every solver target eases half way a frame. `solveLook` turns spine0 by 40% and spine1 by 60% of a limit, to the target or, given a bone (+0x1a0), so that the bone's x axis passes through it (`aimThrough`: the plane of that axis and the target). `solveHead` aims neck then head, flat first, then what is left, by three limits, 54 degrees from the ctor (`aimLimited`; the ROM multiplies flat * pitch, so past small angles it misses the target: 0.65 cosine in a 100-degree test, on purpose kept). `solveTool` is the left arm to the tool target, the elbow put out by the shoulder's axes. `solveFeet` drops the pelvis by half the gap and calls `solveLeg` per foot: two-bone IK from the lengths at +0x1f0 (`kneePlacement`), twist 40% thigh and 60% calf, foot flat on the ground normal under toe height 10, else level. A foot target is (ground normal, height). `solveChestTool` (+0x3b0, was read as neck limits; the five characters kyo, tka, yrt, jei, hrs) re-parents the left tool bone to spine1 at a fixed place and only aims it. **Bones named ??sw?KA swing** (`stepSwing`): K is S hang down, F / R ahead along the bone, T stay and follow by spring, C between the calves (needs A = 0), lower case with wind; A is the one axis allowed (x y z, upper case negative angles only, lower positive only). The spring: velocity = velocity * (1 - p[1] / 10) + (target - pos) * (1 - p[0] / 10). Wind: +0x3a0 direction with strength in w, on by +0x395 (1 and (1, 1, 0, 0.7) from the ctor), a flutter using the bone's +0x2c as its phase. Still unknown: what writes `m_solvers` +0x20 and +0x1a0.

**`ModelPose`** (game/modelpose.h, game/modelpose.cpp): vtable 0x311230, slots dtor 0x150f50, load 0x14ff90, update 0x14f4b0. Kinds of `setBaseMotion`, named from the three-letter tag each ctor writes at +0x14: 0 mdl `ModelPose` 0x151020 (EE 0x3c0), 1 car `CarPose` 0x27ef70 (0x4e0), 2 elv `ElevatorPose` 0x27ffb0 (0x460), 3 nml `NmlPose` 0x2a7a50 (0x410), 4 sky `SkyPose` 0x2b37d0 (0x3e0). **+0x150 is a matrix pointer, not a count**: with it the pose hangs in that matrix and the object takes its place from the pose (`advance`: rotation from the matrix, position from the root bone's +0xb0). A bone record is 0x300: name +0x38, matrix +0x80. `ModelMotion` (0x14, ctor 0x2d0fa0): a motion file bound to one pose; `addMotion` keys it by the file's name at +0x10 unless told. Motion events are strings, eight queued at +0x1e0: HIT_.._BEGIN with _RH_ / _LH_ / _RL_ / _LL_ / _HD_ / _PV_, HIT_END, HIT_SWING, BEGIN, END, STEP_L, STEP_R, CLICK, CANCEL, CANCEL_W, SOUND.

**setBaseMotion's model names:** kyo, tka, yrt, jei, hrs set +0x3b0 and two vectors at +0x350 (kyo 23, 11, 10, 1; the others 23, 14, 11, 1; then 0, 0, 1, pi/2; read as a neck's limits, unproven); ryh_ky and ekm_ky keep bn_d_neck at +0x184; kkb_ models never go distant.

**Textures.** `init` fills an instance's textures by name: slot i's name is file textureNames[node +0x1c[i]], count node +0x28, stored at instance +0x50. A name ending `_s` first gets `modelTexturePremultiplyPalette` 0x2e1700: a second palette at texture +0x3c, rgb * a >> 7, alpha 0x80. The 3D host module must honour it (the user's texture rule: decode at the source, no palette replay).

## Collision (game/stagecollision.cpp, 2026-11-04)

Z is up. Two halves the ROM keeps apart, both in one file here (`game/stagecollision.h`; the names are the port's).

**The file** (keyword `collision` of a stage script, magic `SSS\x1a`): header 0x210 (+4 records, +8 count, +0x10 32 surface names of 0x10), then `CollisionRecord`s of 0x20 (name, `COL_` stripped by `relocate`; +0x10 face count, +0x14 faces, +0x18 prepared), then every record's `StageCollisionFace`s of 0x50: +0 plane, +0x10 three vertices, +0x40 flags, +0x44 attr, +0x46 group (`doorId`), +0x48 owner mesh, +0x4c surface, +0x4d material, +0x4e / +0x4f 2-bit indices of the vertices holding each extreme. Stage 1 as read: `Root` 8,165 faces, box (3406, 2042, -410)..(14051, 11664, 3300), grid 71 x 65 of 150; 24 of its faces are degenerate (zero normal); every face of the stage has flags. PC: the file's two pointer cells are `S2dRef`, and a face's +0x48 is a slot in a table of meshes (`StageCollisionFace::mesh()`), a pointer not fitting.

**A mesh** (`StageCollision`, EE 0x220, 0x234440): the planes on first use of a record, then a 2D grid of `CollisionNode`s; a mesh under 0x21 faces is one cell. Each cell keeps its faces highest first and splits by height (`split`, at least 150 tall and 20 faces, both halves smaller, else undone). Queries: `raycast`, `segmentHits` (the same cell walk, inline three times in each ROM function, one template here), `sphereHits`, `visitSphere`, `heightAt` (only a mesh that does not move). All take mask / flags: a face with a bit in flags is out, one with flags needs a bit in mask. `m_groupOn` turns a face group off (`setGroup`, a broken window); a face's group is `doorId`. Seen lists live in the scratchpad (0x70000000, 0x70002000), limit 0x200. A Root model has `m_matrix` null (world coordinates); any other map object follows its node.

**The world** (`SceneRecordBuckets`, `StageScene::records`): four arrays of `CollisionBody`. `add` pairs by category against the other's mask, on the side whose mask meets; `collide` (once a frame between movement and terminate) walks bodies with a mask, syncs and boxes on demand, tests by shape pairs (`collidePair`), tells both owners through `CollisionOwner` slot 0 (touching: normal, face, depth) and last slot 1 (`onCollide`, the nearest meeting of that body). Vtable: +8 slot 0, +0xc slot 1, +0x10 sync, +0x14 after contacts, +0x18 object, +0x1c parent. A map object's body is category 1 with mask 0: it never starts a test, others meet it. Quirks kept: a capsule's radius counts at its ends only; a segment against a sphere reads each body's other shape; a visitor's stop ends only its row of cells.

## The parameter tables (game/packtables.cpp, 2026-11-04)

`dataEU/packgameEU.pak` is read once at boot by `loadGamePackThread` -> `PackTables::parse` 0x2bcf10 (0x3df0 bytes, one function in the ROM) and freed again; only the tables below stay. Items, by id:

| item | form | into |
|---|---|---|
| 0..2 | CSV sheet, one per StageKind | `gStageObjRecords` 0x539b10, 3 x 0x140 x 0xfc: every character's stats, columns in gamehooks.h |
| 3 | CSV sheet | `gItemDefs` 0x574c10 (0xa0 x 0x20); rows of a weapon kind (< 7) also `gWeaponDefs` 0x576010 (0x50 x 0x54), in order |
| 4 | CSV sheet | `gDoorRecords` 0x537d80 (0x87 x 0x38), columns by find, not operator[] |
| 5 | CSV sheet | `gMotionDefs` 0x577b20 (0x100 x 0x210), second pass for the next motion |
| 6, 7 | lines of `label,` + 4 x (6 floats, flag) | `MotionDef::offsetA/B` sets 0, 1 |
| 8 | lines of `tag,` + 15 floats | `gMotionModels` 0x2f8000 (0x14 x 0x50), data with defaults |
| 9..0x30 | lines of `bone,a,b,c,d` | `gMotionBoneSets`, 4 groups of 10 lists of 0x20-byte entries (data 0x2f8720..0x2fa3e0) |

A sheet: Shift-JIS, CRLF lines; the first line with a first field holds the column names; each next one is a row under the number in that field; once a number repeats, later taken numbers move up. The file's last byte is overwritten by the terminator. A cell keeps its text and its atoi. A column name a sheet lacks reads column 0 (operator[] inserts 0). Item kinds: LW HW G3 G9 G7 R5 R7 B3 B9 B5 B7 II QI KI T OI = 0..15; an unknown one keeps the row before's (parse's unset s0). Side flags of a character: PC, NP, 屍, 闇.

**A model instance** (rena/rena_model.cpp, 2026-11-04; checked on all 1,768 RM2 files of the 114 SLDI packs). `ModelFile::createInstance` 0x2e5210 makes a `ModelInstance` of one node, by index or by name; everything but the object lives in one global-heap block (`ModelInstance::init` 0x2e4800; EE size boneCount * 0x8c + surfaces * 0x50 + 0x48 + meshes * 0x10 + slots * 4 + file materials * 4 + animated * 4). `rootMatrix` +0x28 is the **inverse of the node's matrix**, `boneBinds` +0x2c[i] = rootMatrix * the file's bone matrix (the bone in the node's space), `inverseBinds` +0x20[i] its inverse, `bindMatrices` +0x24[i] the file's matrix. The inverse is an inlined cofactor one (`invertAffineMatrix` in vu0math.h; rows 0..2 w 0, row 3 keeps the w), not the rigid `sceVu0InversMatrix`. `ModelBone` 0x2e9790 stores a record and the matrix pointer that is the record's first word, so the root bone's record is the **node**, cast. **`CharacterPart` was `ModelPartRec`**: `modelFindPart` 0x2e4610 hands them out (first mesh whose name starts with the text), one per mesh, the mesh tree threaded through `next` (the next sibling, or the parent's when last; `buildPartRecs` 0x2e4f50 from mesh 0 reaches every mesh of every node on the disc). `ModelSurface` 0x2e3e10: one per `ModelFileNode::surfaceMaterials` (+0x20, indices into the file's materials): the slots of up to three textures (material +0xc, +0xe, ext +2) in node->textureSlots, `level` 0..4 from material +0x14 (steps 0.15 / 0.4 / 0.6 / 0.8; 0 when material +0x10 is under 0.2). **Texture animation** (`ModelTexAnim`, material +8, textured materials only): kind 1 scroll (ints, `+= speed * ticks / 300`, wrapped to 0xfff), kind 2 wave (floats period, phase, amplitude: `waveValue` = amplitude * sin(phase), phase from -pi, each swing draws amplitude * 0.5..1 from randNext, material flag 8). Kind 0 is none, and only non-zero kinds join `texAnims`. **+0x24..+0x2c of a surface are one thing**: `setFrames` writes blendFix 0..0x80, frame0, frame1, and a WaveComposit material sets the same three to its number, 0, 1. **Lights**: `setAmbient` 0x2e44e0 / `setLight` 0x2e4500 (called by SceneRender::updateLights): `lightColors` +0x80 ([0] ambient, [1..3]), `lightDirs` +0xc0 one column per light, `drawMode` +8 has a bit per light and 0x80 for ambient, so the boss shadow's 0x87 with zero colours is "all lights black". On the disc: material kinds 74,966 textured and 14,848 kind 1; animations 152 scroll, 2,176 wave; levels 0, 3 and 4 only; 348 textured surfaces have an ext. A stage (New Game): 22 instances, map node `Root` 1,565 bones, 382 surfaces, 2,184 meshes.

The RM2 model file (rena/rena_model.cpp): pointers are absolute on the disc, against the base stored at +0x30; `relocate` moves them and writes the base, so a file relocates once. Layout: gamehooks.h. Below `ModelPrim` the fields at +4, +0x14.. of a batch are DMA tag addresses, so a mesh's packets are chains (5 tags, or 7 when `isLong`; decoded at the source by the 3D host module, TU 5).

**The batches of a model** (rena/rena_draw.cpp, 2026-11-04; surveyed on a whole SLDI pack of ROM.000, 11 RM2 files, by a scratch dumper). A `ModelPrim::batches` record is DMA REF tags, each with its VIF UNPACK in the tag's upper words and its data in the file: tag 0 one quadword, the header (`ModelPrim::Header`: count | 0x8000, count, flags, bones - 1) to VU address 0; tag 1 positions V3-32 at 5; tag 2 normals V3-32 at 0x2d; the last two colours V4-8 unsigned (RGBA, 0x80 = 1.0) at 0x7d and UVs V2-16 at 0x55 (4096 = 1.0, STMOD offset: VIF's row adds `ModelSurface::scrollU / V`). Header flags: bit 0 strip (no ADC, each batch a whole strip), else a triangle list; bit 1 weighted. With `paletteCount` (a bone count) two more tags sit before the colours: bone ids V1..4-8 at 0x55, weights V2..4-32 at 0x7d (one bone: no weights), and MSCNT runs the skinning before colours and UVs overwrite those slots. `listFlags` != 0: no end tag per batch, and the last two tags' addresses are lists of colour and UV pointers, indexed by `ModelPartRec::vcmFadeBits` and `ModelMaterial::uvSet`. A rigid mesh's matrix is the bone's `matrix` as it stands (`ModelMesh::bone`, negative the root).

**Skinning** (rena/rena_draw.cpp, 2026-11-04; EE 0x2e43e0, 0x2e68a0, 0x2e9480, VU1 0x07f..0x125). A mesh with `ModelMeshParts::boneCount` != 0 draws with the **unit matrix** (0x2e7610 on the state's +0x80): the palette takes vertices straight to the world, so its normals are world normals and the lights need no turning. **Palette** (0x2e9480, one per prim, to VU address 228): for each of the prim's `paletteCount` entries, `bone = mesh.boneMap[prim.paletteBones[i]]`, matrix = M then `inverseBinds[bone]` then `bones[bone].matrix` in vector order, M = `boneBinds[mesh.bone]`, or unit when negative. `ModelPrim` +0xc was misnamed `isLong`: it is the palette's size, and non-zero is what makes 7 tags. **Part box** (0x2e68a0): `ModelMeshPart::lists[0]` holds one box per mesh bone (centre, half size); each goes through that bone's matrix, the corners are joined into a world box, and that box meets the usual 0x2e2a60 test with the unit matrix. **Per vertex** (VU1 0x07f picks the loop from header +0xc, bones - 1): position = sum of weight * palette[id] * (x y z 1); normal = the same sum on the rotation, then normalised (ERLENG), but the one-bone loop 0x113 only turns it. Ids are **signed** V1..4-8 (USN 0), count **VU quadwords** (0, 4, 8: palette index * 4) and -1 fills unused slots, always with weight 0; weights are floats, V2..4-32, and a one-bone batch has no weights tag at all (tag 4 is zero). **`ModelFlag_Distant` (0x40, VU 15 y) forces the one-bone loop**: a far model takes each vertex's first bone only. **A stage in a New Game run:** 62 skinned meshes, 62 parts, all `paletted` (part +0x24), bones per vertex 1..4, `listFlags` 0 or 1. `ext_shc_01`, the boy of d0101, has 8 meshes (`head_00`, `side_hair10`, `top_hair10`, `face`, `upper01`, `arms10`, `lower01`, `foot10`), 53 bones. `oMK01_01`'s 54 skinned meshes are `iMK99_wave00_101..`, each hung on its own bone (`mesh.bone` 1..) and all sharing the same two bones 54 and 55: copies of one piece of **sea**, placed by M.

**The part box test** (rena/rena_camera.cpp, 2026-11-07; EE 0x2e2a60 `boxClipTest`, set up by 0x2e5bb0 / 0x2e5fc0). `rigidPartClip` 0x2e6b50 calls it with the part's box (centre with its own w, half sizes), the state's matrix (+0x220) and the state's **+0xc0 block** (`RenaClipView`): +0x00 the view-screen copied from 0x59b0c0, +0x40 / +0x44 pointers to the state's two clip matrices (+0x00 from 0x2e5fc0 at a margin of 0 pixels, +0x40 at 300), **+0x50 six frame planes, +0xb0 six guard-band planes**, +0x110 / +0x114 near and far (0x59b198 / 0x59b19c times 0x59b190). Plane 0 is the view-screen's w column minus near, plane 1 far minus w (the guard band's far 500 further), planes 2..5 the clip matrix transposed. The box goes through the matrix (centre, three half axes), is transposed with pextlw / pextuw / pcpyld / pcpyud so one VMULA chain gives, per plane, the centre's distance and the three axes' reach, and VCLIPw.xyz (distance, -, -, |a| + |b| + |c|) classifies it: behind any frame plane -> **2** (not drawn); inside all -> **0**; else the crossed planes (a stack int[8]) against the guard band's same ones: behind one -> 2, crossing one -> **1** (needs clipping), else 0. The two cfc2 of the unrolled pair read the first VCLIP's flags, then the second's, by pipeline timing. The host does not build the planes yet: rena_draw.cpp's `boxOutside` (the host projection, widened for every aspect) stands in.

The GS state of a pass (table 0x310180): opaque ALPHA_1 0x2a, TEST_1 0x5050b (alpha >= 0x50 or nothing is written, Z test GEQUAL); 0x2fdb60 is the shadow pass's state (the lamp paragraph below); blended ALPHA_1 0x44, TEST_1 0x50000. CLAMP_1 repeat, TEX1_1 0x61 linear. Blended materials (flag 1) wait in a table, sorted ascending on the screen z of the part's centre + index: far to near, the ROM's z falling with distance.
**What VU1 does to a colour** (`vudis.py 2ec908 3f8`, read 2026-11-04). Entries 0 (0x00a set-up), 6 (0x4e: frame matrices), 2 (0x198), 4 (0x194). The per-frame block 0x4e builds vf24..26 = the three light directions (VU 20..22, one per row: `renaSetLights` 0x2e8080 transposes them) times the mesh's rotation (VU 212..214), each column scaled by VU 221; vf27..30 = VU 16..19, ambient and the three colours. **Lit, 0x2a9**: `vf5 = vf24 * n.x + vf25 * n.y + vf26 * n.z` (so each lane is a light's direction . the turned normal, no negation), `vf6 = max(vf5, 0)`, `vf7 = vf27 + vf28 * vf6.x + vf29 * vf6.y + vf30 * vf6.z`, stored in the buffer at +0x55. **Then 0x2bb**: `colour = min(max(vertex RGBA (0x7d) * (vf7.xyz, 1), 0), 255)`: the vertex colour is **multiplied** by the light. **The file's vertex alpha is never loaded** (`lqi.xyz`): every vertex's alpha is VU 25 w, 0x80 but in a ModelMaterial_Kind1's two frames (0x2e6b70 sets it to the blend). **Unlit, 0x29e**: the mesh's `vuFlags` & 4 (VU 15 z): the vertex colours are copied, the same way. VU address 15 is x the pass's mask (`param_3` of 0x2e6590), y `ModelInstance::flags`, z `ModelMesh::vuFlags`; y & 0x200 (`ModelFlag_TagC`) runs 0x128: **back-face culling**, the cross product of each triangle against the eye sets the ADC bits of the faces turned away (the host does not cull at all); y & 0x100 (`ModelFlag_Ghost`) runs 0x011, an alpha per vertex from the angle to the eye; y & 0x80 with VU 176 the **point light** kernel 0x545 (it was called the spotlight here; the lamp paragraph below); VU 26 y is the material's flags byte (`ModelMaterial` +1), bit 8 runs 0x186: every vertex moves by VU 175 (the surface's first vector) turned by the mesh. The default lights of `drawSysInit` are ambient 0.1 and whatever the dir lights hold: `SceneRender` gives the rest, and its numbers come from scripts: "The lights of a stage" below.
**The scene's lamp in VU1 (rena/rena_draw.cpp, model_renderer.cpp, 2026-11-05).** `vudis.py` used to print a LOI's float as a lower instruction: `bal it=vi31 is=vi0 imm=0` was **LOI 255** everywhere (now `loi 255`). Read with that fixed. **Point light** (kernel 0x545, 0x2e05f0 uploads VU 176..179: (1, 0, weight, -weight / radius), position with radius in w, colour, shade), only with `gDrawSysState.render` (the main pass; otherwise VU 176 is zeroed) and `ModelInstance::flags` & 0x80: for each vertex `strength = max(0, weight + slope * distance)`, then the **vertex colour itself** gets `+= 255 * strength * (shade + colour * max(0, n . l))` (material flag 0x10, kernel 0x59a: `+= 255 * strength * colour`, no facing), **before** 0x2bb multiplies the vertex colour by ambient + directional lights. So a lamp is seen through the room's light sum. **Spot** (entered at 0x2eb after the main kick, lit path only): needs `flags` & 1, no material flag 0x20 (foliage), VU 200.x != 0 (0x2e7820: 2 when `ModelInstance::spot` is active); 0x66b only tests the part's box against the cone. VU 201..204 world -> light (0x2e00f0: the record's inverse rigid matrix with y and z swapped, so x, up, ahead), 205..209 the cone's planes, 210 (a, b / radius, c / radius^2, _) from the record's +0x90 (0.8, 3, 4), 211.w 1 / tangent, 204.w 1 / +0x80 (that field, `intensity`, widens x: it is an aspect), 171.x reach * 1.2 (0x2d5710, inline in drawSysRender; the record is `StageScene::render` + 0x80). Per vertex: `ahead` = the light's z; behind: no reach; else S, T, Q = (0.5 * ahead + x * scale, 0.5 * ahead + y * scale, ahead) times the first pass's Q, alpha = 0 at or past 171.x (the VU's second flag test reads the `subx` thanks to a stall at the `div`), else min(255, 255 / (a + b * ahead + c * ahead^2)), colour the first pass's lit colour, same XYZF. **GS state at VU 0x30 (packet 0x2fdb00, VU 0x36 TEX0 0x2fdae0):** TEST_1 0x5050b (As >= 0x50 or nothing), ALPHA_1 0x58 (Cs * Ad + Cd, Ad what the first pass left: its texture alpha), CLAMP_1 0 repeat, FOGCOL 0, FRAME with FBMSK 0xff000000, TEX0 RGBA MODULATE. **The "second GS pass" of the opaque table, 0x2e7c30 / 0x2fdb60 (VU 0x98, 0x9e), is the shadow's, not an alpha fringe:** its TEX0 is gp-0x86b8, CLAMP 5, ALPHA 0x44 (blended pass 0x6a, then 0x52 subtracting). So plants have hard edges in the ROM too unless the shadow pass says otherwise; the old note is withdrawn. **Back-face culling** (0x128 list, 0x14f strip): `e1 = a - b`, `e2 = c - b`, the face is dropped (ADC) when `(e1 x e2) . (c - eye) <= 0`, odd strip faces with the sign turned; the eye is VU 4, in the mesh's space. **Depth (2026-11-06):** 0x2e7e90 sends the same ZBUF for the opaque table and the blended one, and the blended table has no ZBUF of its own (0x310180, record 1: ALPHA_1 0x44, TEST_1 0x50000), so what blends writes depth, every pixel of it, alpha test off; only the ghost's pass 0x2e7960 sets ZMSK, and the boss shadow's target around its draw. **The cone test (2026-11-06):** VU 205 is the far plane, (-ahead, reach * 1.2 + ahead . lamp), 1200 for reach when reach is 0; 206..209 the sides through the lamp, normals inwards, of half width tan(angle / 2) * +0x80 and half height tan(angle / 2). 0x66b takes the batch's box (1(vi10), 2(vi10): the positions as they stand) by its centre and radius along each normal, and a box wholly outside skips the batch's spot pass. The picture repeats (CLAMP_1 0), so this is what keeps its copies off a batch out of the cone. A batch that crosses the cone still shows them.

**What a lamp shows (2026-11-06).** The ground's light is the point light `<name>_pl2`, per vertex. The spot's pictures, `T_TORCH1_sl.tm2` and the init pack's `spotlight`, are 64 x 64, 4-bit, one row of 16 colours, every one RGB 0 with alpha 0..0x54: under ALPHA_1 0x58 and MODULATE (the packet at 0x2fdae0, read again) the spot's pass adds nothing on the disc's data, and the host agrees. Most torches have no picture of their own: T_TORCH3_sl and T_TORCH4_sl take `spotlight`. `<name>_sl2` keeps priority -1, so the scene never picks it. The bulb's look is an effect, spawned by `StageLight::attachEffect` for a spot named TORCH: `dento1`, texture 3 of the effects, 64 x 64, white with a radial alpha up to 0x80, ALPHA_1 0x49 (Cd * As + Cd), colours 0x80 with alpha 0x10..0x20, size 12.9 + 2 growing 1.444 a frame, life 4, one a frame, flags 0x2401; and `dento2`, the same picture, ALPHA_1 0x48, colours 0x24 0x21 0x1e, size 20, life 15, flags 0x3601. The torch model's parts `bn_nl_0`, `bn_nl_1` are its body, a few units across (`ItemLamp::setDark`).

**A shadow in VU1 (rena/rena_draw.cpp, model_renderer.cpp, 2026-11-05).** Kernel 0x393, after the main kick and material flag 4 (0x4df), before the spot. It runs only with `ModelInstance::flags` & 0x10 (`ModelFlag_TakesShadow`, the old `kMapModelFlag10`: every map model has it), VU 150.x != 0 (`renaSetShadowCount`: `sShadowCount` in the main pass, 0 in the far and target passes) and material flags & 3 == 0 (`ModelMaterialFlag_Blended`, `ModelMaterialFlag_NoShadow` 2). The unlit path 0x213 never reaches it. The other branch, 0x3ad / 0x40e with state 0x9e (ALPHA 0x6a then 0x52), is **dead**: 0x39f overwrites the 1 of 0x39e with 0. For each shadow (VU 64 + 8 * i: projection, then clip at + 4, `drawSysUploadShadows` 0x2d7a60): 0x618 skips the batch when all 8 corners of its bounds lie beyond one side of the clip box (the host tests the vertices instead); then the batch again at the first pass's XYZF, so a culled face stays culled. Per vertex P = projection * mesh matrix * (x y z 1): S, T = P.xy * the first pass's Q, so plain perspective UVs; colour (0, 0, 0, a), a = trunc(max(P.w, 0)) when P.z < 0, else 0. The projection's z and w are -128 z / size[2] and that + 128, so a is 0x80 at the shadow's origin and 0 at the box's far end, and nothing above the origin (the matrix's local z points down, pitch 2.62..2.97). **GS state 0x2fdb60**: NREG 4, packed: TEST_1 0x50000 (no alpha test, Z GEQUAL), ALPHA_1 0x44 (opaque, (Cs - Cd) * As + Cd; 0x6a in the blended table, where no vertex arrives), CLAMP_1 5 (clamp, both axes), TEX0 gp-0x86b8 (0x31af28, filled by `renaSetShadowTexture`, shadow_00). The FRAME pair after it is past the tag's EOP and is never sent, so FBMSK stays 0 and **the frame's alpha takes the shadow's**, which the spot then reads as Ad (0x58): on a shadowed batch the spot is scaled by the shadow's alpha, also where it is 0. The host does the same. **Unread:** the GIF tags VU 180..184 (no writer found by searching for the address), so whether a shadow is fogged is not known; the host keeps it black. **Casters:** characters only, `applyCharacterModelFlags` 0x17dcb0 (not `CharacterKind_NoShadow`) and 0x17afdc; a first-person player is out of `DrawSysGroup_Main`, so it casts nothing.

**The natives that choose the view** (game/scriptrunner.cpp, batch 115). 0x70 `nativeSetPlayer` 0x1c81a0: `findCharacter(name, 0)`, then `StageScene::setPlayer`; a name with no character clears the player. 0x64 `nativeScriptView` 0x1c8dc0: the literal name "NULL" (data 0x304ba8; the script passes the string, not a null) removes view priority 3; any other name is looked up with `find0` and, unless it is already the top view, pushed at priority 3 with flag 1, above the main camera 0 and below the demo camera 4. In an active event, which a sub-scene never is, the point light `scr_demo_pl` (a second literal, data 0x304bd0; built by `SceneRender::setup` on a stage) hangs from that view and turns on, and on "NULL" is detached and turned off. The light lookup has no null test in the ROM. 0x1b54a0 is `StageNodeBase::setParent` out of line. Both natives read ScriptRunner +0xa4 and drop it.

**The main camera** (game/stagecamera.cpp, vtable 0x311fa0, EE 0x3c0; between it and StageCamera a kind 8 class 0x311d30, ctor inline, which adds only m_linked; the camjack derives from that too). `CollisionOwner` at +0x340 (the byte first, vptr +0x344: a body of bucket 2, category 0x4000, mask 2, shape 1; all its hooks empty). It only moves with a link: no m_linked, no step. **Offsets**: `MotionDef::cameraFocus` / `cameraEye` (the last two columns' groups of items 6 and 7, `gMotionDefs` +0x110, +0x190) by [AB area 0/1][body 0..3]; body = StageObject +0x20c, from the model name's `_lh_ _th_ / _mh_ _ms_ / _sh_ _ss_ / _ch_`, heights 180 / 170 / 160 / 110. `cameraShift` (+0x108) with `GameOptions::_4c`: 15 / 30 to the side. The focus is a point in the character's space, the eye an offset turned by the character; the wanted eye is focus + that. **Over the shoulder** while the character's +0x290 is set: seven places (0x2f7b10), the first both rays reach, scrZ eases to 610; it looks at the character's +0x2a0. **Then**: rays from the anchor keep the wanted place visible (faces with 0x8000000 may lie between, `m_blockedFrames` waits at most 30 frames before giving up), two side probes of 6 units, characters' `GroundVolume` tops + 15, ease by |d|^2 / divisor capped at 25, divisor 150 (300 in an AB area) + 1000 * how much it moves towards what it looks at; the stick's turn eases into the rotation.
**The camera file's tail (0x177d00..0x179a80, batch 214).** After StageCamera's ctor come the camjack's deleting dtor 0x177f40 and getKind 0x178010 (0xb), the kind 8 class's empty onLinkChanged 0x178020 (slot +0x260, shared by 0x311d6c, the camjack and ViewCamera's 0x3185ec), JackCamera::onViewTop, StageMainCamera, the kind 8 class's dtor 0x179840 (Ghidra had it as StageMainCamera's) and getKind 0x1798e0 (8), StageMainCamera's dtor 0x1798f0, then the adjustor thunks 0x1799f0..0x179a70. The port names the kind 8 class `LinkedCamera` (Ghidra only; no host class). StageMainCamera's second-base slot 3 (ScenePairTable's fourth step) is a thunk into `cameraStep`, as ViewCamera's: gamehooks.h writes it. StageCharacter's file opens at 0x179a80 with its inlines; its slot +0x25c (the cameras' `cameraStep`, 0x179a90) restarts the brain: setEntry with the a1 none of the three callers of +0x25c sets, then setState(state, 1).
**A stage's camera** (game/stagecamera.cpp): the screen matrix is sceVu0ViewScreenMatrix(scrZ 450, ax 1, ay height / (width * 3 / 4) = 4/3, centre 2048, z 1..0xffffff, near 1, far 1600) * rotateX(pi/2): the object's axes are x right, y ahead, z up. So a 512 x 512 frame is 4:3 with round circles; the 0.9333 of drawSysInit is replaced at the first frame.

## Doors (game/stagedoor.cpp, 2026-11-05)

**Class:** `StageDoor` on `StageMapObject`, vtable 0x3112f0, EE 0x2c0, kind 2, from `createMapObject` by name: `_dp_%02d` type 2 (R, MODEL_dr_r), `_ds_%02d` type 1 (T, MODEL_dr_t). **Modes** +0x248: 0 closed, 1 opening, 2 open, 3 closing, 4 locked, 5 barred (the script only shows a notice). +0x250 pivot, +0x260 / +0x270 reach points (model 47.5, -35 / 39), reach 75; +0x280 / +0x284 the two yaws; +0x288 user; +0x29c hp, +0x2a0 key kind, +0x2a4 window. **Faces:** groups of the Root's mesh, first = 100 + (type - 1) * 0xb4 + (number - 1) * 4: 0, 1 shut, 2, 3 open; while it moves the door's own mesh collides (`m_collides`). **Motions** 0x43..0x4c: DoorLink plays dr_plo, dr_psc, dr_pso, dr_plc, dr_lcr, dr_lcl, dr_op / dr_cl on the door's pose (group 3 of the character's motions, flags 0x42); events Begin and End on the door's model move it (DR_%02d_OPEN1 / 2, CLOSE1 / 2), Click on the character's turns the key or lands the blow. **The script is the door's brain:** see open-items.md, Done.

## The stage's notices (game/pausemenustate.cpp, game/scenepool.cpp, 2026-11-05)

`PauseMenuState` (StageScene +0xc4) is not a pause menu: the old name stays with its callers. It queues 11 notices `{kind, serial, frames, delay, hold, text}` and keeps one `NoticeEntry` per kind (five, the fifth unused so far), each with two lines of 0x40 wide characters and `start`, `end`, `hold` in seconds of the scene's clock. Kind 0 is the purpose (shown, it goes to `StageMission::setPurposeText`), 1 a message, 2 a hint, 3 a message while an event runs; an event shows only kind 3, and otherwise kind 3 waits. `isActive` is an entry past its `hold` with an `end` still to come: it takes the input focus from the player, and the Use button (`PadAction_Use`) dismisses it. Forever is 0x7fffffff frames with `hold` 0; `pinned` needs `hold` forever too and stays until native 0x1aa. `clearList` clears only kind 3, not every entry as the old comment said.

`ScenePool` (+0xc0) is the four lines at the foot of the screen: slots 0 and 1 the voice's subtitle, skipped when the option is off; 2 and 3 the notices. One line goes in slot 3, with slot 2 the empty string. The origin is (0x800, 0x878), 120 pixels under the GS window's centre, printed centred; the printer goes back to origin 0x700 afterwards. The strip behind the text is an autotile: cells of 20 x 24 pixels from `InitItem_PoolImage`, picked by a mask of the four corners the lines above, here and below cover (table 0x2fb4b0), one row more above and below the slots, colour 0x80101010, sprite prim 0x15e. Widths round up to an even number of cells so that a run centres. The image is a 256 x 48 4-bit TIM2 (`b9042cdee36af9a5` in a `YAMIPAN_TEX_DUMP`): white, all the shape in its alpha, which peaks at half (0x40 of 0x80), so the strip is a veil over the picture, not a bar. Each row of cells is drawn on its own line and its corners reach half a cell into the lines beside it; an empty line between two full ones is bridged where both cover. The frame is 512 wide, so a 20-pixel cell is about 27 pixels of a 683-wide window.

## The view camera (game/viewcamera.cpp, 2026-11-05)

**Class:** `ViewCamera`, vtable 0x3185b0, EE 0x440, kind 10, on the kind 8 class that adds `m_linked` (+0x300); three objects, `camview`, `camview_zoom`, `camview_snipe`, off the view stack until pushed. **Vtable offsets:** the tables of an object are laid end to end, and code calls past the first one's end through the vptr at +0: from 0x3185b0, +0x24c `onViewTop`, +0x25c `cameraStep`, +0x260 `onLinkChanged` (0x178020, empty here). `rtti.py` and the brief's slot numbers count from the third table, so "slot 130" and "+0x24c" are the same entry.

**The tree:** character -> `m_mount` (+0x340, a `StageNode`, half a turn round z) -> the camera. `onViewTop` moves the mount under `m_linked`; the ctor hangs the camera from the mount once.

**A step:** rotation from the motion's `m_lampAngles`; position y from `(|m_leanScale[0]| - 1) * 5`, so the eye moves forward with a lean; dirty on the mount's tree, the pose solved when it hangs from something, `m_linked->updateMatrix()`, dirty off; then row 3 of the world matrix is overwritten with `getEyeMatrix` applied to the position. The dirty off is what keeps the row: the `updateMatrix` the scene calls after `cameraStep` rebuilds nothing. It steps twice a frame: `sideHook3` from `ScenePairTable::actionTerminate`, and the scene's camera loop.

**The eye:** `PlayerMotion::getEyeMatrix` 0x1e5f70 is `getLampMatrix` plus row 3 moved by `gMotionModels[m_modelFamily].a` with its lanes reversed, or (0, 11, 11) at 0x305040 with no family. For the family `_li_` the two agree. The field `a` was unread until now: it is this eye offset.

**Who asks for it:** a script sets the show flag ForceView (property 0x56 sub 7, byte 10), or the character on the pad is not the player (`forcedViewChange`); `CharacterInput::stepView` turns that into `wants.viewChange` every frame while `allows.viewSwitch` holds, and `pushView` ignores a camera already on the stack. Stage 1's Root sets the flag.

**What hides what:** `ModelPartRec::drawMode` 0 is drawn, 2 is hidden with all under it. `prepareDrawFlags` clears the two parts of every character each frame, then with a kind 10 or 11 view on top hides the followed character's `m_parts[0]` (the part "head"), or `m_parts[1]`, the whole tree, for `isMode1`. Any other view hides an owned object's target whole.

**Script properties:** `StageObject::setProperty` 0x189f40 is slot +0x1fc of 29 object vtables; `SubSceneModelObject`'s 0x268ae0 is the same list, and now calls it. The numbers are in game/objectproperty.h. A character has its own, unwritten (open-items.md, Done).

## The gun sight (game/menuprogset.cpp, 0x2b9ef0..0x2bb600, 2026-11-06)

`MenuProgSet` (EE 0x1c, scene +0xa8; its name is an old guess, it is the sight) holds the init pack's items 7..0xc as Tim2Textures and draws in PacketList_Scene2d, at the end of `StageScene::draw`, what `PlayerMotion::setScope` chose: state 0 nothing, 1 a rifle's (`drawScope` 0x2ba8d0), 2 the R_64C's (`drawR64C` 0x2b9ef0). `this` is in a0 and the zoom in f12: the decompiler prints them swapped.

| item | image | size | what |
|---|---|---|---|
| 7 | +0x04 | 128 x 128, 8-bit | the rifle's mask: alpha only, a soft dark edge, never quite opaque at the sides (200 of 255 at the middle of the edge) |
| 8 | +0x08 | 32 x 32 | the spot, white with an alpha disc; added at 0x20, drawn 32 x 36 |
| 9 | +0x0c | 128 x 128, 8-bit | the R_64C's mask: a hard hole |
| 0xa | +0x10 | 8 x 8 | a tick, 2 x 6 black |
| 0xb | +0x14 | 512 x 8 | the line across, drawn at (0, 0xfc) |
| 0xc | +0x18 | 8 x 448 | the line down, drawn at (0xfc, 0), stretched to 512 |

A mask is one corner of the picture, 256 x 256 on the frame, its right and bottom copies mirrored by UVs running backwards. The UVs count from the texture's power of two (PLZCW) so that -1 can be written: the sampler has to repeat. **The ROM sets neither CLAMP_1 nor TEX1_1 here**: it inherits them, rena's model passes leave repeat and linear. The art is squeezed 3:4 (hole 146 x 194 texels), so the hole is round on the 512 x 512 frame shown 4:3. R_64C order: the whole frame + grey at alpha 0x20 (`sightBrighten`, ALPHA_1 0x48), mask, eight ticks from x 235 leftwards on y 249 at a step of -120 / ((1 - zoom)^2 * 4 + 2) (`sightDrawTicks`), spot added, the two lines. Rifle: mask and spot only. Mask quads go out as strips 64 wide (`sightDrawMaskQuad`), first two vertices XYZ3 so that each starts on its own; TEST_1 0x33001 is ATST NEVER + AFAIL RGB_ONLY, colour only, past any depth.

**Two ROM slips.** The right and bottom quarters end at span - 1, one past the outer texel: the last two pixels of the frame wrap to the picture's clear inner edge, a line nobody sees at a screen's edge. In state 2 the first quarters run to last + 1, wrapping to the opaque outer texel for two pixels at the middle, which the cross covers.

**PC (any aspect).** Mask, ticks, spot and lines are drawn squeezed and the mask's outer column is drawn on out to the display's edges, so the hole stays round at 16:9 and 21:9; the fill is the whole display. The first slip would be a clear line in the middle of a wide display, so the host ends at the outer texel (`backU`, `backV`), which moves those UVs by 1 in 128. Ticks and lines are nearest: 1:1 on the GS, scaled up here bilinear mixes in their clear white neighbours, a pale edge on the black (hazards.md).

`quatBetween` 0x2bb510 sits at the end of this range (the turn taking one vector to another, used by the critters' walk): the file is its home only by address.

## The items' physics (game/itemphysics.cpp, 0x23c010..0x23de20, 2026-11-06)

A thrown item or a dropped weapon is 2 to 4 points held apart, integrated by Verlet; the model only follows them. `motionBoneSuffix(motion, "bn_col<n>_")` finds the nodes, the digits after the name are the radius in tenths, the bone's row 3 turned (x, -z, y) the rest point (+0x170). Links: 2 nodes one, 3 a triangle, 4 all six pairs (the ROM's order: 0-1, 2-3, 0-2, 1-3, 0-3, 2-1). Stage 2's eight weapons have 2 or 3 nodes.

**The frame.** Side hook 2 is `update`: position += velocity; 30 passes over the links, each moving both ends by 0.005 x the error x the vector between them, so they are soft; a node whose face of last frame has a floor flag (0x42400c) is pressed 1 unit along -normal, any other falls 1 unit; damping back to 0.995; the first new contact with dot(velocity, normal) > 0 and speed > 2 plays the landing sound (`StageThrownItem` only, kind 0x17; the 800 passed in f12 is never read). Then the collision world: `syncBody` shoots a ray from each node's last position to its new one and sends it back by distance + radius + a bounce (floor: speed squared / 50 upward; wall: speed / 3 outward; both capped at 15), damping 0.8; `collisionHook0` does the same for the spheres, or for all nodes at 0.9 when the touch sphere is pressed. Side hook 3 is `endFrame`: velocity = position - previous, x and y damped, **z never** (the ROM stores it back unchanged); then the owner's world matrix is the nodes' frame times `m_restFrame`, with row 3 putting bone 0 on node 0.

**Bodies** (bucket 2, `on` follows `m_moving`): category 0x400 mask 2, one sphere around the nodes, thrown items only (the character's side is `CharacterGround::collisionHook0`, which asks `getMeanVelocity`); 0x800 and 0x1000, mask 1, the node spheres against walls (rays 0xe002003, a weapon's 0xea021f3) and floors (0x42400c), both skipping 0x40000.

**A weapon is long and thin:** `m_isWeapon` (+0x204) tries a missed wall ray again level with `heightOverFloor` (500 down from the owner, hit + 50) and keeps its wall spheres at that height, so a blade cannot lie through a low wall.

**Rest.** `keepsMoving` sums the node speeds in an **int**: each step truncates (cvt.w.s), so nodes under 1 unit a frame add up to 0 and the item counts as still; after 1 s of that `setMoving(false, true)` writes the matrix back to the owner's rotation and position and `setDirty(true)` gives the transform back. `setMoving(true, ...)` calls `placeNodes` (rest points through the world matrix, pulled back along rays from the owner's point, no bounce) and `setDirty(false)`, because while it moves the matrix belongs to this class.

**Proved:** `itemcmp.py`, 20 random lives on the ROM's own code and the host's, 6,046 ops, 8,946 rays, 115 landings, 12 rests, every field and every stand-in's arguments as bits: 0 differences (`--plant` on 6 lives: 1,158). **Seen,** temporary harness, removed (YAMIPAN_TELEPORT=2, LW_HOCHO forced into the air 90 over the path, runs of 30 s): it fell from z 122.8 to 37.8 (the player stands at 32.8), stopped moving within 45 frames, and lies flat on the path, handle and blade, progress/20261106-item-knife-landed.png (cropped, brightened 4x). Dropped beside the player the game's own "Pick up" came up. **Not seen:** a thrown item (none on stage 2), its touch sphere, the landing sound, a real throw or a real dropped weapon: only the same calls forced.

**`m_parent` is ambiguous in these classes.** `StageObject` has one of its own: for the thrower, write `ItemPhysics::m_parent`.

## The area clip (game/areaclip.cpp, 0x2dac70..0x2e00c0, 2026-11-07, batches 189-190, `verify`)

What the camera's place hides, so the map's far parts are not drawn. **The file** (`areaclip` keyword, 20 on the disc, all inside SLDI packs, magic `ac1\0`, `AC1` once relocated, 23 to 119 boxes, 63 to 481 names): +4 box count, +8 name count, +0x10 and +0x20 the map's corners, +0x30 the boxes (0x40 each, a matrix taking the world to the cube -1..1), +0x34 the names, +0x38 per box the names it hides, +0x3c per box the boxes characters hide in (only 3 of the 20 files have any filled). Lists are shorts ending in -1.

**The grid:** 8 x 8 cells over the map, each listing the boxes that touch it seen from above (`boxTouchesCell`: a corner of the box in the cell, a corner of the cell in a face of the box, or an edge across an edge; z is ignored). It only makes the search short.

**The frame** (`AreaClip::update`, from `StageScene::draw` just before `drawSysRender`): the boxes in the camera's cell that hold the camera, then four sets of u_short: what the boxes left hid and no box still held hides, what the boxes entered hide, and each minus the other. **Entering hides, leaving shows:** left minus entered gets `setClipped(0)`, entered minus left `setClipped(1)`. Nothing is hidden until a box is entered, and `setup` hides nothing. Then, when the file has +0x3c: each active character's `clipped` is cleared, and set when it stands in a box the camera's boxes name.

**The targets** (`findTarget`, base `ClipTarget` vtable 0x31a700, the ROM's three slots): a `ModelInstance` (+4), a `ModelPartRec` (+0xe, which `drawTree` skips on, with all under it) or a `ClipEffect` (0x31a6e0: `setEmitting(clipped == 0)`). The order decides: the lead's model and meshes, models named `far`.., a character, any model, a particle effect. This is why the two bytes were called `distant` and `flagE` before: `drawSysBuildShadows` also sets the model's for a character past the draw distance, and until now nothing cleared it.

**Proved:** `areaclipcmp.py`, on all 20 files from the disc: the whole ctor on the smallest file (64 cells), 1,140 sampled (box, cell) pairs on the other 19, 4,000 random frames, 134,343 setClipped calls, 3,375 frames with the camera in a box, 931 hidden characters: 0 differences. `--plant` (three bugs): 409 differences on a shorter run. **Seen,** temporary probes, removed: stage 2 (89 boxes, 107 names, all found: 23 models, 84 meshes), the camera's box 15 hides 11 at the start, walking in from it box 18 hides 16 more and leaving 15 shows 3; the same frame with `setClipped` skipped shows the same world, so what is hidden is out of view; no crash on stages 2, 7, 9, 12, 20. **Not seen:** a character hidden, an effect stopped (stage 7's file has one), nothing to compare with from a real PS2. `findTarget` answers null for some names (stage 7: 57 of 481, `AN_B204kmn_03`, `oDC07_03`), the code allows that and the ROM may do the same, but no run of the ROM says so.

## Teleport (game/teleport.cpp, yamipan/ui/teleport_panel.cpp, 2026-11-06; not in the ROM)

**What an event is, from the disc (`tools/reverse/eventscan.py`).** 30 stages with 57 parts (a part is `StageSelect::variant`, a pack `pks<stage>c<part>EU.enc`; stages 0, 27 and 29 have one), 39 films (the navigator's single-part nodes, 30..68). 144 file events `demoEU/s<stage>/<name>.edp`, 13 event movies (`kStageMovies`: every number a `d` file skips, EV0102 fills d0102), 165 scripted events `sd_*`, which have no file: each is an entry of `scripts[0]`, and **its name is case-sensitive** (`findEntry`, `sd_MT04_08`; `StageSimpleEvent::begin` ends at once and silently without it). d1608 and 22_06 are on the disc and no script starts them. What starts an event says where it happens: `Root` the stage's start, `AREA*` a trigger volume, `F_*` a character, `o*` an object.

**The roads it takes are the game's own.** `teleportPending` cuts what runs: `runLayout` as a cancel, `runNaviLayout` as its message, `stageThread` as the pause menu's navigator jump (StageExit_Navi), and `menuFlow` reads the title as New Game. `menuPlayLoop` calls `teleportTake`, which fills the record as `applyNaviAnswer` does, and `StageMission::afterScripts` starts the event as native 0x1f2 / 0x155 would. New in the ROM's flow: leaving in the middle of an event, so `stageThread` deletes the sub-scene and takes the parent back first.

**From its trigger (the panel's default).** Standing in an area's box is not enough: `AREA02`'s d0103 needs `global[9] == 1` and the box, and what it does after the event is the point: `global[9] = 2`, `global[0x1c] = 3`, `placeAt` for the player and the partner, `fadeIn(15)`. So on the stage's first frame `teleportStageFrame` asks every active script object for the event (`ScriptRunner::findEventBlock`: a walk over the entry as loaded, so the part and difficulty cannot be wrong, with a native's inline words from the ROM's own table), puts the player on that object's floor (`findFloorFaceExcept`, CharacterGround's mask; no floor, the player stays), and `jumpTo`s the block: the code after the last condition around the call. A jump past the call is such a condition; one landing before it closed a block the call is not in. `Root` only when nothing else starts it, since it is the stage's whole story. 0x155 and 0x1f2 only work on a stage's first frame, so those events, unused ones and brain-script ones start directly, as `StageMission::afterScripts` would. What earlier steps of the stage would have set stays unset. Two natives walked a `findAll` list with no test, as the ROM does, and that read through null off the story path: `hideLoose` 0xa8, `forEachNamed`; both now test it.

## The elevator (game/stageelevator.cpp 0x2939b0..0x295470, 2026-11-06, batch 184, `verify`)

**`StageElevator` : `StageMapObject`** (vtables 0x318310, 0x318334 at +0x1c, 0x31834c at +0xf4, 0x31855c at +0x204; EE 0x300), kind 4, for the model oZS99_01 (`createMapObject`, data 0x3030f8); on the disc the scripts of stage 9 and its neighbours use it (`sssall.json`: packs s09 and s10). `StageMapObject(5)`: mesh 0 the car, 1..4 the doors, from the table 0x2fb5f0 with their bones 0x2fb610; odd meshes are left doors. The meshes are made in `setMotionList` 0x295010 before the base's, on the collision file kept at +0x240, then ride their bones (`setInverseTree`); `arg` is not read, 1 goes to the base. It starts with `setScene(ClosedB3)`; a script's 0x1cc then places it (stage 9: 0x13, shut at B1).

**Buttons** (`ElevatorButton`, also the bit `1 << button` of +0x2fc, which native 0x21d reads): 1, 2 the landings', 3, 4 the car's, 5 release. `press` 0x294280 asks the pose, plays ELV_BUTTON unless the pose answers 1, and keeps the lamp bit only for the landing the car is not on; arrival clears both bits of the landing reached. **Places:** ten vectors +0x250.. with w 1 until worked out, one pair by `ElevatorLanding`. Panels bn_b1_i, bn_b3_i, in the car bn_bx_ib1 for both directions; the place takes x, y of the panel and z of the door bone; `canCall` 0x294140: within 150, within pi / 4 of the way back from the panel, and cos >= 0.577 with the panel's facing. Forward at yaw t is (sin t, -cos t). `vecDirection(out, a, b)` is a - b, so `getSwitchPlace`'s direction points at the panel.

**`sideHook3` 0x294560** compares the pose's state with +0x2f4, last frame's. Opening or closing begins: ELV_DR_STRT on bn_bx_dl, and `setTreeScriptFlag` on ENV_SL61 at B1, ENV_SL62 at B3 (object group 5), on when opening. They end: ELV_DR_END. A ride begins: ELV_MOVE parented to bn_bx at a zero offset (data 0x309940), rumble ELEVATOR1 and ELEVATOR2 on the rider (the player, else the first active character whose `floorObject` is this). It ends: ELV_ARRIVE, ELEVATOR2 off, ELEVATOR1. The side flag `kElevatorAtB1` turns when a ride begins, so `isAtB1` excludes `RideToB1` and `isAtB3` `RideToB3`.

**A door on someone, `collisionHook0` 0x294cb0:** only while closing, a door mesh, a character's wall sphere (`kGroundWallBody` 0x20, which is also the car's category: the kind test tells them apart). A segment 35 long along the door bone's row 1 (+ left doors, - right), at the sphere's z, must touch the sphere: `reopenDoors`, ELV_DR_REOP, ELEVATOR3 on the character.

## The car and elevator poses (game/vehiclepose.cpp 0x27dc40..0x280010, 2026-11-06, batch 172, `verify`)

Two `ModelPose` subclasses, made by `StageModel::setBaseMotion` for kinds `ModelPose_Car` and `ModelPose_Elevator`. No source string names the file. The car object is game/stagecar.cpp (next section), the elevator object game/stageelevator.cpp (section above). `tools/reverse/vehiclecmp.py` is the proof so far.

**`CarPose` (vtable 0x317c50, EE 0x4e0).** The car object calls `setDrive` 0x27ed70 (targets: speed and brake limited to 10, steer to a quarter turn), then `drive` 0x27e650 with four ground contacts, or nulls, which clear everything; `getWheelPoints` 0x27ec80 is where it asks the ground: the corners (+-91.69, -0.55, +-57) through `bodyMatrix` +0x470. `drive`: speed, brake and steer ease to their targets at 0.02, 0.1, 0.1 a step, with the rates 1 and 1.2 known by heart (constants) and `powf` otherwise; a nearly stopped car stops by 0.8 a step; speed loses `brake / 10`; velocity is the second bone's forward by `speed * 0.7`; `tilt` +0x420 is 100 times the change in velocity, xy limited to 30; `drift` +0x410 is what `followGround` adds to the position. `update` 0x27dc40 is `ModelPose::update`'s shape with `solve(Solve, -1, 1)` and `followGround` 0x27de20 where the solvers go. `followGround` off the ground lets every bone go; on it: the root rides at the height of the four contacts, `bn_body` aims along the plane's normal less `tilt`, each `bn_??_accel` reaches for 28 over its contact, each `bn_??_wheel` rolls by its travel (175.9292 units a turn, the right side the other way), the front wheels' parents and `bn_str_w` turn by `steer`, then the whole car turns around the parent of the rear left accel by `steer * speed * 0.03`, a roll around z. **The ROM's slip:** the last loop calls `wrapAngleLane` 0x135ec0 on the rotation's lanes past pi and drops the answer, so they are never wrapped; the host keeps that. Nothing reads +0x440..+0x44c but the clear.

**`ElevatorPose` (vtable 0x317c70, EE 0x460).** 18 motion files of `oZS99_01` from the table 0x2fb630 (`ElevatorMotion`: op, cl, mt, so, sc for B1 and B3, and the lamps fb1, fb3, bb1, bb3 on and off). `state` +0x408 is an `ElevatorStep` in the low nibble and `kElevatorAtB1` 0x10: open 1, opening 2, closed 3, closing 4, riding to B3 5, to B1 6. `queue` +0x410 holds 16 steps and `queueStep` +0x450 the place in it; the buttons (`callInsideB1/B3`, `callFloorB1/B3`, `release`) write the steps from entry 1 and set the place to 0, and `stepQueue` 0x27f2b0 goes on when the bones' motion ends, so entry 0 is never played. The pair open, close waits for 120 frames of `timer` +0x40c. `reopenDoors` 0x27f5a0 is what the object calls when something stands in the closing doors (with ELV_DR_REOP): open, and the pair goes in at the place. `setScene` 0x27fdc0 takes the script's values 2, 4, 5, 6, 8, 10 (`ElevatorScene`). `stepQueue` tests for the two rides after mapping them to closed, so that branch is dead in the ROM.

## The number lock (game/numlock.cpp 0x2a6ab0..0x2a7b10, 2026-11-07, batch 197, `verify`)

**`NumLock` : `StageMapObject`** (vtables 0x318870, 0x318894 at +0x1c, 0x3188ac at +0xf4, 0x318abc at +0x204; EE 0x270 in a heap block of 0x280), kind 5, for the models oMK21_06 and oSJ08_07 (`createMapObject`, data 0x303108 / 0x303118). Ctor 0x2a7340 (`StageMapObject(1)`, `setDirtyTree(1)`), deleting dtor 0x2a6ab0; sideHook0 / 1 / 3 empty (0x2a6b60, 0x2a71c0, 0x2a6b50); `setMotionList` 0x2a71d0 calls the base's with arg 1 (placement to bn_top), then copies the code from `StageResult` +0x90 (`numLockCode`, four chars, cleared by `StageResult::begin`), or draws four digits 0..7 not all zero and writes them back: **the code stays the same across retries of the stage**. Fields: code +0x240, a stack of one (+0x244 m_active, +0x245 four dials, depth +0x24c: `pushDials` 0x2a6cd0 / `popDials` 0x2a6c40, natives 0x201 / 0x202), a light for the view, direction (0, pi/2, 0) +0x250 and colour (1, 1, 1) +0x260, handed to `ModelInstance::setLight` by the view 0x2a8b60. Dial moves: `scramble` 0x2a6b70 (native 0x22c), `spinDial` 0x2a6e40 (never the code's digit nor the one shown), `setDial` 0x2a6ee0 (to the code's), `dialDown` / `dialUp` 0x2a6f00 / 0x2a6f60 (the view's NLOCK_REV), `dialsRight` 0x2a6fc0, `dialsTurning` 0x2a7070 (an angle 0.01 or more from its number's); `setInView` 0x2a6e30 is empty. Before this file they were free functions `numLock*` (batch 090's names), now members.

**`NmlPose` : `ModelPose`** (vtable 0x318b10, tag "nml", EE 0x410), `setBaseMotion`'s kind 3 (`baseMotionKind` of object kind 5). load 0x2a7890: the indices of bn_kanagu +0x3c0, bn_key +0x3c4, bn_k04..bn_k01 +0x3c8 (dial 0 is bn_k04), then the dials snap; it answers 1, not the bone count. update 0x2a7420: ModelPose::update's shape (advance, solve slot 5, Final from Current, no walk, no solvers) with `easeDials` 0x2a75d0 after the solve: each angle +0x3e0 moves 1/5 of the way a frame (1/1 after `setDials` 0x2a7840 set the snap flag +0x400) to -number * 2pi / 8, the short way, and the dial bone's Current rotation is the quaternion of that angle about x (data 0x2fb720). The numbers are floats at +0x3f0 (`getDial` 0x2a7800 truncates, `setDial` 0x2a7820). The float order is the ROM's: ((-n * pi) * 2) / 8, then angle + (1/frames) * delta through ACC (adda / madd).

**The number-lock view is written (game/gadgetview.cpp, below):** `createMapObject` builds it into StageScene +0x90 with the first lock. `findNumLockTarget` 0x29f440 (game/scriptrunner.cpp, 2026-11-07): the first object of group 1 (`StageObjectList` +0x98) the number lock's handle has a pair for (slot +0x18, `hasObject`); null with no handle. Still not written: the seven-key pad's view (0x2b2c80) and `SceneOwnedObject::isSevenKeyOpen` 0x2b2c10 (a stub: the object of that name is kind 6 and 0x2b2620 says its keys stand at the code).

**After a lock's screen (`missionRestoreCheck` 0x29f2a0, game/scriptrunner.cpp, 2026-11-07).** `nativeUseLockGadget` (0xaa) calls `saveObjectFlags(kind, object)` when the gadget's screen comes up: `StageMission` +0xb8 is that **`LockGadgetKind`** (2 the number lock, 3 the seven keys; it was read as a pointer `restoreArg`), +0xbc the object, and a non-zero kind clears `restoreAsked` +0xc0. `StageMission::onRestore` 0x2868c0 asks `missionRestoreCheck` once: kind 3, the pad closed (owned[2] not busy): `isSevenKeyOpen(+0xbc's name)`; kind 2, the lock closed: for `findNumLockTarget`'s object, bit 0 `isLockOpen`, bit 1 `wasTried` (**`LockRestoreBits`**: an int, not the bool Ghidra shows); 0 otherwise, also for kind 1, which the ROM's switch names and answers nothing for. `SceneOwnedSlot` (SigLamp 0, NumLock 1, SevenKey 2) moved from playermotion.cpp to stagescene.h.

## The seven keys (game/sevenkeys.cpp 0x2b22f0..0x2b3860, 2026-11-07, batch 210, `verify`)

NumLock's twin, two dials of four positions instead of four of eight. **`SevenKeys` : `StageMapObject`** (vtables 0x318f60, 0x318f84 at +0x1c, 0x318f9c at +0xf4, 0x3191ac at +0x204; EE 0x2d0 in a heap block of 0x300), object kind 6 (`StageObject_SevenKeys`, the motion Seven_Keys), for the models oYC11_21..oYC11_27 (`createMapObject`: strncmp "oYC11_2" (data 0x303128) and a digit 1..7); the first one builds the handle `SevenKeysGadget` (0x2b2c80, vtable 0x314ce0) into StageScene +0x94. +0x240 the code (two chars, `setCode` 0x2b23e0 takes 0..3 only, `setMotionList` 0x2b2920 clears it), +0x250 mesh 0's matrix (the world matrix turned pi/2 about z, position kept) and +0x290 its inverse. `keyUp` 0x2b2560 / `keyDown` 0x2b24a0 turn a dial round the four; **turning dial 1 turns dial 0 with it** (the puzzle). `keysOpen` 0x2b2620: no dial turning and both at the code (`isSevenKeyOpen`, the mission's restore check). Native 0x205 `setSevenKeyNumbers(name, a, b, c, d)`: dials (a, b; negative keeps), code (c, d). Property 0x6b reads a dial (`sevenKeysPoseValue` 0x2b23b0, sign-extended byte).

**`SkyPose` : `ModelPose`** (vtable 0x319260, tag "sky", EE 0x3e0), `setBaseMotion`'s kind 4: +0x3c0 bone of bn_k02 (dial 0), +0x3c4 bn_k01 (dial 1), +0x3c8 angles, +0x3d0 positions (floats), +0x3d8 snap. Angles -pos * 2pi / 4 about x, eased a fifth a frame as NmlPose's.

**The view `SevenKeysView`** (vtable 0x319200, `GadgetView(7)`, EE 0x30): +0x28 the dial (0..1), +0x2c the effect "mon" (spawned at (-1.2, 2.5, 3), parented to bn_top in view, x = 10 - 20 * dial, y 13, z 10). frame: while no dial turns, Squat or Use leaves (no "tried" flag, unlike the lock); Lleft keyUp, Lright keyDown (sound EVE_S11_01), Lup / Ldown pick the dial. enterView / leaveView as the lock's (own light (0, pi/2, 0) white x3 and ambient, rain off / on, drawMode 0), with no open check on leaving.

## The gadget views (game/gadgetview.cpp 0x2a7b10..0x2a9c70, 2026-11-07, batch 205, `verify`)

**A handle and a body.** StageScene `owned[3]` (+0x8c SigLamp, +0x90 the number lock, +0x94 the seven-key pad) holds 8-byte handles: `SceneOwnedObject` (vtable 0x318ca0; the gadgets' own 0x314c40, 0x314c90, 0x314ce0), vptr and the body at +4. Slots 1..15 of every handle are the same two-instruction jumps 0x2a9bc0 (slot 1) .. 0x2a9a00 (slot 15) to the body's slot at the same offset; slot 0 0x2a9be0 deletes the body through its slot 0. Both halves derive from a one-slot root (vtable 0x318b90, deleting dtor 0x2a7c90). Handle-only methods: `getRecordFlag31` / `setRecordFlag31` 0x2a8920 / 0x2a8910 (body +0x31, SpNumLockLink and native 0x220), `wasTried` 0x2a8930 (+0x30), `isLockOpen` 0x2a8940 (the pair's object of that name is a NumLock with `dialsRight`); the last two are called from `missionRestoreCheck` 0x29f2a0. Slot +0x18 is `hasObject` (vtable 0x318ca0: +8 slot 0, +0xc prepare, +0x10 addPair, +0x14 removePair, +0x18 hasObject).

**The body `GadgetView`** (vtable 0x318c40, ctor 0x2a9940 (capacity), dtor 0x2a98c0, EE 0x28): pairs a script names (`addPair` 0x2a9700: natives 0x1ee / 0x1ec / 0x204; `find1` the object, `find0` the camera; refused when listed, full or a look runs), +4 objects[], +8 cameras[], +0xc count, +0x10 capacity (SigLamp 2, number lock 1), +0x14 state, +0x18 in view, +0x19 user came, +0x1a cancel asked, +0x1c user, +0x20 / +0x24 the picked pair. Slots: 1 prepare, 2 addPair, 3 removePair 0x2a9610, 4 hasObject 0x2a9570 (what MotionGadgetSpecial asks before requesting 0xfc / 0xfd / 0xfe; the host's old name was `start`), 5 canUse 0x2a94c0 (idle, a pair whose object is active (+0x34), no fade running), 6 takeMotionText 0x2a93d0 (GadgetLink::onRequest: picks the pair, state 1), 7 isActive, 8 takeMotionUser 0x2a93a0 (GadgetLink::onStart), 9 cancel 0x2a9380 (was `isSlot2c`; native 0x1e6), 10 update 0x2a9050, 11 draw, 12 getUser 0x2a7b80 (was `getTarget`: prepareDrawFlags hides the user's body while in view), 13 isBusy 0x2a7b90 (state 2 or in view), 14 isInView 0x2a7bd0 (was `hasTarget`), 15 isLooking 0x2a7be0 (state 3; was `isSlot44`, native 0x1eb); body only: 16 frame, 17 enterView, 18 leaveView. **The look** (`update`): 1 Selected, waits for the user, then the scene's fade to black (0.5 s); 2 at full black (`isFullInMode2`) fades back in, state 3 and `enterView`; 3 `frame` each frame until cancel, then to black, 4; 4 at full black fades in and `leaveView`, back to 0 when the fade stops. A cancel in 1 drops the pick, in 2 goes to 4. `pushCamera` 0x2a9300: `pushView(camera, 4, 1)`, the object's model loses `ModelFlag_CastsShadow`, its children hidden and inactive; `popCamera` 0x2a9290 the reverse.

**SigLamp** (handle ctor 0x2a7bf0, body vtable 0x318b30, EE 0x60, dtor 0x2a7b10): `prepare` 0x2a8650 makes `SigLamp_sl` (SpotLight, white, radius^2 12e6, cone 0, priority 3, mode 2, not limited by the map) and `SigLamp_pl` (PointLight, (200, 196, 180) / 255, radius^2 174243, no shadow), both at (5, 0, 0), added to the scene and detached. `enterView` 0x2a8180 saves the camera's rotation and scrZ (sets 540), the point light's y as the beam's length, hangs both lights under the camera, hides the lamp's model (`shown` 0) and its children. `frame` 0x2a8380: ends (cancel) when the user's or the partner's (+0x7c) endurance is gone, the stage is over or cleared, or Squat is pressed; Lamp held lights both (SIGLAMP at the camera when it changes); the left stick turns the camera from where it stood, 50 degrees about x by y, 60 about z by -x; a ray (arg 1, mask 5, flags 0x2040000) from the point light at y 0 to y = length in the camera's frame puts the point light where it hits. `draw` 0x2a7ce0, while in view, on list 9: the texture "siglamp" as four sprites, each a quarter of the 512 x 512 window, mirrored (TEX1 0x60, TEST 0x33001: RGB only, CLAMP 5, ALPHA 0x44).

**The number-lock view** (handle ctor 0x2a89b0, body vtable 0x318be0, EE 0x34, dtor 0x2a88b0): +0x28 the dial (0..3), +0x2c the effect `numlock` (spawned by `prepare` 0x2a8f80 at (-1.2, 2.5, 3), not emitting), +0x30 tried, +0x31 the record flag. `enterView` 0x2a8b60: the effect emits on `bn_key`, rain stops, the lock's light (+0x250 / +0x260) into the model's three lights and ambient. `frame` 0x2a8d00: while no dial turns, Squat leaves and Use leaves as a try; pad left `dialUp`, right `dialDown` (NLOCK_REV), up / down pick the dial, the effect at x -1.2 - 0.8 * dial. `leaveView` 0x2a8a30: a lock set right plays NLOCK_UL and is hidden and made inactive; effect off and detached, rain back, the model's light mask cleared. `draw` 0x2a8a20 is empty.

## The car (game/stagecar.cpp 0x289cc0..0x28fa90, 2026-11-06, batch 174, `partial`)

**`StageCar` : `StageMapObject`** (vtables 0x318070, 0x318094 at +0x1c, 0x3180ac at +0xf4, 0x3182bc at +0x204; EE about 0x410). `createMapObject` builds it for oGK99_01 and oDC99_01; its ctor writes **`StageScene` +0x100**, which is why the old stand-in `StageViewTarget` is gone: that field is the stage's one car, now `StageScene::car`. `PlayerGear::setLightOrder` was a method of `CarParts` too (a0 is `m_gear`'s +0x2b0), and `GateParts` was `CarParts`: the sub-scene's "gate" is its copy of the car. Stage 7 (pks07c1, c2) has the car; the disc's other scripts with car natives are in pks26 and `AREA_AC01`'s pack (work/sssall.json).

**The frame:** side hook 1 0x28d290 turns the pedals into `CarPose::setDrive`; side hook 2 0x28cda0 lets a warp start over (`startOver` 0x28d850), drives the pose on the four contacts, advances 16 times after a warp, asks `getWheelPoints`, runs the five timers and clears the frame's hits; side hook 3 0x28c930 applies damage and shows it, sets the lights' shadow owner, runs the sounds. Side hook 0 is empty, so the map object's own 0x1756b0 does not run on a car. The world's contacts come between: `collisionHook0` 0x28b470, `syncBody` 0x28c440 lays the 40 spheres on `bn_col`, `collisionHook3` 0x28c310 takes the frame step. **The sphere body (category 0x20, bucket 1, mask 1) is on only with a driver**; without one side hook 2 calls `collisionHook3` itself, so a car with no driver hits nothing.

**The spheres** (radius 40, 20, 35, `CarSphere`): 0..3 the corners, 4 and 5 the seats (on `bn_dmg_sl`, `bn_dmg_sr`), 6..12 and 13..19 a row along each side, 20..24, 25..29, 30..34, 35..39 rows across. A corner stops it: head on (the drift within 0.9 of the normal) it sets `m_hitFront` or `m_hitBack`, which give a drag of 0 next frame, otherwise `m_hitSide`, 0.9. CARCLASH1..4 and CAR_CLSH1..3 by km/h (under 4, under 12, above). A seat sphere dents slot 0 or 1. The rest push it out by the depth, in the plane only, and `scrape` 0x28af70 turns it along the wall by a 64th of the angle when the next spheres of that end are in too. Bullets and blades break windows by the face's group (`doorId` - 1). **The mesh body takes characters:** a `carHit` with slide level 2 or more, unless the sheet's kind is 3, takes slot 5 then 4 and sets the drag by kind: 5, 6, 0xa, 0xc 0.1, 7 0.9, the rest 0.6.

**Damage:** `m_health[10]`, 100 each. Slots 0..5 set `CarParts::damaged` and show `bn_dmg_sl`, `bn_dmg_sr`, `bn_dmg_f?_A` for `_B`, `bn_bld_01`; slots 6..8 are the windows (part + 6): gone, `StageCollision::setGroup(part + 1, false)`. The windscreen shows part 2 at health 1 and part 0 at none. A window has two switches: `mapSwitches` outside, `BKW%02di` inside, and `isInView` picks one: from a driver's seat in view, the inside's. `bn_bld_02` has no flag that sets it, and **the ROM's slip:** slot 9 writes one byte past the 0x40 of `CarParts`; the host skips it.

**The units:** the drift is units a frame and km/h is `length * 0.036 / dt`. The ROM's other slip: the pedal log +0x3f8 is shifted up the wrong way, so all ten end up as last frame's, and the accel note is "the pedal rose by more than 102 since last frame". Engine: CAR_EG_ST under 4 km/h for 0.3 s, else CAR_EG_DV, pitch 1 + speed / 20 up to 2. The sounds' f12 (800, and +0x398 for the horn) is not read by `addSourceNamed`.

**What a run-over costs:** `CarRunOver` at +0x240, five ints 0, 0, 20, 80, 1000 from the ctor, `at` 0x28e850 clamps to the last. Its only caller 0x2103d0 (not written) is a character's hit: for a driver's car it passes the victim's `CharacterGround::slideLevel`, and level 4 takes the victim's endurance times 4 instead. **Getting in, `StageCharacter::rideCar` 0x17af50 (off, side):** nobody with `driving` 0 rides. A driver (`isDriver` 0x215310, `driving` 1, in a car or not) first calls `resetPlace`. In (off 0): `m_gear` the scene's car, a mode 1 rider is also `m_driver`; the pose hangs in bone bn_dr or bn_pa by `parentMatrix`; motion car_dv 0xba for a driver of sheet kind 2, 4, 8, 0xa, else car_s 0xb9; no shadow; children hidden and off. Out: off 3 the seat, off 2 the seat turned round, else a driver the same and a passenger the door place. The player also pushes camview at priority 1 going in and removes it going out. Callers: native 0x18d, and the three links 0x283060, 0x283600, 0x2837d0 (off 2 and 3 are 0x283060's: a character dragged out, its partner too). `GettingOnCar` 0x280f50, `GettingOffCar` 0x280e20 and `DragOutCar` 0x280a60 are not written, so no button gets anyone in yet.

**Who gets in, and how (game/playermotion.cpp, batch 176, 2026-11-06).** Three specials and six links, all in `PlayerMotion`. `initList` makes `GettingOnCar` 0x280f50 and `GettingOffCar` 0x280e20 for `driving` 1, `DragOutCar` 0x280a60 (derived from GettingOffCar: same side byte +0x18, adds the victim +0x1c, and its `clearAllowed` 0x281630 clears the victim too) for `flag3be`. The ctor's links, after Revive: `driving` 1 ApproachCar, ApproachOut; `driving` 1 or 2 Drive; always SpWait (plain), NPCReaction (cr_2, cr_3, cr_1); `flag3be` ApproachBite. Motions: 0xbc ApproachCar, 0xbd ApproachOut, 0xbe ApproachBite; 0xb9, 0xba Drive. **`StageCharacter::findCarDoor` 0x280fd0** (car, facing): the door whose `getDoorBone` lies within the radius that call answers, both raised 50, the car within 50 in height, clear by a ray (mask 0xe8021f3, flags 0x40000); facing: the character's look within 45 degrees of the door. Answers 0, 1 or -1; the car's point functions take `door == 0` as side. **On:** `onUpdate` allowed when no fade runs, `findCarDoor(car, true)` hits. **Off:** the car `isCreeping`, nobody holding the driver or a partner aboard; the first side whose sill (and door place, when a partner is aboard) is clear by ray and sphere. **Drag out:** a foe with `flag3be`, not of kind Srt, its bite candidate a living character in a car `isCrawling`, free, biteable, and the one beside it free. Off and drag re-run `onUpdate` in `onApply`, then `resetPlace`. **The door links** run behind a fade: ApproachCar and ApproachOut share start (pad off, 0.5 s to black) and leave (back, pad on): CAR_DROP at the first run, at full black `rideCar` 0 or 1 (the partner too: for getting in only if `getCarSpecialFlag` and `isPassenger` 0x215300), then CAR_IN, CAR_DRCL. ApproachBite does not touch the pad: CAR_PLOUT, at full black the prey by `rideCar` 2, the other occupant by 3, placed at the first clear of five `getDoorPoint`s, the foe at `getDoorPlace`, both turned from the prey; then `startBite`. Its `onCancel` frees the prey unless a bitten motion is already requested. **Drive** `onRun`: kinds 2, 4, 8, 0xa keep their pose; a driver asks 0xb9 settled with the wheel at rest, else 0xba.

**How the prompt gets there (F_PC29, read 2026-11-06).** The player's script (entry F_PC29 of AREA_AC01_3.bin, words at +0x7e4..+0x84c; which stage file holds it was not checked): not driving and `canGetOnCar` (0x167): `offerUse(100, label)` (0x79), so the item list shows "Get in" as kind `UiUse_Script` + 101; the next frame `getMenuMode() == 5` (Use was pressed: `pendingCount` above 0) calls 0x168, which is the one-frame `setSpecial`. Getting out goes by 0x16b and 0x169 the same way, by the look of the words at +0x914..+0x9e0, not traced.

**The door and the drive, seen on the real path (2026-11-06, New Game forced to stage 7, open-items.md has the pad).** Item kind 108 (script use) is up while native 0x167 answers 1, and the list has the focus one frame after the slot appears, since `getInputFocus` caches per frame. X in that window: `UiItemList::pick` -> `pickedItem` -> native 0x168 -> `GettingOnCar` -> motion 0xbc -> 0xb9 at the seat, about 40 frames later `m_gear` is set. `Control_CarDrive` (flag 0xb) stays 0 for about 200 frames after sitting, then the pedals work; it goes 0 again during the veranda's film and hint. Throttle 255 gave 23 units a frame on the road down to (6464, 3180, 78).

## Character voices (game/charactersound.cpp, 2026-11-06)

A cue's name comes from who the character is. `getSoundKind` 0x2a6980 turns the sheet's ctype into the row of the name table 0x30a058: 1 PC (ctype 0), 2 NPC (1), 3 enemy, `E%02d` from the sheet's SEtype (2, 4, 5, 6, 8, 0xa, 0xc), 4 UMI (0xd), 5 SRT (3), 6 YRT (7), 7 RYO (never answered), 8 TKS (0xf), 9 BOSS1 (0xb). ctypes 9, 0xe answer 0, none. The ROM's PC and NPC names are one string: data 0x2fb6c0.. holds NPC_..., and the PC's name is that pointer + 1. The table runs DW1, DW2, THROW, BM_UPL1, BM_UPL2, BM_UPM, BM_UPH, BM_LFUP, BM_DWL, BM_DWM, BM_DWH, JUMP, BITE, UNTIE, TRP_I, TRP_O, SURP, PANIC, END, and two more groups after a null at 0x2fb70c. 

**Two tables, two calls.** `playVoice` 0x2a6440 takes stream cues from the StDT (`findCue`; the S-prefixed names, SPC_DIE, SE44_DIE); `playVoiceHeard` 0x2a65c0 takes effects from the stage's SEDT by binary search and plays them with `playNamedHeard2` at bn_head. A null cue is silence in both. The SE table is per stage: stage 2's has 28 of the 69 names charactersound.cpp can ask for. `playShout` alone goes straight to `playNamed` with no priority, and adds a noise at its loudness: `SceneNoise_Any` for the player, `SceneNoise_Ally` for the rest. An attack voice: PC and NPC by the long weapon's item kind, none NPC_AT_NW1 or NW2 (by `hasAlteredVoice`), light melee AT3 / AT4, firearm AT5, anything else AT1 / AT2; enemy, UMI and RYO use the damage name, `second` picking L. `playTrapHitVoice`'s two switches on the sound kind leave only kinds 1, 2 and 3 reachable.

## What a character sounds like each frame (game/charactersound.cpp, 2026-11-06, batch 181)

`StageCharacter::updateSounds` 0x2a4a40 runs in the character's update. In order: **the voice lets go**: state 2 (a cue) when `isCuePlaying` says no, state 1 (a source) when `findSource` loses it; then state 0 and priority 0x7fffffff, the only place that happens outside a stop. A resumable `E%02d_BITE` of an enemy also stops once the motion is neither 0x9e nor 0x9f. **The breath**, only with no voice playing: `getBreathName` 0x2a2280 by `m_winded` (+0x3f4). A living character seen through by the party's view (`isViewedThrough` 0x212300: `getPlayerAltFlag`, the companion, a character, is this one) has `PC_BRH_J`, `NPC_BRH_L_J` by level, `E%02d_BRH_N_J` / `C` / `B` by `getBrainPace` (1, 6 N; 2..4 C; 5 B), `TKS_BRH_J`, `BOSS1_BT_J`. Not seen through: `TKS_BRH_L`, and a winded PC or NPC `NPC_BRH_L` or `_H`. It plays as a source at bn_head (+0x43c), kept while the name stays, stopped when it changes. **How it moves**, by sheet kind, in +0x440 and +0x444: 0xb the boss (1 BOSS1_STAY on `isHoldableMotion`; `isMovingMotion` 2 BOSS1_MVL or 3 BOSS1_MVH on 0x1a, 0x19, 0x16, 10, 9, 0x2d, 0x24; 4 BOSS1_STR on 0xe4; anything else keeps it); 0xd the floor's `surface` while moving, sound column 29 of the table 0x2fc3b0 (`getSurfaceMoveSound` 0x2cede0, the ninth family of 0x30f830, with one sound); 3 and 7 SRT_MOVE / YRT_MOVE while moving. `playMotionSound` 0x2a2070 borrows the same slot with the motion id + 5, which is why the states stop at 4: a value above 4 means "stop this when the motion changes". **Its own words:** state 0, alive, `m_voiceOn` (+0x44c, 1 from `initSounds`, a script's property), `m_voiceDelay` (+0x448) at 0: an enemy that is neither player nor partner says `SE%02d_WND` (pace 1, 6), `_CAU` (2..4), `_ATK` (5); the next is 20..30 s away for 1, 6, 10..20 s for 2..5, 30 s else, at 25 frames a second. `initSounds` 0x2a6910 starts the delay at 0..290 frames. `stopSounds` 0x2a6830 is `stopBodySounds` 0x2a6380 (the voice), `stopCuesOf`, the move sound, the breath.

**One-shots.** `playFallDownSound` 0x2a29f0: FALLDW1 by default, 2 on surface 1, 3 on 5..9, 0x10..0x14, 0x19..0x1c, 4 on 0x1d, 0x1f, nothing past 0x1f. `playBumpSound` 0x2a2b10: with a long weapon, column 30 (WALLHIT..). `playJobSound` 0x2a2b60: `JOB%02d`, + 5 unless `hasMaleModel` 0x215320 (the pose's name[4] is m, a reading). `hasAlteredVoice` 0x215af0: F_E.._TKA on stages 0x1c, 0x18, the sheet's kind 0 on 0x11, 0xe, kind 1 on 5.

## Motions (game/playermotion.cpp, 2026-11-04)

**`PlayerMotion` (character +0x224, EE 0xc20) is the character's motion state, not the player's:** every character has one. It is the state machine between "something asks for a motion" and `StageModel::playMotion`. A request is a `gMotionDefs` index: `request(id)` 0x1ec7a0 (180 callers) asks unless a request stands, `requestNow(id)` 0x1ec650 (72) takes over and stops a walk. `m_request` (+0x3c) is cleared at the end of every `stepLower`, so a request lasts one frame and **whoever wants a motion asks each frame**; asking for the running motion does nothing unless its 同一リクエスト column or `m_forceStart` (+0x66) is set. `motionId` (+0x40) is what plays.

**The terminate hook (`StageCharacter::sideHook3` 0x179d30, read 2026-11-05, batch 136).** The last of the frame's four phases, in this order: `CharacterGround::endFrame`; `PlayerMotion::endFrame` 0x1e06f0; `m_smoke[0]` runs while `m_shadowLight` is a light that is not YAM_, `m_smoke[1]` outside `nodeTree->contains` of the position + 50 in z; `m_splash` stands where a ray from `bn_head` to the position meets mask 0x10000000 (kind 0xb: `bn_pelvis` to `bn_l_toe1`, and `btsplash` as it enters); on a floor of surface 0x1f the F_HAMON ripples stand at the feet at the character's height and each step event throws F_SPLASH; the lamp: `setValue2b0` -1 for a down stranger, 2 for the player, 0 for the rest, `setCastsShadow` for the player only; `updateBlock98For`; `updateSounds` 0x2a4a40 (stub); the end of the timed motion rate (+0x458..+0x464; a draining stage ends it on leaving motion 0xd6 and `releaseAction`s if it ended inside; +0x464 doubles as that byte latch); last, whatever it carries that has no lights of its own (`m_lit` 0) takes the character's. `PlayerMotion::endFrame`: `takeHits` 0x1e18c0 (0x25c0 bytes: only its first line, the wall flag's copy, is written), **the writer of `m_runTime` / `m_runDistance`** (run motions with no request; distance is the farthest from where the run began; winded it runs backwards at twice the rate, from 10 s at most), a Yri or Kne's `m_stanceFlag` (on 0.5 s after a non-YAM_ shadow light, off 0.5 s after it goes, in the listed rest motions), then `MotionEvent_HitSwing` and `MotionEvent_Sound`: the sheet's S0..S3 in order, `m_soundEvent` counts them, `shotKind` picks the voice function (table in `PlayerMotion::stepSoundEvents`; kind 0xd falls into 0xe in the ROM). `releaseAction` 0x1dedb0 is the motion's, not the character's: the idle, or `activeLink->finished` when the motion has a next one.

**The frame:** the start hook 0x17a860 calls `beginFrame` 0x1e5d90; the control hook 0x17a810 runs input, brain and 0x1e3e80 (which does looks, feet, wind and the fidget timers; the walking requests come from elsewhere); `stageUpdateCharacterMotions` 0x17df60 runs `stepLower` 0x1e9dd0 (the body), `stepUpper` 0x1e8a30 (hand poses, group 1: l_torch, l_gun, l_rifle, l_fist, l_def; r_weapon, r_gun, r_rifle, r_fist, r_def) and `stepFace` 0x1e8870 (group 2, the table +0x70 from the second list; +0x46c the talking face); the movement hook 0x17a490 calls 0x1e0e20 and `StageModel::advance`. Groups of `addMotion` / `findMotion`: 0 body, 1 hands, 2 faces. `stepLower(true)` is reset's: flag 0x10, from the first key.

**`stepLower`:** the running motion's 遷移モーション is requested when it ends; `resolveHold` turns the request into a hold (+0x54, +0x58) of the same crouch side; `arbitrate` 0x1e6d60 puts sq_u (0x27) or sq_d (0x28) in front of a request that changes stance (ability +0x3a6), storing the request and the character's +0x360 block in `MotionQueue` +0x5c, and handles u_sp (0x32), where both characters wait at 0xfb; a pending walk starts (0xf9 / 0xfa while it runs, the request waits in `MotionQueue` +0x60); then the link's hooks, then the label: `lowerLabel` is the rifle column `m_weaponClass` when the weapon is a rifle, unless it starts with `*`, which makes it the upper label (`upperLabel`, played with arg 0x200). A motion loops unless 再生終了待ち is set. Last the ground gets 接地判定, 壁判定, キャラ反発LV.

**Links and conditions (read 2026-11-04, batch 121): this is who asks for the motions.** A frame has three stages. (1) The start hook 0x17a860 clears `CharacterAllows` (+0x360, 0x44 bytes), and `beginFrame` runs Always' `onUpdate`, then the running link's `update`, which runs each of its active conditions' `onUpdate`: each writes a byte saying what the running motion allows (+0x360 squat, +0x361 turn, +0x362 move, +0x363 quick turn, +0x390 head, +0x391 aim, +0x39b view, +0x39c zoom, +0x39d lamp). The start hook then clears `CharacterWants` (+0x2c0, 0xa0 bytes). (2) The control hook 0x17a810: input 0x236b90 and brain 0x1dd2f0 fill the wants (`moveKind` +0x2c0, `moveYaw` +0x2c4, squat +0x2c8, quick turn +0x2c9, head +0x300 / +0x310, aim +0x320 / +0x330, view +0x348, zoom +0x34c / +0x350, lamp +0x354); `controlStep` 0x1e3e80 runs Always' `onApply`, then `MotionLink::apply` 0x27cac0: the link's slot +0x10, then each active condition's `onApply` while no request stands, then the link's slot +0x1c. (3) `stepLower` plays it. **Move is last in Walk's list and asks for `pickStance(0)` when nothing asked, so the idle needs nobody else.** `CharacterMoveKind`: 1 w_l, 2 t_l, 3 w_r, 4 t_r, 5 and 6 w_b, 7 and 9 w, 8 run. Move also does stairs (0x17..0x1a on a face of flag 8), `ops` at an edge and `wa` into a wall after a long run (player of kinds 0, 1 only); `w_run` 0x16 sits between the two paces when ability +0x3aa is set. `moveYaw` goes into `m_turnStep`[2] (+0xb78), and `applyMovement` 0x1e0e20 adds `m_moveStep` (+0xb60) and `m_turnStep` (+0xb70) to the character, so the walking distance itself is the motion's root, through `StageModel::advance`.

**The objects.** Links (the ROM's list `MotionConditionList` at +0xc: head, count, nodes of 0xc with the value in a block of its own; `nodeAt` 0x27d940 answers the head for an index out of range; +8 a block holding the motion, no reader found; vptr +0x1c; slots +0xc update, +0x10 apply, +0x14 request, +0x18 start, +0x1c run, +0x20 leave(next), +0x24 cancel; byte +0x14 finished), in the map +0x18: **Walk 0x27c520** (vtable 0x317bf0, also +0xc00), Jack 0x27c370, Pickup 0x27c140 (+0xc08), Up 0x317b60, Fall 0x317b30, Jump 0x317b00, Ladder 0x27b730, Door 0x27b640 (+0xc0c), Battle 0x27b000, BtAttack 0x27aea0, Gun 0x27aa90, GunAttack 0x27a910, Reload 0x3179b0, DiveFall 0x27a0b0, DiveLand 0x317950, TrapSet 0x317920 (+0xc14), TrapCancel 0x3178f0 (+0xc18), TrapHit 0x3178c0, Throw 0x317890 (+0xc1c), Bite 0x278db0, Bitten 0x315eb0, Damage 0x317830, DamageLand 0x317800, DamageFatal 0x3177d0, **DamageDead 0x2775d0** (vtable 0x3177a0, +0xc10), Revive 0x317770, then ApproachCar, ApproachOut, Drive, SpWait, NPCReaction, ApproachBite and SigLamp, NumLock, SevenKey; a number ending 0x317xxx is the vtable set on a base link 0x27cc50. **Correction:** the first reading had Walk at 0x2775d0 and called +0xc00 a "look state"; `PlayerLookState` is gone: its `active` and `angle` are WalkLink's `edgeFound` +0x90 and `edgeDrop` +0x60, the floor's depth behind the wall ahead, which is why the main camera rises below -170.

**The car links' hierarchy (2026-11-09, batch 238).** ApproachCar (vtable 0x317fe0: dtor 0x2835a0, onStart 0x283910 pad off and fade in, onRun 0x2837d0, onLeave 0x283780 fade out and pad on) is the base of ApproachOut (0x317fb0: its own onRequest 0x283700 and onRun 0x283600, ApproachCar's onStart / onLeave) and ApproachBite (0x317f80: overrides all five, onRequest 0x283500 .. onCancel 0x2834c0); both deleting dtors (0x283d80, 0x283d10) store 0x317fe0 before `~MotionLink`. Drive 0x317f50 sits on MotionLink directly. The port had a host base `CarDoorLink` for onStart / onLeave and ApproachBite on MotionLink; it now has the ROM's tree. MotionLink's defaults: 0x1ec640 request (slot +0x14), 0x1ec3f0 cancel (+0x24), 0x1eee10 / 0x1eee20 slots +0xc / +0x10.

Conditions (vptr +0x10; slots +0xc onUpdate, +0x10 onApply; byte +0xc active; base 0x276510, vtable 0x317750), in the map +8, in the ctor's order, vtable then onUpdate / onApply: Always 0x317730 0x275b60 / 0x275160, Squat 0x317710 0x2750f0 / 0x274990, BtOn 0x3176f0 0x274810 / 0x274650, GunOn 0x3176d0, Turn 0x3176b0 0x273fa0 / 0x273f10, Move 0x317690 0x273da0 / 0x272d50, Level 0x317670, LevelLift 0x317650, Ladder 0x317630, Jump 0x317610, Door 0x3175f0, PutItem 0x3175d0, PickupItem 0x3175b0, TrapSet 0x317590, TrapCancel 0x317570, Throw 0x317550, Showt 0x317530, Reload 0x317510, QuickTurn 0x3174f0 0x2702a0 / 0x270260, Jack 0x3174d0, Bite 0x3174b0, LadderMove 0x317490, Lock 0x317470 0x26f8e0 / 0x26ef30, BtAttack 0x317450 0x26e9e0 / 0x26e500, BtAtkCombo 0x317430 0x26e4e0 / 0x26e280, BtMove 0x317410 0x26e260 / 0x26df60, GunAttack 0x3173f0, Snipe 0x3173d0, GunMove 0x3173b0. **Walk's list:** QuickTurn, Squat, Lock + BtAttack or BtOn (ability +0x3c2, +0x3c3), GunOn, Level, LevelLift, Ladder, Jump, Door, PutItem, PickupItem, TrapSet, TrapCancel, Throw, Showt, Reload, Bite, Jack, then the special links of data 0x2fb3a0 (30 names: cr_2, GettingOnCar, sp_*..; kinds 0xb and 0xd add 0x2fb420 / 0x2fb440), Turn, Move. The list +0x2c is what 0x1df0b0 fills: named actions a script switches on, classes 0x317d30 (`0x2814a0`: motion by name through table 0x577b20, or a given id), 0x317d10 (`0x2812e0`, the partner), 0x317cd0 / 0x317cb0, 0x317cf0, 0x317c90 and `0x280410` (cars, Zs9901); slot +0x18 0x1df030 clears +0x15 enabled, +0x14 0x1df0a0 clears +0x14 allowed, +0x1c 0x1da4e0 sets enabled. Unwritten.

**The fight (read 2026-11-05, batch 128).** The sheet's link Battle (0x4d..0x58, bt_on, bt_s, bt_w.., the turns) holds Lock, BtAttack, BtMove; BtAttack (0x59..0x67, the blows) holds Lock and BtAtkCombo. Who has ability +0x3c2 and +0x3c3 gets **BtOn** in Walk: `ready` (R1) asks `pickArmedMotion(0)`, bt_on, whose 遷移モーション is bt_s; ability +0x3c2 without +0x3c3 gets Lock and BtAttack directly in Walk. `allows.armed` +0x38d says the stance may stand, and when `ready` goes BtMove asks bt_off (slot 8). `pickArmedMotion` slots: 0 on, 1 stand, 2 w, 3 w_b, 4 w_r, 5 w_l, 6 t_r, 7 t_l, 8 off, columns no weapon / weapon. **BtMove:** with no target the stick's sides are the turns and forward does nothing; with a target (`m_290`) the walks close on it and strafe by abilities +0x3a8, +0x3a9. **The blows** come from `wants.fire`, tap 1 or hold 2 of the Use button (the squat button with a firearm), and need a weapon or ability +0x3c4 and `m_ba8` run out. No weapon: 0x59, with +0x3c5 a coin with 0x5a; 素手コンボ 0x61, chain 0x61, 0x62, 0x63. Kind 0: 0x5b / 0x5c by a coin, hold with ability +0x3c6 0x5d; 武器コンボ 0x64, 0x65, 0x5d. Kind 1 the same from 0x5e / 0x5f, 0x60; 0x66, 0x67, 0x60. `MotionEvent_Cancel` turns BtAtkCombo on. A blow ends with the motion, with `CharacterGround::bumped` or on a part it breaks (`findBlowPart`: kinds 1..6 not 3, a blow's body on, within 140 flat), those two with `m_ba8` 0.65. **Lock:** controlled and no target: `lockFind`. `findTargetAhead` 60 degrees, characters, nearest, within `lockReach` (a melee weapon's parameter 5, else 500); then 15 degrees within 15000 by angle, taken only within `lockKeepReach` (parameter 5, three times for a firearm on Easy), and when that finds nobody 30 degrees of the other groups within 500. With a target the d-pad's sides `findTargetBeside`, and a target that is no character then looks at the other groups. `isTargetValid`: +0x36, +0x34; a character is an enemy, slot +0x278 set, `m_endurance` above 0. `canSeeTarget`: the ray from bn_head and 60 degrees of pitch. Off the pad `m_294` is the target. `faceTarget`: a quarter of the angle, at most 6 degrees a frame, into `m_turnStep`, `leanBody`, and tool, head and look targets as the motion's row says. **Sides:** `m_sides` +0x590 is the sheet's PC, NP, 屍, 闇 bits, copied in 0x21cc70 at 0x21ddf0, and they are the sides it fights: F_PC01 has 12, corpse and dark. `isEnemyOf` turns the target's sheet kind into a side (0 PC, 1 and 0xf NP, 2, 3, 8, 0xa, 0xd corpse, 4..7, 0xb, 0xc dark, 9 and 0xe none); `m_foe` +0x450 wins while it is on the pad. A character taken over fights as the player (`getSides`; the ROM inlines that recursion three levels).

**The special actions (read 2026-11-05, batch 132).** `PlayerMotion` +0x2c is a second map of conditions by name, filled once by `initList` 0x1df0b0 (called from the ctor at 0x1edb88, before the links), and `WalkLink`'s ctor adds the names of data 0x2fb3a0 (30, plus 5 at 0x2fb420 for kind 0xb, 4 at 0x2fb440 for kind 0xd) after its plain conditions and before Turn and Move; links 0x281aa0, 0x281b30, 0x282da0, 0x282fa0 add others. Base vtable 0x317d50 (EE 0x18..): +0x14 allowed, +0x15 enabled, slots +0x14 clear allowed (`initSlots14` walks them before the links' onUpdate), +0x18 clear enabled (`initSlots18` walks them after input is cleared), +0x1c set enabled. So **a switch lasts one frame**: the script or the brain calls `setSpecial` 0x17b3c0 / 0x1def90 each frame it wants the action, and onApply asks for the motion if the condition allows it. `isSpecialAllowed` 0x17b3b0 / 0x1def60 answers +0x14. Classes: 0x2814a0 vtable 0x317d30, a motion by id or by the row of `gMotionDefs` with that label (0x2813e0): cr_*, all sp_*; 0x2812e0 vtable 0x317d10 (cr_1..3): the switch stays in +0x1c until seen, a living partner only, in a car 0xf2 becomes 0xbb and 0xf3 says PC_END; 0x280410 vtable 0x317c90, the stage's gadgets (+0x18 kind, +0x1c arg, +0x20 a text from a native): 0 Elevator* on object zs9901 (arg 1..4 B1, B3, BxB1, BxB3), 1 SigLamp `owned[0]`, 2 NumLock `owned[1]` (arg 1 Open and 2 Close only ask 0xd1), 3 SevenKey `owned[2]`; motions 0xfc, 0xfd, 0xfe. Cars, not written: 0x280f50 vtable 0x317cf0 GettingOnCar, 0x280e20 vtable 0x317cd0 GettingOffCar (driving 1), 0x280a60 vtable 0x317cb0 DragOutCar (flag3be). **Callers:** natives 0x19f..0x1a2 (elevators), 0x1a7 (by name), 0x16c and 0x1b64d0.. (cars), 0x1aaed0, 0x1ac800, 0x1acb00, 0x1acd00, 0x1aeb40, 0x1aee20, 0x1c46d0 (gadgets; 0x1acb00, 0x1c46d0 and the brain's 0x1da430 also call `setText`), and the brain 0x218190, 0x21ac60, 0x21f610, 0x2251f0, 0x225fc0, 0x2257c0, 0x226ff0, 0x2273f0, 0x22a3c0, 0x22a4f0, 0x22b580, through the methods 0x1da2d0, 0x1da430 of vtable 0x315cbc.

**The bite (read 2026-11-05, batch 132).** Bite condition vtable 0x3174b0: onUpdate 0x26fd30 needs `flag3be`, a candidate +0x624 (no writer found: no sw, sd, sq or addiu with that offset on a character; 0x21abb0 is its only getter; the brain reads it) that is a character, not `m_biteProof` +0x5b9, not at brain state 0, held by nobody (+0x2b0), with nothing in +0x400, with flag3bf, with endurance, in no motion of the list, within max(85, reach - 10) + 10 (reach `getAttackReach` 0x20ce80), within 50 in height, in front within 60 degrees, head to head clear: then allows +0x392 and the target +0x394. onApply 0x26fb60 on wants +0x340 (no input sets it: the brain) calls `startBite` 0x26fbc0: the biter 0x9e, facing the prey, walks on the file; the prey goes to 0xa0 (biter behind, the prey turned half round) or 0xa1. Links: **Bite** 0x278db0 vtable 0x317860 (+0x20 target, +0x24 hits), **Bitten** plain (vtable 0x315eb0, no code). onRun: on the biter's click `takeDamage(biter, 7, 1, m_biteDamage / (hits + 1))` (+0x5dc, only the property setter gives it; 1 at least), blood at bn_neck (0x26a010), BT_BITE, the player event BITE; a pressing player counts a hit every 0.15 s (`m_bf0`) and is free past 4 hits, a brain's prey by chance 20, 30, 50, 70, 100 percent, and `m_biteProof` or brain state 0 at once. It breaks (0xa4) on a timed rate, a wall between the heads, or a pose strayed from the walk by more than max(10, nearRadius / 3). Kind 3 (F_SRT) ends by itself and, when the prey's motion leaves, takes its own whole endurance. onLeave: 0xa1 -> 0xa6, 0xa0 -> 0xa4, the prey's MSP back to the sheet's.

**Status (read 2026-11-05, batch 132).** `setFlinch` 0x211150 clamps to 0..flinchResist and sets +0x3e1 when 0 goes to the limit (a sheet with limit 0 sets it each call, as the ROM does). `setEndurance` 0x211430 clamps to 0..endurance; for the player and the partner a value at or below 0 is ignored while `isInputBlocked`; at 0 `setBrainState(7, 0)`, and for those two `startGameOver(this, m_lastHitBy)` (+0x620, written by 0x20e770 and 0x20eb40); above 0 at brain state 7 `CharacterBrain::revive` 0x1dc950. `setBrainState` 0x21ba10: out of the scene at 0 (tree hidden, m_viewActive, the ground's byte, children inactive, sounds), back from 0; at 7 the DamageDead link's +0x20 = m_reviveTime in frames; then the brain 0x1dbca0. `GameOverSequence`'s ctor 0x2b1cb0 keeps the two objects at +0xc, +0x10: they are pointers, not ints.

**The game over (`game/gameoversequence.cpp`, batch 151, 0x2b1670..0x2b22f0; the file name is the port's).** `GameOverSequence` +0 state, +4 timer, +8 skipped, +9 view taken (nothing reads it), +0xc victim, +0x10 cause, +0x14 camera. Start while the fade is not full sets skipped and starts the 2 s fade to full (mode 2), the same as no victim. State 1: gear in hand (+0x400), or `PlayerMotion::m_bfc` in motion 0x42, 0x94, 0x95, goes to 3, else 2. State 2: `DeathCamera` "CAM_DEAD" on the view stack at priority 3, on the victim, for 50 frames (100 for an own fall, victim == cause). State 3: the party's view on `pickViewTarget` (victim null: nobody; cause null or the victim: the scene's player; else the cause if `canBeJacked` or `isPlayer`; `StageParty::startGameOverView` 0x1fe7c0, mode 4), 100 frames (200), cut short when the victim lies dead (`LifeState_Corpse`) and something else felled it. State 4: the fade; `getState` then answers 5 once it is full, which is `isGameOver`, and the scene ends in "Mission failed." with Retry / Give Up. The camera comes off the stack on leaving state 2 only when a target exists, and is never deleted here (the scene's list owns it). **`DeathCamera`** kind 12, vtable set 0x318cf0 / 0x318d14 / 0x318d2c, EE 0x380, on the kind 8 class: eye offset (0, -100, 200) turned round z by +0x360 then through the linked world matrix, focus = the linked place + (0, 0, 80), a ray from focus to eye (the main camera's mask 0xfc2600f, flags 0xc0000) pulls the eye in, `aimFrom`. The turn eases in for 30 steps and out for 30, accel 0x3b64c389: half a turn. It steps twice a frame like `ViewCamera` (slot +0x14 of 0x318d14 is a thunk into `cameraStep`), so the half turn takes 30 frames of the 50. Slot +0x264 `resetShot` is the class's own; +0x260 tails into it. **`SceneDirector`** +0 colour, +4 from, +8 to, +0xc frame, +0x10 frames; byte-wise lerp. `drawLetterbox` is the ROM's inline pair of fills, 0..0x40 and 0x1c0..0x200 of the 512-square frame, shared with a sub-scene's.

**What a hit does (read 2026-11-06, batch 179; game/playermotion.cpp).** `PlayerMotion::takeHits` 0x1e18c0 runs first in the terminate half (`endFrame`). A deadly fall (`m_bfc` on 0x42, 0x94, 0x95) ignores everything until the frame it lands. With `m_biteProof` (+0x5b9) or brain pace 0 nothing below counts, but the fall at endurance 0 still does. In order: **light**: `m_lightDamage` above 0, a `m_shadowLight` that is not a YAM_ one and `m_lightHurtWait` (+0x3f0) run out: takeDamage kind 5 for a Hard light, 6 otherwise, the sheet's 光ﾀﾞﾒｰｼﾞ; the wait is 光怯み / 30 s plus 0..19 percent; Motion_Burn 0xad. **What the ground says reached it** (`wasHit`): a sleeper of sheet kinds 5, 6, 0xc (brain pace 5) struck from the front by someone without the Kyo model shrugs it off and "kikanai" (効かない) shows, one of it, no damage, no reaction. A bullet: part by bone (table 0x2f8650: head and neck 0, arms 2, 3, legs 4, 5, the rest 1), kind 4, ammoParam1 + the shooter's `m_shotScale`, 4 of the blood effect. A car: `takeBlow` from its driver. A character: `takeBlow` 0x2103d0, count the weapon's param11 or the sheet's 血しぶき. A blow's part is drawn: 60 percent body, 10 each of the others; damage the weapon's param1 + the victim's `m_meleeScale`, or the striker's 素手攻撃; a driver hits for `CarRunOver` by `slideLevel`, the worst 4 x the victim's endurance. Effects wait `m_hitTime` 0.15 s between them. **Reaction**, only when the motion's ダメージ推移 is 1, `m_shrugsHits` (+0x5b8) off, not sheet kind 3, not shrugged, and for non-leads the flinch reached ひるみ耐性値: on a ladder dm_ld 0xae; else by the striker's motion's ダメージ種別 (`MotionDamageKind`), front or rear from `hitInBack`: 1 plain, 2 hard with flag3cd, 3..6 the dm_0_1..4 sides with flag3cc, else plain or hard. A shot from past 200 up or 800 away, with flag3ce, is dm_hit_f / dm_hit_b; a head shot hard; in a car 0xbf. A car by `slideLevel`: Light hard, Heavy hard for leads and kinds 7, 0xa, Worst dm_car_f, dm_car_b, kind 7 dm_c, a lead dm_c at endurance 0 or hard. Then the flinch goes to 0 and `m_unarmedMsp` back to the sheet's. **Trap** near, AI level 1 or less, within `trapReach` (50 running, 40, 25 walking, 10): trp_s 0x9b, TRAP_ON heard, the walk to the trap, `m_sprungTrap` (+0xbf4), `spring`. **Landing** (the frame `landed` turns on, not sheet kinds 3, 7): the drop is `floorHeight` minus z. From a fall, past 305 with 段差下り 0, or past 395 with 1: (drop - 300) / 10 + 10. Off a ladder or a dive past 10: drop / 10 + 10; with `m_bfc` the rest of the endurance for non-leads. `takeFallDamage` 0x20e770: twice that off each leg, that off the endurance, cause 8, DMGLND; a high fall with no damage LNDH. **Shoves:** `climbedOver` 1 point (kind 9), dm_f_h; `atDoor` dm_f. **Last:** a motion with ダメージ推移 above 0 at endurance 0 asks ld_fal on a ladder, 0xc0 in a car, dm_c else. `m_shrugsHits` also keeps `m_hitThisFrame` off. `spawnBlood` 0x26a010: rotation (pi/2, 0, atan2(-x, y)), the bone's place less radius - 10 along the blow, the others within 20 across and 15 up; no name: the owner's `m_bloodEffect`, and count 0 means three of "damage".

**Being hit, lying down and getting up (read 2026-11-06, batch 178; game/playermotion.cpp).** The sheet's rows, dumped by a temporary log: 0xa4 dm_f, 0xa5 dm_f_h, 0xa6 dm_r, 0xa7 dm_r_h, 0xb7 dm_hit_f, 0xb8 dm_hit_b link **Damage**; 0xa8..0xac dm_0_1..5, 0xad burn, 0xae dm_ld no link; 0xaf dm_lnd link **DamageLand**; 0xb0 dm_c (then 0xb3), 0xb5 dm_car_f (then 0xb4), 0xb6 dm_car_b (then 0xb3) link **DamageFatal**; 0xb1 dm_rev_b no link; 0xb2 dm_rev link **Revive**; 0xb3 dm_ded, 0xb4 dm_ded_b link **DamageDead**, no next. 0x94 dv_f, 0x95 dv_b link DiveFall, 0x96, 0x97 DiveLand, 0x9b trp_s TrapHit (next paragraph). `Damage` (vtable 0x317830, EE 0x24): first frame clears its three bytes and, with `m_dropsWeapon` (+0x3d8, the script's Flag3d8) and `m_armLost`, throws the long weapon clear, 15 units ahead or back; only with `divesOverEdges` (+0x3b6) it then looks for an edge in the first second: `faceAhead` + `findEdge` with the object null and **a ninth argument on the stack, +0x21, the blocked flag, which the decompiler drops**. A drop below 0, or blocked, marks it; a ray 50 up from the target towards the head's reach (head z - feet z + 20, or 170 without bn_head) says whether the way is clear; on a car floor an edge not on the car needs a second ray, flags 0x40140. It walks to the point at rate 0.35 and asks dv_f or dv_b there (within 5 units, squared 25). dm_f, dm_f_h, dm_hit_f are the back way (`forward` false, angle + pi): a blow from the front throws it back. `DamageLand` at the end: dm_ded with endurance 0, else dm_rev. `DamageFatal` (0x3177d0): the body follows the floor (+0xb54 under +0xb37) at 2 s for kinds 0, 1, 9, 0xa, on MotionEvent_Click for the others, `Revive` clears it the same way; dm_car_* with endurance left goes to dm_rev, dm_rev_b; with none, kinds 5, 6, 0xc go to dm_ded and the rest to the sheet's next. First frame: drops the weapon, `shiryodead` at bn_pelvis for kind 3, `yamidead` for character class bit 8. Yrt's push body goes off at 1.2 s. onRequest sets 0x200000 of the wall body's `rayMask`, onCancel clears it. `DamageDead` (0x3177a0, +0xc10 of PlayerMotion): first frame `reviveFrames` = `m_reviveTime` x fps; down to 0, **and it stays at 0: only below 0 does it rise**, and -1 comes from `takeCorpse` 0x21ab10 when the brain takes the corpse. Then dm_rev or dm_rev_b and `initStatus`. For non-party, non-Yrt, the model's +0xac goes to 1 after 2 s, back to 0 on leaving. **Who starts them:** 0xb0 at 0x1e3000 and 0xb5, 0xb6 at 0x1e36bc, inside `PlayerMotion::takeHits`; 0xaf at 0x27ba44; 0xb3 at 0x277c4c; 0xb4 at 0x196b74.

**Falling over an edge and being caught (read 2026-11-06, batch 180; game/playermotion.cpp).** `DiveFallLink` (vtable 0x317980, EE 0x80, ctor 0x27a0b0) runs dv_f 0x94, dv_b 0x95 and the ladder's fall 0x42. `onRequest` copies Damage's `edgeBlocked` (+0x21) to `m_bfc` (+0xbfc, the fall nobody survives) and `edgeClear` (+0x22) to its own `headClear`. `onStart` turns show flag byte 3 off for the player or partner when `edgeBlocked` is set, `onLeave` turns it on again; only the script's property and 0x230400 read that byte, its meaning is not read. `onCancel` clears `m_bfc` and takes the view off. **First frame:** with no head room the body will turn pi/4; a deadly fall calls `startCamera` 0x279780: the player or partner get `cam_dive` from `StageObjectList::find0`, the edge from `faceAhead`, or WalkLink's wall point; the eye 100 back and 100 up from the sway matrix, the target 60 past the edge and 30 down; a player whose top view is kind 9 flies from that view for 60 frames, else the camera cuts; `pushView(camera, 1, 1)`, `deathFrames` = 90 - the flight. Anyone else just drops the weapon (with `m_dropsWeapon`). **Each frame:** when the motion ends its last `m_positionStep[2]` is kept up into `m_moveStep[2]`, so the body goes on falling at that speed; past 50 units fallen a body with no head room turns half of what is left round bn_pelvis, is pushed out of a wall between the feet and the pelvis, and gets its wall check back; `stepCamera` 0x2794e0 flies the camera round the look point in the ground plane (the look eases only in the last 30 frames, 25 units at most), then counts `deathFrames` to `takeFallDamage(endurance)`. On the ground past 25 units: 0x96 from dv_f, 0x97 from the others, `groundCheck` 1. `DiveLandLink` (0x317950) at the motion's end: 0x96 gives dm_rev or dm_ded by endurance, 0x97 dm_rev_b or dm_ded_b, and for kind 7 the front ones. `TrapHitLink` (0x3178c0, EE 0x24): the first frame drops the weapon and holds for the sheet's 罠耐性 / 30 seconds, plus up to 19% by chance; `m_bf0` is 0.15, ticked by `CharacterInput` only while the left stick moves, and at 0 takes 2 s off; asks 0x9b while `m_motionTime` is under that, then 0x9c. `onLeave` is `startLinger` on the trap; the host tests for null, the ROM does not.

**Seen at a real edge (2026-11-06, batch 181, forced blow, the game's edge):** stage 2, drop 180: dm_r, `findDive`, dv_f, 0x96, 27 lost, FALLDW3, dm_rev, SPC_GETUP, idle; from behind dm_f, 0x95, 0x97, dm_rev_b. d2303, no floor below (`blocked`): `m_bfc`, `cam_dive`, "Mission failed". Open: open-items.md.

**The links of the ROM's ctor, in order (44, map +0x18):** Walk, Jack, Pickup, Up, Fall, Jump, Ladder, Door, Battle, BtAttack, Gun, GunAttack, Reload, DiveFall, DiveLand, TrapSet, TrapCancel, TrapHit, Throw, Bite, Bitten, Damage, DamageLand, DamageFatal, DamageDead, Revive, ApproachCar, ApproachOut, Drive, SpWait, NPCReaction, ApproachBite, SigLamp, NumLock, SevenKey, SpFear, SpRing, SpRobo, SpDrop, SpPadlock, SpNumLock, BossAttack, BossIdle, BossAttack (some only by kind). All of them are in the host, in that order (the last eleven since batch 182).

**The stage's own links (read 2026-11-06, batch 182; game/playermotion.cpp, code 0x281a40..0x282d98, dtors 0x283a00..0x283cb0).** The sheet's rows, dumped by a temporary log: 0xc7 sp_robo SpRobo; 0xc8 sp_ron, 0xc9 sp_rs, 0xca sp_roff SpRing (next 0xc9, 0xca, 0); 0xcb sp_fear SpFear (next 0xcc); 0xce sp_drop SpDrop; 0xd1 sp_lkon (next -1), 0xd2 sp_look, 0xd3 sp_lock SpNumLock; 0xd7 sp_shut SpPadlock; 0xd8, 0xd9, 0xe7, 0xe8 BossAttack (labels sp_at1_1, sp_at1_2); 0xdb sp_spw, 0xdf sp_idle BossIdle; 0xfc, 0xfd, 0xfe label s, links SigLamp, NumLock, SevenKey. **Where the disc's scripts name them** (a temporary scan of each stage part's scripts for the text): sp_shut 2.1, 2.2; sp_robo 7.2; sp_ron 9.1, 9.2; sp_fear 10.1, 10.2; sp_drop 21.2; sp_tank 28.2. sp_lkon, sp_numb1, NumLock and SevenKey are in no script's bytes. **What each does:** `GadgetLink` (vtable 0x317f20, one class under three names, built when `StageScene::owned[i]` is set) passes the special's text and the character to the gadget's slots +0x20, +0x28 and ends when slot +0x3c says no. `SpFearLink` tops the endurance up from the sheet until `CharacterGround::wasHit`, then 0 and standing; bitten out of it (0xa0, 0xa1) leaves 1. `SpRingLink`: `oZS09_04` is a telephone by the look of the shot and by the sound EVE_S09_04 at P_AC01, started on the motion's sound mark in 0xc8 and stopped in 0xca; the name Ring is the ROM's. `SpRoboLink` (oDC07_05) and `SpDropLink : SpRoboLink` (oMK21_09, vtable 0x317e60 over 0x317e90) bring the object to the character, play <name>_sp and put it back on leaving. `SpPadlockLink` walks to oMT02_30 through its world matrix: its transform's position is 0 0 0, the matrix holds 5625.9 4996.9 640. `SpNumLockLink`: an object of getKind 5, `StageObject_NumLock` (vtable 0x318870 on StageMapObject, no host class), dials in `NmlPose` +0x3f0, the code at the object's +0x240. `BossAttackLink` and `BossIdleLink` have no code: only their calls.

**The three maps are `LiteralMap<T>` (game/stagelist.h, 2026-11-06, batch 166).** `StringMap<T, false>`: the same tree on strcmp, chained in insertion order, but the node keeps the caller's key instead of a new[]'d copy, so its ctor and dtor differ and nothing else. Instances: links +0x18 0x1eee30, 0x1eee50, 0x1eef90, 0x1ef1a0, 0x1ef1f0, 0x1ef260, 0x1ef2e0, 0x1ef990, 0x1efad0; conditions +0x8 0x1ef040, 0x1ef060, 0x1ef0f0, 0x1ef540, 0x1ef520, 0x1ef590, 0x1ef610, 0x1ef850, 0x1efce0; specials +0x2c 0x1eeee0, 0x1eef00, 0x281730, 0x1ef210, 0x2817e0, 0x1d1280, 0x281800, with no balance in the ROM. The ctor balances the conditions before `initList` and the links at its end. Replacing `std::map` changed three things: a walk over the specials goes in insertion order, not by name; `find` answers null for a null value; the first of a name stays. `tools/reverse/literalmapcmp.py` plays random inserts, finds and balances on all three ROM instances beside the host: 10,890 ops, 0 differ, tree shape, chain, count and last compared after each op. **Bitten 0x315eb0** has its own dtor 0x1eedb0 and no other code: `BittenLink`. Its vtable therefore lists the base's empty defaults (`jr ra`, batch 240, 2026-11-09): onUpdate +0xc 0x1eee10, onApply +0x10 0x1eee20, onRequest +0x14 0x1ec640, onStart +0x18 0x1eaba0, onRun +0x1c 0x1e5d50, onLeave +0x20 0x1eabb0, onCancel +0x24 0x1ec3f0; they sit between the links' code in playermotion.cpp, so each is a 1-function gap in the map until pinned. All seven are named and pinned there now (onRequest last, batch 241, 2026-11-09, between startRequested 0x1ec400 and requestNow 0x1ec650).

**The status block and the script's properties (read 2026-11-05, batch 134).** `reset` 0x17c620 ends in `resetActionState` 0x21e050, which calls **`initStatusBlock` 0x21cc70 with a0 = this + 0x590**: one block of the character, filled from the sheet each reset. In order, EE offset = sheet column: +0x590 sides, +0x594 体力, +0x598 耐久力, +0x59c..+0x5a8 the limbs, +0x5ac 0 (flinch), +0x5b0 素手MSP, +0x5b4 0, +0x5b8..+0x5ba 0 bytes, +0x5bc AILV, +0x5c0 視界, +0x5c4 音感 / 100, +0x5c8 回復, +0x5cc 光耐性 (0..2), +0x5d0 光ﾀﾞﾒｰｼﾞ, +0x5d4 光怯み, +0x5d8 **素手攻撃** (what a forcing takes off a door, so not only a script's), +0x5dc **かみつき** (a bite's), +0x5e0..+0x5e8 rates / 100, +0x5ec 復活時間 / 30, +0x5f0 **操り耐性** (the drain of stages 0xf.. reads it), +0x5f4 罠耐性 / 30, +0x5f8 射撃 == 999 (blanks), +0x5fc 射撃 / 100 (the scatter), +0x600..+0x608 射撃間隔, 攻撃間隔, 噛みつき間隔 / 30, +0x60c リロード / 100, +0x6c0 打撃攻撃力修正値, +0x6c4 射撃攻撃力修正値. Zeroed: +0x610..+0x62c, ten ints of +0x630, +0x668, +0x670.. with +0x68c, and +0x690 = -1 (`m_lifeOverride`). **初期M (+0x40) is the brain's first state:** 4 -> 6, 3 -> 5, 2 -> 2, 1 -> 1, else 0, handed to `CharacterBrain::setState`. F_PC01: start 1, walk 1.00, run 1.00, turn 1.50, recovery 2, 素手攻撃 0, かみつき 0, 操り耐性 1, 射撃 100 (scatter 1.00), sides 0xc. **`getProperty` 0x2132d0 / `setProperty` 0x212530** (vtable 0x312240 + 0x1f8 / 0x1fc; the getter's jump table 0x306d50 has 0x74 entries and Ghidra cannot follow it, so read it from the asm) carry the ids of `CharacterProperty` in game/objectproperty.h, plus copies of StageObject's; the map object's 0x174b00 and the sub-scene's 0x269160 are copies too, so the host keeps one, **`StageObject::getProperty` 0x18a5b0**. Runner slot 86 (+0x160, base 0x172050 answers 0) is `cmdIsLit`, Prop_SwitchedOn's read; a character's goes to the lamp it holds (slots +0x264 / +0x260). Property 0x2e is the long weapon's `ModelInstance::shown`, and a store can only set it. 0x5a and 0x5b always read 3. 0x33 reads the party's companion id while `getAltSoundFlag` holds. Stage 1 asks of F_PC01: read 0x29, 0x04, 0x4a..0x4c, store 0x36, 0x61; of Root read 0x3b, 0x3c. **`ScriptPoint` (EE 0x20: flag +0, vector +0x10, `set` 0x20b220) comes seven times in a row, +0x6e0..+0x7a0:** +0x6e0 set by the status block's step 0x21c3d0, +0x700 where 0x21bfd0 found the character, once, +0x720 the script's (native 0x94, properties 0x4a..0x4c) or the brain's (0x21ab60, 0x219040), +0x740, +0x760, +0x780 by `beforeScripts` 0x21c7c0, from foes it tracks at +0x680, +0x684, +0x67c. +0x7a0 is only a flag as far as read: +0x7a8 beside it is the cached seen enemy (1 = pick again, 0x21a040, 0x21a1c0). **The sight table +0x468** has 0x18 lines of 12 bytes, one per character id - 1 (`SightEntry`), with its owner +0x588 = the character; `beforeScripts` steps it through 0x217a80. `resetActionState` writes all of this; nothing in the host fills it until the brain and `beforeScripts` are written.

**Firearms (read 2026-11-05, batch 129).** The sheet's link Gun (`pickGunMotion` slots: 0 on, 1 w, 2 wb, 3 wr, 4 wl, 5 t_r, 6 t_l, 7 aim, 8 fire, 9 rl_b, 10 rl, 11 rl_e, 12 rl_a, 13 at_01, 14 off; columns pistol, rifle, R_MINIMI) holds Lock, Snipe, GunAttack, GunMove; GunAttack holds Lock and GunAttack, the second off until the sheet's frames have gone by (parameter 9, through the scope parameter 4, each + 0.5 at 30 Hz, against `m_motionTime`). **GunOn** in Walk: `allows.armed` needs a firearm and, for the controlled character, 75 clear ahead at the head's height (`wallAhead`, from the feet's x, y at bn_head's z); `ready` asks slot 0. **GunMove:** not armed or not ready: slot 14; a wall within 70 also asks slot 14 unless it is already at aim or through the scope; the stick's turns are the aim's turns with no target and strafes with one (ability `strafe`, `walkBack`); last, nothing asked: slot 7. It also gives the pose its `lookBone` (+0x1a0, "no writer found yet" until now): `rightTool`, or `leftTool` for a sheet of ctype 10; `dropGun` clears it with the target and the scope when leaving for anything but the other gun link. **The shot (`GunAttackCondition::onApply`):** the spread settles first, whatever the trigger says. `wants.fire` 1 or 2 is the butt (slot 13, a blow as in BtAttack; not through the scope); 3 or 4 a shot, 4 keeps Lock on through it, 3 turns it off. 3 is never set by the input, so it is a brain's. An empty gun plays NO_BULLET at the weapon. No target: from the weapon's position along `getLampMatrix` for `WeaponDef::range`, or from camview_snipe's along its own axis; a target: to `m_aimPoint`, from the weapon (controlled) or bn_head (not), and for a character target with `m_shotScatter` set (+0x5fc, was misnamed `m_aimLock`) `ShotSpread::scatter` 0x269db0. A wall between head and weapon, or `m_blanks` +0x5f8, makes it a blank. **`ShotSpread`** (MotionProbe +4, EE 12 bytes): amount, rate, settled. The cone's half angle is atan((len / 10 * amount) * (1 - settled / 2) / len), so with amount 1 it is 5.7 degrees unsettled, half that settled, whatever the distance. `settled` grows by 1 / (3 s of frames) for the controlled character, at once for others, and restarts on entering either gun link. **The scope:** `PlayerMotion::setScope` 0x1dfd90 (was `clearNoFlash`; +0xbd4 was `noFlash`, it is the scope, and `fire` uses it to hide the flash): `SnipeCondition` allows up of the d-pad for a rifle (kind 5, 6) whose parameter 4 is set, the right stick and down while in it. In: zoom +0xbd8 = 0.3, rotation kept in +0xbe0, the target dropped, the body hidden (`m_tree->setShown`), `menuProgs->state` 1, or 2 for R_64C, camview_snipe linked and pushed at priority 1, 2 for a character taken over. `setZoom` 0x29a840 maps `span * zoom + low` back to 0..span, and scrZ = 350 + over * (m_range - 350) / span. **`low` 76.8 and `span` 101.4** are computed by three small static initialisers from an int 30 (`256 * 30 / 100`, `255 - 2 * low`), one copy per object file (0x310b10, 0x310ec8, 0x310f18): an analog dead zone. **Correction:** +0xbd8 is a float, the zoom; `StageScene::draw` 0x198c30 calls `MenuProgSet::draw` 0x2bb260 with a0 = scene +0xa8 and f12 = that float. The host had it as `player->m_motion->overlay->draw()`, a call on a pointer nobody set. `MenuProgSet` is the sight's pictures, not only "menu programs". **Reload:** ability +0x3bd, a firearm not full, not R_MINIMI. `wants.reload` (a brain's) or two seconds of motion 0 asks it: G_NN38, R_MURATA and G_26NEN slot 9, then rounds one by one on slot 10's clicks, slot 11 to close; the rest slot 12. A player needs the weapon's ammo item in `m_items` with a count; others load from nothing. `m_reloadRounds` +0x3dc is what is left of the request. Sounds: 0x2a3010, 0x2a3060, 0x2a30b0. **Traps and throws:** TrapSet carries I_WANA, wants 60 ahead clear at 50 up and a floor within 20 of the feet, motion 0x98; on the click it drops the trap there, turned a quarter from the character, and arms it (TRAP_SET). TrapCancel `findTrap` (group 3, kind 0x16, `canSpring`), motion 0x99, walks to `getWorkPlace`, disarms and takes it (TRAP_OFF). Throw: 100 clear ahead of bn_head, motion 0x9d, the long weapon hidden meanwhile, `throwItem` on the click with the wants' vector, zero meaning (0, -20, 5). **The firearms of the sheet (printed from `gWeaponDefs`, 2026-11-05):** name, range, parameter 4, parameter 9, magazine, ammo: G_9MMMG 1600, 0, 1, 25, B_9MM; G_9MM 1600, 0, 10, 9, B_9MM; G_NN38 1600, 0, 10, 5, B_38SP; R_89 2000, 1, 1, 30, B_556; R_89_2 2500, 1, 1, 30, B_556; R_MINIMI 1800, 1, 1, 200, B_556; R_64 2200, 2, 2, 20, B_762; R_64C 2500, 2, 2, 20, B_762; R_MURATA 2200, 40, 10, 5, B_762; G_26NEN 1600, 0, 12, 6, B_9MM; R_YOYAKU1 1500, 0, 10, 6, B_9MM. Rows 0..0x30 are the melee weapons, with none of these. Every rifle but the unused R_YOYAKU1 has a scope. **Nothing read so far writes `wants.trapSet`, `trapCancel`, `throwItem`, `call` or `reload`:** the input does not (asm 0x236b90 has no store to +0x2d8..+0x2da, +0x2f0, +0x2f8), so the writers are the item use 0x218850 and the brain.

**What a character carries (read 2026-11-05, batch 127).** `m_items` +0x22c holds it all. `giveItem` 0x216aa0 refuses an item with a parent, hangs it under the character at the origin, hides its model and clears `m_lit`; then by kind: the lamp (0x14) is `m_lampItem`, shown, under `bn_l_tool`, or `T_TORCH2` under `bn_spine1` at `chestToolPosition` with its lights under `bn_l_tool`; a weapon (0x15) takes `m_weapons[0]` and sends the old one to [1] with `swapWeapons`, or drops [0] when both are full, under `bn_r_tool` (a ctype 10 sheet holds firearms in `bn_l_tool` and keeps the second weapon shown); a thrown item (0x17) under `bn_r_tool`; `ItemKind` 7..10 stack by name up to `m_maxCount`, the new object is switched off. `dropItem` 0x216920 undoes it at the character's position, `m_lit` 1, `m_lightSet` -1. **Pickup:** `findItem` walks group 3 for free items (no parent, no holder, transform dirty, not used, not sprung, `canPickFar`, seen from `bn_head` with mask 0xe002003), nearest; `PickupItem` stores it in +0xbd0, where Always turns the head to its pick point, and allows it when `canPick` agrees; the item list then offers Use as kind 5; motion 0x12, or 0x13 when the point is more than 65 above the feet, walking to it at 0.35; `PickupLink::onRun` gives the item on `MotionEvent_Click`.

**Ledges, ladders and gaps, and the Use button (read 2026-11-05, batch 125).** `WalkLink::onUpdate` finds a wall face and the edge behind it; **Level** (0x272630 / 0x272360) allows +0x368, 1 up or 2 down, by `edgeDrop`: down fal_l below 0, fal_m past -75, fal_h past -150; up u_060 below 75 (stepLevel 1), u_090 below 105 (2), u_120 below 150 (3), else u_180 (4). A face of flag 0x100 (kFaceGap) offers Level only while not walking ahead and Jump only while walking ahead (`isForwardMotion`). Stage 1 and 2's lead (`isOpeningLead` 0x215ce0) never drops past 125. **Ladder** wants a face of flags 0x2020, ability +0x3b4; the floor behind it, 1000 up or down, says which end one is at, +0x36c 1 foot, 2 top. **Jump**: flags 0x180, ability +0x3b5, a raycast 25 past the radius and `sidesClear` 0x26c2d0. All three wait while another character near is on the same way (`levelBlocked` 0x1dff40, `ladderBlocked` 0x1e0360, with `getLadderRank`: kind 1 first, then 0, then the rest). **The Use button does not reach them directly:** `UiItemList::update` (game/uiitemlist.cpp) reads the allows of the controlled character and takes the kind of highest priority (`UiUseKind`: 0 up, 1 down, 2 ladder, 5 unread, 6 jump, 7+ `post` from scripts, natives 0x79, 0xf6) into the back slot; the slots flip each frame; `CharacterInput::stepItems` asks `offer(who, kind)`, and the answer becomes `CharacterWants` +0x2cb, +0x2cd, +0x2ce. A kind's label and priority are scripts' (natives 0xf7, 0xf8, each `kinds[i]`, default "未定義", priority 0x80). `isReady` (kind past 6) is what makes the list the input focus, so only a script's prompt takes the pad from the character. **Links:** Up 0x317b60: `onRequest` takes the rise, `onStart` and `onRun` set `setRootScale(1, 1, rise / made, 1)`, made being 60, 90, 120, 180: a climb is the sheet's motion stretched to the height. Fall 0x317b30 (fal_*, ld_esp): holds the motion's last step into `m_moveStep[2]` once it ends, then at the floor picks by how far it fell: under 25 nothing, under 75 lnd_l, under 150 lnd_m, under 295 lnd_h, then by the sheet's 段差下り (+0xe0): 2 lnd_h, 1 lnd_h under 395, else 0xaf; a Click event turns the watch off and on. Jump 0x317b00: lnd_h on landing. Ladder 0x317ad0: only ld_s has it, with LadderMove: Back, Walk, Run climb, 35 under the bottom and 185 under the top end it; with allows.squatAction the squat button is ld_esp. LevelLift is the pair's u_sp: wants +0x341 is set by neither the input nor anything written; the brain is the guess.

**`pickStance(slot)`** 0x1eb660: a table of { standing, crouching } by whether `motionId` is a crouch: 0 s / sq_s, 1 w / sqw, 2 w_b, 3 w_r, 4 w_l, 5 run / ka, 6 t_r, 7 t_l, 8 call, 9 jack, 10 w_run / sq_ka. Second table (tr_) with `m_winded` (+0x3f4) on kinds 0, 1; third (_al) for Yri / Kne with +0xb20.

**The rest of the frame's control step (`controlStep` 0x1e3e80, one 0x1ed0-byte function in the ROM; the host cuts it in `stepEvents`, `stepAim`, `stepFeet`, `stepWind`, `stepHealth`, `stepFidgets`).** After the links ran: the six Hit events of the model name the bone a hit comes from (`CharacterGround::setHitBone` 0x1f00a0: bn_r_hand, bn_l_hand, bn_r_foot, bn_l_foot, bn_head, bn_pelvis) and set the contact, HitEnd clears it, both only with an active link. StepLeft / StepRight play a step at bn_l_foot / bn_r_foot (bn_base when the model has none): `playLadderStep` 0x2a2100 on ladder motions, else `playStepSound` 0x2a49c0 on the floor face's `surface` at pace 0 crouched walk, 1 walk, 2 sq_ka / sqw_ka, 3 running. **Grounded is `+0x20 || !+0x10`** of the ground: off the ground, in a motion whose 接地判定 is 1, `m_fallSpeed` grows by 120 / fps a frame into `m_moveStep[2]`. The pelvis takes the floor's normal at rate 0.07 (`setAimDirection`), or for kinds with flags 7 / 8 the line along the object it stands on. Feet: flag 3, `m_stepsOn[0]`, the motion's IK and grounded: a ray 50 up and down under each foot gives a plane (stairs keep up, height only), rate 0.2. Wind: only for poses whose bone names are lower case (`ModelPose::lowerNames`), on outside the boxes of `StageNodeTree::contains` 0x1908d0.

**体力 is stamina (`m_health` +0x594), `stepHealth`.** Running (run, tr_run, w_run, the stairs at a run, sq_ka, sqw_ka) costs one point a frame at 30 Hz; 5% of the sheet's value a second comes back, for kind 0 only while resting (`isRestingMotion`), with gear, or in the alternate sound state; other kinds always. At 0: `m_winded` (the tr_ stance table, Move walks). At 40% it jumps to full. At full, `m_winded` clears and kinds 0, 1 may fidget. Stages 0x19, 0x1b (kind 1) and 0xf, 0x17 (kind 0) drain instead (`isStageDrained` 0x215940, the same test as `getMotionRate`): while the player controls someone else by `m_drainRate` (+0x5f0, a script property) of that someone a frame, and when it hits 0 `StageScene::altLatch` is set; on the player himself during the timed change by `m_slowFrom` a frame. **Fidgets:** `m_fidgets` (+0x3e0, set by `resetActionState`, property setter 0x212530), no gear, not the player seen through a Kind10 view of himself, standing or sq_s with the head free for `m_idleDelay` (10..20 s): idle0 / idle1 by a coin, sq_idle0 crouched; a character with no health on the sheet fidgets only as kind 0xf, idle0.

**Step sounds:** table 0x2fc3b0 (`src/game/stepsounds.inc`, generated from the ELF): 44 rows by surface, 32 names: 0 the surface's own, 1..5 PC_GNDnn_H/W/S/R/L, 6..10 NPC_, then three each for SBT, YBZ, YKO, YOT, RYO, TKS, 29 UMI, 30 WALLHIT, 31 POP (`getSurfaceSound` 0x2cf0b0). Families with three columns fold the pace: 4 -> 2, 3 and 2 -> 1, else 0. The ROM caches the entries in bss 0x599860; the host looks them up each step.

**`MotionProbe::findEdge` 0x26b960:** faces with flags 0x142, ability `stepLevel` above 0. A second ray along the face's normal finds the edge; `findFloor` 0x26c180 looks 205 up, then 1000 down, one `radius2` past it; `drop` is signed. Arguments are (face, **object** at WalkLink +0x40, target, drop, floor, point, angles, blocked): +0x40 was not a vector. `m_42c` set: the wider test with a sphere 15 further on, and Kind3 objects (cars) count differently. `getLampMatrix` 0x1e60b0 is identity * R(lean) * R(rotation) * Rz(pi) at bn_head; `getLampTarget` 0x1e6160 (was `getLampAngles`: it answers a point) casts along the spot's radius from the head, or from a Kind10 / 11 view; a target nearer than 200 is pushed to 200. `startWalk` 0x1ead00 / `startWalkAt` 0x1eaea0 take a float rate, the `m_walkKind` of before.

**The sheet's ids, from a dump of the host's `gMotionDefs`:** 0 s, 1 tr_s, 2 s_al, 3 w, 4 tr_w, 5 w_al, 6 w_b, 7 w_r, 8 w_l, 9 run, 0xa tr_run, 0xb t_r, 0xc t_l, 0xd / 0xe the same with upper s_al, 0xf t_q, 0x10 call, 0x11 jack, 0x12 pu_l, 0x13 pu_m, 0x14 idle0, 0x15 idle1, 0x16 w_run, 0x17..0x1a stairs, 0x1b ops, 0x1c wa, 0x1d sq_s, 0x1e tr_sq_s, 0x1f sq_s_al, 0x20..0x23 sqw, _b, _r, _l, 0x24 ka, 0x25 sq_t_r, 0x26 sq_t_l, 0x27 sq_u, 0x28 sq_d, 0x29 sq_call, 0x2a sq_jack, 0x2b sq_idle0, 0x2c sqw_w, 0x2d sq_ka, 0x2e..0x32 up (u_060..u_180, u_sp), 0x33..0x35 falls, 0x36..0x38 landings, 0x39 jp, 0x3a..0x42 ladder, 0x43..0x4c doors, 0x4d..0x58 battle stance (bt_), 0x59..0x67 melee attacks, 0x68 / 0x69 bt_off, 0x6a..0x79 pistol (gn_), 0x7a..0x88 rifle (rf_), 0x89..0x93 the third gun (mm_), 0x94..0x97 dives, 0x98..0x9c traps, 0x9d th, 0x9e / 0x9f bite, 0xa0..0xa3 bitten, 0xa4..0xb8 damage and death, 0xb9..0xbb car, 0xbc..0xbe approaches, 0xc3.. sp_ specials.

**F_PC01's idle is motion 0, label `s`,** and Move asks for it (above); the sheet's 初期M (+0x40) is the brain's first state, not a motion (below). **The pace:** `moveStep` 0x17a490 gives `advance` the rate +0x5e0 for walks, +0x5e4 for runs, the Door link's +0x24 for doors, times `getMotionRate` 0x20d400 and seconds / 0.0333. +0x5e0..+0x5ec come from the sheet in `initStatusBlock` 0x21cc70 (移動速度, 走り速度, 旋回速度 / 100, 復活時間 / 30); a script may replace them (`setProperty` 0x212530: the same divisions).

**The pad to the wants (read 2026-11-04, batch 124; game/characterinput.cpp).** `CharacterInput` (+0x21c, EE 0x60): +0 active, +4 owner, +8 `CharacterPad` (a GamePad with one slot at 0x3164f8, 0x239d50, owner at +0x178), +0xc the left stick's 32-frame history, +0x10 the look vector, +0x24.. the scope. `CharacterPad::update` blanks by the scene: all buttons and the right stick under an open menu, everything under a game over, all buttons under an event, the Use button while `getInputFocus` is someone else, and with the party running everything for the player unless `UiShowFlags` byte 9 is set. The stick's angle (`m_angle[3]`, 0 is up, positive left): within 10 degrees of the side in front or 30 behind it turns on the spot (`t_l`, `t_r`) or with L1 strafes; else it walks, runs from length 0.7, or backs. R1 (`ready`) widens those bands to 80 degrees, 45 with L1. Down flicked and let go in 0.12 s is the quick turn. The default buttons by `ButtonMap`: action 0 Cross Use, 1 Circle squat, 3 Square lamp, 4 R1 ready, 5 R2 zoom, 6 L1 strafe, 7 L2 alternate action (for a scenario's lead, sight-jack: `StageParty::start` / `stop`, "camjack"). `UiShowFlags` holds the script's switches for these. **`PlayerMotion::scopeOn` (+0xbd4) is the scope** (camview_snipe): the left stick then moves the aim with 狙撃補間率 and 狙撃ブレ率 of the sheet. `getActionState` 9 is at the wheel: `m_gear` is a car (CAR_HORN, CAR_EG_ST; `driving` the sheet's 運転スキル), motions 0xb9 / 0xba; R1 on, R2 back, by pressure.

**The character's ground (`game/characterground.cpp`, 0x1eff00..0x1f4bb0, vtable 0x315ee0, EE 0x1c0).** A `CollisionOwner` with six kinds of body, by category: 2 `volume` (bucket 2, spheres at bones, what it is hit on, the main camera keeps out of its box), 4 `blowBodies` (bucket 2, capsules from a bone's last position to its position, radius 50), 0x40 `meleeBody` (bucket 1, segment against the map), 8 `nearBody` (bucket 0, one sphere, only with `flag3ba`), 0x10 `pushBody` (bucket 0, character against character), 0x20 `wallBody` (bucket 1, spheres against the map, mask 0xe8021f3, with 0x20000000 for kinds 5, 0xc, 0xd). The bone tables are per family 0..9 (the sheet's kind and the body size); every offset in them is zero. **A frame:** `moveStep` ends in `beginFrame`; `collide` syncs the bodies, `meetWall` pushes out along the normal, at most 12 a frame, and along a corner when a second mesh's face nearly faces the first; `collisionHook3` of the wall body sweeps the way walked at 50 above the feet, then `checkFloor`; `endFrame` looks for the floor itself when the wall body is off, and keeps the position. **Floor:** a ray from 100 above the feet, faces 0x42400c, not 0x40000; a landed character with 接地判定 1 takes it down to 50 below and sticks to it, otherwise 10 below. **Character against character:** `repelLevel` from the motion, the player's levels below 2 count one less; equal: half each, or at level 2 `winsStruggle`; the lower yields. **Doors:** `doorsBlock` 0 leaves doors and door faces out (`exceptList` holds them, the last slot the view target). **Cars:** a car's speed and `getHitLevel` throw a character: levels 2, 3, 4 are 1.5 s at 25, 3 s at 50, 5 s at 75, easing by 0.8 a frame.

## Around the scripts: seeing, hearing, being seen (game/stageplayer.cpp, 2026-11-05)

`StageScene::runStageScripts` calls `StageCharacter::beforeScripts` 0x21c7c0 on every active character before the scripts and `afterScripts` 0x21c310 after them and the brains. Before: `stepStatus` 0x21c3d0 (a0 = this + 0x590), `ageSightTable` 0x217a80 (a0 = this + 0x468), `updateVisibility` 0x20d8c0, then, when the mission has thrown items, which it sees; last it resets the sight caches +0x7a4..+0x7b4 and +0x7a0. After: door bookkeeping (+0x618 `m_doorInUse`, the character's door while it is the door's user, and +0x614 `m_lastDoorMode`, that door's mode then; read by `doorJustOpened` 0x21c290 and `doorJustUnlocked` 0x21c2d0, a0 = this + 0x590, the only callers natives 0x8c / 0x8a: the door became Open / stopped being Locked since the last frame's scripts; +0x614 is not cleared when there is no door, the null +0x618 guards it), clears the frame's bytes, and **a brain state of 0 takes the character out** (shown off, +0x18, the ground's enabled byte, children off).

**0x20d8c0 is not a move, whatever the roadmap called it.** It gives +0x7b8, how visible the character is: `1 - clamp(lamp * posture * area, 0, 1)`. `posture` is `moveScale[]`, concealment by motion (0 stand 1.1, 1 walk 1.0, 2 run 0.8, 3 crouch 1.25, 4 crouch-walk 1.15, 5 crouch-run 0.9: `isCrouchMotion` / `isCrouchRunMotion` / `isRunMotion` / `isMovingMotion`). `lamp` is 0.25 with the lamp on. `area` is the dark of the place: the range of the `VISIBILITY_MASK` / `_01` / `_02` / `_03` volume it stands in (name character 11 'M' is a mask, 12 the level), else the mission's default; a Hard light on it (`m_shadowLight`, mode 1) answers 1 at once, and for the player and the partner the light's weight lessens it (+0x3d4), or a YAM_ light raises it. +0x7bc keeps the area's value; a native near 0x1adaa0 reads it.

**`canSee` 0x210940 is the sight test:** `isPointInView` 0x210b30 on a character's `bn_head` with scale = its +0x7b8 (a seer with its lamp on: double, at least 0.5), anything else's position at scale 1. `isPointInView`: from `getLampMatrix`, out to m_sight, `scale * m_sight` nearer, inside viewAngle (the whole cone: the ROM doubles the angle before comparing) or a half circle within 150, with a clear ray (mask 0x1e42600f, flags 0x800). Class 8 bodies ignore the scale.

**The noises: `SceneNoiseList`, scene +0xe0** (the port called it SceneUiState until this run; nothing of it is UI). Two lists of 0x20 entries {source, loudness, kind}: `addNoise` and `addSourceNoise` write the back one, `pollSoundSources` (start of the frame) flips, `stepStatus` reads the front. A resumable sound (SeEntry +0x1d) takes one of 16 watches and makes a noise while it plays. `stepStatus`: kind 1 reaches everyone, 2 allies, others only non-allies; within the noise's loudness; a ray from the head to it: a wall takes all of the reach, a face with 0x400 half of it unless a second one follows. Kind 0 is a chance, `hearing * (loudness - distance) * 100 / loudness` percent, and the quietest noise heard sets +0x65c and `m_point6e0`; kinds 1, 2, 3 only set bits. The bits go to +0x658, **which is `m_scriptValues[10]`**: the scripts read what a character heard through `getScriptValue`.

**Volumes.** A map box or point that is nothing else is a `BoxVolume` / `SphereVolume` (group 7, their script by name), slot +0x24c `contains(point)`, slot +0x260 `containsObject` (the box's table only). **A box's runner slot +0xfc is `containsObject` (thunk 0x17f130, table 0x3125bc), so native 0x44 on a box asks whether the object is inside: this is how an `AREA_` script hears the player.** A sphere's slot is the default no (table 0x312a7c). Both classes have their own empty sideHook0..3 (box 0x17e940..0x17e910, sphere 0x17f080..0x17f050, slots +0x214..+0x220) and three bases: the main table (vptr +0xf4), a 4-slot base at +0x18 (vptr +0x1c, tables 0x3125a4 / 0x312a64: the side hooks, thunks -0x18) and the script runner at +0x40 (thunks -0x40: dtors, containsObject); the 16 thunks 0x17f090..0x17f150 end the run (batch 237). Stage 1 has three, AREA01 at (6959, 6342, 810) downstairs, which ends the stage, AREA_AC10 and AREA11 upstairs, and no `VISIBILITY_`, so its darkness is the default 0.5 (measured: F_PC01 0.525 walking, 0.500 standing).

## The sight-jack (game/stageparty.cpp 0x1fe6b0..0x204550, 2026-11-06, batch 156, `partial`)

**The object's template instances (0x206890..0x207108, batch 196, 2026-11-07).** `BearingVector`, 12-byte elements, capacity +0 / count +4 / data +8: insert 0x206890, reserve 0x206d60, clear 0x206f70, dtor 0x206f80. An insert doubles capacity from 1 until it fits, and a value inside the vector is handled: one in the part that moves is read at its new place. `RippleLists`: dtor 0x206f00, clear 0x206ff0 (last element first). `RippleWaves`: dtor 0x207090, clear 0x207100; they are emitted here because ~SightJack inlines ~RippleSheet (rows at +0xc, then columns at +0), and the sheet's ctor uses the same 0x207090 for its temporaries. `CharacterPad::pressedBind`, 0x2067d0, is an inline of the pad, emitted here for `keep` and `recall`. Host: std::vector, `bearingvectorcmp.py`.

`StageParty` (`gStageScene->party`) is one word, a pointer to the record of 0x148 bytes, `SightJack` here; all its methods forward (most are jumps, so the decompiler shows the record's code under StageParty's names: `restore` 0x1fe6c0 is 0x1fed30, `setVisionTarget` 0x1fe7a0 is 0x1fef50, `start` 0x2039b0, `stop` 0x203840, `update` 0x203240, `draw` 0x2024c0). Every name is the port's, from the sounds JAK_IN, JAK_MODE, JAK_NOISE, JAK_TUNING and the code.

**The pad.** `CharacterInput::stepAlt` toggles it on PadAction_Alt (L2). The left stick tunes: `tune` 0x201080 gathers every character `canView` lets through as {atan2 from the viewer, distance, object}, `pickBearing` 0x1fff00 takes the least off the stick's line (stick angle plus the viewer's facing, row 0 of its matrix) within `m_angle` (+8, 20 degrees by default, native 0x13d; reach `m_range` +4, 10000, native 0x13c). The four face buttons bind: `CharacterPad::pressedBind` 0x2067d0 gives 0 Triangle, 1 Cross, 2 Circle, 3 Square, 4 none, by the action map's order 2, 0, 1, 3; `keep` stores the object in `m_binds[bind]` (+0x1c), `recall` calls it back. The shoulder buttons (actions 4, 5, 6: `anyShoulderHeld` 0x200400) go through a hold state +0xbc: a tap under 10 frames steps through foes, `keepNearestFoe` 0x200a50 on bit 4, `stepAlertedFoe` 0x200580 on bits 8 and 2 (foes with brain state 2..5).

**Modes +0xb8:** 1 tuned (+0x14), 2 nobody, 3 kept (+0x18) with the stick pushed, 4 kept, 5 kept and lost with the stick pushed. `viewed` (inline everywhere, also 0x1fea50) answers by them. `aimAt` 0x200150 puts the camjack on the eyes (`PlayerMotion::getEyeMatrix`), or the object's matrix for a non-character; a kind 0xf character (a flyer) eases the camera's rotation instead, 60 units above its place. The fill +0x3c (`JackFill` 0x2082e0, ScreenFill plus level +0x24, colour +0x28) is the noise: 1 nobody, 0 clear, tuning off/m_angle. Above 0.5 JAK_NOISE, under 0.3 JAK_TUNING.

**canView 0x200d10:** in reach; not a character: yes. Not the viewer; brain not 0 (slot +0x278) unless it is the vision; sheet kind not 3 or 7 (Srt, Yrt); not m_bfc in motions 0x42, 0x94, 0x95; the vision only for the player with `canSeeVisions` 0x215480 (stage 0x12 sheet kind 1, stages 0x15 and 7 kind 0).

**Other users of the record:** `startGameOverView` (+0x38 on who: aims and nothing else), `warnAttacker` (+0x2c for 0.3 s: pushes the camera if the jack is off, plays PETITJACK on the keep list), and a vision (+0xe4 with the two lists +0xe8, +0x110, the sound KAKOSI): `beginVision` 0x1fede0 hides the scene, shows the first list, hides the second, shows the vision's object, sets its brain to 1. `endVision` is inline five times.

**The camera** is the kind 11 class on vtable 0x311ac0 over the kind 8 class 0x311d30 (m_linked +0x300), EE 0x340, scrZ 450, 512 by 512, far 1600. Its slots +0x25c and +0x260 are the empty defaults 0x1775e0, 0x178020: `aimAt` writes the matrix itself. +0x24c 0x178030 is the base's render snap, and a flyer hands over its rotation.

**The draw 0x2024c0 is a gate over passes:** a game over or warning (film, flecks, frame 0x2cfb20, lists 9, 8, 0xc, markers 0x205f50), a vision's fade (two tinted grains), off (0x201380), a vision (0x1ffcd0, 0x1ff010), and by the target: nobody or alt 0x201a40 `drawNobody`, a corpse 0x2016d0 `drawCorpse`, a live one 0x201bc0 `drawLive`. Written: the gate, those three, `JackFill::draw` 0x2081b0 (the grain in the fill's colour, then in black, alpha `level * 108 + 12`). Not written: stage 0x1b's boss film.

**The passes over the frame (written 2026-11-06, batch 158).** List 8, under the 2D: `gsFrameScale` 0x2cfb20 multiplies the frame by level / 128, +0x48 = 0xfa so 1.95, flat sprites with ALPHA_1 0xa9 (0x69 past 0xff); this is why the jack's view is brighter than the game. `gsFrameContrast` 0x2cf880 is ALPHA_1 0x41, (Cd - Cs) * As + Cd, the character's tint: partner 0x1e0a1e14, taken over 0x1e004050, brain state 5 0x1e505000, states 2..4 0x16503200. `gsFrameAdd` 0x2058e0 is ALPHA_1 0x48, for a viewer with `m_altAction` set. `jackMarker` 0x205f50 projects bn_head (plus `gMotionModels[family].b`) through the view's +0x280: two white lines across the frame, four rays of the colour, within 1000 a glow; viewer 0x80e03050, partner 0x8030e050, foe 0x805030e0. `drawBindFlash` 0x1ffcd0 lights the top, right, bottom or left half for binds 0..3. Stages with their own view: partner on 6, 0x18 F_E24_SNPC1 (table 0x305f80); 0x15 F_E21_SNPC1, 0x18 F_E24_SNPC2, 0x1b F_SNPC2 (0x306380); 0x1b F_E27_BOSS (0x2d10f0); a vision 0x305b80.

**The palette views (written 2026-11-06, batch 159).** `SightJack::drawPaletteView` 0x1ff010 replaces the frame, no blend, with a gradient map of it: `gsShrinkFrame` 0x1ff460 halves it bilinear (a2, 1 at all three calls, is how many times), `gsLuminance` 0x2b6b90 sums (R * 0x26 >> 7) + (G * 0x4b >> 7) + (B * 0xe >> 7) in the green of a work buffer, 0x2b7f90 moves that byte to the top byte through a PSMCT16 view (FBMSK 0x3fff leaves bits 8..15 of each halfword), `gsUploadClut256` 0x1ffb30 sends the table, and PSMT8H sprites with CLD 1 stretch the result back, bilinear. White is index 251. The tables are in CSM1 order, each entry's top byte its true index, which proves the order: 0x305b80 **sepia** (0x331c00 to 0xf8d9b4; an earlier note called it blue), 0x305f80 grey, 0x306380 negative (white, pale blue 0xa0a0c0, down to dark red 0x300000). Host: `FramePalettePass`, half the render size, so the softness goes with the picture. **`gsLuminance` is its own object** (game/gsluminance.cpp, 0x2b6b90..0x2b85a0, between menuscene and prgmccard, pinned 2026-11-07, batch 209): `gsLuminance(packet, out, work, src, w, h)` in six packet steps, scratch pages right after `work` (work + w/64 * h/32): `gsLumSetContext` 0x2b71b0 (context 2, no Z, point sampling, COLCLAMP), `gsLumWeighGreen` 0x2b6cc0 (work = src G * 0x4b, modulate), `gsLumShiftPages` 0x2b7b00 (each CT32 page copied to scratch local to local through PSMT4 views 8 pixels apart: R reads as G, B as alpha), `gsLumAddGreen` 0x2b75f0 (work G += scratch G * FIX, FBMSK G only; at 0x26, then 0x80), `gsLumWeighAlpha` 0x2b72b0 (scratch G = 0xe * scratch alpha, Cs * Ad), `gsLumGreenToHigh` 0x2b7f90. The host shader frame_palette.frag uses the same weights and per-channel truncation, so the file has no code on PC.

**The flecks (written 2026-11-06, batch 161): `DemoFleck`, class Vt_00318860, 8 bytes, is a vtable and a 0x1f0 `FleckSet`,** held by the record's +0x44 and the event's `DemoCamera` +0x30c (the same object, `StageParty::getFlecks`). 0x2a11c0 and 0x2a11d0 are thunks onto the set at +4. Set: +0 the texture (init pack item 5), +4 the seed (1), +8 28 large records, +0x158 12 small ones, +0x1e8 and +0x1ec the living counts. A record is 0xc: x, y, cos * size * 16, sin * size * 16, life, age, cell. Each `step` ages every record, frees those at their life and refills free ones, 9 large and 4 small at most a frame. Large: life 4 or 5, one of 3 cells 64 by 64 (tables 0x2fb678, 0x2fb688), half diagonal 112..223 pixels. Small: life 3, one of 4 cells 37 by 32 (0x2fb6a0, 0x2fb6b0), 20..31. Place: 0x7000 plus a random part of 0x2000 + size * 2, both axes, a size in pixels added to 16ths. The quad is a turned square on a strip of 4; the ROM's REGLIST tag sends RGBAQ once, then UV + XYZ3, UV + XYZ3, UV + XYZ2, UV + XYZ2. **The blend takes colour away:** ALPHA_1 0x8000000046 is (0 - Cd) * As + Cd, so the sheet's colour does not count; host `BlendMode::DestinationFade`. Large alpha: level 2.5 times data 0x309d8c (rows for life 4, 5; the ROM's base 0x309d3c is that table less 4 rows, not a table of its own), so 4..7 of 128. Small: 20 of 128. **ROM quirks kept:** the draw steps a copy of the seed for each large fleck, then reads the seed itself, so all have one level in a frame; the float the callers leave in f12 is never read, so `draw` takes none (the earlier `level` argument was wrong). `sinCosPair` 0x135d50 writes sin, cos.

**The film (written 2026-11-06, batch 160): `JackFilm` is 0x60 bytes at the record's +0x54, the event's `DemoCamera` draws the same one, and `StageDaylight` takes a `clone` 0x204250.** +0xc band, +0x10 colour (0, black: an earlier note took the seed for it), +0x14 and +0x18 the brightness at level 0 and 1 (0.3 and 1.1, or 0.95 on the light stages), four corners of 0x10 at +0x1c (progress, step, shown, leaving), +0x5c the seed 0x44444444 of `demoFleckRandNext` 0x2a2050. `JackFilm::draw` 0x204550: each corner, when progress reaches 1, takes a new random frame and a fade of 16..23 draws. The ROM composes at twice a frame's size: `drawCorner` 0x205070 lays shown, then leaving at (1 - progress) over it, into the four corners, the right and bottom ones mirrored (`gsStripSprite` 0x205b80 with x0 > x1). `gsGreenToAlpha` 0x2053a0 makes that a PSMT8H image, and it goes over the whole frame through a table built in the scratchpad at 0x70000000 (`gsUploadClutOrdered` 0x205670), ALPHA_1 0x44, bilinear: clear below centre - band / 2, solid colour past centre + band / 2, between them (index - start) * 0x80 / band, where centre = 255 - (level * (high - low) + low) * 255. The RGBAQ left by the last corner does not reach the last draw: `gsGreenToAlpha` sets 0x80808080. Host: `FilmOverlayPass`, the table as three lines of a shader.

**How a jack starts.** A scenario's lead: `CharacterInput::stepAlt` calls `StageParty::start` straight. Everyone else: L2 sets `m_wants.altAction`, `JackCondition` asks for the stance `MotionStance_Jack` (jack, sq_jack), whose link `JackLink` (0x27c370, vtable 0x317bc0, key "Jack", its one condition Jack, which asks out to Idle on the next L2) starts the party in `onRequest` and stops it in `onCancel`; `onRun` keeps asking while the character is the scene's player. All behind `UiShowFlags` byte 9 for a controlled character.

**ScreenFill (0x1518a0, 0x151de0..0x151f60; game/demoevent.cpp beside StageFade).** The grain: the init pack's fill image, in tiles of 64 pixels over the rect, a textured TRISTRIP each, uv from `0x1010 + rand & 0x3ff` (1/16 texel, CLAMP_1 repeat), turned by a random unit vector and enlarged by up to 1.8. +8 texture, +0xc colour, +0x10 x, +0x14 y, +0x18 w, +0x1c h, +0x20 has an image. The scene's own (+0xa0, 0x04808080, alpha 4) is laid over every frame; the party's (`JackFill`, 0x745759) is the static.

**`StageDaylight`, what the code called `SceneModelHolder` (0x2cfe20..0x2d0a10, batch 162, 2026-11-06).** There is no model in it. `StageScene` +0x12c, built in the stage setup with the node tree and the stage number. Stages 3, 0x11, 0x12, 0x14, 0x16 take row 0 of the tables 0x30f930 (gradient, 0x20 bytes) and 0x30f990 (film, 0x18 bytes), 0x19 and 0x5d row 1, 0x1b row 2; every other stage is off (+0 clear). Each frame it asks `StageNodeTree::contains` (the BoxGrid of the stage's `.bbx`) for the camera's `getMatrix` position (vtable +0x14). A change of answer zeroes both timers. **Outside the boxes** the gradient rises: no alpha to `top` / `bottom` by `peakAt`, on to the settled colours by `settledAt`, then held, added over the frame (ALPHA_1 0x48, table 0x2fd9b0) with Gouraud from the frame's top edge to its bottom. **Inside** the settled colours lose their alpha over `fadeSeconds` and a clone of the jack's film (band 0x100, levels 0..1) goes from clear to 0.3 by 0.6 s and to -0.5 by 2 s, band 700 or 400 from its first draw on; going out, what is left of it runs on to clear over 2 s. `setPlace` and a change of camera put the state in with the ramps already over, so only walking through a boundary shows anything. The sight-jack calls it when it moves the view. "Daylight" is a reading of the colours, unproven: on stage 3, the Yamijima Gold Dredge, the camera is inside at the start and the whole of the first room.

**Which stages let the player jack (scan of work/scripts, 2026-11-06).** `Prop_ShowFlag` sub 5 is `UiShowFlags` byte 9, `Control_Alt`; the set stores "value != 1", so a script writing 1 locks the jack and 0 frees it, and the ctor leaves it free. Scripts carrying E-numbered entries: **only ever lock** E01, E04, E05 (E04: `Root` word 568, on its straight path as far as a linear read shows, right after the `Prop 0x12` store); **lock and free** E00, E06, E07, E08, E12, E15, E16, E22, E25, E26, E28, E29; **never touch it** E02, E09, E10, E11, E13, E14, E17..E21, E23, E24, E27. So node 4 not jacking is the script's, not a bug. Stage 2 is the test: New Game forced to stage 2, L2 works with no forcing.

## The brain (game/characterbrain.cpp, 2026-11-05)

**The class.** `CharacterBrain` (StageCharacter +0x220, EE 0x9b0, vtable 0x315b10, vptr +0xb4) **is a script runner**: base `ScriptProxy` (vtable 0x315690, code 0x1a68b0..0x1a7a20, every command forwards to the object at slot +0x1b8 or the target at +0x1bc, both the character's runner here). +0xb8 is a second runner, `BrainSubRunner` (vtable 0x315cf0), parent the brain, forwarding to it: the script that runs beside a walk (commands 0x43..0x52). +0 of the brain, the runner's parent, is the character's runner, so a brain script shares the character's tables and locals. +0x174 enabled, +0x178 owner, +0x17c state, +0x180 route, +0x184 `BrainWait`.

**Eight slots, +0x220, EE 0xf0 each (`BrainFrame`: ScriptFrame + look + overlay id), +0x210 the running one, +0x214 used bytes.** A state owns an even slot and the odd one above it: state 1 slot 0 / 1, states 2..4 slot 4 / 5, state 5 slot 6 / 7, state 6 slot 2 / 3; state 0 none; state 7 suspends (0x1dca60: saves, runs nothing, `revive` comes back through +0x9a0 / +0x9a4). Odd slots are overlays, `startOverlay` 0x1dc5f0 (natives at 0x1b7ab8, 0x1b7bbc). Slots 4 and 6 do not survive a drop to below them; 0, 2 and the overlays do. `switchSlot` 0x1dccb0 is the heart. Who fills them: native 0x2f, `StageCharacter::scriptSetBrainEntry` 0x21b970 (0x21b9b0..0x21b9f0 are its four `setSlotEntry` calls, written): kind 1 slot 0, 6 slot 2, 2 slot 4, 5 slot 6. The entry index goes into `scripts[1]`, the pack's second script file (`work/scripts/F_E04_SB1_0.bin` for stage 4, first entry F_E04_SB1), whose entries carry the character's name, many per name. **A brain with nothing to do is the script's choice, not a gap:** in node 4 `F_E04_SRT1` gets entry 52, whose whole code is "wait 30, restart", so the Srt stands; a run logged no `runCommand` at all and that is right. Commands are words with top byte 2 and id 0x19.. (`200004f`, `2000037`).

**Commands, `runCommand` 0x1d8630, ids 0x19..0x53 (`BrainCommand`):** walk, turn, wait, look, attack, end. It first copies the stack, sp and its own word to +0x18c..+0x1d0: anything that interrupts it (`retryIf`, the ask* family 0x1dac60..0x1dbbd0, what the character calls) puts them back, so the command runs again when the script resumes. `BrainWait` (+0x184, EE 0x10) says how the command ends: route idle, at once, the goal in sight (60 degrees), frames. `lookAt` / `turnTo` / `moveTo` read a flags string: `s` / `?` the point at +0x6e0, `t` the foe, `r` run, `b` the third mode, digits and letters the move's flag bits, `n` `o` `g` which of the named objects (`findNearest` 0x1de050, along the route or straight). A look (+0x1e0, EE 0x30) is kept per slot: the route's look task dies in a switch and `restartLook` starts it again.

**The alarm, state 5:** a foe character (class bits 0xc) biting the player or partner: `counter[1]` of StageResult for the player, and at most once in 5 s unless the sheet's kind is 3 or 7 or the player scopes: option `_10` 2 the party's warning (0x1feab0), 1 the screen's flash (0x263ed0). Mission +0xb0 holds the time.

**`t0` is live at every `lookAt` call (0x1dd8a0).** Ghidra shows four arguments; the fifth, in t0, is 1 at all 13 calls, set by the switch's compare before it or as the wait kind. It is the `force` of the look.

**`CharacterRoute` (2026-11-05, game/characterroute.cpp 0x21efa0..0x22fc50, batches 152, 155, `verify`):** one file, the classes run in vtable order 0x3160a0..0x316498. Three slots: action (+0xc, `RouteAction`: slot +0xc priority, +0x10 update), move (+0x10, `RouteMove`: +0xc update, +0x10 getGoal, +0x14 getTarget, +0x18 pathsChanged), look (+0x14, `RouteLook`: +8 update, deleted with no destructor). A task answers 0 when it is done. Everything it does goes through `m_wants`, as the pad: `moveKind`, `moveYaw` clamped to `getTurnRate`, the use* bytes; it reads `m_allows` to know what the frame permits. A request sets +0x1c, and the next `update` only clears it. `stop` and every new action ask `actionYields(level)`: no action, priority below 1, or below level. `+0x20` is how long wants.ready holds (`holdReady`). `+0x30` the last move mode (1 walks, 2 runs, 3 backs up with no path). `+0x40` where a walk stopped short, w 666 before that. The walk (`RouteWalk`, 0x316220, EE 0x60, `update` 0x223080) asks `RouteFilter` for a path every 10 s or when the target changes floor area, steers at the next node and hands anything in the way to a `RouteStep` (+0x54): `RouteTurn` over 60 or 80 degrees, `RouteLadderStep`, `RouteFollowNode` at a junction, `RouteDoorStep` to shut a door behind it, `RouteElevator`, `RouteRoll`, and four detours: trap, door leaf, wall, character. The flags are `RouteMoveFlag`, the letters of `BrainWait::moveTo`. **`RouteFilter` is a 4-byte handle on a 0x60 record, not a list of paths:** +0 length, +4 and +0x28 the area and node arrays, +0x34 the cursor, +0x38 the junction, +0x5c the owner, `FUN_002414c0` the area search, `FUN_002407f0` the node search; none of it is written. `kDrops` 0x2fa820 is how deep a ledge each step level drops: -5, -75, -105, -150, compared with `WalkLink::edgeDrop`. A character on the stage kind `CharacterKind_NoShadow` is held at z 880 while it moves, its look range is about a quarter turn: it flies on its side. `CharacterKind_A` swaps weapons at 200 units from its target.

**The actions of `CharacterRoute` (2026-11-06, 0x2251f0..0x229ec0, the commands 0x22c0f0..0x22d810):** all of priority 0 but `RouteHoldAction` and `RouteNumbAction` (2). A new action is built before the old one is deleted, so an old task's destructor can end the move its successor just started: the ROM's order, kept. `RouteFleeAction` (vtable 0x316340, slots +0x14 and +0x18 empty there) and `RouteBackAction` (0x3163a0, the same) are real bases: one ends the move in its destructor, the other the look. The light is `m_shadowLight` when it is not a YAM_ light (`shiningLight`): `RouteFleeLightAction` aims at a point 30 along the light's y axis and turns 95 degrees off it, walking for a Yri or Kne, running for the rest. `RouteHuntAction` (`attack` with range 0), in the order it tries: Srt and Yrt leave a light within 300 of `bn_head`; a tackle on a running target (`flag3be`, no rifle, under 50); a bite under 10 (`m_6c8` 1, or no weapon and no `flag3c4`); a burst of 0.5 to 1.5 s (`m_6c8` 2, magazine above 0x13); shots from 0.9 of the range, or a blow under the reach (15, +45 Yrt, +45 a Yrt target biting it, +15 Srt, -10 on a sloped floor, not for Yrt); bare hands think every 10 to 30 s of biting the player or partner, 80% with an ally within 500, else 40%. `attack` with a range: a flyer swoops (`RouteSwoopAction`: at the target, 180 degrees, runs out 1000 past the range), others hold 0.8 to 1.2 of it (`RouteStalkAction`). `findWayOut` 0x227f30: of the area's list (rute header +0x34), the stored path whose end lies most against the threat, by the 2D dot product of the two unit vectors, skipping `CharacterRoute::stoppedAt`.

**The path search and the rute file (2026-11-05, game/routefilter.cpp 0x23f290..0x242e80, batch 153, `verify`; the names are the port's):** `RouteFilter` is a 4-byte handle on the 0x60 record `RouteSearch`: `RoutePath` (+0 length, +4 vector of 4-byte legs) with the walked leg +0x10, the leg past the next pair +0x14, two leg pairs +0x18 / +0x20, the vector of nodes +0x28 with the cursor +0x34, junction +0x38, the path set +0x3c, allowed names +0x40, all names +0x4c, `RoutePlayer` +0x58, owner +0x5c. **The rute file as the accessors read it** (core +4 = the file, EE pointers after the fix-up 0x258260; magic tested with `| 0x202020`): +4 nodes (a count, then 0x20-byte records from +8: +0 short crossing, +2 flags, +0x14 position), +0xc gates and +0x10 links (a count, then 0xc: two node pointers and a short), +0x14 chains (a count, then 8: length, pointer to a count + node pointers), +0x1c area-to-area tables of crossings, +0x20 the stored paths, +0x2c the sets of path names (the whole header: next paragraph). A leg is { kind, index }: 1 link, 2 gate, 3 chain, 4 node. **The search:** same area: `areaPath` (a listed area answers 1 with no steps); otherwise, over each crossing of the two areas, `crossingPath` (stored, by the path set) + `wayToCrossing` at each end, the far one reversed; the cheapest wins. `buildNodes` turns legs into nodes; a run of gates goes through the funnel `pullThroughGates` 0x240d40. `advance` drops a node at under 40 units, or when the owner has passed it by the angles; a corner without `RouteNodeFlag_Open` sharper than 30 degrees at a walk or run becomes `junctionNode`. The leg cursor and its pairs are read by nothing outside `advance`: only the node cursor steers the walk.

**The rute file, whole (2026-11-05, game/routeplayer.cpp 0x252840..0x25b8e0, batch 154, `verify`, compared with the ROM by `rutecmp.py`; the names are the port's).** Magic "ord\0" (the fix-up 0x258260 writes "ORD\0"), 15 header words, offsets from the file's start: +4 nodes (count, a word, records of 0x20 from +8: +0 area, +2 flags, +4 a name like "Flag_094_s1s3", +0x14 position), +8 not an offset (the nodes' bytes), +0xc gates (0xc: two nodes, a short), +0x10 links (0xc: three nodes; each direction its own link), +0x14 chains (8: length, count + nodes), +0x18 count + every area number (read by nothing), +0x1c area, area, then count + { crossing, path }, +0x20 stored paths (8: length, count + legs), +0x24 area to chain numbers, +0x28 chain end to chain end to chain numbers (keyed by node pointer, so sorted by file order), +0x2c sets of path names, +0x30 one crossing table per set, +0x34 like +0x24 (FUN_00227f30 reads it, unwritten). Every table is searched by an MSL lower_bound instance, one per table (0x258bc0..0x258f10); 0x25b590 and mslTreeIncrement are shared with other files. `RoutePlayer` (0xc) holds the 0x24 record `RouteFile` (+0 the 0x10 `RouteAreaSearch`, +4 the file, +8 owner, +0xc the sorted areas of the nodes with flag 1, the listed areas, +0x18 the set names, a tree of pointers, so unique by pointer and in file order, not by text). **Host:** RouteFile::buildTables reads the file into host records instead of relocating it; `RouteNode *` still points into the file. **Between two areas** a path is stored per crossing, by set first. **Inside an unlisted area** the character is on chains: `nearestSegment` 0x256db0 takes the nearest segment in 3D (a VU0 macro function, 0x258900), and `areaPath` 0x253420 joins two points: same segment straight, same chain along it, chains sharing an end at that end, else the cheapest of the four stored runs between their ends. `wayToCrossing` 0x257680 goes by the first leg of the area's stored way out: a gate is 0.1 away, a link start with flag 1 is walked to straight, the rest goes along the chains (`wayAlongChains` 0x255330). The Shift-JIS そんな馬鹿な in 0x256b60 prints when a side is none of the three.

**Foes as a script sees them (2026-11-05, game/playermotion.cpp, natives in game/scriptrunner.cpp):** `m_sightTable` +0x468 (0x18 lines of 0xc, then the owner at +0x588: one object in the ROM, methods 0x2179c0..) keeps per character number a weight `seen`, what canSee said this frame and its distance. Scripts add to the weight (0xf3 by number, 0x97 by name, NEAR, ALL; 0x98 / 0x99 to all in or out of view, under a limit; 0xc1 clears), never below 0, and `getBestFoe` is the heaviest. Four picks are cached per frame in `m_foeCache` +0x7a4 (best, nearest seen, nearest seen with weight 0, nearest), set back to 1 ("not asked") by `beforeScripts`. `getEnemyCandidates` 0x20b120: the mission's `byClass[getSides()]`, or every character while `m_foe` +0x450 is set. Items: `seesBurner` / `seesOpenTrap` look over the mission's burners and traps for one free (no parent) and `m_inUse`. A corpse is gone when its count `m_reviveFrames` is over; within 250 of the view target the count is held at 5 unless it lies on a car (`takeCorpse`, native 0x95, sets -1). `m_scriptValues[10]` is the frame's hearing bits (0xe2 any, 0x93 ally, 0xa9 foe).

**The stage's waiting characters (2026-11-05, game/scriptrunner.cpp, game/playermotion.cpp).** A mission keeps 100 spawn points `{position, state, rotation}` at +0x750 (state 0 none, 1 off, 2 kept; `setSpawnPoint` from the map's P_SPW names, natives 0xa0 keep, 0xa1 drop, 0x174 drop all). `missionPickSpawnPoint` lists the kept ones, marks those near the top view and inside it, and answers one out of view: mode 0.. the mode-th nearest (then outward), below 0 at random, -1 none. `missionSpawnAt` gives the point to the first character off stage whose sheet kind is 3 (7 with strong) and whose name is F_SRT or F_YRT: only +0x66c changes. `runStageScripts` then calls `canSpawn` on every character off stage each frame (true with a point, or with the respawn record +0x6a0 whose w 0x218610 sets to 1), and `spawn` takes its script entry by name, the point or the record, `reset`, and turns it on; on stages 0x1a, 0x1d a place 750 over the floor and stage 0x1b always arrive in motion 0xed. The status block sets +0x66c back to -1 and clears the record, so `reset` ends a spawn's request. **Search points:** group 6's objects called P_SRC<group>_<n>. `getNearSearchPoint` keeps at +0x668 the n 0 point of the first group within 50; natives 0xc4 / 0xc5 answer its handle and its highest n, 0xff the rank-th nearest group from a place, 0x100 the point n of the group. **Leaving:** native 0x17e `leaveLoud`, 0x13b `leaveQuiet`, 0x9f `leaveNoticed`, 0x17f `leaveAtOnce`: not when the life state says 6 or 7; in the motions 0x9e, 0x9f the script yields; else `m_lifeOverride` 1 (a kind 3 spawns shiryoin at bn_pelvis, and the cry YRT_VANISH / SRT_VANISH), the last one resets with the brain off. **Reach:** a character's runner slot +0xfc (native 0xe9, 0x44) is 0x20ca80: distance under the blow's reach, or a bullet's when that is shorter.

**The blow (2026-11-05, game/playermotion.cpp, batch 144).** `takeDamage` 0x20eb40 (from, kind, part, amount) answers what the endurance lost. Kind is `getHandClass` 0x210540: 2 at the wheel, 9 bare hands, 3 long weapons of item kind 2..6, 2 of kind 1, 1 the rest; 4 and 7 come from elsewhere (7 the bite, which writes no flinch here). Part: 0 head (three times, unless the lead is hit by a heavy sheet kind), 1 body, 2..5 the limbs (half, each limb losing the whole amount). Above sheet kind 1 and off brain pace 5 every blow is three times and on the body; motion 0x9b triples it too; sheet kinds 3 and 7 take all of it wherever it lands; the lead hitting an ally counts a quarter. Flinch: a driver gives the sheet's whole limit, bare hands the hitter's m_unarmedMsp (spent by the sheet's 素手MSP減少値), kind 4 the limit on the head or the weapon's ammoParam12, kinds 1..3 the sheet's 武器MSP修正値 plus the weapon's param12. Then m_lastHitBy, m_hitThisFrameBy, setEndurance, m_62c; a blow by the player leaves m_68c 10, one by whom the pad controls m_foe. A hit on who the pad controls latches `altLatch`. Stages 0x1b, 0x19 (sheet kind 1), 0x17, 0xf (kind 0) end a running slowdown. StageResult::counter[2] counts the player's hits. **Slots of a character's script runner:** the table at 0x31227c is the third of the block that starts at 0x312240, so a runner's +0xNN is at 0x31227c + NN and an object's +0xNN at 0x312240 + NN; the runner's are thunks (-0x40) into the object's. Reading 0x31240c as the base (from a hit on the object's own slot) names the wrong functions.

**What the last natives say about the classes not written (2026-11-05, game/scriptrunner.cpp).** Runner slot +0x9c answers the object for 6 vtables (0x31132c, 0x3115fc, 0x3180ac, 0x31834c.. through 0x1774a0 -> 0x1722f0, an identity) and logs and answers null for 35: those six are the car family; `StageScene::viewTarget` (+0x100) is the same class in natives 0x154, 0x170, 0x19e, 0x1f3 and in `rideCar`, so the field is the car the view stands in, +0x3c0 its driver, +0x39c mode, +0x3a0, +0x394, +0x3b9 unread. **The elevator** has floors B1 and B3 and scripts number them 1 and 3: pose +0x408 bit 0x10 is set at B1, clear at B3, low nibble the phase; `elevatorMove` 0x294f90 asks the pose for 2 or 4 (state 1, 3: to B3) or 8 or 10 (state 0x11, 0x13: to B1); the special that leaves a floor is named for the other one (`ElevatorBxB3` at B1). **Past sight:** `StageParty` record +0xe4 target, mode word +0xdc 8, sound KAKOSI, lists +0xe8 and +0x110 with counts +0x138, +0x13c, frames +0xe0 (the script counts 60 a second, the record 50: `* 5 / 6`, native 0x20d); the name is a guess from the sound. **The notice level** is a clamp of 5 to sdata 0x31a99c, 0x31a9a0, both 5. Natives 0x1ca and 0x1cb read `self`, not the target: the only two here that do.

**The ripple sheet (2026-11-06, game/ripplesheet.cpp 0x2d10f0..0x2d3700, batch 164, `verify`, compared with the ROM by `ripplecmp.py`; the names are the port's).** What the notes called stage 0x1b's boss film. `SightJack` +0xb4, built on stage 0x1b only (15 by 15 points, one wave a list, speeds 0.3..3, peak 8), drawn last in `drawLive` when the view is on `F_E27_BOSS` and `m_camera`'s matrix has [3][2] above 195. EE 0x30: a vector of 15 wave lists for the sideways push, one for the downward, the spec at +0x18. A wave is `{age, life 0.5..10, speed, phase}`: `sin((phase + age * speed + where) * pi)`, `where` being the point's place over the sheet, 0..2, times a triangle swelling and dying over its life, times `push + pushOverSpeed / speed` (-2.3, 8.67 after `setPeak(8)`). At the end of its life a wave rolls anew. A point's height is `120 / pushOf(speedMax) * (x + y) + 128`. **On the GS:** the frame goes into the other buffer as a TRISTRIP grid of 16 by 16 cells with bent positions and straight UVs, the height as alpha (ALPHA_1 0xa: Cs), and comes back in 64-wide sprites, alpha with it. Then `gsFrameContrast` 0x1e004050 with FBMSK 0xff000000, then with DATE, DATM 1 and ALPHA_1 0x99 two full-frame fills of Cd * Ad: where the height is 0x80 or over the frame brightens by (height / 128) squared. So the crests of two waves meeting glint. A height past 0xff wraps: the ROM shifts it into RGBAQ and the rest falls in Q. **The ROM's slip, kept:** the bottom left corner of the top right cell has its height 0x80 over. The gate byte at 0x31aed8 (gp -0x7998) is read here and written by no code, in BSS: always on. **The host:** `RippleSheet::draw` passes the points to `YamiPan_DrawUiFrameRipple`, and `FrameRipplePass` (rendering/, frame_ripple.frag) does the three steps at once, with the contrast clamped once as the GS does, and leaves the frame's alpha.

## The navigator (game/navimenu.cpp, 2026-11-05)

`menuPlayLoop` runs it between stages. `runNaviFile` 0x295470 loads `2dEU/navi<lang>.d2b` and runs it modal with `currentNaviAnswer` as the layout's arg: the record's node, negated after a clear. `PrgScaleNavi` (EE 0x628) is the map: a 16 by 52 grid of 69 nodes, each column 74 pixels and each row 25. A negative arg makes the ctor jump along the cleared node's link, which is how the cursor lands on the next node by itself. The layout and the widget talk through 34 consecutive variables (`NaviVar`): the layout sets the request, the widget clears it and leaves a result. The route, the nodes walked, lives on the widget and in `SaveExtra` 0x5995e0, which the save keeps. The answer is a node with bits, and `applyNaviAnswer` puts it in the record: `select.stage`, `.variant`, `._10`.

## The VU0 math

0x135990..0x1366c0, game/vu0math.cpp, the last game code before klib.s. Two parts, one hand: the game's own
helpers up to 0x1360d0, then **libvu0 in reverse order**: rotateMatrixY / X / Z 0x1360e0.. are sceVu0RotMatrixY /
X / Z, 0x136380 `_ecossin`, then UnitMatrix, CopyMatrix, TransMatrix, ScaleVector, InterVector, DivVector,
InversMatrix, Normalize, InnerProduct, OuterProduct, a three-row MulMatrix, MulMatrix, ApplyMatrix; 0x136090 is
sceVu0RotTransPers. The identification is by behaviour and size: ApplyMatrix 0x2c, OuterProduct 0x1c and
Normalize 0x3c equal ReiPan's named list (ff3_functions.txt, in forward order there); the rotations here are
longer (0xdc against FF3's 0xa0), so this is a different libvu0 build. Names stay the port's.

- **Two sine series.** The game's (`sinCosVec`, `sinCosPair`) is Taylor, data 0x300e60 and the same four again
  at 0x300e50, on the angle folded to -pi/2..pi/2 for the sine and on pi/2 - |a| for the cosine. libvu0's is
  fitted, data 0x2f3930: it gives the cosine from pi/2 -+ a, the sine is sqrt(1 - c^2) with the sign of the
  angle. So past -pi..pi the three `rotateMatrix*` mirror and both series drift: callers wrap first.
- **Composition.** `applyMatrix(m, v)` sums m's rows weighted by v's lanes, so `multiplyMatrix(a, b)` applies b
  first and `rotateByAngles(out, in, angles)` is R * in: `in` first, then the turn. `getLampMatrix` is
  R(rotation) * R(lamp): the look inside the body.
- **w.** `vecDirection` and `vecNormalize` answer w 0 (vf6 cleared first); `setVecLength`, `scaleVec3` keep in's;
  `addScaledVec3` has b's; `vecCross` leaves vf6's stale w, read by nothing found.
- **Zero.** The VU's 1 / 0 is the largest float, so a zero vector stays zero through all of them: `vuInverse`.
- `wrapAngleVec` tests pi + a and pi - a through the sticky sign flag, status 0x80, over all four lanes.

**The map object (`game/mapobject.cpp`, 0x174a10..0x175700, 0x1758f0..0x1763e0, 0x176480..0x1775b0, pinned 2026-11-06, batch 167).** The file holds `StageMapObject`, `createMapObject`, `MapObjectParts`, the bone map's `StringMap::find` instance 0x177270, `DoorRecord`'s ctor 0x1772f0 (run by 0x310a00 over 0x537d80) and ends in 39 adjustor thunks 0x177340..0x1775a0: -0x18 the side table, -0x40 the runner, -0x200 the `CollisionOwner`. The ctor 0x1765e0 takes the number of collision meshes: 1 everywhere except the class of 0x295360, which passes 5. Script slot +0x9c (`getScriptMapObject`) is 0x1774a0 -> 0x1722f0, `return this`, in six tables, all on this class; a camera answers through 0x1775c0, the unsupported log and null. The car natives 0x198 and 0x19a therefore work on any map object; 0x1ac and 0x1c8 write car fields with no check, and only cars are asked. `getProperty` 0x174b00 is a full copy of `StageObject::getProperty` 0x18a5b0 plus id 0x6b (`Prop_KeyValue`), which on kind 6 reads the pose's float at +0x3d0 + sub * 4 through 0x2b23b0 and 0x2b3640. `breakPart` 0x175900: finds the part by id, turns off face group part + 1, shows switch piece 0 (the broken one), stops the part's effect, and with its flag plays glass%d_1 and BREAK_GLASS, or for a door doaglass%d_1 (the record's value) from the part's bone. The ROM loads 800.0 into f12 for the sound, and nothing on that path reads it. The last part gone clears +0x36; a door then always refreshes its faces with `attachToRoot`. Kind 3 is the car: `getKind` 0x289cc0, `createMapObject` for oGK99_01 and oDC99_01, EE 0x440.

## Particles (game/stageparticle.cpp 0x18c050..0x1908d0, 0x190a80..0x1920c8, 2026-11-06, batch 168, `verify`)

**Files.** A stage's two `particle` files, version 0xf: header 0x10 (+8 count), then flat 0x158-byte records, read in place (`ParticleDef`, game/stageparticle.h). `tools/reverse/ptcdump.py` lists a dump. File B (65 + 22 in stage 1, B the 22) is searched first. The 16 textures start at common01; a definition's +0x130 indexes them, UVs are 1 / 16 texel, a tile sheet by +0x144, +0x148.

**`StageNodeTree` is the particle system** (the old name stands; it owns the effects, the textures, the boxes). `spawn` -> `create` 0x1918d0: the definition; a `StageParticle` (EE 0x280, kind 0x1d); under parent, at pos / rot unless the definition follows the view; `restart`; pushed on the live list. +0x10 names a chain (`m_chain`, spawned with it; every setter walks the chain), +0x30 a splash (`m_splash`, mode 1: it only emits through `emitAt`, and an OnFloor effect hands it its particles on floor surfaces 0x1d, 0x1f), +0x20 an effect at each particle's death.

**A frame.** `StageParticle::update` 0x18e280, one function in the ROM, cut on the host: the effect culled at draw distance * 1.2 unless it has a stop time or follows the view; `substeps` emissions spread between last frame's place and this one (a fast effect leaves no gaps); each emission: delay, pulses (`stepHeld`), then every particle: gravity, attract, drag, move (collides with the map with flag 0x80, ends past `m_rangeSquared`), spin, colour over four keys, growth, life. **A definition with emitDelay 0 does not age its particles.** A free particle restarts up to `burst` a step. Ending has two steps in `StageNodeTree::update`: stopped and undrawn, `m_emitting` goes off and `m_dying` on; next frame it is deleted. **`setRunning(false)` keeps an effect for its holder**: without `m_running` it is never deleted, which is why holders can keep their pointers.

**Draw.** Packet lists 3 (`PacketList_ParticlesFar`: flag 0x80000, view scaled by 1 / 20, between the far models) and 6 (`PacketList_Particles`, over the main models), each sorted by key = depth less index, **ascending: nearest first**. Each particle a textured quad with TEST_1 0x51001 (no depth write), the definition's ALPHA_1, one FOG value for the effect (FGE off with flag 0x200; a subtractive blend scales alpha by it instead). Host: `renaBeginEffects` opens a pass with the camera's screen matrix, quads go in view space through `YamiPan_ModelSetEffect`, `YamiPan_ModelDrawQuad` (ModelRenderer, a pipeline per BlendMode, the fog pushed per draw). `drawCells` 0x18c3b0 (definition +0x7a above 0) only cuts a quad against the GS's affine UVs: one host quad shows the same. The on-frame test takes the display's width.

**In the files of stage 1:** ALPHA_1 low bytes 0x44 alpha, 0x48 additive, 0x42 subtractive, 0x46 DestinationFade, **0x49 (Cd * As + Cd)**, 0x41 (`BLOOD` only). Flag 0x40 (Refract) is used by no definition on the disc, nor is 0x80 (Collides): tools/reverse/ptcall.py, 766 definitions in 30 files, ALPHA_1 0x48 x357, 0x44 x288, 0x49 x67, 0x42 x47, 0x46 x5, 0x41 x2. **Seen:** stage 2's rain.

**The search points' sort and the one-slot walks (game/playermotion.cpp, 2026-11-07, batch 204).** 0x21e120..0x21efa0, after `resetActionState`, is playermotion.cpp's out-of-line tail. `mslSort_SearchPoints` 0x21e120 is a second instance of the effects' MSL sort, over `pickSearchPoint`'s 8-byte {object, distance} pairs by distance, its own counter gp-0x7e10 = 0x31aa60 (from -4, as 0x31a8f0); here the two partitions are out of line: 0x21e490 (below the pivot, the main loop) and 0x21e700 (up to it, the run equal to a pivot nothing is below); median 0x21e350, selection sort 0x21e9b0. All five read the same as game/mslsort.h, so `pickSearchPoint` calls `mslSort` (it had `std::stable_sort`, which orders equal distances differently). After it, six one-slot iterator classes (vtables 0x316040..0x316090, slot 0 `next`, EE {vptr, list +4, slot +8}, `next` answers the slot's object and steps to the next one that passes, null past the end): 0x316040 `FreeObjectWalk` (an inner walk at +4, skips objects with `m_parent`), 0x316050 `ItemWalk` (group 3's active objects, objects +0xa8), 0x316060 `StageObjIterator` (any list's active objects; a second ctor 0x21eca0 over group 7, objects +0xc8, called only by FUN_001c1bf0), 0x316070 `SearchGroupWalk` and 0x316080 `SearchObjectWalk` (group 6, the search points), 0x316090 `OffStageCharWalk` (inactive characters, objects +0xa0). Active is `m_active` +0x34. The host writes every use as a `StageObjArray` loop at the caller.

**The effects' sort (game/mslsort.h, 2026-11-06, batch 186).** `drawEffects` 0x190c70 keeps a far and a near list of {effect, key} pairs, reserved to the count of effects, filled from the last effect down, and sorts the far list first, then the near one, both ascending, both drawn from the front. 0x192150 is MSL's std::sort, not stable: under 21 elements a selection sort 0x1935b0, else a quicksort whose pivot is the median 0x193480 of two samples and the last element. The samples are n/4 + c and 3n/4 + c', where c and c' are a counter 0x31a8f0 (starts -4, wraps 4 to -4) taken % 5, stepped twice per pass and kept between calls, so the same list can come out in another tie order on another frame. A pivot with nothing below it takes the second partition, which skips everything equal to it. Ties are common: every ParticleFlag_NoFarLimit effect has key 0. 0x1920d0..0x193900 is what the object emitted for its containers: 0x1920d0, 0x192660, 0x1927b0, 0x1928f0 a vector of 4-byte elements (other TUs use these too: the instance is shared), 0x192db0, 0x193210, 0x193410, 0x193770 the pairs' vector, 0x1933a0..0x1938f8 the dtors and clears of BoxCells, BoxColumn, BoxList. Only the sort is written; the rest is std::vector.

**The script picks' sorts (game/scriptrunner.cpp, 2026-11-07, batch 220).** scriptrunner.cpp's object emitted two more instances of the same template, right after the file's last function, 0x29fdb0..0x2a11b8 (pinned there): `mslSort_SpawnPoints` 0x29fdb0 over `missionPickSpawnPoint`'s 12-byte records {index, distance, inView byte} (median 0x2a0270, partitions 0x2a0a70 / 0x2a0d40, selection 0x2a10f0, counter gp-0x7c1c = 0x31ac54) and `mslSort_PickNamed` 0x2a0040 over `scriptPickNamed`'s 8-byte {object, distance} pairs (median 0x2a0410, partitions 0x2a0550 / 0x2a07c0, selection 0x2a1040, counter 0x31ac50). Both compare the float at +4 with <; both counters start -4 in the ELF. `sortcmp.py --instance picknamed|spawnpoints`: 400 lists each, 0 differ, `--plant` 115 and 90 differ. The host sorted both with std::stable_sort until then: two points at the same distance (a script asking for the nth nearest) now come in the ROM's order.

## Critters (game/stageparticle.cpp 0x190990, 0x2b3860..0x2b5740, 2026-11-06, batch 169, `verify`)

Native 0x20f makes a nest, `StageNodeTree::spawnCritters` 0x190990; the tree's vector at +0x2c owns them, its update and draw run them first. They are sprites that live on the collision faces, not particles. Names are readings: nothing in the ROM names them.

**`CritterNest`** (EE 0x240, vtable 0x319280, kind 0x1e): kind +0x200, most +0x204, range +0x208, delay +0x20c (0 in the ctor, 150 after), texture index +0x214 = 12 for every kind, cell +0x218 in 1 / 16 texel, turned +0x228 (kind 1: the cell lies a quarter turn round). The ctor runs 300 updates, before the nest has its place. A nest takes its frame only within the update's range of the view. Draw: list 5 after the models, TEST_1 0x51001, no fog, depth 0..1000, not state 3.

**`Critter`** (EE 0xb0, vptr at +0xa0): place, normal, ahead, matrix, heading about the normal +0x70, half sizes +0x74, +0x78, surface face +0x7c (null: it falls), state +0x80 (`CritterState`), timer +0x84, turn left +0x88, speed +0x8c, two cells of a walk. Kind 1 is `CritterRat` 0x2b4070 (class 0x319500, 4 by 8); the others `CritterCrawler` 0x2b4950 (0x319510 5 by 5, 0x3194e0 10 by 5). Both turn at 6 degrees a frame, then stand or walk; within 100 units of the player they go Scared for a frame and run at 3 towards the nest, where they hide. A rat stops at a face under 0.7 of its surface and feels 4 down; a crawler stands on whatever it hits, feels 3 down and goes round an edge (2 under the face, 5 back). The two ROM functions repeat their ends; the host has `Critter::endFrame`. `quatBetween` 0x2bb510 is inline there.

Only AREA01 and ALERT01, 02, 04 scripts call 0x20f (37 scripts, tools/reverse/sssall.py).

## The fog around a character (game/stagefog.cpp, 0x2ce0c0..0x2cedb0, 2026-11-07, batches 193-194, `verify`)

Two effects that hang from a character's `bn_head` and change the frame's light set while the view is theirs. They were `SceneBufferSet` and `SceneBlock98`, stubs; the file name is the port's (no source string). `CharacterFog` is the block both share (effect +0, shown +4, owner +8; `attachTo` 0x2cebd0 is called on both). `StageScene` +0x98 is `glowFog`, +0x9c `smokeFog`; `StageMission` +0xb4 is `smokeRange` (native 0x1fa `nativeSetSmokeRange`, also read by `nativeSmokeNear`; stage 17 sets 500, the mission's default is 400).

- **`SmokeFog`** (`hatsu_fog`, EE 0x38): the first I_HATUEN made on a stage builds it, eight fogs, stopped. Once, the active characters take them in list order, skipping kinds 0xe, F_YRT and F_SRT (only eight are used, the list is still walked to its end). Each frame a thrown item that `isSmoking` and `isInSmokeArea` joins the list of up to five. A fog runs while its owner is within `smokeRange` of a listed flare; it is shown for the character the view is on, and then the frame's forced light set is `sceneRenderGroup(2)`. **The ROM's slip, kept:** `show` is not reset between fogs in `updateRange`, so the fogs after the view's are shown too.
- **`GlowFog`** (`koh_fog`, EE 0x14): built by the first character of kind 5 or 0xc, always hung on the player's head, each frame (`updateGlowFog`). Such a character in BrainState 2..5 runs its own `m_glow` (koh, at bn_spine_base) while it has endurance, and counts for the nearest distance to the player. **So until this file `m_glow` never ran.** The fog runs while the view is the player's, no owned view has a target and the nearest is under 500; the light set is then `sceneRenderGroup(1)`.
- **Which stages:** a 22 s sweep of part 1 of stages 1..30 with temporary logs: smoke set on 17 (7 SMOKE_A boxes) and 20 (2 boxes); stage 14 has 2 boxes and no I_HATUEN in those seconds; glow fog on 18, 21, 26, 28, 29. Stage 17's boxes, min .. max: (5406, 7826, -44)..(6438, 8845, 106); (2793, -222, -1039)..(4993, 9778, 0); (1373, 6620, -626)..(3373, 8120, 374); (2201, 4015, -503)..(3201, 4815, 0); (1466, 1035, -503)..(3266, 1835, 297); (6074, 3831, -10)..(6883, 5835, 190); (6852, 1394, 164)..(9457, 2605, 364).

## The sky mist (game/skymist.cpp 0x2ad030..0x2afc20, 2026-11-07, batches 201-202, `verify`)

StageScene +0xa4 (seeded as `MenuProgHolder`, now `SkyMistHolder`; `createSkyMist` 0x19af40 with init pack
item 6, a 128 x 256 TIM2 whose two 128 x 128 halves are the cells, white, the shape in the alpha) holds a
0x1cc0-byte `SkyMist` (ctor 0x2aef00): faint puffs laid flat in a plane above the eye (eye z + height +
a layer 1 lower per puff drawn), each a quad of half-diagonal (size cos, size sin), coloured through four
keys by age (`MistKeys`, ten sets each at 0x30ab00 / 0x30aec0, the ctor writes their inverses into the
ROM data) and faded by its squared distance to the eye over the radius squared. Per stage
(`getStageSelect()->stage`): 0x1b none; 0x16 / 0x17 / 0x19 / 0x5d radius 1000, height 700, list 5
(TEST z GEQUAL), the others radius 2000, height 1000, list 3 after the far models (TEST z ALWAYS). Most
stages drift: 143 puffs (70 on 0x16 / 0x17) and 8 wisps (one every 32 frames) placed at radius..3 radius
in the world and moved along the unit xy direction from the eye to a per-stage point (`mistTarget`) times
3 (near stages) or 1 (-1.8 on 0x1c). 0x19 / 0x5d swirl instead: up to 3 spawns a frame on the radius
circle around the point (0, 0, 0 there), pulled in at 800 / s, slowing x0.97 within 750; their
`invLife` is never set, so their colour stays at key 0. The ROM's GS packet (a REGLIST fan per puff,
projected on the scratchpad with camera +0x280) is drawn on the host as view-space quads of the effect
pass (`renaBeginEffects` with camera +0x200). Its tests are kept: alpha below 2, any corner past w 10000,
a corner off the GS window (x or y not in (0, 4095]) before any near one, every point of a cut puff: no
puff; corners nearer than w 500 are cut there (0x2ae200 and three cases, one polygon cut on the host).
**`mistWrap` 0x2af6b0 is broken in the ROM:** it divides 0.5 by v, not by the period, so an offset past
the radius always comes back as v + 2 radius; kept as written. **Seen (stage 7, a temporary harness,
removed):** 116..123 puffs and 3..4 wisps alive; about 0.4 puffs a frame pass the tests in the
default view (eye 8414, 2633, 289), and the frames with and without the mist (and with the colour forced
bright) look the same: the puffs that pass lie above the frame.

**The frame filter (2026-11-07, game/framefilter.cpp 0x2849c0..0x286790, batch 207, `verify`; the
names are the port's, FS2's own gs* prefix kept).** `gsFrameFilter(packet, env, w, h, block, passes,
arg)` is what `StageScene::drawViewFilter` (camview / camjack, by who the camera follows: `passes` 3 /
`arg` 1 for the player on stage 6 and the partner on stage 12, 1 / 1 for F_E24_SNPC2 / F_E21_SNPC1) and
`StageScene::draw` (event d0101 while the demo camera's cut is below 2: 1 / 5) call over the 512 x 512
frame, `block` GsVram area 4 >> 5. It halves the picture `passes` times (`gsFrameHalve` 0x286070,
bilinear by TEX1_1 0x60, UV from texel 1: each pixel the mean of four), ping-ponging between two scratch
buffers (`block`, and `block` + (w/2) / `gsPageWidth` 0x2866f0 pages, which stands in for the frame), and
stops early after a size of 2 mod 4. After each half, when the debug byte 0x31ac10 (gp-0x7c60, 1, never
written) is set and the size is 256 or less, `arg` times a 3-tap box blur along x then y: three
`gsFrameTap` 0x285450 sprites, the whole picture moved by 0, then -1, then +1 pixel, blended by
ALPHA_1 0x44 with alpha 0x80 / 0x40 / 0x2a (replace, half, a third: the mean of three), TEX1_1 0 for
the taps. Then `passes` - 1 doubles back (`gsFrameDouble` 0x2859b0, bilinear), stopping short of half
the frame, each blurred the same way (taps +1 first, then -1) when 0x31ac14 (gp-0x7c5c, 1) is set; the
last double lands in the frame. TEST_1 0x33001 first; CLAMP_1 0, TEST_1 0x50000 and the frame's FRAME_1
back last. Every sprite is cut into page-wide strips (gsPageWidth: 128 for PSMT8 / PSMT4, 64 for the
other colour and Z formats). **Host:** each sprite is a step of `YamiPan_DrawUiFrameFilterStep` after
`YamiPan_DrawUiFrameFilterBegin` (yamipan_drawui.h), drawn by `FrameFilterPass`
(src/yamipan/rendering/frame_filter_pass.cpp, shader frame_filter.frag): the game's blocks become
scratch textures at the render size, sizes and shifts are in the 512 frame's pixels scaled to it, so the
softness goes with the picture at any size or aspect. **Seen:** d0101's ceiling lamp, filter on and off
in the same New Game run (progress/20261107-d0101-frame-filter-on.png / -off.png): the planks and the
lamp's ring go soft, the mean brightness is unchanged.

## The pause menu and the map menu (game/pausemenu.cpp, 0x20acb0..0x20b120, 2026-11-07)

Start and Select in a stage (`StageScene::wantsPauseMenu` / `wantsMapMenu`) make the stage loop pause
the sound and call `pauseMenuStart` / `mapMenuStart`: each clears its answer (sbss 0x31ae30 /
0x31ae34) to -1 and starts a thread in ThreadSlot_Dialog (`pauseMenuThread` 0x20ad20, `mapMenuThread`
0x20b010); the loop polls `pauseMenuResult` / `mapMenuResult` each frame under `drawPaused`. Both show
the BG movie screen (opaque red, `overlayShowScreen`) and release it with the answer.

- **Pause** (`runPauseMenu` 0x20ad70): the stage screen `2dEU/s<stage>m<variant + 1>[b]<lang>.d2b`
  (`stageScreenPath`; alt, the "b" layout of stages 22 / 27, when the player's character sheet
  `StageObjRecord::kind`, ctype, is non-zero; the ROM's inline lookup answers no record for a bad
  index), run with `runLayoutFile(path, lastAnswer, false, ...)` so the cursor comes back where it
  was. Answers 0 resume, 1 restart, 2 continue, 3 quit (the BG screen stays up), else a screen and the
  layout again: 4 the navigator (exitKind reset to -1 first; Leave back to the menu, Help page 8, a
  pick: `applyNaviAnswer`, PauseMenu_Navi), 5 the archive (help page 9 while it answers 1 or 3; the
  save change is not saved from here), 6 `runStageScreenH`, 7 help page 0, 8 options, 9 help page 7.
- **Map** (`mapMenuThread`): `stageScreenPathMp`'s layout taken from the stage loader
  (`getStageFiles()->loader->getData`, slot 5), `runLayout(data, -1, false, null)` until it answers
  other than 1 (1: help page 7); the BG screen is released twice, the answer is 0.
- **Pausing needs the player's `m_allows.standing` (+0x3a1)**, set by `AlwaysCondition::onUpdate`
  (0x276468). The port read a separate `StageObject::m_canPause` nobody wrote, so no pause menu ever
  opened; fixed with this file (hazards.md).

**Seen (YAMIPAN_TELEPORT=7, scripted Start / Down / Start / Select, 75 s, no crash):** Akiko Kiyota's
page (portrait, profile, Mission / Item / Stage / Map / Other / Quit, the mission text "Reach the road
to Uryuga Forest with Soji Abe", Down moves to Item with its help line), Start resumes (answer 0 on
2dEU/s07m1EN.d2b), Select shows the map: the orange grid, the player's blue cross, the compass, the
control hints. progress/20261107-pause-menu-stage7.png, -map-menu-stage7.png. **Not seen:** a map
drawing under the grid (none in this run: whether stage 7 part 1 has one is unread), Triangle / Circle
in the menu (Triangle did nothing), the screens 4..9, Quit, Restart, the alt layout.
