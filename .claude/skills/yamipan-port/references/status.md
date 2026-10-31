# YamiPan status (generated)

Generated 2026-10-31 08:31 at commit `c01e3d6` by `tools/reverse/status.py` (`refresh.py` runs it). Do not edit by hand.

## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)

- ELF symbols (named linkonce sections): 0; tagged game-symbol in Ghidra: 0
- library matches planned: 0; tagged lib-match / -low in Ghidra: 0 / 0
- classes (named vtables): 0

## Naming (game code; library ranges excluded)

- game functions: **6393**, named **76** (1%); library functions: 0
- by provenance: game-symbol 0, lib-match 0, lib-match-low 0, lib-context 0, game-inferred 0
- context-name ledger: 0 entries; overrides of wrong library matches: 0
- placed in a source file: 6/6393 in 2 files

Least-named game-code regions (64 KB blocks):

| block | named | inferred files |
|---|---|---|
| `00110000` | 0/180 (0%) | - |
| `00120000` | 0/188 (0%) | - |
| `00150000` | 0/376 (0%) | filehost.c, rxfile.c |
| `00160000` | 0/265 (0%) | - |
| `00170000` | 0/203 (0%) | - |
| `00180000` | 0/198 (0%) | - |
| `00190000` | 0/225 (0%) | - |
| `001a0000` | 0/277 (0%) | - |

## Reconstruction (src/)

- reconstructed FS2 functions: **0**; stubs: **0**

## Translation units (one session each: `tus.py`)

- TU summary failed: ModuleNotFoundError("No module named 'tus'")

## Frontier (called by reconstructed code, not written yet)

- empty: nothing reconstructed calls an unwritten function yet. Start from `main` (crt0 `_start` calls it) or the queue in `references/open-items.md`.

