# YamiPan

A function-by-function reconstruction of a PS2 game, ported to PC -- the fourth after
MioPan (Fatal Frame 2), ReiPan (Fatal Frame 3) and RosePan
(Rule of Rose). No game data or executables are distributed: dump them from your own disc.

Status: before bootstrap. The reversing setup comes first: Ghidra with bethington's GhidraMCP 7.0.0, several
instances side by side, one per game (`tools/reverse/ghidra_mcp.py`, `tools/reverse/ghidra_bridge.py`).

The workflow, the project's decisions and its accumulated knowledge live in the Claude Code skill
`.claude/skills/yamipan-port/`; the tools in `tools/reverse/` (see its README).
