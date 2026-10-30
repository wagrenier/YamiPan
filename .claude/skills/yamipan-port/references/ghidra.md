# Ghidra: several instances, one per game

YamiPan's first piece of work (2026-10-30): until then every port's tools talked to whatever answered
on `127.0.0.1:8089`, so only one Ghidra could run. Now each game has its own Ghidra project and process,
and the tools and Claude's `ghidra` MCP server find the right one. Facts below were read from the
plugin's bytecode (GhidraMCP 5.14.2, bethington/ghidra-mcp) and tested with two live Ghidras.

## How GhidraMCP serves

- **One HTTP server per Ghidra process** (JVM). The plugin is part of the **CodeBrowser** tool, not the
  project window: the server starts when a CodeBrowser opens and is shared by every CodeBrowser of that
  process ("TCP server already running -- sharing with this tool window"). Any program open in any of
  them is reachable with `program=`; one Ghidra holding several games serves all of them on one port.
- **Port**: Tool Options -> GhidraMCP HTTP Server -> Server Port (default 8089). When another process
  holds it, the plugin takes the next free one of 16 (8090..8104) and logs "Port 8089 was in use; HTTP
  server bound to fallback port 8090". All 16 busy: an error dialog. Other options there: Enable TCP
  Transport, Enable UDS Transport, Strict Naming Enforcement.
- **`GET /mcp/instance_info`** answers `{pid, project, project_path, programs, tcp_port, tools}`.
  `project_path` is the project's location + name without `.gpr` (`C:\Users\willi\Ghidra` for
  `C:\Users\willi\Ghidra.gpr`). `programs` lists the project's *files* (`{name, path, open}`), and
  **`open` is wrong** whenever a program's name differs from its file's (RoR's program `SLUS_214.48`
  lives in the file `/Rule of Rose - US` and always shows `open: false`): what is really open comes from
  `GET /list_open_programs` (`{programs: [{name, path, is_current, ...}], current_program}`).
- **Unix sockets** (the plugin's multi-instance transport): `ghidra-<pid>.sock` in
  `/tmp/ghidra-mcp-<user>`, which on Windows resolves against Ghidra's drive (`F:\tmp\ghidra-mcp-willi`).
  Python on Windows has no `AF_UNIX` (3.14 included), so neither the tools nor the bridge can use them;
  the plugin removes a dead pid's socket when it starts.
- **One project, one process.** Opening a project takes `<name>.lock`; a second Ghidra cannot open it.
  Two Ghidras therefore means two projects.
- **Running tools are saved per project** (`<name>.rep/projectState`, including the CodeBrowser's
  options and open programs) and restored when the project opens: a project closed with its CodeBrowser
  open comes back serving MCP. A project whose CodeBrowser was closed comes back without a server.
- **Opening a project rewrites the shared preferences at once** (`LastOpenedProject`, `RecentProjects`
  in `%APPDATA%\ghidra\ghidra_12.1.2_PUBLIC\preferences`): after starting a second Ghidra on YamiPan's
  project, a plain `ghidraRun` opens YamiPan's project, or fails on its lock while it is open elsewhere.
  Start the shared project by its `.gpr` (`ghidraRun C:\Users\willi\Ghidra.gpr`) when it matters.
- **The headless MCP server** (`com.xebyte.headless.GhidraMCPHeadlessServer`, `--port --project
  --program`) has neither `instance_info` nor the port fallback: usable only pinned with
  `GHIDRA_MCP_URL`. Not set up here.
- **Projects are created by importing**: `analyzeHeadless <location> <name> -import <ELF>` creates the
  project and imports (the emotionengine-reloaded extension recognises a PS2 ELF as
  `r5900:LE:32:default`); `-process` cannot create one. `-noanalysis` imports in seconds.

## The setup

| what | where |
|---|---|
| shared project (FF1, FF2, FF3, RoR, ...) | `C:\Users\willi\Ghidra.gpr`: RosePan's and ReiPan's tools still pin 8089, so start it first |
| YamiPan's project | `C:\Users\willi\YamiPan.gpr` (`data/ghidra.json` "project"); created at bootstrap by importing the game |
| the game's program | `data/ghidra.json` "program": the name `list_open_programs` shows (null until imported) |
| client + CLI | `tools/reverse/ghidra_mcp.py` |
| Claude's MCP server | `.mcp.json` `ghidra` -> `tools/reverse/ghidra_bridge.py` (the bridge's venv python) |

How the tools pick an instance (`ghidra_mcp.find`): `GHIDRA_MCP_URL` pins one and skips the rest; else
the cache (`work/ghidra_instance.json`: port + pid, trusted only when that instance still has the
project); else a scan of 8089..8104 (0.4 s: a closed port on Windows does not refuse, it times out) and
a match on the project (a path or a name), or -- when no instance has the project -- on the program,
instances with it open first. Exactly one must fit; otherwise the error lists what runs. A request
whose connection fails re-resolves once (Ghidra restarted, maybe on another port); a request that
reached Ghidra is never retried (it may have written). `GhidraError` is an `OSError`, so "no instance"
reads as "Ghidra is down" to code that catches `OSError`.

```
python ghidra_mcp.py list      # every instance: port, pid, project, open programs; * = YamiPan's
python ghidra_mcp.py which     # YamiPan's instance and whether the program is open, or why none
python ghidra_mcp.py launch    # start Ghidra on YamiPan's project unless it runs; waits for the server
```

The `ghidra` MCP server (`ghidra_bridge.py`) is GhidraMCP's own bridge with four pieces replaced: at
start it connects to YamiPan's instance or to none (the bridge alone took whatever held 8089);
`list_instances` lists every instance (`connected`, `yamipan` flags); `connect_instance` takes a
project name or path, a program name or a port and refuses ambiguity (the bridge alone fell back to
8089); a lost connection resolves the connected project again. When Ghidra was started after the
session, call `connect_instance("YamiPan")` (or the port) before the Ghidra tools appear.

## Working with several

- **Pass `program` on every request**, in the query string (the clients do). The instance may hold
  several programs, and a POST body's `program` is ignored: the write lands on the active program.
- **Several Claude sessions on one Ghidra** work for reads. Writes serialise on Ghidra's transactions,
  and a save while another holds the lock fails ("Unable to lock due to active transaction": retry,
  then check the names really landed). Two sessions naming the same program at once also race in the
  name ledger: one naming session per program at a time.
- **One process per game** keeps a crash local: a JVM crash closes every program in that Ghidra (the
  install folder holds 30+ `hs_err_pid*.log`), so RoR's autopilot keeps its Ghidra when YamiPan's dies.
  Each process has its own heap (`MAXMEM` in `ghidraRun.bat` / `support/launch.properties`).
- **The first Ghidra to start gets 8089.** Tools that pin it (RosePan's `rosepan.py`, ReiPan's
  `reipan.py`, ReiPan's `ghidra` MCP entry with `GHIDRA_MCP_URL=http://127.0.0.1:8089`) talk to
  whichever that is: start the shared project first, or move them onto `ghidra_mcp.py` (open-items.md).

## Hazards (GhidraMCP; most carried from ReiPan and RosePan)

- **`program` in a POST body is ignored** and the write goes to the active program (renamed 69 FF2
  functions in ReiPan once). Query string, always.
- **`dry_run` is not a probe.** Ignored in a POST body (the write happens); `create_function` ignored it
  even in the query string. `disassemble_bytes` honours `dry_run=true` in the query string.
- **"Unable to lock due to active transaction"**: a write or save while the last save holds the lock.
  The write usually happened; retry the save. Unsaved names vanish when Ghidra reloads: when a save
  failed, diff the name ledger against Ghidra before finishing.
- **Naming policy rejects names** (`Exit`, token-subset collisions): `strict_mode: "off"`. Ghidra
  rejects whitespace (`operator new` -> `operator_new`) but keeps `::`, `<>`, `$`, `()`. Renaming to
  `FUN_<addr>` makes the name default again.
- **`get_function_by_address` answers for any address inside a body**: "is there a function at X"
  means Entry == X.
- **`search_byte_patterns` ignores the mask**: search exact patterns.
- **gp-relative globals have no xrefs**: "no other writer" means nothing; search the asm for the gp
  offset.
- **Scripts are disabled** unless Ghidra runs with `GHIDRA_MCP_ALLOW_SCRIPTS=1`; the tools decode the
  ELF in Python instead.
- **`instance_info`'s `open` flag** and **the CodeBrowser requirement**: above.
- **A bridge started before Ghidra** registers no Ghidra tools: `connect_instance`.
