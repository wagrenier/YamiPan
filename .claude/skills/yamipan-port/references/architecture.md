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
- Source-path strings exist (bootstrap.md 1 anchors): `rxfile.c` 0x3016c0, `filehost.c` 0x301908,
  `rena_2d_texture_tim2.cpp` in an assert-style message at 0x30fdd0. Not yet counted.

## Address map

| range | contents |
|---|---|

crt0, game code, middleware, Sony libraries / kernel stubs / libgcc / newlib (`data/layout.json`
lib_ranges), linkonce satellites, VU code, `.data`, `.ctors` / `.dtors`, vtables, `.rodata`, `.sbss` /
`.bss`.

## Naming sources

| source | count | tool | tag |
|---|---|---|---|

## Program flow

Entry -> main -> the main loop; the boot sequence the smoke pad walks.

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
