# YamiPan reversing tools

YamiPan is the fourth PS2 port by the same author (MioPan: Fatal Frame 2, ReiPan: Fatal Frame 3,
RosePan: Rule of Rose). The reversing tools are carried over from RosePan, the newest set, when the
port is bootstrapped (`.claude/skills/yamipan-port/references/bootstrap.md`). What exists so far is the
Ghidra layer, written first so that several Ghidras can run side by side (one per game).

Run every tool from this directory: `python <tool>.py`.

## Ghidra: several instances

| script | what it does |
|---|---|
| `ghidra_mcp.py` | Finds the Ghidra instance that holds YamiPan's project among every Ghidra running (GhidraMCP on 127.0.0.1:8089..8104, one port per Ghidra process) and talks to it. Library: `Ghidra(program, project).http(method, endpoint, body, **query)` (`program` always in the query string), `up()`, `find()`, `scan()`, module-level `http()` / `up()` for the configured program. CLI: `list` (every instance, `*` = YamiPan's), `which` (what the tools resolve to, or why none), `launch` (start Ghidra on the project unless it runs, wait for its server). |
| `ghidra_bridge.py` | The `ghidra` MCP server of `.mcp.json`: GhidraMCP's `bridge_mcp_ghidra.py` loaded unchanged, with start-up, `list_instances`, `connect_instance` and reconnect replaced by `ghidra_mcp.py`'s discovery, so Claude's Ghidra tools attach to YamiPan's instance and never to whatever holds 8089. |

Configuration: `data/ghidra.json` (`project` = the Ghidra project's path without `.gpr`, `program` =
the game's program name once imported, `ports`, `ghidra_dir`, `bridge`, `bridge_python`). Environment
variables override it: `GHIDRA_MCP_PROJECT`, `GHIDRA_MCP_PROGRAM`, `GHIDRA_MCP_PORTS`, `GHIDRA_DIR`,
`GHIDRA_MCP_BRIDGE`, `GHIDRA_MCP_BRIDGE_PYTHON`; `GHIDRA_MCP_URL` pins one instance and skips discovery.
The resolved instance is cached in `work/ghidra_instance.json` and re-checked with one request.

How the plugin behaves, the per-game project setup and the traps: the skill's
`references/ghidra.md`.

## Carrying a tool over from RosePan

`rosepan.py` becomes `yamipan.py` (paths, address map, provenance tags, annotation prefix). Its
`GHIDRA` URL and `http()` go: import `ghidra_mcp` and route `http()` through
`ghidra_mcp.Ghidra(PROGRAM).http(...)`, `ghidra_up()` through `.up()`. Every other tool reaches Ghidra
only through `yamipan.py`, so nothing else changes.
