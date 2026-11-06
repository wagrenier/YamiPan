# YamiPan reversing tools

YamiPan is the fourth PS2 port by the same author (MioPan: Fatal Frame 2, ReiPan: Fatal Frame 3,
RosePan: Rule of Rose); its game is Forbidden Siren 2 (Europe, `SCES_538.51`, 2006-05-17). The ELF is
stripped (one unnamed section for the whole image, no symbols) and was built with Metrowerks
CodeWarrior, not GCC; its Sony libraries are the SDK 3.0 objects FF3 and RoR link. These tools turn the
binary into names, files and translation units and drive the one-TU-per-session port. They were carried
over from RosePan, the newest set, on 2026-10-31 (bootstrap 3), moved onto GhidraMCP 2.0.0 and, on
2026-11-02, onto bethington's GhidraMCP 7.0.0.

Run every tool from this directory: `python <tool>.py` (`py <tool>.py` where `python` is the Microsoft
Store stub). Paths come from `yamipan.py` and can be overridden with environment variables:

| variable | default |
|---|---|
| `YAMIPAN_GAME_DIR` | `C:\Users\willi\Documents\Decomp\Forbidden Siren 2` (the ELF and the disc's files; never committed) |
| `YAMIPAN_ELF` | `<game dir>\SCES_538.51` |
| `YAMIPAN_DATA` | the game dir (the data root; passed to the game as `YAMIPAN_DATA_DIR`) |
| `YAMIPAN_PROGRAM` | `data/ghidra.json` "program" (`SCES_538.51`) |
| `YAMIPAN_REIPAN` / `YAMIPAN_MIOPAN` / `YAMIPAN_ROSEPAN` | `../ReiPan`, `../MioPan`, `../RosePan` (library references for libmatch.py, platform layer source) |
| `YAMIPAN_MINGW_BIN`, `YAMIPAN_NINJA`, `YAMIPAN_VCVARS` | g++ / ninja on PATH, else RosePan's CLion MinGW and Python ninja; vswhere's vcvars64.bat |

## Scheduled runs

| file | what it is |
|---|---|
| `routine.md` | The prompt of the desktop app's scheduled task `yamipan-next-tu` (every 20 minutes): one step per run (a bootstrap step until `tus.py` exists, then one TU) on the branch `fs2-port`, commit, push, report. Sessions never edit it; the user does (then updates the task's prompt to match). |
| `preflight.py` | Gates a run: `python preflight.py` checks the stop switch (`work/STOP`), a running autopilot, the run lock (`work/run.lock`: exit 3 while the previous run is still going, exit 1 when older than 6 h), a clean tree, a work branch and Ghidra serving the program, then takes the lock; `--release` drops it. To stop the runs: create `work/STOP`. |

**`tus.py` arrived 2026-11-02, ahead of the build** (open-items.md "Next up" 0(b)): the routine now
runs one TU per run. Until `CMakeLists.txt` exists it works in **source-only mode**: `check` runs
callcheck on the TU's file and layoutcheck's lint (no compiler, no smoke run), `finish` skips the
build, refuses `--status done` and records the session as "not built". `autopilot.py` (which sessions
never edit) is still not carried over; it comes with the first build (bootstrap 7).

## Pipeline

```
python fetch.py            # Ghidra -> work/hash_fs2.json, work/fs2_tags.json             (read-only)
python features.py         # ELF -> work/feat_fs2.json: calls, tail jumps, strings, data refs (offline)
python sections.py         # ELF symbols -> work/sections.json (FS2: none, stripped)
python libmatch.py         # library names from reference ELFs; look at the plan, then --apply (once)
python rtti.py             # vtables -> work/classes.json (CodeWarrior ABI: 490 tables in FS2)
python export.py           # file inference -> <repo>/yamipan_functions.txt
python status.py           # -> .claude/skills/yamipan-port/references/status.md
python refresh.py          # all of the above that is not a one-time write, in order
```

`libmatch.py --apply` is the one-time Ghidra seeding (bootstrap 4, host task `setup/ghidra_seed.cpp`);
take `python restore_names.py --snapshot` first.

## Ghidra: bethington's GhidraMCP 7.0.0, several instances

| script | what it does |
|---|---|
| `ghidra_mcp.py` | Finds the GhidraMCP 7.0.0 server with YamiPan's program open (one server per Ghidra process, on 127.0.0.1:8089..8104; each serves every program open in its process, named per request) and talks to it. Library: `Ghidra(program).http(method, endpoint, body=None, **query)` -> the parsed JSON answer (`program` added to the query string; a POST's parameters in `body`, sent as JSON; raises `GhidraError` when no server has the program open, `GhidraRequestError` with `.code` on an `{"error": ...}` answer), `.up()`, `find()`, `scan()`, module-level `http()` / `up()` for the configured program. CLI: `list` (every server, `*` = YamiPan's), `which` (what the tools resolve to, or why none), `launch` (start Ghidra on the project unless a server has the program open). |
| `ghidra_bridge.py` | The `ghidra` MCP server of `.mcp.json`: bethington's own bridge (`bridge_mcp_ghidra` from the plugin checkout, `data/ghidra.json` "bridge"), auto-connected to YamiPan's project, with a note to pass `program=` and `strict_mode="off"`. Run by the venv that has `mcp` < 2 (`.mcp.json`), never by hand. |

Configuration: `data/ghidra.json` (`project` = the Ghidra project's path without `.gpr`, for `launch`
and to match the server; `program` = the game's program as Ghidra names it; `ports` = "8089:16";
`ghidra_dir`; `bridge` = the plugin checkout). Environment variables override it: `GHIDRA_MCP_PROJECT`,
`GHIDRA_MCP_PROGRAM`, `GHIDRA_MCP_PORTS`, `GHIDRA_DIR`, `GHIDRA_MCP_BRIDGE`; `GHIDRA_MCP_URL` pins one
server's port and skips the scan (the program is still checked). The resolved port is cached in
`work/ghidra_instance.json`.

Every other tool reaches Ghidra only through `yamipan.py`: `http()`, `paged(endpoint, key)`,
`function_names()`, `function_at()`, `has_function()`, `rename()` (naming gate off), `decompile()`,
`disassemble_function()`, `save_program()` (lock retry; Ghidra down: returns "Ghidra has unsaved
changes: Ctrl+S"), `create_function()` (lock retry; Ghidra down: False), `set_tag()` (`data/tags.json`,
and Ghidra's function tags while it answers). `ghidra.py asm` / `ghidra.disasm()` are offline
(`mipsdis.py`): 7.0.0's `disassemble_bytes` has no dry run.

The API, how the plugin behaves, the setup and the traps: the skill's `references/ghidra.md`.

## Tools

| script | what it does |
|---|---|
| `yamipan.py` | Paths, the address map (`data/layout.json`), the Ghidra helpers above (create with lock retry, rename, tag, save with retry), the provenance record `data/tags.json`, cached state. Import it; not a CLI. |
| `elf32.py` | Minimal ELF32 reader: sections (with names), segments, VA->bytes, `_gp` from `.reginfo`. FS2's ELF has one unnamed section for the image: the game ELF gets `layout.json` "image_sections" (the address map's regions: `.text`, `.vutext`, `.data`, the two code islands, `.bss`) instead. |
| `gcc2.py` | GCC 2.x name mangling: `demangle()` / `ghidra_name()` / `mangle()`. FS2 keeps no mangled names; used for reference names and `display()`. CodeWarrior's mangling is ARM-style like GCC 2's (unchecked). |
| `fetch.py` | Function starts, names, opcode hashes and instruction counts (paged `get_function_hash`), and every function's provenance tag (`data/tags.json`). |
| `features.py` | Per-function features decoded from the ELF: call order, tail jumps (`j` out of the body), function pointers, strings, floats, data refs, instruction mix. Bodies are [start, next start), so sizes include code Ghidra has no function for. |
| `sections.py` | The ELF's own symbols (named linkonce sections, `.ctors` / `.dtors`). GCC 2 only: FS2 has none; kept because export / features / rtti import it. Its CodeWarrior counterpart (static initialisers in the code islands?) is open. |
| `libmatch.py` | Library / runtime names: each reference library function (FF3's SDK 3.0 libs from ReiPan's `ff3_functions.txt`; FF2's runtime, newlib and SDK 2.8 from MioPan's `symbols.txt`) compared word for word with relocations masked, at FS2's function starts and after every `jr ra`. Unique matches only; identical bodies under several names are settled by their callees. `--list`, `--ranges` (library spans for `layout.json`), `--apply`. Needs the reference ELFs next to their lists (not in the checkouts on this machine). |
| `rxcmp.py` | `python rxcmp.py <name-regex> [--lo A --hi A] [--show NAME] [--data]` -- a library FS2 shares with Rule of Rose, compared word for word by name in both ELFs (`yamipan_functions.txt` / `../RosePan/ror_functions.txt`, RoR's ELF in `Decomp\Rule of Rose`): j / jal targets, `lui` and gp / lui-based offsets masked. One line per function that differs ('size', '!' with the offsets) or exists on one side only, then the identical count. `--show NAME` prints both disassemblies side by side, '*' on the differing words; `--data` pairs the data addresses the identical functions form (RoR -> FS2 with the delta and users): the data map for a carried-over TU. Offline. First used for `rx/rxsys.c` (126 of 126 identical). |
| `rxalign.py` | `python rxalign.py <lo> <hi> <ror_lo> <ror_hi> [--min-run N] [--batch out.json] [--lib TEXT] [--missing]` -- SCE's rx middleware is the library Rule of Rose links: aligns FS2's function sizes in address order with `../RosePan/ror_functions.txt` (longest common subsequence, fewest breaks among the longest), keeps pairs in runs of >= N (3) ('='), fills the gaps between them by partitioning address spans ('~': Ghidra merged small functions; the merged ones print '+' and `--missing` adds them to `data/missing_functions.json`; a lone pair left by changed code prints '^', read it) and writes RosePan's names as a `name_ctx.py` batch (lib-context). Rerun over a named range: 0 entries means nothing to rename. Offline. Unverified word for word until RoR's ELF or Ghidra program is here. Works for any library both games link in the same order (batch 010: the SIF cmd / rpc library, `rxalign.py 139bc0 13b600 260508 262300`; batch 012: libmpeg in two ranges, its archive members linked in another order); `--lib` names the library in the whys (default "rx library"), and RoR's own placeholders (`FUN_...`) never go into a batch. |
| `codeptrs.py` | `python codeptrs.py [--missing] [--create]` -- code pointers Ghidra has no function for: lui + addiu/ori pairs forming a code address (with what the register is used for next: `arg a0..t1` of the next jal, `store`, `jr`) and data words pointing into code. Classes them `start` (after `jr ra` or a tail `j` to a known start, then delay slot and nops), `case` (two or more adjacent data words into one body: a switch table) or `interior` (read it). `--missing` adds the starts to `data/missing_functions.json`, `--create` creates them in Ghidra and saves. Offline except `--create`; run `refresh.py` afterwards. 2026-11-02: 39 starts created, 0 left; 29 interior (the first code island's entry points among them). |
| `rtti.py` | Classes from vtables, CodeWarrior ABI (rewritten at bootstrap 4): tables the code builds with lui/addiu and stores (`sw`), plus same-shaped ones between them (`{type_info* or 0, 0, fn...}`, 4-byte words); names from the type_info, `data/class_names.json` or `Vt_<addr>`; bases from type_info lists or co-stored shorter tables. `--missing` writes slot targets Ghidra has no function for to `data/missing_functions.json` (create them: `ghidra.py mkfunc`, or F in Ghidra). |
| `export.py` | `yamipan_functions.txt`: every function with size, name and provenance, grouped into source files: pinned ranges, then string anchors (FS2's own `file.c` / `.cpp` strings), class anchors, then link-order fill. Writes `work/class_files.json`. |
| `status.py` | The generated status page (seeding state, naming, placement, reconstruction, TUs, frontier). |
| `refresh.py` | fetch + features (if Ghidra is up), sections, rtti, export, status. |
| `tuid.py` | Is the TU the file its key says? Source-path strings, other strings, classes, name prefixes, caller / callee TUs (also in `tus.py brief`). |
| `stage.py` | `python stage.py <names\|addrs\|file:x.cpp\|class:Name\|range:lo-hi> [--asm]` -- per-function bundle in `work/stage/`: class role and vptr offset, callers, callees, tail jumps, strings, floats, decompilation (and disassembly). |
| `ctxcard.py` | `--top N` most-called unnamed game functions (or given addresses): callers, callees, tail jumps, strings, floats, class role, named neighbours with files, decompilation. |
| `name_ctx.py` | `python name_ctx.py <batch.json>` -- apply hand names (`game-inferred` / `lib-context`) with evidence to Ghidra, `data/tags.json` and `data/context_names.json`; refuses whitespace and duplicates, never renames a `game-symbol`; replacing a library match records `data/overrides.json`; saves (save_program), then refresh.py. |
| `modview.py` | `range <lo> <hi>` / `tree <func> [depth]` -- one line per function: name, provenance, size, call sites, callees, strings. Offline. |
| `dec.py` | `python dec.py <addr\|name> ...` -- decompile. |
| `ghidra.py` | `asm` (read-only disassembly of any range), `func`, `xrefs`, `refs` (xrefs + `jal`/`j` encodings + data words + low halves: "is it really unreferenced"), `find` (over the ELF's bytes, with an optional bit mask), `segments`, `mkfunc` (create + save). |
| `mipsdis.py` | Offline R5900 disassembler over the ELF, annotated with cached names; for code Ghidra has no function for, and **for VU0 macro code** (decodes COP2 with field masks and broadcast lanes, which the decompiler loses; lqc2 / sqc2 still print the GPR name of the vf number). |
| `reach.py` | `reach.py <name|addr> ... [--list]`, `--slots <vtable> <n>`: the unwritten game functions under a root, transitively (direct calls only, so a floor for comparing roots). `references/roadmap-3d-control.md` is made with it. Offline. |
| `sssdis.py` | `sssdis.py <script file> [--natives lo-hi]`: the stage scripts ("SSS", VM 0x1d1c40) read statically: each entry and its native calls with the constant arguments; by default the lighting natives 0x1b3..0x1c3. |
| `sssnatives.py` | `sssnatives.py`: writes `src/game/scriptnatives.inc`, the pops, code words and address of all 541 script natives read from the ROM, so an unwritten native still leaves the VM right. `--list lo-hi` prints them with size, `acc`, `yield` and callees. |
| `sssall.py` | `sssall.py`: every stage script on the disc (raw and inside the SLDI packs) into `work/scripts/`, with `work/sssall.json`: which natives each calls. `--table` prints the count per native, `--unwritten` only those `src/game/scriptrunner.cpp` does not bind, most used first: the order to write natives in. `sssdis.py` reads the files it leaves. A few minutes; offline. |
| `vudis.py` | VU microcode over the ELF, following the image's VIF MPG codes: `vudis.py 2ec908 3f8 --all` is FS2's VU1 program (the image's DMA tag is the first 8 bytes). A lower word that is a LOI (the upper's bit 31 set) prints as `loi 255`. |
| `eerun.py` | A fuller EE interpreter than mipsemu: byte memory holding the ELF, 128-bit registers, FPU, the VU0 macro ops seen so far, real calls, a bump heap behind operator_new. Runs the ROM's own non-leaf functions from Python. |
| `wayoutcmp.py` | `wayoutcmp.py [files] [questions]`: `findWayOut` of game/characterroute.cpp against the ROM's 0x227f30 on rute files from the disc; the host function is cut out of the source and compiled with routeplayer.cpp; 0x136560 is hooked, `eerun.py` has no vsqrt. |
| `rutecmp.py` | `rutecmp.py <dir> [queries] [--files N]`: game/routeplayer.cpp against the ROM over rute files from the disc (magic `ord `, raw and in SLDI packs), questions and answers on both sides, prints the differences and which kinds of answer it covered. The pattern for any function a smoke run cannot reach. |
| `mipsemu.py` | A tiny EE integer interpreter: run a leaf function over every input to check a C rewrite of a switch-table function; `Emu` with load / call hooks for non-leaf ones. |
| `sinit.py` | Run a static initializer's straight-line stores symbolically and dump the table it fills. |
| `datadump.py` | `python datadump.py <addr> <count> "<fields>"` -- a data table as host C++ initializers (pointers printed as the symbols they point at). |
| `tm2png.py` | TIM2 -> PNG (4/8-bit indexed with CSM1 CLUTs, 16/24/32-bit, PS2 alpha scaled): files, or `--scan` to carve every embedded TIM2 out of an archive (`ROM.00x`). The reference a drawn screen is checked against. |
| `restore_names.py` | `--snapshot` every name before bulk writes; restore from a snapshot. |
| `shotscan.py` | Brightness of three rows of each `work/shots` PNG in numeric order (`LOW HIGH STEP`), to find bars, fades and scene changes: shot numbers are presents, not game frames. |
| `build.py` | Build (MinGW + ninja, configures on first use, reuses a sibling's fetched deps), `--run N [--pad S] [--shot-every F] [--env K=V]` smoke run with screenshots and stub hits. Needs the bootstrapped PC project (bootstrap 6) and a toolchain (none on this machine yet). |
| `stubgen.py` | Undefined references -> `YAMIPAN_STUB` definitions in their TU's file (address from a trailing `// FS2 0x...` on the declaration, else the Ghidra name, also `Class::Method`). |
| `callcheck.py` | Calls in each `FS2 0x...`-annotated function vs FS2's callees (jal + tail jumps), by name's last component or the source definition at that address; `// callcheck: A, B (why)` explains invisible calls. |
| `layoutcheck.py` | Every game header on its own under MinGW GCC and MSVC so both evaluate the `static_assert` layouts; the game, `src/sdk` and the Pan layer under cl with each target's `/permissive` setting, so a Visual Studio build stays green (not the flags in CMakeLists.txt: hazards.md); lints structs without asserts (or a `layout: host` note), plain `long`, `wchar_t`, `packed`. |
| `gdbrun.py` / `gdbdrive.py` | The game under MinGW's gdb: a fault holds it and writes `work/gdb/crash.txt`; `--cmd` inspects, `--break` stops a hang. |
| `crashsym.py [log]` | The `[crash]` lines a plain run prints on a fault (`yamipan::crash`, `core/crash_report.cpp`) as functions and source lines, through addr2line; default log `work/run_relwithdebinfo.log`. Try this before gdb: a fault that depends on memory or timing can vanish there. |

## Provenance tags

Kept in `data/tags.json` (address -> tag; the record, committed) and mirrored to Ghidra's function tags
while Ghidra answers.

| tag | meaning |
|---|---|
| `game-symbol` | FS2's own symbol. The retail ELF is stripped: only if a symbolised build turns up. Never renamed. |
| `lib-match` | Library / runtime code matched word for word (relocations masked) to a symbolised reference. |
| `lib-match-low` | A short body (3..5 instructions), no clean function end, or names the references disagree on: a hint. |
| `lib-context` | A library / runtime function named by reading it. |
| `game-inferred` | A game function named from its code and context (strings, vtable, callers). |

`data/` holds what must survive: `ghidra.json`, `tags.json`, `tus.json` (TU state, queue, settings;
bootstrap 7), `layout.json` (image sections, library and satellite ranges), `context_names.json` (hand
names with evidence), `overrides.json` (library matches replaced by hand), `lib_matches.json` (applied
library names).

## Ghidra naming convention

Ghidra names are C++ names as written in source: `Class::method`, `Class::~Class`; free functions
plain (`main`). GhidraMCP 2.0.0 kept `::` literally in a flat name (no namespaces), accepted templates
and `$`, and rejected whitespace (7.0.0 with `strict_mode` off: `rx*` and `Class::method` names checked) (`operator new` -> `operator_new`). Two functions with the same
qualified name carry their parameters without spaces. `yamipan_functions.txt`, briefs and callcheck
read these names.

## Platform contract (what the tools need from the PC project)

The Pan layer (`src/yamipan/`, bootstrap 6, written as classes: skill `references/cpp-port.md`) keeps
these hooks:

| hook | where (RosePan) | used by |
|---|---|---|
| `cmake-build-<config>/YamiPan/YamiPan.exe`, target `YamiPan`, `compile_commands.json` | CMakeLists.txt | build.py, layoutcheck.py, gdbrun.py |
| `YAMIPAN_SHOT_DIR` / `YAMIPAN_SHOT_EVERY`: save every Nth presented frame as `shot_<frame>.png` + `last.png` | `rendering::Screenshotter` | build.py --run, tus.py finish (progress/) |
| `YAMIPAN_AUTOPAD`: scripted pad 0, `<read>:<BUTTON>[*<reads>],...`, host input masked | `io::InputSystem` | build.py --pad, settings.smoke_pad |
| `YAMIPAN_DATA_DIR`: the game's data folder | `Paths` | build.py (`yamipan.GAME_DATA`) |
| `YAMIPAN_TEX_DUMP=<dir>`: write every newly decoded TIM2 file to `<dir>/<hash>.tm2`, for `tm2png.py` | `rendering::TextureStore` | `build.py --env`, a screen's references |
| the EE main thread named `YamiPanGame` | `ee::Kernel` | gdbrun.py / gdbdrive.py |
| `[stub] <name> (FS2 0x%08x) reached` from `YAMIPAN_STUB` / `YAMIPAN_BRIDGE` | common/yamipan_stub.h | tus.py (reached stubs rank first), status.py, stubgen.py |
| log lines `[W]` / `[E]` for warnings and errors on stdout | `log::` | build.py --run summary |

## Things that bit (FS2-specific ones in the skill's hazards.md, Ghidra ones in ghidra.md)

- **7.0.0 answers errors with HTTP 200 `{"error": ...}`** and serves every open program: always go
  through `ghidra_mcp.py` (it adds `program=` and raises on errors), never the port directly.
- **`disassemble_bytes` writes** (no dry run): never probe writes. Writes collide with saves ("Unable
  to lock due to active transaction"): `create_function` and `save_program` retry.
- **A plugin built for another Ghidra half works**: `get_functions` (decompile, function at an
  address) fails with `SleighLanguageDescription`; build it for the Ghidra in use (ghidra.md).
- **Never pipe `name_ctx.py` / `refresh.py` into `head`**: the closed pipe kills the refresh and leaves
  stale names behind (stubs at address 0, callcheck "missing" calls).
- `python -I` drops the script directory from `sys.path`; run the tools as `python tool.py` from here.

