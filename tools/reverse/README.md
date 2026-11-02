# YamiPan reversing tools

YamiPan is the fourth PS2 port by the same author (MioPan: Fatal Frame 2, ReiPan: Fatal Frame 3,
RosePan: Rule of Rose); its game is Forbidden Siren 2 (Europe, `SCES_538.51`, 2006-05-17). The ELF is
stripped (one unnamed section for the whole image, no symbols) and was built with Metrowerks
CodeWarrior, not GCC; its Sony libraries are the SDK 3.0 objects FF3 and RoR link. These tools turn the
binary into names, files and translation units and drive the one-TU-per-session port. They were carried
over from RosePan, the newest set, on 2026-10-31 (bootstrap 3) and moved onto GhidraMCP 2.0.0.

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

**`tus.py` and `autopilot.py` are not carried over yet**: the routine switches from bootstrap steps to
the TU loop as soon as `tus.py` exists, so it arrives with bootstrap 7 (first build, first TU), and
`autopilot.py` (which needs it, and which sessions never edit) with it. `status.py` and `tuid.py`
import `tus` lazily and skip the TU section until then.

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

## Ghidra: GhidraMCP 2.0.0, several instances, 5.14.2 beside it

| script | what it does |
|---|---|
| `ghidra_mcp.py` | Finds the GhidraMCP 2.0.0 backend serving YamiPan's program (one backend per CodeBrowser window, on 127.0.0.1:8080..8095; each serves the program active in it) and talks to it, checking before every request that the program is still the active one. Library: `Ghidra(program).http(method, endpoint, **params)` -> the answer's parsed `data` (raises `GhidraError` when no backend serves the program, `GhidraRequestError` with `.code` when the backend reports an error), `.up()`, `find()`, `scan()`, module-level `http()` / `up()` for the configured program. `Ghidra5(program)`: the GhidraMCP 5.14.2 client for what 2.0.0 lacks (one server per Ghidra process on 8089..8104, config `ports5`, found by project + open program; `program` in the query string; text answers; untested until 5.14.2 is installed). CLI: `list` (every backend, `*` = YamiPan's), `which` (what the tools resolve to, or why none), `launch` (start Ghidra on the project unless a window serves the program). |
| `ghidra_bridge.py` | The `ghidra` MCP server of `.mcp.json`: the GhidraMCP 2.0.0 wheel's own server, its HTTP client re-aimed before every request by `ghidra_mcp.py`'s check, plus `list_instances` and `connect_instance`. Run by the venv that has the wheel (`.mcp.json`), never by hand. |

Configuration: `data/ghidra.json` (`project` = the Ghidra project's path without `.gpr`, for `launch`;
`program` = the game's program as Ghidra names it; `ports` = "8080:16"; `ports5` = "8089:16" for
5.14.2; `ghidra_dir`). Environment variables override it: `GHIDRA_MCP_PROJECT`, `GHIDRA_MCP_PROGRAM`,
`GHIDRA_MCP_PORTS`, `GHIDRA_DIR`; `GHIDRA_MCP_URL` pins one 2.0.0 backend's port and skips the scan (its
program is still checked). The resolved ports are cached in `work/ghidra_instance.json`.

Every other tool reaches Ghidra only through `yamipan.py`:

| helper | plugin | without 5.14.2 |
|---|---|---|
| `http()`, `paged()`, `function_names()`, `function_at()`, `has_function()`, `rename()`, `decompile()`, `disassemble_function()` | 2.0.0 | -- |
| `http5()`, `save_program()`, `create_function()`, `ensure_tags()`, Ghidra's copy of `set_tag()`, `ghidra.disasm()` (`disassemble_bytes`), fetch's bulk hashes | 5.14.2 | save: returns "Ghidra has unsaved changes: Ctrl+S"; create: False (press F in Ghidra); tags: `data/tags.json` only; disassembly: `mipsdis.py` offline; hashes: empty |

The API, how the plugin behaves, the setup and the traps: the skill's `references/ghidra.md`.

## Tools

| script | what it does |
|---|---|
| `yamipan.py` | Paths, the address map (`data/layout.json`), the Ghidra helpers above (create with lock retry, rename, tag, save with retry), the provenance record `data/tags.json`, cached state. Import it; not a CLI. |
| `elf32.py` | Minimal ELF32 reader: sections (with names), segments, VA->bytes, `_gp` from `.reginfo`. FS2's ELF has one unnamed section for the image: the game ELF gets `layout.json` "image_sections" (the address map's regions: `.text`, `.vutext`, `.data`, the two code islands, `.bss`) instead. |
| `gcc2.py` | GCC 2.x name mangling: `demangle()` / `ghidra_name()` / `mangle()`. FS2 keeps no mangled names; used for reference names and `display()`. CodeWarrior's mangling is ARM-style like GCC 2's (unchecked). |
| `fetch.py` | Function starts and names (2.0.0's paged `functions`; hashes only from 5.14.2), and every function's provenance tag (`data/tags.json`). |
| `features.py` | Per-function features decoded from the ELF: call order, tail jumps (`j` out of the body), function pointers, strings, floats, data refs, instruction mix. Bodies are [start, next start), so sizes include code Ghidra has no function for. |
| `sections.py` | The ELF's own symbols (named linkonce sections, `.ctors` / `.dtors`). GCC 2 only: FS2 has none; kept because export / features / rtti import it. Its CodeWarrior counterpart (static initialisers in the code islands?) is open. |
| `libmatch.py` | Library / runtime names: each reference library function (FF3's SDK 3.0 libs from ReiPan's `ff3_functions.txt`; FF2's runtime, newlib and SDK 2.8 from MioPan's `symbols.txt`) compared word for word with relocations masked, at FS2's function starts and after every `jr ra`. Unique matches only; identical bodies under several names are settled by their callees. `--list`, `--ranges` (library spans for `layout.json`), `--apply`. Needs the reference ELFs next to their lists (not in the checkouts on this machine). |
| `rxalign.py` | `python rxalign.py <lo> <hi> <ror_lo> <ror_hi> [--min-run N] [--batch out.json]` -- SCE's rx middleware is the library Rule of Rose links: aligns FS2's function sizes in address order with `../RosePan/ror_functions.txt` (longest common subsequence), keeps pairs in runs of >= N (3) and writes RosePan's names as a `name_ctx.py` batch (lib-context). Offline. Unverified word for word until RoR's ELF or Ghidra program is here. |
| `rtti.py` | Classes from vtables, CodeWarrior ABI (rewritten at bootstrap 4): tables the code builds with lui/addiu and stores (`sw`), plus same-shaped ones between them (`{type_info* or 0, 0, fn...}`, 4-byte words); names from the type_info, `data/class_names.json` or `Vt_<addr>`; bases from type_info lists or co-stored shorter tables. `--missing` writes slot targets Ghidra has no function for to `data/missing_functions.json` (create them: `ghidra.py mkfunc` on 5.14.2, or F in Ghidra). |
| `export.py` | `yamipan_functions.txt`: every function with size, name and provenance, grouped into source files: pinned ranges, then string anchors (FS2's own `file.c` / `.cpp` strings), class anchors, then link-order fill. Writes `work/class_files.json`. |
| `status.py` | The generated status page (seeding state, naming, placement, reconstruction, TUs, frontier). |
| `refresh.py` | fetch + features (if Ghidra is up), sections, rtti, export, status. |
| `tuid.py` | Is the TU the file its key says? Source-path strings, other strings, classes, name prefixes, caller / callee TUs. Needs `tus.py` (bootstrap 7). |
| `stage.py` | `python stage.py <names\|addrs\|file:x.cpp\|class:Name\|range:lo-hi> [--asm]` -- per-function bundle in `work/stage/`: class role and vptr offset, callers, callees, tail jumps, strings, floats, decompilation (and disassembly). |
| `ctxcard.py` | `--top N` most-called unnamed game functions (or given addresses): callers, callees, tail jumps, strings, floats, class role, named neighbours with files, decompilation. |
| `name_ctx.py` | `python name_ctx.py <batch.json>` -- apply hand names (`game-inferred` / `lib-context`) with evidence to Ghidra, `data/tags.json` and `data/context_names.json`; refuses whitespace and duplicates, never renames a `game-symbol`; replacing a library match records `data/overrides.json`; saves (5.14.2), then refresh.py. |
| `modview.py` | `range <lo> <hi>` / `tree <func> [depth]` -- one line per function: name, provenance, size, call sites, callees, strings. Offline. |
| `dec.py` | `python dec.py <addr\|name> ...` -- decompile. |
| `ghidra.py` | `asm` (read-only disassembly of any range), `func`, `xrefs`, `refs` (xrefs + `jal`/`j` encodings + data words + low halves: "is it really unreferenced"), `find` (over the ELF's bytes, with an optional bit mask), `segments`, `mkfunc` (5.14.2: create + save). |
| `mipsdis.py` | Offline R5900 disassembler over the ELF, annotated with cached names; for code Ghidra has no function for. |
| `mipsemu.py` | A tiny EE integer interpreter: run a leaf function over every input to check a C rewrite of a switch-table function; `Emu` with load / call hooks for non-leaf ones. |
| `sinit.py` | Run a static initializer's straight-line stores symbolically and dump the table it fills. |
| `datadump.py` | `python datadump.py <addr> <count> "<fields>"` -- a data table as host C++ initializers (pointers printed as the symbols they point at). |
| `tm2png.py` | TIM2 -> PNG (4/8-bit indexed with CSM1 CLUTs, 16/24/32-bit, PS2 alpha scaled): files, or `--scan` to carve every embedded TIM2 out of an archive (`ROM.00x`). The reference a drawn screen is checked against. |
| `restore_names.py` | `--snapshot` every name before bulk writes; restore from a snapshot. |
| `build.py` | Build (MinGW + ninja, configures on first use, reuses a sibling's fetched deps), `--run N [--pad S] [--shot-every F] [--env K=V]` smoke run with screenshots and stub hits. Needs the bootstrapped PC project (bootstrap 6) and a toolchain (none on this machine yet). |
| `stubgen.py` | Undefined references -> `YAMIPAN_STUB` definitions in their TU's file (address from a trailing `// FS2 0x...` on the declaration, else the Ghidra name, also `Class::Method`). |
| `callcheck.py` | Calls in each `FS2 0x...`-annotated function vs FS2's callees (jal + tail jumps), by name's last component or the source definition at that address; `// callcheck: A, B (why)` explains invisible calls. |
| `layoutcheck.py` | Every game header on its own under MinGW GCC and MSVC so both evaluate the `static_assert` layouts; lints structs without asserts (or a `layout: host` note), plain `long`, `wchar_t`, `packed`. |
| `gdbrun.py` / `gdbdrive.py` | The game under MinGW's gdb: a fault holds it and writes `work/gdb/crash.txt`; `--cmd` inspects, `--break` stops a hang. |

## Provenance tags

Kept in `data/tags.json` (address -> tag; the record, committed) and mirrored to Ghidra's function tags
while 5.14.2 answers (2.0.0 has no tags).

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
plain (`main`). GhidraMCP 2.0.0 keeps `::` literally in a flat name (no namespaces), accepts templates
and `$`, and rejects whitespace (`operator new` -> `operator_new`). Two functions with the same
qualified name carry their parameters without spaces. `yamipan_functions.txt`, briefs and callcheck
read these names.

## Platform contract (what the tools need from the PC project)

Bootstrap 6 copies RosePan's platform layer; these hooks must survive the rename:

| hook | where (RosePan) | used by |
|---|---|---|
| `cmake-build-<config>/YamiPan/YamiPan.exe`, target `YamiPan`, `compile_commands.json` | CMakeLists.txt | build.py, layoutcheck.py, gdbrun.py |
| `YAMIPAN_SHOT_DIR` / `YAMIPAN_SHOT_EVERY`: save every Nth presented frame as `shot_<frame>.png` + `last.png` | the renderer | build.py --run, tus.py finish (progress/) |
| `YAMIPAN_AUTOPAD`: scripted pad 0, `<read>:<BUTTON>[*<reads>],...`, host input masked | sdk/libpad.cpp | build.py --pad, settings.smoke_pad |
| `YAMIPAN_DATA_DIR`: the game's data folder | io paths | build.py ($YAMIPAN_DATA) |
| `YAMIPAN_GDB_DIR`, the game thread named `YamiPanGame` | os host | gdbrun.py / gdbdrive.py |
| `[stub] <name> (FS2 0x%08x) reached` from `YAMIPAN_STUB` / `YAMIPAN_BRIDGE` | common/yamipan_stub.h | tus.py (reached stubs rank first), status.py, stubgen.py |
| log lines `[W]` / `[E]` for warnings and errors on stdout | io log | build.py --run summary |

## Things that bit (FS2-specific ones in the skill's hazards.md, Ghidra ones in ghidra.md)

- **2.0.0 serves the active tab**: `ghidra_mcp.py` checks it before every request; nothing else may
  talk to the port directly.
- **Nothing is saved without 5.14.2**: after a naming batch, save in Ghidra (Ctrl+S) and say so in
  the report.
- **5.14.2: `program` must be in the query string** (`Ghidra5` puts it there); `dry_run` in a POST
  body is not honoured: never probe writes. Writes collide with saves ("Unable to lock due to active
  transaction"): `create_function` and `save_program` retry.
- **Never pipe `name_ctx.py` / `refresh.py` into `head`**: the closed pipe kills the refresh and leaves
  stale names behind (stubs at address 0, callcheck "missing" calls).
- `python -I` drops the script directory from `sys.path`; run the tools as `python tool.py` from here.
