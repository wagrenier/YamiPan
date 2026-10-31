# Open items -- priorities, decisions, cross-TU questions, done log

The user's priorities first ("Next up"), then decisions with their date and who made them, then open
questions. Resolve items here when they are settled (move them to "Done" with the commit).

## Next up (in order)

1. **The game**: Forbidden Siren 2, Europe, `SCES_538.51` (decided, below). Still to do:
   - the annotation tag (`<TAG>`, the game's short name in capitals, like `FF3` / `ROR`);
   - `yamipan.py`'s `ANNOT` and the stub log line;
   - `yamipan.py`'s ELF and data-root defaults, set to the game folder.
2. Bootstrap (bootstrap.md, in order): identify the binary -> its own Ghidra project (done:
   `C:\Users\willi\Documents\Ghidra Projects\CLaude Decomp.gpr`, `data/ghidra.json`) -> tools from
   RosePan on `ghidra_mcp.py` (first: the GhidraMCP 2.0.0 gaps, Open questions) -> name seeding -> TU
   map -> platform layer -> first build and run -> the queue.
3. Fill architecture.md as each step learns something.

## Decisions

The user's standing rules, carried from the earlier ports (SKILL.md "The user's rules" says how they
apply here):

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
- Pushing each finished TU's commit was approved for ReiPan and RosePan (2026-10-22); **not yet asked
  for YamiPan** -- ask once the loop runs and record the answer here.

## Open questions

- Which game (Next up 1). Does it share an engine or middleware with FF2 / FF3 / RoR (then names and
  code can be ported, bootstrap.md 1)?
- **GhidraMCP 2.0.0's gaps (asked 2026-10-31, the user decides before bootstrap step 3).** It has no
  save, no create function / disassemble bytes, no function tags, no bulk function hashes, no data
  types or structs, no scripts. RosePan's tools use the first four (`save_program`, `create_function`,
  `disassemble_bytes`, the tag endpoints, `get_bulk_function_hashes`). Options: (a) add the endpoints
  to the plugin in `../GhidraMCP` (a clone of LaurieWired's upstream: this means keeping a fork and
  building the extension for 12.2); (b) do without: hashes and disassembly from the ELF in Python
  (`mipsdis.py`), provenance only in the name ledger, saving and making functions by hand in Ghidra;
  (c) bethington's 5.14.2 beside it, if it loads on 12.2. Until then, save by hand (Ctrl+S) after
  every naming batch.
- RosePan's and ReiPan's own Ghidra tools (`rosepan.py`, `reipan.py`, pinned to 8089 and 5.14.2) do
  not run on this machine: their checkouts are read-only references here (cloned 2026-10-31).
- Parallel sessions on one game (several worktrees, one Ghidra): reads are fine, naming batches race
  (ghidra.md "Working with several"). Decide whether only one session per game names at a time, or the
  ledger learns to merge.

## Done

- 2026-10-31: the Ghidra layer on GhidraMCP 2.0.0 -- `ghidra_mcp.py` (finds the window by its active
  program via `/health` on 8080..8095, checks it before every request, parsed `data` and
  `GhidraRequestError`), `ghidra_bridge.py` (the wheel's server around that check, plus
  `list_instances` / `connect_instance`), `.mcp.json` (the venv beside Ghidra 12.2),
  `data/ghidra.json`. Tested live: every endpoint, a stdio MCP session (32 tools), the check refusing
  a write when the program was not active.
- 2026-10-30: multi-instance Ghidra -- `tools/reverse/ghidra_mcp.py` (discovery + client + CLI),
  `tools/reverse/ghidra_bridge.py` (the MCP bridge, instance-aware), `.mcp.json`; tested with two live
  Ghidras (the second bound 8090 by itself).
