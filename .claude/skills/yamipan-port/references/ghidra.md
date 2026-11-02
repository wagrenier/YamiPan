# Ghidra: bethington's GhidraMCP 7.0.0, one server per process, several Ghidras

History: YamiPan's first piece of work (2026-10-30) ran on bethington's GhidraMCP 5.14.2; on 2026-10-31
the user moved to Ghidra 12.2 with LaurieWired's GhidraMCP 2.0.0 (one backend per window, no save, no
function creation); on **2026-11-02 the user installed bethington's GhidraMCP 7.0.0**, which took 2.0.0's
place (both install as `Extensions\GhidraMCP`, so only one can be there), and the tools were ported to
it the same day. 2.0.0's facts are in this file's git history before 2026-11-02. Facts below were read
from 7.0.0's source (`../ghidra-mcp`: `src/main/java` the plugin, `python/bridge_mcp_ghidra` the MCP
bridge) and its live schema, and tested against Forbidden Siren 2.

## How GhidraMCP 7.0.0 serves

- **One HTTP server per Ghidra process** on `127.0.0.1:8089` (the default; another Ghidra takes another
  port), serving **every program open in that process**: each request names its program with
  `program=` in the query string (omitted: the active one). The tools always pass it.
- `GET /mcp/instance_info`: `{pid, version, project, project_path, programs: [{name, path, open}],
  tcp_port}` (every program of the project, `open` or not); `GET /mcp/health`: versions, the active
  program, uptime; **`GET /mcp/schema`: every endpoint with its method and parameters** (205 in 7.0.0;
  read it rather than guessing).
- Parameters: GET in the query string; POST in a **JSON body**, with `program` still in the query string.
- Answers: plain JSON per endpoint; **a failure is `{"error": "..."}` with HTTP 200**; an unknown path is
  an HTML 404.
- Lists page with `offset` / `limit` and report `total` (`get_function_hash`: `total_matching`); the
  list's key varies (`functions`, `references`, `items`, `instructions`). `limit` 10000 is accepted.
- **Writes**: transactions in the open program; **`GET save_program` saves** (`{"saved": true}`, or
  "No unsaved changes"). `create_function {address, name, disassemble_first}` creates a function (a
  merged body shrinks to fit).
- `rename_function {old_name: name or 0x<addr>, new_name, strict_mode}`: a **naming gate** wants
  PascalCase verbs; YamiPan passes `strict_mode: "off"` (the game's style is rx*, `Class::method`).
  The gate's "not PascalCase" warnings with it off are harmless.
- **Needs a build for Ghidra 12.2.** The release built 2026-10-21 for 12.1.4 loads on 12.2 but every
  endpoint that touches the language description fails with
  `Error in /get_functions: ghidra/program/model/lang/SleighLanguageDescription` (12.2 moved the
  class): `get_functions` (decompile, function at an address, tags), `force_decompile`. Fixed upstream
  2026-10-28 (b06c679d); build it from `../ghidra-mcp` (below).

The calls the tools make (`yamipan.py`, `ghidra.py`, `fetch.py`):

| endpoint | used for |
|---|---|
| `GET find_functions` (`sort_by=address`, paged, key `functions`) | `function_names()` |
| `GET get_functions` (`function=0x..`, `fields=`) | `decompile()` (`decompiled_code`), `function_at()` (`entry_point`, `body_start`, `body_end`, `signature`), current tags |
| `GET disassemble_function` (`address`) | `disassemble_function()`: `{instructions: [{address, instruction}]}`, delay slots with `_` |
| `GET get_xrefs_to` (`address`, paged, key `references`) | `ghidra.py xrefs / refs`: `{from_address, type, from_function}` |
| `GET list_program_items` (`kind=segments`) | `ghidra.py segments` |
| `GET get_function_hash` (no `function`: every function, key `functions`) | `fetch.py`: name, start, opcode hash, instruction count |
| `POST rename_function`, `create_function`, `add_function_tag` / `remove_function_tag` | `rename()`, `create_function()`, `set_tag()` |
| `GET save_program` | `save_program()` (name_ctx.py, ghidra.py mkfunc) |

Not used: `disassemble_bytes` (**no dry run in 7.0.0: it writes instructions**; `ghidra.py asm` is the
offline disassembler), the naming gate, the documentation / archive / emulation services.

## The setup

| what | where |
|---|---|
| Ghidra 12.2 | `C:\Users\willi\Documents\ghidra_12.2_PUBLIC_20261016\ghidra_12.2_PUBLIC` (extensions: GhidraMCP 7.0.0, ghidra-emotionengine-reloaded) |
| YamiPan's project, program | `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr`, `SCES_538.51` (`data/ghidra.json`); the project also holds `SLUS_214.48` (Rule of Rose, open-items.md item 3) |
| its port | 8089 (the default) |
| plugin source and bridge | `C:\Users\willi\Documents\git\ghidra-mcp` (`data/ghidra.json` "bridge") |
| client + CLI | `tools/reverse/ghidra_mcp.py` |
| Claude's MCP server | `.mcp.json` `ghidra` -> `tools/reverse/ghidra_bridge.py` (the checkout's `bridge_mcp_ghidra`), run by `C:\Users\willi\Documents\ghidra_12.2_PUBLIC_20261016\.venv\Scripts\python.exe` (has mcp 1.30, pydantic 2) |

Building and updating the plugin (JDK 27 on PATH; Gradle comes with the wrapper):

```
cd C:\Users\willi\Documents\git\ghidra-mcp
./gradlew buildExtension -PGHIDRA_INSTALL_DIR="C:/Users/willi/Documents/ghidra_12.2_PUBLIC_20261016/ghidra_12.2_PUBLIC"
```

-> `build/distributions/GhidraMCP-7.0.0.zip`; in Ghidra File -> Install Extensions, replace GhidraMCP
with it, restart; then `python ghidra_mcp.py list` and a new Claude session (`.mcp.json` is read at
session start). The bridge runs from the checkout, so a `git pull` there updates it.

How the tools pick a server (`ghidra_mcp.find`): `GHIDRA_MCP_URL` pins a port (the program is still
checked); else the cache (`work/ghidra_instance.json`: program -> port); else a scan of 8089..8104
(parallel; a closed port fails after the 0.4 s connect) for the one process with the program **open**
(and, for YamiPan's program, the configured project). The server is re-checked when the last check is
older than 30 s; a failed connect re-resolves once; a request that reached Ghidra is not retried.
`GhidraError` (an `OSError`) means no server or a broken connection; `GhidraRequestError` (`.code`
NOT_FOUND / LOCKED / HTTP_404 / ERROR, `.message`, `.status`) is the server's own error.

```
python ghidra_mcp.py list      # every server: port, project, open programs, version; * = YamiPan's
python ghidra_mcp.py which     # YamiPan's server, or why none
python ghidra_mcp.py launch    # start Ghidra on the project unless a server has the program open
```

The `ghidra` MCP server is bethington's own bridge, started by `ghidra_bridge.py` with `--project
"CLaude Decomp"` (auto-connect, retried while Ghidra is down) and a note in its instructions to pass
`program="SCES_538.51"` and `strict_mode="off"`. It loads the `function`, `listing` and `program` tool
groups (67 tools) on connect; `search_tools` / `load_tool_group` add the rest; `list_instances` /
`connect_instance` switch projects.

## Working with several

- **One Ghidra per project**; each process gets its own port in 8089..8104. The tools find YamiPan's
  by the open program and the project, so the port only has to be in range.
- **Other programs may be open beside SCES_538.51** (requests name their program); still, writes of
  a tool that forgets `program=` land in the active one.
- **Several Claude sessions on one Ghidra**: two sessions naming one program still race in the name
  ledger: one naming session per program at a time.
- **One process per game** keeps a crash local: a JVM crash loses every unsaved change in that Ghidra.

## Hazards (GhidraMCP 7.0.0)

- **A release built for another Ghidra half works** (above): reads that list functions answer, the
  decompiler and `get_functions` fail. Build from the checkout for the Ghidra in use.
- **`disassemble_bytes` writes** (no dry run): never use it to "just look".
- **Errors come back as HTTP 200 `{"error": ...}`**: `ghidra_mcp._call` turns them into
  `GhidraRequestError`; anything talking to the port directly must check.
- **Save after writing**: `name_ctx.py` and `ghidra.py mkfunc` call `save_program()`; a crash before
  it loses the batch.
- **`create_function` can answer success and create nothing** (2026-11-02, bootstrap 4a: 21 of
  1,016 starts, mostly vtable slots in bytes auto-analysis never disassembled): a second call creates
  it. `yamipan.create_function` now retries until `has_function` says yes.
- **"At or containing"**: `get_functions` and `disassemble_function` take any address inside a body.
  "Is there a function at X" means `entry_point` == X (`yamipan.has_function`).
- **The EE loader's RAM mirrors**: emotionengine-reloaded maps `RAM(uncached)` at 0x20000000 and
  `RAM(uncached_and_accelerated)` at 0x30100000, and auto-analysis defined strings there too: of
  5,421 strings only 2,298 are distinct, the rest copies at 0x2.../0x3.... Keep addresses below
  0x02000000.
- **gp-relative globals have no xrefs**: "no other writer" means nothing; search the asm for the gp
  offset.
- **A changed `.mcp.json` or bridge needs a new session**: Claude Code starts the MCP server at
  session start.
