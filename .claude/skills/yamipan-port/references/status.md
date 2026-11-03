# YamiPan status (generated)

Generated 2026-11-02 17:28 at commit `86600cb` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **6670**, named **1026** (15%); library functions: 912
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 1400, game-inferred 175
- context-name ledger: 1575 entries; overrides of wrong library matches: 0
- placed in a source file: 1031/6670 in 29 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `00110000` | 0/54 (0%) | - |
| `00180000` | 0/336 (0%) | - |
| `00190000` | 0/247 (0%) | - |
| `001a0000` | 0/301 (0%) | - |
| `001b0000` | 0/242 (0%) | - |
| `001c0000` | 0/233 (0%) | - |
| `001d0000` | 0/237 (0%) | - |
| `001e0000` | 0/75 (0%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **0**; stubs: **0**

## Translation units (one session each: `tus.py`)

- 42 TUs: todo 42
- next: **game/gamesystem.cpp** (frontier empty: start from main)
- pinned queue: (empty)

## Frontier (called by reconstructed code, not written yet)

- empty: nothing reconstructed calls an unwritten function yet. Start from `main` (crt0 `_start` calls it) or the queue in `references/open-items.md`.

