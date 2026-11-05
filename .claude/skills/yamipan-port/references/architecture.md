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

**`game/mathutil.cpp` 0x133800..0x1350d0 (TU 42, 2026-11-03, batch 071)**: the game's math
helpers, all free functions: `copyVec4`, `matrixToRotYXZ` 0x133830 (Euler of Rz Rx Ry) /
`matrixToRotZYX` 0x1338f0 (of Rx Ry Rz: inverts sceVu0RotMatrix), `normalizeAngle` 0x1339c0 and its
twin `wrapAngle` 0x134a20 (one turn), `colorToFloat` (/ 255) / `colorToFloatGs` (/ 127.5),
`approachAnglesXZ` 0x133af0, `lerpVec4` 0x133be0, `approachVec3` 0x133c40 (step = rate * squared
distance, capped), `getBounds` 0x133d20, `subAngles` / `addAngles` / `addAngle` (fmodf wrap; 76
callers), and **the game's own Cody & Waite libm**: `asinAcos(x, mode)` 0x1344a0, `atan2Cw(y, x)`
0x1346a0, `atan2Octant(y, x)` 0x134940 (the usual atan2: 66 callers), `sinCos(x, mode)` 0x134a90
(sin 0 / cos 1; 38 callers), tables 0x300b90..0x300e48, errno EDOM through newlib's `__errno`
0x123528. Newlib's float libm 0x116fb8 is linked beside it. The VU0 helpers after the clocks
(0x135990..0x1366c0: `setVecLength` 0x135fe0, `scaleVec4` 0x136490, matrices, distances) are the
next TU; `game/vu0math.h` is their stand-in header.

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
empty on purpose. **Sony's sample TIM2 library** is `rena/tim2.cpp` 0x2e97a8..~0x2e9d90 (four of its nine
written: `Tim2GetPictureHeader`, `Tim2GetMipMapPictureSize`, `Tim2GetImage`, `Tim2GetClut`). Disc
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
The class after it, 0x263b90..0x2640e8, is **`game/stagealert.cpp`** (its own TU): every caller passes
gStageScene +0xf4; ctor 0x264070 (members +0xc / +0x10 / +0x14, +0x14 a 0x3c-byte object from
0x151600(0x700, 0x700, 0x200, 0x200, 0) with 0x151380(obj, 0x80)), deleting dtor 0x264000, the ALERT
/ CAUTION layouts on gStageScene +0x30 (0x263c10 a 4-state machine timed by 0x1353d0 on gStageScene
+0x60, 0x263f10), 0x263ed0 / 0x263f10 call 0x151330(0.01, 0.25, 0.25, obj), 0x263f80 tests
GameOptions +0x10; 0x263b90 is empty (0x287f20).

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
menu is bracketed the same way (inline in the ROM): `KeepList::removeUnkept` on scene +0x30,
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

**`game/stagescene.cpp` 0x1953f0..0x1a68b0 (TU 46, 2026-11-03, batches 079-086, `partial`; the file name and the one-file extent are the port's guesses)**: the **stage scene** (`StageScene` 0x130) and the **stage's files** (`StageFiles` 0x160) in one run with no source-path string: they share the path chooser `StageFiles::dataDir` 0x1a1ca0 and the string-map templates 0x1a4e80..0x1a6790, and the this-adjust thunks 0x1a67c0..0x1a68a0 end it (0x1a68b0 is the dtor of the next run's class, vtable 0x315690). The **scene's member map** (constructor 0x19c180, destructor 0x19bbe0: about 30 member objects, which only a stage or only a sub-scene builds) is the file comment of `src/game/stagescene.h`. Facts written: `getMission` 0x196870 = the scene's **+0xe8 mission state** (0x287900, 0x13d0 bytes) or the nearest parent's (+0x10c, `getParent` 0x196950), recursion unrolled eight levels; **`getPurposeText` 0x195d50 is a member** (formatTextValue passes gStageScene in a0): mission + 4. **The view stack** +0x34 (`StageViewStack`: capacity 8, top -1, entries of 0xc whose +0 is the object) gives StageSound its listener. **`drawPaused`**: a sub-scene only runs its **`DemoStream`** (+0x28: a `SoundStream` subclass, 0x80, unassigned@002b0d50, dir "streamEU/demo/", +0x74..+0x7c 0x80606060 / 0x100 / 0x1bc); a stage calls `StageSound::update(top of the view stack, player->getAltSoundFlag() 0x214f60, sceneFlag)` where sceneFlag = `StagePlayer::isMode1` 0x2152d0 (+0x400 set, +0x3c8 == 1), else with a view target (+0x100, its +0x3c0 character): target is the player and the listener not Kind11, or listener Kind11 and target == `StageParty::getCompanion` 0x1fea50. **`raycast`** walks group 1 (the Root objects, `StageObjectList` +0x98) skipping inactive ones (+0x34), takes those with +0x200 set and +0x37 clear, tests each mesh of +0x20c (count +0x208) with `StageCollision::raycast` 0x233810 (mask, flags, arg reordered) and keeps the face nearest `from` (`vecDistance` 0x135f20). **`StageFiles::load`** sets the loader's directory (`FileLoader` +0x14 is that pointer, +0x18 its copy: `FileLoader::setDir`, inline everywhere; stageLoad's "m_arg = 0" was the same) from `dataDir` (bg/ chara/ motion/ item/ -> ../data/debug/ or ../data/proof/ by the flags +0x124..+0x127, else ../data/), loads the script, then runs **`parseScript` 0x1a20f0 twice** (+0x128 = 1: each keyword only `addFile`s its path; load all; +0x128 = 0: each keyword takes `getItem(findIndex)` into its container). parseScript copies the item, splits it into lines (0x1a3500 on a line list 0x1a5a60 with vtable 0x311210), resets the containers (0x1a3dc0), skips empty and '#' lines, strtok-like 0x129a60 on " \t" (0x303d40) and strcasecmp against the keyword table 0x31a928.. (target 0x1a1b50, 0x1a1820, texture 0x1a1430, model 0x1a0e70, motion 0x1a0ae0, collision ("SSS\x1a" magic, 0x23ae40), particle, 0x1a0950, script (0x1d3a40), sndxv, sndseh 0x1a0580, a language-suffixed item (getInitItemForLanguage), areaclip, ...), all skipped while gStageScene is a sub-scene. **The script language** (read 2026-11-03, batch 081): lines of words split on " \t\r\n", `#` a comment, keywords in table 0x31a928 (target, end, texture, model, motion, mdl, collision, particle, map, script, rute, sndxv, sndseh, font, areaclip, bbx, lpt, sndsth, message), case-insensitive; an unknown keyword fails the load. Blocks: `target NAME` (table 0x31a974: script, camera, character, bg, bg_motion, particle, item, map, sound, effect; a second target before `end` fails) .. `end`, which in the second pass hands the lists the block filled to the target's handler and then empties them all (`reset`). Keywords only list and take files; the targets give them to the game: **script** needs two script files (the first's short +0xa counts `StageScene` +0x44 int slots), a rute (`RoutePlayer`, 0xc, vtable 0x316a78, at +0x4c), a font (from the init pack by language, `getInitItemForLanguage`, whatever path the line names) and at least one texture (the last one listed goes to the scene's printer), bbx optional (`StageNodeTree::setBoxes` 0x190900, a BoxGrid of size 8; a sub-scene's script block does only this); **character** makes a `StageCharacterModel` (0x1c) per model line, two motion lists each, filed under the model's name in the map +0x13c; **effect** needs the texture `siglamp` (`SigLamp` into `StageScene` +0x8c). File fix-ups after load: collision must start "SSS\x1a" and is relocated (0x23ae40), scripts are relocated with their second table (0x1d3a40). `splitLines` is Shift-JIS aware (lead 0x81..0x9f, 0xe0..0xfc). Containers: `StageList<T>` and `StringMap<T>` in `src/game/stagelist.h`, an unbalanced strcmp tree whose nodes are also chained in insertion order; the ROM has one instance per element type, 0x1a4960..0x1a6790. **Keyword handlers and the bg / particle targets** (read 2026-11-03, batch 082): `model PATH [NAME [NAME2]]` loads the model and the .tpk beside it (the path up to its first dot + .tpk; a `.tpk` is a pak of `<name>.tm2`), relocates it (`ModelFile`, magic at gp-0x7b48, `relocate` 0x2e2d20) and makes one Tim2Texture per name in the model's list (+0x3c, count +0x1e) unless the name is taken; `nothing` keeps the model list's place with a null; one name is the model's, of two the first is the alternate. `motion PATH` is a text + the .mpk beside it: each line's last `\\` word is an item of the pak, a `#` line a null entry (`readMotionList` 0x1a1df0). `map PATH` relocates the map (`StageMapFile::relocate` 0x186ee0, six section pointers). `sndseh PATH`: `fileWithSuffix` puts the options' suffix before the extension, the first pass loads the SEDT at once and lists each named bank's `file` (+0x10 + 0x40 * i) and, for a stream bank, `streamFile` (+0x18 in the record). `sndsth Stream_sNN_SE`: streamEU/Stream_sNN_SE<suffix>.sth, whose banks the first pass lists, and ../data/sound/msg/SCNN[_<language>].fsm. `message PATH_<language>` goes to the mission (`StageScene::setMessages` 0x196840, a stage only). A `script/` texture in the second pass is the init pack's item 21 named after the language path. The language inline: 0..3, 5..9 `getLanguageCode`, 4 "Ct", else empty. **The bg target** 0x19f680 walks models with collisions beside them: each node of the model, last first, is a new `ModelInstance` (0x2e5210, vtable 0x31a7f0) and an object named after the node: a stage `createMapObject` 0x1766c0 (the name picks the class: oGK99_01 / oDC99_01, oXXX_dp / _ds with a number, ...) then `setupMapObject` 0x1760c0, a door (kind 2) taking its index in **`gDoorRecords`** bss 0x537d80 (0x87 of 0x38, by name, +0x298); a sub-scene its own lighter classes (`_dp` / `_ds` 0x2c0 vtable 0x314ef0 with the record's +0x34 at +0x280; gates 0x2bbdb0; others 0x2bc040). Then the scene takes the model's name (+0x68), script (+0x38) and areaclip (+0x58, `AreaClip` 0x2dccf0, an 8 x 8 grid); the first lpt goes to +0x5c, its magic "lpt" turned to "LPT" as +8 becomes a pointer. **The particle target** 0x19f140 gives `StageNodeTree` 16 textures in insertion order from common01 on and the first two particle files (kind 0xf at +4, data at +0x10).

**The action lists (`game/scenepairs.cpp` 0x171300..0x171a30, 2026-11-04, batch 117).** `ScenePairTable`, StageScene +0x14, EE 0x2c: five doubly linked lists of 8 bytes (head, count; the ctor's "five pairs"), then a word nothing touches. A node is {box, next, prev}; the box is a 4-byte heap copy of the entry pointer. An entry is the object's side base (+0x18: flag, then vptr). `add` appends. `remove` takes the first hit in each list and only `~StageBullet` and `~StageLight` call it: every other entry stays until the table goes, and `~StageScene` deletes the objects first, which is safe because the table's dtor never reads an entry. Lists: 0 items and bullets, 1 map objects and an event's objects, 2 lights, 3 characters (`StageCharacter::setup`), 4 unused (`ScenePairListId`). The steps call side slots +8, +0xc, +0x10, +0x14 (`sideHook0..3`) for entries whose flag is set: `actionStart`, `actionControl`, `actionMovement`, `actionTerminate`.

**The character, built (`game/stagecharacter.cpp` 0x17b3e0..0x17dcb0 + 0x17dd10..0x17df60, 2026-11-04, batch 119).** `StageCharacter` (vtables 0x312240 / 0x312264 / 0x31227c, EE 0x800) is **also the scene's player**: one class in the ROM, so the host's `StagePlayer` stand-in is gone (`typedef StageCharacter StagePlayer`), which is why `gStageScene->player` and `partner` read real fields now. Build order: ctor (input 0x60 always; brain 0x9b0 only when the scene has a rute file, its +0 the character's ScriptRunner; a 16-byte heap vector at node +0x10; rotation order flag `m_xform.rotationXyz` = 1), then `StageFiles::placeCharacter` sets `m_itemDef`, position and rotation, then `setup`: name, `addCharacter`, `ScenePairList_Characters`, script entry by name, model, the sheet's kind to `applyCharacterModelFlags` and `CharacterHidePart`, `m_loop` = 0, `initAbilities`, `createBlock98`, PlayerMotion 0xc20 and `loadMotions` (the bones exist only from here), `setupBody(true)`, body size from the **pose's** name (`pose->name`, table 0x2f7ba0: _lh_ _th_ 0, _mh_ _ms_ 1, _sh_ _ss_ 2, _ch_ 3, default 1), CharacterGround 0x1c0, the sheet's two item columns (`findAll3`, walked from the last placed, `giveItem` until one takes), `m_parts` ("head" and the first part record), blood and effects, then `reset(0)`. **Effects:** blood name yamiblood for class bit 8, else otblood (kind 0xd) or BLOOD; class bit 8 also spawns yamismoke1 at bn_pelvis, yamismoke2 on stages 0x11..0x15, and for kinds 5 / 0xc koh at bn_spine_base; every character has a splash (bthamon for kind 0xb, else L_SPLASH) and F_HAMON at both feet (kinds 0, 1, 2, 4, 8, 9, 0xa, 0xe) or one on itself (5, 6, 0xc, 0xf). A body with no model data gets shiryotai at bn_head and shiryotai2 at bn_pelvis, bn_r_thigh, bn_r_calf (`setupBody`). `setupBody` also sets `m_lightProxy` = bn_pelvis, a **bone node, not an object** (the field was typed `StageObject *`, and only its `getWorldMatrix` is read), and copies shadow size and offset from `gMotionModels`. **`m_abilities`** (+0x3a4, 0x2c): all 1, three values from the sheet, `driving` 2 / 0 / else 1, stepLevel 0..4 (5 reads 4), then a clear list per kind; only the sheet's columns are understood. **Status** +0x594..+0x5b0 is a copy of the sheet; the ROM clamps each to 0..its own value, which changes nothing. `m_weapons` +0x238 / +0x23c is what the old `m_handNode` and `StagePlayer::weapon` both read. The lamp held is `m_lampItem`; the old `StageMapHolder` was that lamp: its +0x240 the `ItemLamp`, whose first spot's `m_switchedOn` is what the map shows as rMapP. The side hooks are thunks to +0x210.. : 0x17a860 start, 0x17a810 control (input 0x236b90, brain 0x1dd2f0, motion 0x1e3e80), 0x17a490 movement, 0x179d30 terminate: step 5, unwritten.

**The plain map object's frame (`game/mapobject.cpp`, same batch).** Side table 0x3115e4: `sideHook2` 0x1771b0 advances the model by dt/(1/30), calls `updateMatrix` on every node of `boneMap`, clears +0x210 (who touched it, written by the collision hook 0x1753f0, not ported). `sideHook0` 0x1756b0 ends a one-frame dirty: map objects rest with a clean transform, so `setPosition` 0x174aa0, `setRotation` 0x174a40 and `setYaw` 0x174a10 (slots +0x20c, +0x210, +0x214) set dirty and +0x21c when it was clean. Before this run the host object had the base's setters, so a script's move of a map object never rebuilt its matrix. `sideHook1` / `sideHook3` are empty. The door class (0x174940, side table 0x311314) shares `sideHook0` and the three setters and has its own 0x174180, 0x1741c0, 0x174390, all still unwritten.

**The stage's frame (`game/stagescene.cpp`, batch 086; running since 2026-11-04).** `StageScene::update` 0x199400, in the order of its timing marks (each label names the stretch that ends at it): `ScenePairTable::actionStart` (the table's five lists of action objects: slots +8, +0xc, +0x10, +0x14 are start, control, movement, terminate); "script": `runStageScripts` 0x19aba0 for a stage, or a sub-scene's own script, which ends when it stops running and the demo voice has stopped, or on start when `m_skippable`; then the game-over or clear sequence; "action(control)": `stageUpdateCharacterMotions`; "action(movement(&motion))"; "collision": `SceneRecordBuckets::collide`; "action(terminate)"; "calcmatrix(root)": `calcMatrices` over groups 1..8, each active root node `propagate` + slot +0x20, then the model's pose (`ModelPose::solve`, 3, -1, 0) and what hangs from its bones, then group 0, the cameras with `m_viewActive`, slot +0x25c + slot +0x20; a stage's owned objects (+0x30), party, +0x98 and +0x9c; "particle": `StageNodeTree::update` with range `getDrawDistance` * 1.2, `SceneBoxList::update`, `updateRumble`; StageSound (or DemoStream) and the cue subtitle into `ScenePool` slot 0; `SceneRender::update` / `updateLights`; a stage's pause state, and with no event the menu, the item list and the alert; the blackout (`StageMission` +0xda: `SceneDirector` ramps to black over 15 frames and back, then is deleted); `controlRequest` +0x84 (0 nothing, 1 back to the player, else a StageObject to take over, which needs the player's alternate flag, and becomes `controlled` +0x80, followed by the main camera); objects with `m_dead` +0x37 are deleted; "others": `updateVcms` (groups 1, 2, 3, 8), `drawSysTick` (seconds * 300), `frameCount` +0x64. `StageScene::draw` 0x198970: clear colour, `prepareDrawFlags`, the camera's matrices into `drawSysCurrent()` +0xc, the draw distance = camera +0x2cc + +0x2d0, `AreaClip::update`, `drawSysRender` ("rena draw"), `StageNodeTree::draw`, the menu program, then packet lists: 9 view filter (`gsFrameFilter`) and party for a stage, or for a kind 13 view the d0101 filter and `DemoCamera::drawOverlay`; 8 the model holder +0x12c; 9 ScreenFill; owned objects; 10 item list and menu (only with no event); 12 pool text (a stage whose printer has a texture) or subtitles; 11 alert, fade, director, or for a sub-scene the fade and the **letterbox**: black bars y 0..0x40 and 0x1c0..0x200 of the 512-tall frame. **Camera kinds** (`getKind` from the vtables): 7 StageCamera (map cameras, cam_dive), 9 `StageMainCamera` "camera" (0x3c0, vtable 0x311fa0), 10 `ViewCamera` camview / camview_zoom / camview_snipe (0x440, 0x3185b0), 11 the party's camjack (0x340, 0x311ac0), 13 `DemoCamera` camera_demo (0x340, 0x316ed0). The view stack +0x34 stays sorted by priority (main camera 0, demo camera 4); its top has `m_viewActive`. `setupCameras` 0x199f00 builds them, a spare bullet and the point light "flash" (both off), runs the bg script's entries (a sub-scene: all but the last, skipping names ending in m), binds bones, and takes the first two active characters as player and partner when no script did. The **view filter** table 0x31a8f8 gives passes / arg: the player on stage 6 and the partner on stage 12 {3, 1}, F_E24_SNPC2 / F_E21_SNPC1 {1, 1}, else none. `SceneArrayPair` +0x124 holds 3 bytes and one NodeTransform for every object but group 4's; `restoreFlags` walks the same order.

**The stage's script step, sound and list files (`game/stagescene.cpp`, batch 088; written, not yet run).** `runStageScripts` 0x19aba0, a stage with a script and no event (an event only polls: slot +0x10 of the vptr at +0x2c, 3 ends it): `SceneUiState::pollSoundSources`; each character of the mission's list +0x69c (count +0x6a0) steps (`beforeScripts` 0x21c7c0) or, inactive, may spawn (`canSpawn` / `spawn`); `StageMission::tickPending` (thunk 0x286ce0 of 0x286e30); with no game over the runners of the list +0x740 (+0x40 of each object) and the characters' brains; `afterScripts` 0x286c40, which starts the **pending event: mission +0xc8 is its name** (what `isBusy` tests: one byte, not a flag), else lets the fade go on when `getMenuMode` is 1; each character's `afterScripts` 0x21c310. `updateBuffers` / `updateBlock98` follow the object the top camera (kinds 9, 10, 11) links to: eight character blocks (shown while near a listed object within mission +0xb4) and the player's block, off while an owned object has a target or the link is not the player. `applySoundTarget` 0x19d9e0 runs once (`StageSound` +0x308): each SEDT becomes the table, its banks are made (`loadBank`, or `openStreamBank` with the streamFile), then the .sth, streamEU/, the .fsm, and **each StBank's header +0xc is pointed at the loaded file called name**; last `compact`. `applyBgMotionTarget` 0x19f230: a name is MODEL_dr_r (door type 2) / MODEL_dr_t (1), a target of `stageMotionTargetName` 0x175c00 (CAT, CROW, tonbi, oMT04_04, oZS09_04, oDC07_05, oMK21_09, oMT04_99), CAR (kind 3), Seven_Keys (kind 6), else one object by name; each takes the list through slot +0x270. `loadList` 0x1a3a60: init pack items 0x14 (KeepList +0), 7..0xc (`MenuProgSet`, all six or none), 0xd..0x10 and 5 (party), 0x11 item sheet, 0x12 menu, 0x13 pool; then the stage's screen pack, and without a pack loader ../data/listfile/sNNlist.lst, each word a file. Queries: a door's faces carry flag 0x4000000 and id +0x46 = 100 + (type - 1) * 180 + (number - 1) * 4 (`findDoorOfFace` 0x196b20, doors +0x240 / +0x244); `findFloorFace` 0x197280 keeps the highest face below the height (`heightAt` 0x2342c0 for a mesh with +0 clear, else a ray 10000 down), `getFloorAttr` 0x1970f0 its +0x44 from 50 above pos. Mission fields: +0x84 / +0x88 script properties 0x3e / 0x3f, +0x8c / +0x90[6] move scales (a script command sets them in percent), +0xa8 a script value, +0xac start time, +0xb4 the block range, +0xb8 / +0xbc what `saveObjectFlags` 0x1955e0 leaves for `restoreObjectFlags`. `requestSubScene` 0x196060 stores the **name** at scene +4 (so that field is a pointer); d1804 first clears a tree flag on the lamps ENV_PL03..06. `getMissionFrames` 0x196340 is (clock - start) * 30 rounded.

**The script runner's vtable and the object family's shape** (read 2026-11-03, batch 089). `ScriptRunner` (vtable 0x315850, 108 slots, vptr +0xb4) is an interface of **script commands**: the base answers each with a default, `defaultCommand` 0x1cf1e0 (0) or `unsupportedCommand` 0x1cf1b0 (logs the Shift-JIS "unsupported command" 0x304ab0 through the retail-empty 0x1a69a0, answers 0); vtable 0x315690 forwards most commands to a target from its slots +0x1b8 / +0x1bc. Inline defaults are emitted beside each vtable that needs them, so 0x19cf50..0x19d0b0 sit in this TU. **Slot 7 (+0x24) is the class's own name**: null in the base (0x1a7a10), `DemoExecuter` 0x19cf40 in vtable 0x314d30, so that is the real name of what a sub-scene runs (the stand-in was SubSceneObject); other classes' slot 7 is a cheap source of real names. **A StageObject has three vptrs because it has three bases**: +0, +0x18 (vptr +0x1c, four slots: thunks adjust -0x18) and the `ScriptRunner` at +0x40 (vptr +0xf4 = 0x40 + 0xb4; thunks adjust -0x40). The vtable block is contiguous and new virtuals follow its last sub-table, so `getKind`, slot 123 from the vptr at +0, is the entry rtti.py lists as slot 108 of the +0xf4 table: 0x1a45d0 (0x21, SubSceneDoorObject), 0x1a4880 (0x1a, StageMarker), 0x1a4950 (0x24, SubSceneProp). `StageScene::isInView` 0x19b820 is `targetInView` out of line, `playerOrTargetInView` 0x19b900 the scene flag, `viewLinkedObject` 0x19b9f0 the helper the frame also inlines; `stageHideObjects` 0x19c7e0 (reached from the mission's event start 0x288340 and 0x1fede0) takes groups 2..4 off and group 1 off the view. `SceneBufferSet::createFor` 0x2ce620 builds only for an owner named I_HATUEN.

**The map file** (the `map` keyword; `StageFiles::applyMapTarget` 0x19ddf0, batch 085): a 0x24-byte header of six section pointers and six shorts (+0x18..+0x22), the records right after it (`relocate` 0x186ee0 computes the pointers from the counts): **points** 0x2c {name, pos, rot}: `F_` / `player` -> `placeCharacter`, `P_` / `BA_` -> a mark (`P_SPW%02d` goes to `StageMission::setSpawnPoint` +0x750, other names to a `StageMarker`), the rest -> `placeItem`; **cameras** 0x30 {name, pos, aim}: `StageCamera` (450, 512, 512, 1600), looking at pos + aim; **spots** 0x34 {name, pos, rot, cone angle radians, radius}: ENV_CL / ENV_SL (`SpotLight`, once per name) and YAM_CL / YAM_SL (a stage's, off at start); **lights** 0x24 {name, pos, radius}: ENV_PL `PointLight`, else a `SphereVolume` (the ROM's YAM_PL test reads the spot section at the light's index); **boxes** 0x38 {name, pos, size w / d / h, rot}: ENV_CB and ENV_SB to the renderer's grids +0xdfc / +0xe00, ENV_MB_f%02d to `areas[0]` (the floor number `getFloor` answers), ENV_AB to `areas[1]`, a box named after an ENV_ light (8 characters, 10 with 'c' at [8]) to that light's list +0x278, SMOKE_A to `areas[2]`, REVERB_ON to `areas[3]` (`getSoundAttr`), any other a `BoxVolume`; **marks** 0x20 {name, pos}: marks without a rotation. A stage takes every section of every map except a plain ENV map (name[10] == 'd'); a sub-scene takes only an ENV map's lights and another map's boxes. **Items** (`placeItem`): the group map (+0x14c, filled by `applyItemTarget` with {model, motion list}) gives the model, `gItemDefs` the kind: 0xe `StageItemE`, below 7 `StageWeaponItem` (kind 0x15 by getKind then takes its `gWeaponDefs` index, bss 0x576010), 0xf with I_WANA a trap, with I_HATUEN / I_AKIKAN / I_PET / I_NUT a thrown item, else `StageItem`. **Characters** (`placeCharacter`): the last model line of the characters map, F_YRT with a random part ext_eyr01..03, `gStageObjRecords[kind]` by name (F_YRT / F_SRT by `name_Snn`) whose two item names (+0x18, +0x28) are placed first. **The running script** is sbss 0x31ae18 (`ScriptRunner::run` 0x1d27e0 sets it): `StageScene::isBusy` lets an event's own script go on. **Menus** open on pad 0's pressed start / select unless the fade is full, the stage is ending, the mission is busy (+0xc8) or `playerAction` is outside {0, 1, 9, 0x13, 0x14}; `UiShowFlags` [1] / [2] allow them.

**`game/areagrid.cpp` 0x283df0..0x2849c0 (batch 090, 11 functions, all written, `verify`).** `AreaGrid` (EE 0x240): +0 the list of every box, +8 64 cell lists (index x + y * 8), +0x210 / +0x220 the xy-z bounds, +0x230 / +0x234 the cell size, +0x238 a byte the ctor sets to 1 (no reader found). `addBox` only lists; `build` 0x2842f0 grows the bounds from each box's corners, divides them by 8 and enters each box in every cell its bounds touch, so a box added after `build` is in no cell. The grid owns its boxes (the dtor deletes them), so a box also held by a light's list +0x278 must not be in a grid. `contains` 0x284190 answers the **box**, not a flag; `collect` 0x283ff0 the boxes at a point. Cells cover x and y only: z decides in the bounds test and the box test. `AreaBox::contains` 0x284690 tests in box space through `invertRigidMatrix` 0x136500 and `applyMatrix` 0x136690 (the node's slot 3 matrix); `getWorldBounds` 0x284740 takes the eight corners through slot 4. Both slots answer +0xc0 (the transform's world matrix). `seDistanceVolume` 0x283df0 takes and returns unsigned (the u_int to float idiom at the top of the function) and sits here by link position only.

**`game/titlescreen.cpp` 0x231c90..0x231d40 (2026-11-03, 3 functions, all written, `verify`, batch 091; the file name is the port's).** `runTitleScreen` 0x231c90 runs `titleLayoutPath`'s layout through `runLayoutFile` (not cancelable, yield after) with arg 0 with the intro and 1 without, and passes on answers 1 and 2; anything else is 0 (`TitleAnswer`). The layout 2dEU/title<lang>.d2b holds, seen on screen: the "Sony Computer Entertainment Europe presents" card, then Load Game / New Game with a selection colour, "Select Menu Item", the copyright line, which slide in from the side, and a movie widget (presumably the background, black while `sdk/libmpeg.cpp` reads movies as ended). Any button press plays a menu sound (`UiLayout::playSound`, `getSePlayer` 0x2c4140), so the menu pack must exist before the title can answer. `loadingTick` 0x231d20 and `loadingBegin` 0x231d30 are two separate one-instruction functions, both `j overlayShowLoading` 0x267950: the waits (`FileLoader::waitLoaded`, `FileJob::wait`, PackJob::load's inlined copy) call Begin once before the loop and Tick each turn.

**`game/menupack.cpp` 0x2c3d30..0x2c42a0 (2026-11-03, 9 functions, all written, `verify`, batch 092; the file name is the port's).** The resident menu pack: `loadMenuPack` 0x2c3e10 opens dataEU/packmenuEU.pak (sbss 0x31ae98, flag 0x31ae9c) and `createMenuSePlayer` 0x2c3eb0 builds the outside-a-stage `SePlayer` (sbss 0x31aea0, flag 0x31aea4) from the pack's ../data/sound/se/seMenu_SE.seh, then `loadBank` or `openStreamBank` for each named bank; `unloadMenuPack` 0x2c3db0 deletes both. `getMenuPackItem` 0x2c3d70 / `getMenuPackItemSize` 0x2c3d30 (were getSaveResource / getSaveResourceAux) are slots +0x20 / +0x28 of the pack, nothing save-specific; the memcard asks for item 0 (icon1.ico) and 1 (the icon.sys template), and `Memcard` +0x78 is that item's **size** (was `m_iconAux`), which stepSave does not use (it writes kIconSize). `MessageTable` 0x2c4150..0x2c42a0 is the stage's script text lookup, here by position only: one word, the message file (magic 0x6d621a00, count +8, relocated flag +0xc, {key, text} pairs from +0x10, offsets from the file); `StageMission`'s first field points at one (0x287690 builds it, `setMessages` 0x286c30 and the lookup forwarder 0x286c20 go through it), and a script argument starting with '_' is replaced by its text (scriptPrepare 0x29f178). 0x2c42a0 is not in this file: it is slot 1 of Vt_00319f00 (`0x2cc8c0(); return 1`), an inline virtual of the class at 0x2ca870..0x2cc8c0.

**`game/uiworkarea.cpp` 0x243350..0x243390 (2026-11-03, 4 functions, all written, `verify`).** `UiTrack::draw` 0x243350 and `getAnchor` 0x243360 (the base slots: nothing, null), `setUiWorkArea` 0x243370 / `getUiWorkArea` 0x243380 over the word at sbss 0x31ae40 (gp-0x7a30). menuPlayMovie lends the stage pack's buffer through it for the opening movie, the movie widget hands it to `MoviePlayer::setWorkArea`; null means the player allocates. New Game's order on screen: title, runOptionScreen(1) (the Screen page), menuChooseKind (difficulty), saveExtraInit, the character / place / time card from menuPlayMovie's layout.

**`game/prgmccard.cpp` 0x2b85a0..0x2b9ef0 (2026-11-03, 7 functions, all written, `verify`, batch 093; the file name is the port's).** `PrgMcCard` (0x94 bytes, Vt_00319520), the widget type "prgMcCard" that is the logic of 2dEU/mcBase%s.d2b, the load and save screens. It owns `gMenuMemcard` (sbss 0x31ae94: the ctor news it, the dtor deletes it) and talks to the layout only through 22 S2D variables in a row, from the one named by p0: rfMSG_FLAG, rfSAVEBAR, rfLOADBAR, rfSELECT, rfSELECT_CTRL, afSELECT_CTRL, rfYESNO, rfYESNO_CTRL, afYESNO_CTRL, SCN01..04_FLAG, SCN01..04_TYPE, s2sSELECT_CTRL_SEL, s2sSELECT_STATUS, s2sYESNO_CTRL_SEL, s2sYESNO_STATUS, rfBAR_CTRL (read from the file: `McVar`). The layout's arg picks the screen: 0 `stepLoad` 0x2b97b0, 1..5 (the `SaveDialogReason`s) `stepSave` 0x2b86e0, whose prompt is rfMSG_FLAG 1..4. rfMSG_FLAG selects one text image of the layout: all 0x22 values were cycled on screen, `McMessage` has each text. SCN0n_TYPE confirms two guesses: `StageKind` Easy / Normal / Hard are what the screen calls Difficulty, and +3 is "Autosave: ON", so **`GameOptions` +0x4d is `autoSave`** (was `_4d`): with it `stepSave` skips the list and rewrites the save of the module's stamp (`McSave_Auto*`), after re-reading it and comparing stamps; no stamp means McMessage_NoStamp, then the ordinary save. The ordinary save asks for the card's info every time one finishes while a question is up, so a pulled card lands on McMessage_NoCard from any of them. `SaveDialog_Extra`'s text names "Kunitoris" play data. `SaveFile::assignHead` 0x2b9710 is the implicit copy of the part under the sum, emitted here for stepSave. State 2020 has no way in; kept.

**`game/layoutpaths.cpp` 0x2ec160..0x2ec8e0 (2026-11-03, 24 functions, all written, `verify`; the file name is the port's).** One shape 24 times: `getSaveData` (answer ignored), `sprintf` of the name with `getLanguageCode(getLanguage())` into a 0x20 static buffer, answer the buffer. 13 event movies `movieEU/ev<id><lang>.d2b` (bss 0x5a55e0 down to 0x5a5460) and 11 layouts `2dEU/<name><lang>.d2b` (0x5a5440 down to 0x5a5300): navi, navi_tu, mcBase, option, memAlt, newGame, scelogo, topMenu, archive, help, title. Both tables run in file order with descending buffers, so they read as one generated list. The language codes are table 0x2fac40, EN by default in this build. `FUN_002ec110` just before the range is not part of it: a guarded singleton 0x5a52d0 of drawSys.

StageScene ctor / dtor (second run, batch 080, 54 names): every member got a class name (guesses unless noted; `gamehooks.h` stand-ins) and the scene a host field map in `stagescene.h`. Facts: **+0xe4 is a 48-bit LCG** (`Rand48`, 0x2682e0 seeds the drand48 multiplier 0x5deece66d; `Rand48::next` 0x2681e0 = (x >> 17) % n; `StageScene::random` 0x195d40); **+0xd0 `StageMenu`** is open while its +0 == 2 (`getOpenMenu` 0x1963d0); **the four area grids** +0xac..+0xb8 (`AreaGrid` 0x284640, 0x240 bytes; `contains` 0x284190 on +0xb8 is `getSoundAttr`; `addId` 0x284560 via `addSoundArea` 0x19ae30 / `addArea2` 0x19ae40); **`isGameOver` / `isCleared`** test the sequences +0x118 / +0x11c (`GameOverSequence::getState` 0x2b1c50 answers 5, `ClearSequence::getState` 0x2b1620 answers 2, both once the scene fade is full in mode 2); `beginPlay` = `currentMission()->markStart()` (mission +0xac = the scene clock seconds). **0x196960 / 0x196a40 are second copies of getMission / getParent** (`currentMission` / `parentScene`, 211 call sites). A sub-scene pushes its own draw-system record (`drawSysPush` 0x2d7c70 / `drawSysPop` 0x2d7c00, gp-0x7994 saved in gp-0x7990); a stage resets the 44-group table 0x599830 / 0x599860 (`resetStageGroupTable` 0x2cf510). +0xbc is the wide-text printer (`WideFontPrinter` 0x2d4110, vtable 0x319ff0, at 0x700, 0x700); +0xf4 `StageAlert` 0x264070 / 0x264000. The ROM dtor asks a sub-scene's DemoStream isPlaying after deleting it (use after free; the port clears the pointer).

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
the size of the script path buffer (0x40 assumed), `runExtraMode` (0x2c4320, 16 scored entries of the
save, unread).

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
- **The game's movie player is FS2's own, not RoR's rxMpeg** (batch 012; written at TU 34, `game/movieplayer.cpp` in "Modules" supersedes the guesses here): RoR's `RxMpeg` / `rxMpeg*` (snd/rxmpeg.cpp, RoR game code) has no counterpart by size (`rxalign.py 1fa800 1fd000 1810f4 1830fc`: 3 chance pairs). FS2's player is a C++ class at ~0x1fa880..~0x1fcbf0 (vtables 0x315f50 / 0x315f80 without type_info): 0x1fc7b0 initialises it (CreateSema, `sceMpegInit` once behind the flag 0x31ae28, 4 x volume 0x80), 0x1fb9d0 opens a stream (rx stream ring 0x152400, `sceMpegCreate`, `sceMpegAddStrCallback` x4, `sceMpegAddCallback`), 0x1fb630 gets pictures (`sceMpegGetPicture`), 0x1fab50 is the decode thread side (WaitSema, `sceMpegIsEnd`); 0x1fbe00 / 0x1fbe20 wrap `sceIpuRestartDMA` / `sceIpuStopDMA` (the libmpeg stop / restart DMA callbacks). Callers: 0x25bd30, 0x25e260 (the movie scenes: `movieEU/ev*.d2b`). Name it at its TU.
- **libc** (Sony's newlib): `strlen` 0x128498, `strcpy` 0x128380 (R5900 quadword scans), `raise`
  0x127a68 / `_raise_r` 0x1278f8, `_impure_ptr` 0x2f3398.

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
  opaque, TCC 0 is opaque) and the render thread makes the GPU texture. The cache key is a hash of
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

**Stage 1's real values** (the script file s01 dumped from a New Game run; `sssdis.py` reads it; 42 entries, s01, s01_l and one per named light: CAR_pl, T_TORCH1_sl, yami_pl, ...): the entry **s01_l** keys the sets in straight lines, keys 0 and 1 of each set equal. Set 0: clear (0, 0, 0), **ambient (175, 180, 200) = 1.37, 1.41, 1.57**, all six directional colours and angles 0, model ambient (65, 68, 71) = 0.51, 0.53, 0.56, fog 1100..1500 at 230..0, colour 0, range 1600, fade 10 frames. Sets 1, 2 the same with model ambient (50, 54, 57), (40, 44, 47) and fog 1100 / 980. So on this stage **the map has only its ambient, about 1.4 to 1.6 blue-tinted**, not 0.1, and the fog closes at 1500. **Seen 2026-11-04:** `setupCameras` runs all 42 entries, the 18 `addSetKeys` come from s01_l, and with them `SceneRender` holds set 0, ambient 1.37255, 1.41176, 1.56863 and fog 1100..1500 in the running game.

**Why stage 1 is dark (measured 2026-11-04):** the map's vertex colours average 20 of 0x80 (0.16), so with ambient 1.37..1.57 and no directional light a vertex averages 27, 28, 31: a dark map by its own data, not a lost light. **Fog, VU1 0x22f..0x259:** VU address 24 (0x2e5a00; copied to 199) is (A, near value, far value, B * the scales 0x59b190, 0x59b194), and each vertex takes clamp(A + B / W, far value, near value), so a near value of 230 tints everything inside the fog near by 230 / 255 towards the fog colour. Disabled: (0, 255, 255, 0), no fog. It is `DrawSysFog::getFogValue` in microcode.

**The frame** (`update` 0x1f73c0, after the particles): every track but the angles steps; the set is the `m_number` of the ENV_CB box at the view (`viewLinkedObject` null: the top camera's position; else 200 ahead of `PlayerMotion::getLampMatrix`, ray 0xfc2600f, 10 off the wall), or `m_forcedSet` (`sceneRenderGroup(0)`, -1 here); a new set or `m_snap` starts all the `VectorFade`s over the new set's transition frames, at once when snap; the values go to `DrawSysLights` of the current record (that is what the map's models get, `renaSetLights`), the record's fog (far + the top camera's `m_range2`), `drawSysGetFog` 0..3, and every camera of group 0 gets `setRange` (lane 0 of the transition, default 1600). **`StageScene::getActiveCharacter` was the top view** (`views->topObject()`, a camera: it is tested for kind 9 and its +0x2d0 read), now `getTopView`: it does not answer null without a character, the ROM does not crash there and no PC guard is needed for it.

**Per model** (`updateLights` 0x1f8620, groups 1, 2, 3, 8): only models with `m_lit` (`markModelAsItem`: items; the writers of the script side unread). The map's Root model has it clear, so it takes the record's lights. Own set and ENV_SB flag (at `m_lightProxy`'s place if set), the walk of group 5: `lightAt` strongest ENV_ and strongest other, `shadowAt` for characters. Light 0 is the strongest other (a character: only the ENV_), when `m_lightsModels`; lights 1, 2 the strongest ENV_ and other. With a light: direction to it (light 0 z negated), colour `m_modelColor` / `m_color` * weight, ambient plus `m_modelAmbient` * weight; without: the set's model tracks. An item lying free gets its ambient scaled by 1.5 + 5 * (1 - mean)^2 when the mean is below 1, so it shows in the dark (`getModelAmbient`). `shadowDirection` eases 0.7 towards the last light that reaches (the ROM's "strongest" weight is never stored, so it is the last, not the strongest; kept).

**The scene's point and spot** (+0 base, +0x80 member, rena's records 0x2e0xxx; every lit model's `ModelInstance::spot`): of group 5, the highest `m_priority` (never -1) of each class, nearest wins a tie. `yami_pl` (priority 4) hangs under a character of class 8, `scr_demo_pl` / `demo_pl` priority 5. What holds a lamp gets priority 4 for it. The host renders neither record yet (VU1 0x545).

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

**`ItemPhysics`** (vtable 0x316510, a `CollisionOwner` at +0x240, EE 0x210; the name is the port's, the code is the unread run 0x23c020..0x23de20) is what thrown items and weapons have and traps do not: ctor 0x23db50 (thrown 0, 1; weapon 1, 0), `setup(owner, motion)`, `update` in the frame, `draw` in slot 3; +0x1fc still moving, +0x200 the object the body must not meet.

**Items.** `gItemDefs` gained startCount +0x18, pickSound +0x1a, isBlade +0x1b, landSound +0x1c (the last two sounds are parsed by name into indices, `ItemDef::setLandSound` / `setPickSound`, from `PackTables::parse`); `gWeaponDefs` range +0x18, magazine +0x24. `ModelInstance` / `StageModelData` have flags at +0xa: 2 set on every item, 0x400 clear only while an item lies free with a dirty matrix. Pick point: the motion's `bn_pic`, turned (x, -z, y); weapons also `bn_point` and `bn_fire`, turned (x, -z, -y). The muzzle `bn_fire` is where `fire` puts `M_Flash` and the 0.1-second `flash` PointLight. G_NN38, R_MURATA and G_26NEN load round by round: no shell (`yakkyo`), RELOAD_* in place of MAGAZ_*.

**Thrown** (I_HATUEN, I_AKIKAN, I_PET, I_NUT): at rest and used, gone after 10 seconds. I_HATUEN is a flare: 20 seconds of `hatsuen` where it lies with SMOKE_ON, or `heyasmoke` once it rests inside an `areas[StageArea_Smoke]` box (+0x470 is 1 until asked). **Trap** (I_WANA): a sphere of radius 80 (bucket 2, category 0x200, mask 8), on only while open; the close and open motions are s_OBJ_098_CLOSE / OPEN; springs when the character is within 90 and the angle is under pi / 4. **The lamp** `ItemLamp` (EE 0x18, `StageItemE` +0x240): four lights of mode 1 under the item in both trees, `%s_sl` (`m_limitByMap`), `%s_sl2` (white, radius 200, cone 45), `%s_pl`, `%s_pl2` (not usable by `updateLights`), and the part `nl`. `SceneRender::update` asks `getLightMatrix`: the last point's matrix, pulled back 30 from a wall on the line from the holder's `bn_head`.

The list instances at the end (0x184260..0x1845f0) are `stagelist.h`'s, or `CollisionBody`'s +0xd4 list and +0x14 block, which the bullet's dtor has inline.

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

**Both old stand-ins were one thing.** `BoneNode` (root at +0x14) and `ModelLink` (+0x18, next at +0x10) are one `StringMap<StageNodeBase *>`: +0x14 root, +0x18 first (the insertion chain). The maps' code after 0x188d60 is that template twice (0x188df0 / 0x188f30 insert, 0x189050 / 0x189840 / 0x177270 find, 0x1890d0 / 0x189380 its recursion) plus **`balance`** 0x1895c0 / 0x189700 with `height` 0x189ad0 / 0x1898c0, now in stagelist.h. These instances keep the key pointer, the host copies it.

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
The GS state of a pass (table 0x310180): opaque ALPHA_1 0x2a, TEST_1 0x5050b (alpha >= 0x50, else colour only, Z test GEQUAL), then 0x2fdb60 a second GS pass TEST_1 0x50000, ALPHA_1 0x44; blended ALPHA_1 0x44, TEST_1 0x50000. CLAMP_1 repeat, TEX1_1 0x61 linear. Blended materials (flag 1) wait in a table, sorted ascending on the screen z of the part's centre + index: far to near, the ROM's z falling with distance.
**What VU1 does to a colour** (`vudis.py 2ec908 3f8`, read 2026-11-04). Entries 0 (0x00a set-up), 6 (0x4e: frame matrices), 2 (0x198), 4 (0x194). The per-frame block 0x4e builds vf24..26 = the three light directions (VU 20..22, one per row: `renaSetLights` 0x2e8080 transposes them) times the mesh's rotation (VU 212..214), each column scaled by VU 221; vf27..30 = VU 16..19, ambient and the three colours. **Lit, 0x2a9**: `vf5 = vf24 * n.x + vf25 * n.y + vf26 * n.z` (so each lane is a light's direction . the turned normal, no negation), `vf6 = max(vf5, 0)`, `vf7 = vf27 + vf28 * vf6.x + vf29 * vf6.y + vf30 * vf6.z`, stored in the buffer at +0x55. **Then 0x2bb**: `colour = min(max(vertex RGBA (0x7d) * (vf7.xyz, 1), 0), 255)`: the vertex colour is **multiplied** by the light. **The file's vertex alpha is never loaded** (`lqi.xyz`): every vertex's alpha is VU 25 w, 0x80 but in a ModelMaterial_Kind1's two frames (0x2e6b70 sets it to the blend). **Unlit, 0x29e**: the mesh's `vuFlags` & 4 (VU 15 z): the vertex colours are copied, the same way. VU address 15 is x the pass's mask (`param_3` of 0x2e6590), y `ModelInstance::flags`, z `ModelMesh::vuFlags`; y & 0x200 (`ModelFlag_TagC`) runs 0x128: **back-face culling**, the cross product of each triangle against the eye sets the ADC bits of the faces turned away (the host does not cull at all); y & 0x100 (`ModelFlag_Ghost`) runs 0x011, an alpha per vertex from the angle to the eye; y & 0x80 with VU 176 the spotlight kernel 0x545; VU 26 y is the material's flags byte (`ModelMaterial` +1), bit 8 runs 0x186: every vertex moves by VU 175 (the surface's first vector) turned by the mesh. The default lights of `drawSysInit` are ambient 0.1 and whatever the dir lights hold: `SceneRender` gives the rest, and its numbers come from scripts: "The lights of a stage" below.
**The natives that choose the view** (game/scriptrunner.cpp, batch 115). 0x70 `nativeSetPlayer` 0x1c81a0: `findCharacter(name, 0)`, then `StageScene::setPlayer`; a name with no character clears the player. 0x64 `nativeScriptView` 0x1c8dc0: the literal name "NULL" (data 0x304ba8; the script passes the string, not a null) removes view priority 3; any other name is looked up with `find0` and, unless it is already the top view, pushed at priority 3 with flag 1, above the main camera 0 and below the demo camera 4. In an active event, which a sub-scene never is, the point light `scr_demo_pl` (a second literal, data 0x304bd0; built by `SceneRender::setup` on a stage) hangs from that view and turns on, and on "NULL" is detached and turned off. The light lookup has no null test in the ROM. 0x1b54a0 is `StageNodeBase::setParent` out of line. Both natives read ScriptRunner +0xa4 and drop it.

**The main camera** (game/stagecamera.cpp, vtable 0x311fa0, EE 0x3c0; between it and StageCamera a kind 8 class 0x311d30, ctor inline, which adds only m_linked; the camjack derives from that too). `CollisionOwner` at +0x340 (the byte first, vptr +0x344: a body of bucket 2, category 0x4000, mask 2, shape 1; all its hooks empty). It only moves with a link: no m_linked, no step. **Offsets**: `MotionDef::cameraFocus` / `cameraEye` (the last two columns' groups of items 6 and 7, `gMotionDefs` +0x110, +0x190) by [AB area 0/1][body 0..3]; body = StageObject +0x20c, from the model name's `_lh_ _th_ / _mh_ _ms_ / _sh_ _ss_ / _ch_`, heights 180 / 170 / 160 / 110. `cameraShift` (+0x108) with `GameOptions::_4c`: 15 / 30 to the side. The focus is a point in the character's space, the eye an offset turned by the character; the wanted eye is focus + that. **Over the shoulder** while the character's +0x290 is set: seven places (0x2f7b10), the first both rays reach, scrZ eases to 610; it looks at the character's +0x2a0. **Then**: rays from the anchor keep the wanted place visible (faces with 0x8000000 may lie between, `m_blockedFrames` waits at most 30 frames before giving up), two side probes of 6 units, characters' `GroundVolume` tops + 15, ease by |d|^2 / divisor capped at 25, divisor 150 (300 in an AB area) + 1000 * how much it moves towards what it looks at; the stick's turn eases into the rotation.
**A stage's camera** (game/stagecamera.cpp): the screen matrix is sceVu0ViewScreenMatrix(scrZ 450, ax 1, ay height / (width * 3 / 4) = 4/3, centre 2048, z 1..0xffffff, near 1, far 1600) * rotateX(pi/2): the object's axes are x right, y ahead, z up. So a 512 x 512 frame is 4:3 with round circles; the 0.9333 of drawSysInit is replaced at the first frame.

## Motions (game/playermotion.cpp, 2026-11-04)

**`PlayerMotion` (character +0x224, EE 0xc20) is the character's motion state, not the player's:** every character has one. It is the state machine between "something asks for a motion" and `StageModel::playMotion`. A request is a `gMotionDefs` index: `request(id)` 0x1ec7a0 (180 callers) asks unless a request stands, `requestNow(id)` 0x1ec650 (72) takes over and stops a walk. `m_request` (+0x3c) is cleared at the end of every `stepLower`, so a request lasts one frame and **whoever wants a motion asks each frame**; asking for the running motion does nothing unless its 同一リクエスト column or `m_forceStart` (+0x66) is set. `motionId` (+0x40) is what plays.

**The frame:** the start hook 0x17a860 calls `beginFrame` 0x1e5d90; the control hook 0x17a810 runs input, brain and 0x1e3e80 (which does looks, feet, wind and the fidget timers; the walking requests come from elsewhere); `stageUpdateCharacterMotions` 0x17df60 runs `stepLower` 0x1e9dd0 (the body), `stepUpper` 0x1e8a30 (hand poses, group 1: l_torch, l_gun, l_rifle, l_fist, l_def; r_weapon, r_gun, r_rifle, r_fist, r_def) and `stepFace` 0x1e8870 (group 2, the table +0x70 from the second list; +0x46c the talking face); the movement hook 0x17a490 calls 0x1e0e20 and `StageModel::advance`. Groups of `addMotion` / `findMotion`: 0 body, 1 hands, 2 faces. `stepLower(true)` is reset's: flag 0x10, from the first key.

**`stepLower`:** the running motion's 遷移モーション is requested when it ends; `resolveHold` turns the request into a hold (+0x54, +0x58) of the same crouch side; `arbitrate` 0x1e6d60 puts sq_u (0x27) or sq_d (0x28) in front of a request that changes stance (ability +0x3a6), storing the request and the character's +0x360 block in `MotionQueue` +0x5c, and handles u_sp (0x32), where both characters wait at 0xfb; a pending walk starts (0xf9 / 0xfa while it runs, the request waits in `MotionQueue` +0x60); then the link's hooks, then the label: `lowerLabel` is the rifle column `m_weaponClass` when the weapon is a rifle, unless it starts with `*`, which makes it the upper label (`upperLabel`, played with arg 0x200). A motion loops unless 再生終了待ち is set. Last the ground gets 接地判定, 壁判定, キャラ反発LV.

**Links and conditions (read 2026-11-04, batch 121): this is who asks for the motions.** A frame has three stages. (1) The start hook 0x17a860 clears `CharacterAllows` (+0x360, 0x44 bytes), and `beginFrame` runs Always' `onUpdate`, then the running link's `update`, which runs each of its active conditions' `onUpdate`: each writes a byte saying what the running motion allows (+0x360 squat, +0x361 turn, +0x362 move, +0x363 quick turn, +0x390 head, +0x391 aim, +0x39b view, +0x39c zoom, +0x39d lamp). The start hook then clears `CharacterWants` (+0x2c0, 0xa0 bytes). (2) The control hook 0x17a810: input 0x236b90 and brain 0x1dd2f0 fill the wants (`moveKind` +0x2c0, `moveYaw` +0x2c4, squat +0x2c8, quick turn +0x2c9, head +0x300 / +0x310, aim +0x320 / +0x330, view +0x348, zoom +0x34c / +0x350, lamp +0x354); `controlStep` 0x1e3e80 runs Always' `onApply`, then `MotionLink::apply` 0x27cac0: the link's slot +0x10, then each active condition's `onApply` while no request stands, then the link's slot +0x1c. (3) `stepLower` plays it. **Move is last in Walk's list and asks for `pickStance(0)` when nothing asked, so the idle needs nobody else.** `CharacterMoveKind`: 1 w_l, 2 t_l, 3 w_r, 4 t_r, 5 and 6 w_b, 7 and 9 w, 8 run. Move also does stairs (0x17..0x1a on a face of flag 8), `ops` at an edge and `wa` into a wall after a long run (player of kinds 0, 1 only); `w_run` 0x16 sits between the two paces when ability +0x3aa is set. `moveYaw` goes into `m_turnStep`[2] (+0xb78), and `applyMovement` 0x1e0e20 adds `m_moveStep` (+0xb60) and `m_turnStep` (+0xb70) to the character, so the walking distance itself is the motion's root, through `StageModel::advance`.

**The objects.** Links (vptr +0x1c; slots +0xc update, +0x10 apply, +0x14 request, +0x18 start, +0x1c run, +0x20 leave(next), +0x24 cancel; byte +0x14 finished), in the map +0x18: **Walk 0x27c520** (vtable 0x317bf0, also +0xc00), Jack 0x27c370, Pickup 0x27c140 (+0xc08), Up 0x317b60, Fall 0x317b30, Jump 0x317b00, Ladder 0x27b730, Door 0x27b640 (+0xc0c), Battle 0x27b000, BtAttack 0x27aea0, Gun 0x27aa90, GunAttack 0x27a910, Reload 0x3179b0, DiveFall 0x27a0b0, DiveLand 0x317950, TrapSet 0x317920 (+0xc14), TrapCancel 0x3178f0 (+0xc18), TrapHit 0x3178c0, Throw 0x317890 (+0xc1c), Bite 0x278db0, Bitten 0x315eb0, Damage 0x317830, DamageLand 0x317800, DamageFatal 0x3177d0, **DamageDead 0x2775d0** (vtable 0x3177a0, +0xc10), Revive 0x317770, then ApproachCar, ApproachOut, Drive, SpWait, NPCReaction, ApproachBite and SigLamp, NumLock, SevenKey; a number ending 0x317xxx is the vtable set on a base link 0x27cc50. **Correction:** the first reading had Walk at 0x2775d0 and called +0xc00 a "look state"; `PlayerLookState` is gone: its `active` and `angle` are WalkLink's `edgeFound` +0x90 and `edgeDrop` +0x60, the floor's depth behind the wall ahead, which is why the main camera rises below -170.

Conditions (vptr +0x10; slots +0xc onUpdate, +0x10 onApply; byte +0xc active; base 0x276510, vtable 0x317750), in the map +8, in the ctor's order, vtable then onUpdate / onApply: Always 0x317730 0x275b60 / 0x275160, Squat 0x317710 0x2750f0 / 0x274990, BtOn 0x3176f0, GunOn 0x3176d0, Turn 0x3176b0 0x273fa0 / 0x273f10, Move 0x317690 0x273da0 / 0x272d50, Level 0x317670, LevelLift 0x317650, Ladder 0x317630, Jump 0x317610, Door 0x3175f0, PutItem 0x3175d0, PickupItem 0x3175b0, TrapSet 0x317590, TrapCancel 0x317570, Throw 0x317550, Showt 0x317530, Reload 0x317510, QuickTurn 0x3174f0 0x2702a0 / 0x270260, Jack 0x3174d0, Bite 0x3174b0, LadderMove 0x317490, Lock 0x317470, BtAttack 0x317450, BtAtkCombo 0x317430, BtMove 0x317410, GunAttack 0x3173f0, Snipe 0x3173d0, GunMove 0x3173b0. **Walk's list:** QuickTurn, Squat, Lock + BtAttack or BtOn (ability +0x3c2, +0x3c3), GunOn, Level, LevelLift, Ladder, Jump, Door, PutItem, PickupItem, TrapSet, TrapCancel, Throw, Showt, Reload, Bite, Jack, then the special links of data 0x2fb3a0 (30 names: cr_2, GettingOnCar, sp_*..; kinds 0xb and 0xd add 0x2fb420 / 0x2fb440), Turn, Move. The list +0x2c is what 0x1df0b0 fills.

**Ledges, ladders and gaps, and the Use button (read 2026-11-05, batch 125).** `WalkLink::onUpdate` finds a wall face and the edge behind it; **Level** (0x272630 / 0x272360) allows +0x368, 1 up or 2 down, by `edgeDrop`: down fal_l below 0, fal_m past -75, fal_h past -150; up u_060 below 75 (stepLevel 1), u_090 below 105 (2), u_120 below 150 (3), else u_180 (4). A face of flag 0x100 (kFaceGap) offers Level only while not walking ahead and Jump only while walking ahead (`isForwardMotion`). Stage 1 and 2's lead (`isOpeningLead` 0x215ce0) never drops past 125. **Ladder** wants a face of flags 0x2020, ability +0x3b4; the floor behind it, 1000 up or down, says which end one is at, +0x36c 1 foot, 2 top. **Jump**: flags 0x180, ability +0x3b5, a raycast 25 past the radius and `sidesClear` 0x26c2d0. All three wait while another character near is on the same way (`levelBlocked` 0x1dff40, `ladderBlocked` 0x1e0360, with `getLadderRank`: kind 1 first, then 0, then the rest). **The Use button does not reach them directly:** `UiItemList::update` (game/uiitemlist.cpp) reads the allows of the controlled character and takes the kind of highest priority (`UiUseKind`: 0 up, 1 down, 2 ladder, 5 unread, 6 jump, 7+ `post` from scripts, natives 0x79, 0xf6) into the back slot; the slots flip each frame; `CharacterInput::stepItems` asks `offer(who, kind)`, and the answer becomes `CharacterWants` +0x2cb, +0x2cd, +0x2ce. A kind's label and priority are scripts' (natives 0xf7, 0xf8, each `kinds[i]`, default "未定義", priority 0x80). `isReady` (kind past 6) is what makes the list the input focus, so only a script's prompt takes the pad from the character. **Links:** Up 0x317b60: `onRequest` takes the rise, `onStart` and `onRun` set `setRootScale(1, 1, rise / made, 1)`, made being 60, 90, 120, 180: a climb is the sheet's motion stretched to the height. Fall 0x317b30 (fal_*, ld_esp): holds the motion's last step into `m_moveStep[2]` once it ends, then at the floor picks by how far it fell: under 25 nothing, under 75 lnd_l, under 150 lnd_m, under 295 lnd_h, then by the sheet's 段差下り (+0xe0): 2 lnd_h, 1 lnd_h under 395, else 0xaf; a Click event turns the watch off and on. Jump 0x317b00: lnd_h on landing. Ladder 0x317ad0: only ld_s has it, with LadderMove: Back, Walk, Run climb, 35 under the bottom and 185 under the top end it; with allows.squatAction the squat button is ld_esp. LevelLift is the pair's u_sp: wants +0x341 is set by neither the input nor anything written; the brain is the guess.

**`pickStance(slot)`** 0x1eb660: a table of { standing, crouching } by whether `motionId` is a crouch: 0 s / sq_s, 1 w / sqw, 2 w_b, 3 w_r, 4 w_l, 5 run / ka, 6 t_r, 7 t_l, 8 call, 9 jack, 10 w_run / sq_ka. Second table (tr_) with `m_winded` (+0x3f4) on kinds 0, 1; third (_al) for Yri / Kne with +0xb20.

**The rest of the frame's control step (`controlStep` 0x1e3e80, one 0x1ed0-byte function in the ROM; the host cuts it in `stepEvents`, `stepAim`, `stepFeet`, `stepWind`, `stepHealth`, `stepFidgets`).** After the links ran: the six Hit events of the model name the bone a hit comes from (`CharacterGround::setHitBone` 0x1f00a0: bn_r_hand, bn_l_hand, bn_r_foot, bn_l_foot, bn_head, bn_pelvis) and set the contact, HitEnd clears it, both only with an active link. StepLeft / StepRight play a step at bn_l_foot / bn_r_foot (bn_base when the model has none): `playLadderStep` 0x2a2100 on ladder motions, else `playStepSound` 0x2a49c0 on the floor face's `surface` at pace 0 crouched walk, 1 walk, 2 sq_ka / sqw_ka, 3 running. **Grounded is `+0x20 || !+0x10`** of the ground: off the ground, in a motion whose 接地判定 is 1, `m_fallSpeed` grows by 120 / fps a frame into `m_moveStep[2]`. The pelvis takes the floor's normal at rate 0.07 (`setAimDirection`), or for kinds with flags 7 / 8 the line along the object it stands on. Feet: flag 3, `m_stepsOn[0]`, the motion's IK and grounded: a ray 50 up and down under each foot gives a plane (stairs keep up, height only), rate 0.2. Wind: only for poses whose bone names are lower case (`ModelPose::lowerNames`), on outside the boxes of `StageNodeTree::contains` 0x1908d0.

**体力 is stamina (`m_health` +0x594), `stepHealth`.** Running (run, tr_run, w_run, the stairs at a run, sq_ka, sqw_ka) costs one point a frame at 30 Hz; 5% of the sheet's value a second comes back, for kind 0 only while resting (`isRestingMotion`), with gear, or in the alternate sound state; other kinds always. At 0: `m_winded` (the tr_ stance table, Move walks). At 40% it jumps to full. At full, `m_winded` clears and kinds 0, 1 may fidget. Stages 0x19, 0x1b (kind 1) and 0xf, 0x17 (kind 0) drain instead (`isStageDrained` 0x215940, the same test as `getMotionRate`): while the player controls someone else by `m_drainRate` (+0x5f0, a script property) of that someone a frame, and when it hits 0 `StageScene::altLatch` is set; on the player himself during the timed change by `m_slowFrom` a frame. **Fidgets:** `m_fidgets` (+0x3e0, set by `resetActionState`, property setter 0x212530), no gear, not the player seen through a Kind10 view of himself, standing or sq_s with the head free for `m_idleDelay` (10..20 s): idle0 / idle1 by a coin, sq_idle0 crouched; a character with no health on the sheet fidgets only as kind 0xf, idle0.

**Step sounds:** table 0x2fc3b0 (`src/game/stepsounds.inc`, generated from the ELF): 44 rows by surface, 32 names: 0 the surface's own, 1..5 PC_GNDnn_H/W/S/R/L, 6..10 NPC_, then three each for SBT, YBZ, YKO, YOT, RYO, TKS, 29 UMI, 30 WALLHIT, 31 POP (`getSurfaceSound` 0x2cf0b0). Families with three columns fold the pace: 4 -> 2, 3 and 2 -> 1, else 0. The ROM caches the entries in bss 0x599860; the host looks them up each step.

**`MotionProbe::findEdge` 0x26b960:** faces with flags 0x142, ability `stepLevel` above 0. A second ray along the face's normal finds the edge; `findFloor` 0x26c180 looks 205 up, then 1000 down, one `radius2` past it; `drop` is signed. Arguments are (face, **object** at WalkLink +0x40, target, drop, floor, point, angles, blocked): +0x40 was not a vector. `m_42c` set: the wider test with a sphere 15 further on, and Kind3 objects (cars) count differently. `getLampMatrix` 0x1e60b0 is identity * R(lean) * R(rotation) * Rz(pi) at bn_head; `getLampTarget` 0x1e6160 (was `getLampAngles`: it answers a point) casts along the spot's radius from the head, or from a Kind10 / 11 view; a target nearer than 200 is pushed to 200. `startWalk` 0x1ead00 / `startWalkAt` 0x1eaea0 take a float rate, the `m_walkKind` of before.

**The sheet's ids, from a dump of the host's `gMotionDefs`:** 0 s, 1 tr_s, 2 s_al, 3 w, 4 tr_w, 5 w_al, 6 w_b, 7 w_r, 8 w_l, 9 run, 0xa tr_run, 0xb t_r, 0xc t_l, 0xd / 0xe the same with upper s_al, 0xf t_q, 0x10 call, 0x11 jack, 0x12 pu_l, 0x13 pu_m, 0x14 idle0, 0x15 idle1, 0x16 w_run, 0x17..0x1a stairs, 0x1b ops, 0x1c wa, 0x1d sq_s, 0x1e tr_sq_s, 0x1f sq_s_al, 0x20..0x23 sqw, _b, _r, _l, 0x24 ka, 0x25 sq_t_r, 0x26 sq_t_l, 0x27 sq_u, 0x28 sq_d, 0x29 sq_call, 0x2a sq_jack, 0x2b sq_idle0, 0x2c sqw_w, 0x2d sq_ka, 0x2e..0x32 up (u_060..u_180, u_sp), 0x33..0x35 falls, 0x36..0x38 landings, 0x39 jp, 0x3a..0x42 ladder, 0x43..0x4c doors, 0x4d..0x58 battle stance (bt_), 0x59..0x67 melee attacks, 0x68 / 0x69 bt_off, 0x6a..0x79 pistol (gn_), 0x7a..0x88 rifle (rf_), 0x89..0x93 the third gun (mm_), 0x94..0x97 dives, 0x98..0x9c traps, 0x9d th, 0x9e / 0x9f bite, 0xa0..0xa3 bitten, 0xa4..0xb8 damage and death, 0xb9..0xbb car, 0xbc..0xbe approaches, 0xc3.. sp_ specials.

**F_PC01's idle is motion 0, label `s`,** and Move asks for it (above); no reader of the sheet's 初期M (+0x40) was found. **The pace:** `moveStep` 0x17a490 gives `advance` the rate +0x5e0 for walks, +0x5e4 for runs, the Door link's +0x24 for doors, times `getMotionRate` 0x20d400 and seconds / 0.0333. +0x5e0..+0x5ec are written only by the property setter 0x212530 (slot at 0x31243c: value / 100, /30 for +0x5ec), so a script gives them; the host starts them from the sheet's 移動速度 and 走り速度 (open-items.md).

**The pad to the wants (read 2026-11-04, batch 124; game/characterinput.cpp).** `CharacterInput` (+0x21c, EE 0x60): +0 active, +4 owner, +8 `CharacterPad` (a GamePad with one slot at 0x3164f8, 0x239d50, owner at +0x178), +0xc the left stick's 32-frame history, +0x10 the look vector, +0x24.. the scope. `CharacterPad::update` blanks by the scene: all buttons and the right stick under an open menu, everything under a game over, all buttons under an event, the Use button while `getInputFocus` is someone else, and with the party running everything for the player unless `UiShowFlags` byte 9 is set. The stick's angle (`m_angle[3]`, 0 is up, positive left): within 10 degrees of the side in front or 30 behind it turns on the spot (`t_l`, `t_r`) or with L1 strafes; else it walks, runs from length 0.7, or backs. R1 (`ready`) widens those bands to 80 degrees, 45 with L1. Down flicked and let go in 0.12 s is the quick turn. The default buttons by `ButtonMap`: action 0 Cross Use, 1 Circle squat, 3 Square lamp, 4 R1 ready, 5 R2 zoom, 6 L1 strafe, 7 L2 alternate action (for a scenario's lead, sight-jack: `StageParty::start` / `stop`, "camjack"). `UiShowFlags` holds the script's switches for these. **`PlayerMotion::noFlash` (+0xbd4) is the scope** (camview_snipe): the left stick then moves the aim with 狙撃補間率 and 狙撃ブレ率 of the sheet. `getActionState` 9 is at the wheel: `m_gear` is a car (CAR_HORN, CAR_EG_ST; `driving` the sheet's 運転スキル), motions 0xb9 / 0xba; R1 on, R2 back, by pressure.

**The character's ground (`game/characterground.cpp`, 0x1eff00..0x1f4bb0, vtable 0x315ee0, EE 0x1c0).** A `CollisionOwner` with six kinds of body, by category: 2 `volume` (bucket 2, spheres at bones, what it is hit on, the main camera keeps out of its box), 4 `blowBodies` (bucket 2, capsules from a bone's last position to its position, radius 50), 0x40 `meleeBody` (bucket 1, segment against the map), 8 `nearBody` (bucket 0, one sphere, only with `flag3ba`), 0x10 `pushBody` (bucket 0, character against character), 0x20 `wallBody` (bucket 1, spheres against the map, mask 0xe8021f3, with 0x20000000 for kinds 5, 0xc, 0xd). The bone tables are per family 0..9 (the sheet's kind and the body size); every offset in them is zero. **A frame:** `moveStep` ends in `beginFrame`; `collide` syncs the bodies, `meetWall` pushes out along the normal, at most 12 a frame, and along a corner when a second mesh's face nearly faces the first; `collisionHook3` of the wall body sweeps the way walked at 50 above the feet, then `checkFloor`; `endFrame` looks for the floor itself when the wall body is off, and keeps the position. **Floor:** a ray from 100 above the feet, faces 0x42400c, not 0x40000; a landed character with 接地判定 1 takes it down to 50 below and sticks to it, otherwise 10 below. **Character against character:** `repelLevel` from the motion, the player's levels below 2 count one less; equal: half each, or at level 2 `winsStruggle`; the lower yields. **Doors:** `doorsBlock` 0 leaves doors and door faces out (`exceptList` holds them, the last slot the view target). **Cars:** a car's speed and `getHitLevel` throw a character: levels 2, 3, 4 are 1.5 s at 25, 3 s at 50, 5 s at 75, easing by 0.8 a frame.
