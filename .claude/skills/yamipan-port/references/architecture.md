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
  streams, `RXWS` / XWS stream files), rxthread. **Renderer "rena"**, C++: `rena_2d_texture_tim2.cpp`
  0x30fdd0, `rena_pkt_vif1pktmanage.cpp` 0x310640, `rena draw` 0x303bd8. No engine kinship with FF2,
  FF3 or RoR (other developer, other compiler): names and code come from this binary only.
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
| ~0x310a00..~0x310f80 | a second code island (Ghidra 0x310ac0, 0x310b60, 0x310f60; looks like static initialisers) |
| 0x31ad80..0x5a5600 | .bss (cleared by `entry`); gp 0x322870 |

crt0, game code, middleware, Sony libraries / kernel stubs / libgcc / newlib (`data/layout.json`
lib_ranges), linkonce satellites, VU code, `.data`, `.ctors` / `.dtors`, vtables, `.rodata`, `.sbss` /
`.bss`.

## Naming sources

| source | count | tool | tag |
|---|---|---|---|
| ELF symbols | 0 (stripped) | -- | -- |
| Ghidra auto-analysis | 100 (SDK syscalls, thunks) | emotionengine-reloaded | -- |

Expected sources (bootstrap step 4): Sony SDK 3.0 libraries by match (libmatch.py against FF3 / RoR,
both SDK 3.0); MSL C++ / CodeWarrior runtime only by reading (no reference game built with it); rx and
rena from their messages; game code from its strings and callers.

## Program flow

Entry -> main -> the main loop; the boot sequence the smoke pad walks.

- `entry` 0x100008 -> `main` 0x12d910 (argc, argv) -> exit call 0x123538. `main` stores argc/argv in
  gp globals (gp-0x7ae8 / -0x7ae4), registers callbacks (0x12db70, 0x12dab0, 0x12db00 through
  0x130f60 / 0x130a30 / 0x130e00), runs 0x130e50 then 0x2acec0 (thunk) / 0x151280 / 0x148540, then
  tears down a global object (gp-0x7aec) with three member objects (vptr stores above) and calls
  0x12e3a0, 0x130070, 0x152750, 0x1045a0, 0x1051f0. Names: bootstrap step 4.

## Modules

The game's own source files (from its strings), what each does.

## Classes

Name, vtable, vptr offset, size, file.

## Key functions and globals

## Disc and data files

Archive formats, file tables, where the data root starts.

## PC platform layer

What was taken from RosePan, what was dropped, the host draw modules and their users, the platform
contract hooks.
