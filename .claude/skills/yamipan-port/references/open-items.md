# Open items -- priorities, decisions, cross-TU questions, done log

The user's priorities first ("Next up"), then decisions with their date and who made them, then open
questions. Resolve items here when they are settled (move them to "Done" with the commit).

## Next up (in order)

1. **The game**: Forbidden Siren 2, Europe, `SCES_538.51` (decided, below). Still to do:
   - the annotation tag (`<TAG>`, the game's short name in capitals, like `FF3` / `ROR`);
   - `yamipan.py`'s `ANNOT` and the stub log line;
   - `yamipan.py`'s ELF and data-root defaults, set to the game folder.
2. Bootstrap (bootstrap.md, in order): identify the binary -> its own Ghidra project
   (`C:\Users\willi\YamiPan.gpr`, `data/ghidra.json` "program") -> tools from RosePan on
   `ghidra_mcp.py` -> name seeding -> TU map -> platform layer -> first build and run -> the queue.
3. Fill architecture.md as each step learns something.

## Decisions

The user's standing rules, carried from the earlier ports (SKILL.md "The user's rules" says how they
apply here):

- 2026-10-30 (the user): YamiPan ports **Forbidden Siren 2 (Europe, `SCES_538.51`)**. The game's
  data and ELF are in `E:\Games\Emulator\PS2\Games\Forbidden Siren 2 (Europe)`:
  - `SCES_538.51`;
  - `ROM.000`..`ROM.007` and `SPK.ROM`;
  - `MODULES\`, `IOPCONF.CNF` and `SYSTEM.CNF`.

  The tools read the ELF and the data from there; nothing is copied into the repo.
- 2026-10-30 (the user): YamiPan is the next PS2 port; its skill carries ReiPan's and RosePan's
  learning. First step: Ghidra working with several instances (done the same day, ghidra.md).
- 2026-10-30 (session decision, no answer needed yet): one Ghidra project and process per game.
  YamiPan's project is `C:\Users\willi\YamiPan.gpr`, never the shared `C:\Users\willi\Ghidra.gpr`.
  Instances are found by project through `instance_info` on 8089..8104 (`ghidra_mcp.py`), not by port.
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
- RosePan's `rosepan.py`, ReiPan's `reipan.py` and ReiPan's `ghidra` MCP entry (user scope,
  `GHIDRA_MCP_URL=http://127.0.0.1:8089`) still pin 8089: they reach the shared project only while its
  Ghidra started first. Moving them onto `ghidra_mcp.py` / `ghidra_bridge.py` makes the order not
  matter (offered to the user 2026-10-30).
- Parallel sessions on one game (several worktrees, one Ghidra): reads are fine, naming batches race
  (ghidra.md "Working with several"). Decide whether only one session per game names at a time, or the
  ledger learns to merge.

## Done

- 2026-10-30: multi-instance Ghidra -- `tools/reverse/ghidra_mcp.py` (discovery + client + CLI),
  `tools/reverse/ghidra_bridge.py` (the MCP bridge, instance-aware), `.mcp.json`; tested with two live
  Ghidras (the second bound 8090 by itself).
