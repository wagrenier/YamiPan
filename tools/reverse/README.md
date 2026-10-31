# YamiPan reversing tools

YamiPan is the fourth PS2 port by the same author (MioPan: Fatal Frame 2, ReiPan: Fatal Frame 3,
RosePan: Rule of Rose). The reversing tools are carried over from RosePan, the newest set, when the
port is bootstrapped (`.claude/skills/yamipan-port/references/bootstrap.md`). What exists so far is the
Ghidra layer, written first so that several Ghidras can run side by side (one per game).

Run every tool from this directory: `python <tool>.py`.

## Scheduled runs

| file | what it is |
|---|---|
| `routine.md` | The prompt of the desktop app's scheduled task `yamipan-next-tu` (every 20 minutes): one step per run (a bootstrap step until `tus.py` exists, then one TU) on the branch `fs2-port`, commit, push, report. Sessions never edit it; the user does (then updates the task's prompt to match). |
| `preflight.py` | Gates a run: `python preflight.py` checks the stop switch (`work/STOP`), a running autopilot, the run lock (`work/run.lock`: exit 3 while the previous run is still going, exit 1 when older than 6 h), a clean tree, a work branch and Ghidra serving the program, then takes the lock; `--release` drops it. To stop the runs: create `work/STOP`. |

## Ghidra: GhidraMCP 2.0.0, several instances

| script | what it does |
|---|---|
| `ghidra_mcp.py` | Finds the GhidraMCP 2.0.0 backend serving YamiPan's program (one backend per CodeBrowser window, on 127.0.0.1:8080..8095; each serves the program active in it) and talks to it, checking before every request that the program is still the active one. Library: `Ghidra(program).http(method, endpoint, **params)` -> the answer's parsed `data` (raises `GhidraError` when no backend serves the program, `GhidraRequestError` with `.code` when the backend reports an error), `.up()`, `find()`, `scan()`, module-level `http()` / `up()` for the configured program. CLI: `list` (every backend, `*` = YamiPan's), `which` (what the tools resolve to, or why none), `launch` (start Ghidra on the project unless a window serves the program). |
| `ghidra_bridge.py` | The `ghidra` MCP server of `.mcp.json`: the GhidraMCP 2.0.0 wheel's own server, its HTTP client re-aimed before every request by `ghidra_mcp.py`'s check, plus `list_instances` and `connect_instance`. Run by the venv that has the wheel (`.mcp.json`), never by hand. |

Configuration: `data/ghidra.json` (`project` = the Ghidra project's path without `.gpr`, for `launch`;
`program` = the game's program as Ghidra names it; `ports` = "8080:16"; `ghidra_dir`). Environment
variables override it: `GHIDRA_MCP_PROJECT`, `GHIDRA_MCP_PROGRAM`, `GHIDRA_MCP_PORTS`, `GHIDRA_DIR`;
`GHIDRA_MCP_URL` pins one backend's port and skips the scan (its program is still checked). The
resolved port is cached in `work/ghidra_instance.json`.

The API, how the plugin behaves, the setup and the traps: the skill's `references/ghidra.md`.

## Carrying a tool over from RosePan

`rosepan.py` becomes `yamipan.py` (paths, address map, provenance tags, annotation prefix). Its
`GHIDRA` URL and `http()` go: import `ghidra_mcp` and route `http()` through
`ghidra_mcp.Ghidra(PROGRAM).http(...)`, `ghidra_up()` through `.up()`. Every other tool reaches Ghidra
only through `yamipan.py`. RosePan's calls are GhidraMCP 5.14.2's: their endpoints and parameters are
translated to 2.0.0's (bootstrap.md 3), they get parsed data instead of text, and the calls 2.0.0 has
no endpoint for wait on open-items.md's "GhidraMCP 2.0.0's gaps".
