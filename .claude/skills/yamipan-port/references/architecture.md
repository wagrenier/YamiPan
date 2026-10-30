# Architecture -- what is known about the game

Established facts only, each with its address (unconfirmed guesses go to open-items.md). Replace what a
new fact supersedes; no contradictions. Empty until bootstrap (bootstrap.md) names the game; the
sections are the ones RosePan's grew into.

## Binary and toolchain

Serial, region, build date, entry, gp (`.reginfo`), compiler family and ABI, symbols kept (sections,
`.mdebug`), Sony library versions, IOP modules, VU microcode sections.

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
