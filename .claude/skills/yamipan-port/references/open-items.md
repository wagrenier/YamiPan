# Open items -- priorities, decisions, cross-TU questions, done log

The user's priorities first ("Next up"), then decisions with their date and who made them, then open
questions. Resolve items here when they are settled (move them to "Done" with the commit).

## Next up (in order)

1. **The game**: Forbidden Siren 2, Europe, `SCES_538.51`, annotation tag `FS2` (decided, below).
   Still to do (bootstrap step 3 does them):
   - `yamipan.py`'s `ANNOT` (`FS2`) and the stub log line;
   - `yamipan.py`'s ELF and data-root defaults, set to the game folder.
2. Bootstrap (bootstrap.md, in order): identify the binary -> its own Ghidra project (done:
   `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr`, `data/ghidra.json`) -> tools from
   RosePan on `ghidra_mcp.py` plus a 5.14.2 client for what 2.0.0 lacks (Decisions) -> name seeding ->
   TU map -> platform layer -> first build and run -> the queue. The scheduled runs do one step each.
3. **The user**: install bethington's GhidraMCP 5.14.2 in Ghidra 12.2 beside 2.0.0 (Decisions). Not on
   this machine yet; runs that need it say so in their report.
4. Fill architecture.md as each step learns something.

## Decisions

The user's standing rules, carried from the earlier ports (SKILL.md "The user's rules" says how they
apply here):

- 2026-10-31 (the user): **scheduled runs every 20 minutes**: the desktop app's task `yamipan-next-tu`
  (cron `*/20 * * * *`, prompt = `tools/reverse/routine.md`) advances the port by one step per run --
  a bootstrap step until `tus.py` exists, then one TU. `tools/reverse/preflight.py` gates each run
  (stop switch `work/STOP`, run lock `work/run.lock`, clean tree, work branch, Ghidra up), so a run
  still going makes the next one stop at once.
- 2026-10-31 (the user): work happens on the work branch **`fs2-port`** (on origin; `main` stays at
  the Ghidra switch). **Pushing each run's commit to origin/fs2-port is approved**; never force.
- 2026-10-31 (the user): the annotation tag is **`FS2`** (`// FS2 0x...`).
- 2026-10-31 (the user): GhidraMCP 2.0.0's gaps are covered by **bethington's GhidraMCP 5.14.2
  installed beside it** (the user installs it). The carried-over tools keep RosePan's 5.14.2 calls for
  what 2.0.0 lacks -- `save_program`, `create_function`, `disassemble_bytes`, the function-tag
  endpoints, `get_bulk_function_hashes` -- through a 5.14.2 client on its own ports (8089..8104,
  `program=` in the query string, as before 2026-10-31: `git show 8536a1f:tools/reverse/ghidra_mcp.py`);
  everything else and Claude's MCP tools go through 2.0.0.
- 2026-10-31 (the user): Ghidra 12.2 with **LaurieWired's GhidraMCP 2.0.0** replaces Ghidra 12.1.2
  with bethington's 5.14.2, on a new machine. YamiPan's project is
  `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr` (program `SCES_538.51`); the game's
  files moved to `C:\Users\willi\Documents\Decomp\Forbidden Siren 2`. The tools, the bridge and
  `.mcp.json` were switched the same day (ghidra.md).
- 2026-10-30 (the user): YamiPan ports **Forbidden Siren 2 (Europe, `SCES_538.51`)**. The game's
  data and ELF are in `C:\Users\willi\Documents\Decomp\Forbidden Siren 2` (since 2026-10-31):
  - `SCES_538.51`;
  - `ROM.000`..`ROM.007`, `ROMD.008`..`ROMD.010` and `SPK.ROM`;
  - `MODULES\`, `IOPCONF.CNF` and `SYSTEM.CNF`.

  The tools read the ELF and the data from there; nothing is copied into the repo.
- 2026-10-30 (the user): YamiPan is the next PS2 port; its skill carries ReiPan's and RosePan's
  learning. First step: Ghidra working with several instances (done the same day, ghidra.md).
- 2026-10-30 (session decision; updated 2026-10-31): one Ghidra project and process per game. With
  GhidraMCP 2.0.0 a backend is found by the program active in its window (`/health` on 8080..8095,
  `ghidra_mcp.py`), not by port, and every request checks it first.
- 2026-10-28 (the user, RosePan): the PC port displays correctly at any aspect ratio, read from one
  renderer-owned source; never a fixed 4:3 / 640x448 assumption in new host draw code.
- 2026-10-22 (the user, RosePan): drawing never goes through the emulated GS; the game's draw decisions
  are reconstructed and the packet-writing step calls a native host draw module per subsystem.
- 2026-10-15 (the user, ReiPan): methods, not free functions -- a function taking the object in a0 is a
  member of its class.
- ReiPan / RosePan (the user): the picture is the test; a drawing TU is `done` only when the screenshot
  matches the game's own assets; complete code nothing can show yet is `verify`.
- ReiPan / RosePan (the user): automated porting runs at effort xhigh or above; sessions never start
  other sessions, agents or scheduled tasks, and never edit `autopilot.py` or the routine.

## Open questions

- Which game (Next up 1). Does it share an engine or middleware with FF2 / FF3 / RoR (then names and
  code can be ported, bootstrap.md 1)?
- **Can 5.14.2 sit beside 2.0.0?** Unchecked: whether bethington's 5.14.2 loads on Ghidra 12.2, and
  whether both install at once (Ghidra keeps extensions in folders named after them, and 2.0.0 already
  has `Extensions\GhidraMCP`; if 5.14.2 uses the same name, one replaces the other). If they cannot
  coexist, the fallbacks were: extend 2.0.0 in `../GhidraMCP` (a clone of LaurieWired's upstream: a
  fork, built for 12.2), or do without (hashes and disassembly from the ELF, provenance only in the
  name ledger, saving and making functions by hand). Until 5.14.2 answers, save by hand (Ctrl+S)
  after every naming batch.
- RosePan's and ReiPan's own Ghidra tools (`rosepan.py`, `reipan.py`, pinned to 8089 and 5.14.2) do
  not run on this machine: their checkouts are read-only references here (cloned 2026-10-31).
- Parallel sessions on one game (several worktrees, one Ghidra): reads are fine, naming batches race
  (ghidra.md "Working with several"). Decide whether only one session per game names at a time, or the
  ledger learns to merge.

## Done

- 2026-10-31: scheduled runs -- the work branch `fs2-port` (pushed), `tools/reverse/routine.md`,
  `tools/reverse/preflight.py`, the desktop app's task `yamipan-next-tu` every 20 minutes.
- 2026-10-31: the Ghidra layer on GhidraMCP 2.0.0 -- `ghidra_mcp.py` (finds the window by its active
  program via `/health` on 8080..8095, checks it before every request, parsed `data` and
  `GhidraRequestError`), `ghidra_bridge.py` (the wheel's server around that check, plus
  `list_instances` / `connect_instance`), `.mcp.json` (the venv beside Ghidra 12.2),
  `data/ghidra.json`. Tested live: every endpoint, a stdio MCP session (32 tools), the check refusing
  a write when the program was not active.
- 2026-10-30: multi-instance Ghidra -- `tools/reverse/ghidra_mcp.py` (discovery + client + CLI),
  `tools/reverse/ghidra_bridge.py` (the MCP bridge, instance-aware), `.mcp.json`; tested with two live
  Ghidras (the second bound 8090 by itself).
