# YamiPan status (generated)

Generated 2026-11-02 15:02 at commit `f28e32f` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 490

## Naming (game code; library ranges excluded)

- game functions: **7468**, named **1104** (15%); library functions: 0
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 933, game-inferred 86
- context-name ledger: 1019 entries; overrides of wrong library matches: 0
- placed in a source file: 6/7468 in 2 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `00180000` | 0/336 (0%) | - |
| `00190000` | 0/247 (0%) | - |
| `001a0000` | 0/301 (0%) | - |
| `001b0000` | 0/242 (0%) | - |
| `001c0000` | 0/233 (0%) | - |
| `001d0000` | 0/237 (0%) | - |
| `001e0000` | 0/75 (0%) | - |
| `001f0000` | 0/185 (0%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **0**; stubs: **0**

## Translation units (one session each: `tus.py`)

- TU summary failed: ModuleNotFoundError("No module named 'tus'")

## Frontier (called by reconstructed code, not written yet)

- empty: nothing reconstructed calls an unwritten function yet. Start from `main` (crt0 `_start` calls it) or the queue in `references/open-items.md`.

