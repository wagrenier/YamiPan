# Bootstrap: from an empty repo to the one-TU-per-session loop

RosePan went through this on 2026-10-22 in two host tasks (`setup/ghidra_seed.cpp`,
`setup/platform_bootstrap.cpp`) and was porting TUs the same day. Do the steps in order; each ends with
something checkable. Record what each finds in `architecture.md` ("Binary and toolchain", "Address
map", "Naming sources") and tick it off in `open-items.md` "Next up".

## 1. The game

- The ELF from the user's disc and the extracted disc's files, in a folder the user picks (the data
  root; never committed). Record serial, region and build date. YamiPan: Forbidden Siren 2, Europe,
  `SCES_538.51` (2006-05-17), all in `C:\Users\willi\Documents\Decomp\Forbidden Siren 2`; the tools
  default to that folder, so there is no copy at the repo root.
- What the binary still has, each a naming source:
  - **Symbols**: `.symtab` / `.mdebug` (rare in retail), or a kept **section header table** with
    `.gnu.linkonce.t.<mangled>` sections (RoR: 1,449 inline methods and templates, 284 vtables
    `_vt$<class>`). `elf32.py` lists sections.
  - **Compiler family**: GCC 2.9x (`name__<len><class><params>` mangling, `_$_` destructors, `__tf`
    type-info functions, 8-byte `{delta, index, pfn}` vtable entries, vptr *after* the first
    polymorphic class's data) or GCC 3.x (`_Z` mangling, `_ZTV` vtables, C1/C2 constructor twins, vptr
    at 0). FF2 was ee-gcc 2.96, FF3 GCC 3, RoR GCC 2.9x. The ABI section of reconstruction.md and
    `rtti.py` depend on it.
  - **Source paths and asserts**: `__FILE__` strings (RoR: 73 `.cpp` paths, assert expressions naming
    members) anchor functions to files and fields to names.
  - **Library versions**: `sce...` version strings (`libgraph 3000`); IRX module names.
  - **Engine kinship**: does it share an engine with a game already ported? FF3 shared FF2's, so ReiPan
    ported names (matcher.py, FF2's debug build has full symbols) and code (MioPan's files) and
    reconstructed only the rest; RoR shared nothing. Same developer or middleware -> look for it first.
- Ask the user what is unclear (which disc, which region); they decide the game.

## 2. Ghidra project (its own; ghidra.md)

YamiPan: **done by the user (2026-10-31)**: `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr`
holds `SCES_538.51`, imported (`r5900:LE:32:default`) and auto-analysed (6,393 functions, 100 named:
the SDK syscalls emotionengine-reloaded knows, and thunks). For a new project:

```
"C:\Users\willi\Documents\ghidra_12.2_PUBLIC_20261016\ghidra_12.2_PUBLIC\support\analyzeHeadless.bat" "C:\Users\willi\Documents\Ghidra Projects" <Name> -import "<data root>\<ELF>"
python ghidra_mcp.py launch
```

The import runs auto-analysis (minutes; `-noanalysis` skips it, then analyse in the CodeBrowser).
`launch` opens the project window only: open the program in the CodeBrowser once (Ghidra reopens it
with the project from then on, and that window is what serves MCP, the program as its active tab). Set
`data/ghidra.json` "project" and "program" (the name `/health` shows); `python ghidra_mcp.py which` must
name it. One project per game.

## 3. Tools (from RosePan, the newest set)

Copy `../RosePan/tools/reverse/*.py`, `README.md`, `routine.md` and rename: `rosepan` -> `yamipan`
(module, paths, `ROSEPAN_*` -> `YAMIPAN_*`, exe and target names), the annotation `ROR` -> this game's
tag (open-items.md), provenance tags `ror-symbol` / `ror-inferred` -> `game-symbol` / `game-inferred`
(`lib-match`, `lib-match-low`, `lib-context` stay). Then:

- `yamipan.py` keeps RosePan's shape (paths, address map, tags, annotation and stub regexes, write
  helpers) but its `GHIDRA` constant and `http()` / `ghidra_up()` go through `ghidra_mcp.py`
  (README "Carrying a tool over"). Grep the copied tools for `8089` and `GHIDRA_MCP_URL`: none may remain.
- **RosePan's Ghidra calls are GhidraMCP 5.14.2's**; YamiPan runs 2.0.0 (ghidra.md has its API).
  `ghidra_mcp.http()` returns parsed `data`, not text, and takes no `program`. Translate:
  `list_functions` -> `functions` (paged), `get_function_by_address` -> `function`,
  `decompile_function` (by address) -> `decompile_function`, `rename_function_by_address` stays. No
  equivalent: `save_program`, `create_function`, `disassemble_bytes`, the function-tag endpoints,
  `get_bulk_function_hashes`, `list_open_programs` (used by `rosepan.py`, `name_ctx.py`, `fetch.py`,
  `mipsemu.py`, `ghidra.py`). Settle open-items.md's question on them before carrying those tools.
- Fresh data: `data/tus.json` (no TUs, settings), `data/layout.json` (empty ranges), empty name
  ledgers (`context_names.json`, `overrides.json`, `lib_matches.json`).
- Per-game tools stay behind or get rewritten: `i3racc.py` (RoR's i3r accessors), `vudis.py`'s image
  addresses, `sections.py` / `gcc2.py` only for a GCC 2 ELF with linkonce sections (a GCC 3 ELF needs
  an Itanium demangler: `c++filt`), `rtti.py` for the game's ABI (ReiPan's is the GCC 3 one).
  `tm2png.py`, `mipsdis.py`, `mipsemu.py`, `sinit.py`, `datadump.py`, `hidden.py`, `stubgen.py`,
  `callcheck.py`, `layoutcheck.py`, `gdbrun.py` are generic.
- `libmatch.py` references: FF3's SDK 3.0 libraries (`../ReiPan/SLES_538.25`, `ff3_functions.txt`),
  FF2's ee-gcc 2.96 runtime, newlib and SDK 2.8 (`../MioPan/SLES_523.84`, `symbols.txt`), and now
  RoR's (`../RosePan/SLUS_214.48`, `ror_functions.txt`: SDK 3.0 plus libpad2 / libdbc / libvib, which
  FF3 lacks). Match the game's library versions to the closest reference.

## 4. Seed the names (host task `setup/ghidra_seed.cpp`)

`restore_names.py --snapshot` first. Then, looking at each plan before `--apply`:
the ELF's own symbols (`sections.py`), library matches (`libmatch.py`; unique matches only, merged
reference bodies skipped), classes from vtables (`rtti.py`), functions Ghidra folded into their callers
(`hidden.py` over the whole text, `sections.py --apply` creates tail-jump targets after `jr ra`), and a
sister game's names if one exists (ReiPan's `matcher.py` -> `reconcile.py --plan`: greedy and
sensitive, hazards.md "Names from a sister game"). Record each source's count in architecture.md.

## 5. The TU map

`export.py` -> `<repo>/yamipan_functions.txt`: pinned ranges, then string anchors (`__FILE__`), class
anchors, then link-order fill. `status.py` -> `references/status.md`. The address map (library ranges,
satellite ranges) goes into `data/layout.json` and architecture.md. Expect guesses between anchors:
sessions pin real extents (`tus.py define`).

## 6. Platform layer (host task `setup/platform_bootstrap.cpp`)

From RosePan (it is ReiPan's, which is MioPan's, renamed and grown): `src/sdk/` (the PS2 SDK on the
host), `src/rosepan/` -> `src/yamipan/` (os, io, gs, rendering, UI, config, memory), `src/common/`
(the stub macros), `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, the fetched dependencies. Rename
`RosePan` -> `YamiPan` throughout (identifiers, env variables, files, shader macros; config in
`%APPDATA%/Mikompilation/YamiPan/`). Drop RoR's game-specific modules (`rosepan_i3d*` and the i3d mesh
path, RoR's file layer glue) unless the new game turns out to use the same middleware. Keep the
platform contract the tools rely on (RosePan's tools README "Platform contract"): screenshots
(`YAMIPAN_SHOT_DIR` / `_EVERY` -> `shot_<frame>.png` + `last.png`), the scripted pad
(`YAMIPAN_AUTOPAD`), the data root (`YAMIPAN_DATA_DIR`), the stub log line
(`[stub] <name> (<TAG> 0x%08x) reached`), the game thread's name for gdb, `[W]` / `[E]` log lines, a
skip-intro switch that smoke runs turn off. `src/main.cpp` runs the game's `main` on the game thread.
The draw path is native from the start (SKILL.md "The user's rules").

## 7. First build, first TU, the loop

`python build.py` green with the game's `main` (or its crt0 successor) as the first TU, everything else
stubs. `build.py --run 15`: the window opens, the log shows the reached stubs, `last.png` exists. Then
the queue (`tus.py queue`, pinned from what the boot reaches), the scheduled-task prompt
(`routine.md`), `autopilot.py` (the user runs it), and from here on one TU per session.
