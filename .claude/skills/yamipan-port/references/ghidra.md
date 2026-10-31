# Ghidra: GhidraMCP 2.0.0, one backend per window, several Ghidras

YamiPan's first piece of work (2026-10-30) made several Ghidras run side by side on bethington's
GhidraMCP 5.14.2. On 2026-10-31 the user moved to Ghidra 12.2 with **LaurieWired's GhidraMCP 2.0.0**
on another machine, and the tools were rewritten for it. Facts below were read from its source
(`../GhidraMCP`: `extension/` is the plugin, `src/ghidra_mcp/` the Python MCP server) and tested live
against Forbidden Siren 2. 5.14.2's facts and traps are in this file's git history before 2026-10-31
and in ReiPan's / RosePan's skills.

## How GhidraMCP 2.0.0 serves

- **One backend per CodeBrowser window**, not per process: each window's plugin starts its own HTTP
  server on `127.0.0.1:<port>`, the port from Tool Options -> GhidraMCP HTTP Server -> Server Port
  (default 8080; a change takes effect after a restart or plugin reload). **No fallback**: a second
  window or Ghidra on a taken port logs "Could not start the GhidraMCP backend on 127.0.0.1:8080. Is
  the port in use" (`%APPDATA%\ghidra\ghidra_12.2_PUBLIC\application.log`) and serves nothing.
- **It serves the program active in its window** (`ProgramManager.getCurrentProgram()`). Requests have
  no `program` parameter (unknown parameters are ignored): bringing another program's tab to the front
  retargets every request, writes included.
- **One request at a time** on one thread; a decompile may take up to 60 s (its timeout), and a busy
  backend answers `/health` only after it.
- Loopback only; requests with an `Origin` header or a foreign `Host` are refused.
- Answers: `{"success": true, "data": ...}` or `{"success": false, "error": {"code", "message"}}` with an
  HTTP status (`FUNCTION_NOT_FOUND` 404, `UNKNOWN_ENDPOINT` 404, `METHOD_NOT_ALLOWED`, `BAD_ADDRESS`,
  `MISSING_PARAMETER`, `INVALID_PARAMETER`, `NOT_FOUND`, `INTERNAL_ERROR`, ...). GET and POST routes are
  strict about the method; parameters come from the query string and a form body, merged.
- **`GET /health`**: `{status, version, ghidra_version, mode: gui|headless, capabilities: [gui_context],
  program: {name, project_path, language_id} | null}`. `project_path` is the program's file *inside*
  the project (`/SCES_538.51`). No pid, no project name, no list of open programs.
- **Writes**: each in its own transaction on the Swing thread, `USER_DEFINED`; undoable with Ctrl+Z in
  Ghidra. **There is no save**: changes stay in the open program until Ghidra saves it.
- Lists page: `{items, offset, limit, total, has_more}`, `limit` clamped to 1..2000.
- The release's extension says `version=12.1.4` and loads and runs on 12.2.
- Headless mode exists (`ghidra-mcp --headless --binary ... --project DIR`, PyGhidra, Python 3.10-3.13):
  not set up here (the machine has Python 3.14 only).

The API (GhidraMCP 2.0.0, `BackendHttpServer.registerRoutes`):

| GET | parameters |
|---|---|
| `health`, `program_info` | -- |
| `functions` | `filter` (name substring), `offset`, `limit` (100) |
| `classes`, `namespaces`, `segments`, `imports`, `exports`, `data` | `offset`, `limit` |
| `strings` | `filter` (content substring), `offset`, `limit` (200) |
| `function`, `decompile_function` | `address` (at or containing) |
| `decompile` | `name` (fails when ambiguous) |
| `disassemble_function` | `address`, `offset`, `limit` (500): `{address, bytes, instruction, comment}` |
| `xrefs_to`, `xrefs_from` | `address`, `offset`, `limit` |
| `function_xrefs` | `name` or `address` |
| `function_calls` | `address`, `direction` (both / callers / callees; indirect calls `direct: false`) |
| `read_memory` | `address`, `length` (64, at most 4096): hex, ascii, gaps |
| `current_address`, `current_function` | -- (the GUI's cursor) |

| POST | parameters |
|---|---|
| `rename_function_by_address` | `function_address`, `new_name` |
| `rename_function` | `old_name`, `new_name` (fails when ambiguous) |
| `rename_data` | `address`, `new_name` (renames the primary symbol, or creates a label) |
| `rename_variable` | `function_address` or `function_name`, `old_name`, `new_name` |
| `set_function_prototype` | `function_address`, `prototype` (C) |
| `set_local_variable_type` | `function_address`, `variable_name`, `new_type` |
| `set_decompiler_comment` / `set_disassembly_comment` | `address`, `comment` (PRE / EOL; empty clears) |

**Not there** (5.14.2 had them, RosePan's tools use some): save, create function / disassemble bytes,
function tags, bulk function hashes, creating or applying data types and structs, byte search, scripts,
batches, program selection. How bootstrap covers them is open (open-items.md).

Measured 2026-10-31 (FS2, 6,393 functions): `/health` and small reads 2-10 ms, a 2,000-function page
20-40 ms, a small function's decompile 260-280 ms, a request through `ghidra_mcp.py` (with its check)
about 8 ms.

## The setup

| what | where |
|---|---|
| Ghidra 12.2 | `C:\Users\willi\Documents\ghidra_12.2_PUBLIC_20261016\ghidra_12.2_PUBLIC` (extensions: GhidraMCP, ghidra-emotionengine-reloaded) |
| YamiPan's project, program | `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr`, `SCES_538.51` (`data/ghidra.json`) |
| its port | 8080 (the default) |
| client + CLI | `tools/reverse/ghidra_mcp.py` |
| Claude's MCP server | `.mcp.json` `ghidra` -> `tools/reverse/ghidra_bridge.py`, run by `C:\Users\willi\Documents\ghidra_12.2_PUBLIC_20261016\.venv\Scripts\python.exe` (the wheel `ghidra_mcp-2.0.0` installed) |
| the release | `C:\Users\willi\Documents\GhidraMCP-release-2-0\GhidraMCP-release-2-0` (wheel, extension zip) |

Updating the plugin: install the new extension zip in Ghidra (File -> Install Extensions, restart),
`pip install --force-reinstall <wheel>` with the venv's python, then `python ghidra_mcp.py list` and a
new Claude session (`.mcp.json` is read at session start).

How the tools pick a backend (`ghidra_mcp.find`): `GHIDRA_MCP_URL` pins a port (its program is still
checked); else the cache (`work/ghidra_instance.json`: program -> port, used only inside the configured
range); else a scan of 8080..8095 (parallel; a closed port fails after the 0.4 s connect, a busy backend
gets 10 s) matched on the active program (its name or its file path). Exactly one must fit; otherwise
the error lists what runs (and says when the project is open without a window serving it: its `.lock`
exists). **Every request then checks first** (`/health` on that port: the program still active, else
forget and re-scan). Only a failed connect is retried; a request that reached Ghidra may have written.
`GhidraError` (an `OSError`) means no backend or a broken connection; `GhidraRequestError` (`.code`,
`.message`, `.status`) is the backend's own error.

```
python ghidra_mcp.py list      # every backend: port, active program, versions; * = YamiPan's
python ghidra_mcp.py which     # YamiPan's backend, or why none
python ghidra_mcp.py launch    # start Ghidra on the project unless a window serves the program
```

The `ghidra` MCP server (`ghidra_bridge.py`) is the wheel's own server (`create_server`) around its HTTP
client, re-aimed before every request by `ghidra_mcp.py`'s check; a call with no window serving the
program fails with `NO_BACKEND` and the reason. Added tools: `list_instances` (every backend and its
active program) and `connect_instance(target)` (follow another program, by name or by the port whose
active program to follow, until the next call). Ghidra may start after the session: every tool is
registered anyway and finds it on its first call.

## Working with several

- **One Ghidra per game, each window on its own port** in 8080..8095. The tools find YamiPan's by its
  program, so the port only has to be free and in range. Avoid 8081 for neatness (the wheel's default
  for its HTTP transport, unused with stdio).
- **Keep SCES_538.51 the active tab** of the window that serves it, ideally the only program there.
  With another program in front, YamiPan's tools refuse (`NO_BACKEND`) until it is back.
- **Several Claude sessions on one Ghidra**: requests queue on the backend's one thread (a decompile
  holds the others up to 60 s). Two sessions naming one program still race in the name ledger: one
  naming session per program at a time.
- **One process per game** keeps a crash local: a JVM crash loses every unsaved change in that Ghidra.
- RosePan's and ReiPan's checkouts here are read-only references; their Ghidra projects and 5.14.2
  setup are not on this machine.

## Hazards (GhidraMCP 2.0.0)

- **The active tab is the target** (no `program` parameter). `ghidra_mcp.py` and the bridge check it
  before every request; anything else talking to the port directly does not.
- **Nothing is saved.** Save in Ghidra (Ctrl+S) after a naming batch, and before finishing check that
  the names landed (`function` by address): a crash, or "don't save" on close, loses them.
- **Fixed port, no fallback**: a window that lost the port serves nothing and only the log says so.
  `python ghidra_mcp.py list` shows what really answers.
- **"At or containing"**: `function`, `decompile_function`, `disassemble_function`, `function_calls`
  and `rename_function_by_address` take any address inside a body. "Is there a function at X" means
  the answer's `address` == X.
- **Names** (tested 2026-10-31): `Class::method`, `operator_new`, `f<int>$1(int)` are kept verbatim
  in the global namespace (no namespace is created); whitespace is refused (`INVALID_PARAMETER`).
  Renaming back to `FUN_<addr>` gives the name back (whether Ghidra counts it as a default name
  again was not checked).
- **`rename_variable`, `set_local_variable_type`, `set_function_prototype` commit the decompiler's
  parameters and locals to the database**: renaming back does not undo that; Ctrl+Z in Ghidra does.
- **The EE loader's RAM mirrors**: emotionengine-reloaded maps `RAM(uncached)` at 0x20000000 and
  `RAM(uncached_and_accelerated)` at 0x30100000, and auto-analysis defined strings there too: of
  5,421 strings only 2,298 are distinct, the rest copies at 0x2.../0x3.... Keep addresses below
  0x02000000.
- **gp-relative globals have no xrefs**: "no other writer" means nothing; search the asm for the gp
  offset.
- **Two `ghidra_mcp`s**: the wheel's package has the same name as `tools/reverse/ghidra_mcp.py`. Tools
  run from `tools/reverse/` get the file (a script's directory comes first on `sys.path`); the bridge
  drops its directory from the path and loads the file as `yamipan_ghidra`. Keep it that way.
- **A changed `.mcp.json` needs a new session**: Claude Code reads it at start (the old one pointed at
  another machine's E: and F: drives, so the `ghidra` server failed with "Connection closed").
