"""Regenerate the port's status page: .claude/skills/yamipan-port/references/status.md

python status.py

Offline: reads work/ (fetch.py, features.py, sections.json, classes.json), data/, yamipan_functions.txt and
src/. It is the generated half of the yamipan-port skill; refresh.py runs it after every refresh.
"""
import json
import subprocess
import sys
import time
from collections import Counter, defaultdict

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

feat = {f['addr']: f for f in json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
funcs = yamipan.functions()
tags = yamipan.load_json(yamipan.WORK / 'fs2_tags.json', {})
ledger = yamipan.load_json(yamipan.DATA / 'context_names.json', {})
overrides = yamipan.load_json(yamipan.DATA / 'overrides.json', {})
libplan = yamipan.load_json(yamipan.WORK / 'libmatch.json', [])
symbols = yamipan.load_json(yamipan.WORK / 'sections.json', {}).get('functions', [])
classes = yamipan.load_json(yamipan.WORK / 'classes.json', [])
rec = yamipan.reconstructed()
stub = yamipan.stubs()
done = {a for a in rec if a not in stub}
generic = lambda n: bool(yamipan.GENERIC_NAME.match(n or 'FUN_'))
in_lib = yamipan.in_lib

try:
    commit = subprocess.run(['git', '-C', str(yamipan.REPO), 'rev-parse', '--short', 'HEAD'],
                            capture_output=True, text=True).stdout.strip() or '(no commit yet)'
except OSError:
    commit = '?'

L = []
w = L.append
w('# YamiPan status (generated)')
w('')
w(f'Generated {time.strftime("%Y-%m-%d %H:%M")} at commit `{commit}` by `tools/reverse/status.py` '
  '(`refresh.py` runs it). Do not edit by hand.')
w('')

# ---- seeding -------------------------------------------------------------
by_tag = Counter(tags.values())
w('## Ghidra seeding (one-time: sections.py --apply, libmatch.py --apply)')
w('')
w(f'- ELF symbols (named linkonce sections): {len(symbols)}; tagged game-symbol in Ghidra: {by_tag["game-symbol"]}'
  + ('' if by_tag['game-symbol'] >= len(symbols) * 0.95 else '  **-> run `python sections.py --apply`**'))
w(f'- library matches planned: {len(libplan)}; tagged lib-match / -low in Ghidra: '
  f'{by_tag["lib-match"]} / {by_tag["lib-match-low"]}'
  + ('' if by_tag['lib-match'] + by_tag['lib-match-low'] >= len(libplan) * 0.9 else
     '  **-> run `python libmatch.py --apply`**'))
w(f'- classes (named vtables): {len(classes)}')
w('')

# ---- naming --------------------------------------------------------------
game = {a: f for a, f in funcs.items() if not in_lib(a)}
total = len(game)
named = sum(1 for f in game.values() if not generic(f['name']))
w('## Naming (game code; library ranges excluded)')
w('')
w(f'- game functions: **{total}**, named **{named}** ({100 * named / max(total, 1):.0f}%); '
  f'library functions: {len(funcs) - total}')
w(f'- by provenance: game-symbol {by_tag["game-symbol"]}, lib-match {by_tag["lib-match"]}, '
  f'lib-match-low {by_tag["lib-match-low"]}, lib-context {by_tag["lib-context"]}, '
  f'game-inferred {by_tag["game-inferred"]}')
w(f'- context-name ledger: {len(ledger)} entries; overrides of wrong library matches: {len(overrides)}')
assigned = sum(1 for f in game.values() if f['file'])
files = Counter(f['file'] for f in game.values() if f['file'])
w(f'- placed in a source file: {assigned}/{total} in {len(files)} files')
w('')
w('Least-named game-code regions (64 KB blocks):')
w('')
blk = defaultdict(lambda: [0, 0, Counter()])
for a, f in game.items():
    b = blk[a >> 16]
    b[0] += 1
    b[1] += not generic(f['name'])
    if f['file']:
        b[2][f['file']] += 1
w('| block | named | inferred files |')
w('|---|---|---|')
for b, (tot, nm, fl) in sorted(blk.items(), key=lambda kv: kv[1][1] / kv[1][0])[:8]:
    w(f'| `{b << 16:08x}` | {nm}/{tot} ({100 * nm / tot:.0f}%) | {", ".join(f for f, _ in fl.most_common(3)) or "-"} |')
w('')

# ---- reconstruction ------------------------------------------------------
w('## Reconstruction (src/)')
w('')
per_file = defaultdict(lambda: [0, 0])
for a, (path, _) in rec.items():
    per_file[path.relative_to(yamipan.REPO).as_posix()][0 if a not in stub else 1] += 1
w(f'- reconstructed FS2 functions: **{len(done)}**; stubs: **{len(stub)}**')
if per_file:
    w('')
    w('| file | reconstructed | stubs |')
    w('|---|---|---|')
    for p, (r, s) in sorted(per_file.items()):
        w(f'| `{p}` | {r} | {s} |')
w('')

# ---- translation units ---------------------------------------------------
w('## Translation units (one session each: `tus.py`)')
w('')
try:
    import tus
    M = tus.Model()
    stats = [M.stats(k) for k in set(M.members) | set(M.db['tus'])]
    by_state = Counter(s['status'] for s in stats)
    nxt, why = M.pick()
    w(f'- {len(M.members)} TUs: ' + ', '.join(f'{st} {by_state[st]}' for st in tus.STATES if by_state[st]))
    w(f'- next: **{nxt}** ({why})')
    w(f'- pinned queue: {", ".join(M.db["queue"]) or "(empty)"}')
    for st in ('active', 'partial', 'verify', 'blocked'):
        for s in sorted((s for s in stats if s['status'] == st and s['explicit']), key=lambda s: s['key']):
            note = (M.entry(s['key']).get('notes') or '').strip().splitlines()
            w(f'- {st}: `{s["key"]}` {s["done"]}/{s["total"]}' + (f' -- {note[0][:160]}' if note else ''))
    hist = sorted(((x.get('date', ''), k, x) for k, e in M.db['tus'].items() for x in e.get('sessions', [])),
                  key=lambda t: t[0], reverse=True)[:8]
    if hist:
        w('')
        w('Recent sessions:')
        w('')
        for d, k, x in hist:
            w(f'- {d} `{k}` {x.get("status", "")}: {x.get("summary", "")[:160]}')
    ranked = sorted((s for s in stats if s['score'] and s['status'] not in ('done', 'verify', 'blocked')),
                    key=lambda s: -s['score'])[:12]
    if ranked:
        w('')
        w('| TU (by frontier score) | state | done/stubs/total | KB | score | reached stubs |')
        w('|---|---|---|---|---|---|')
        for s in ranked:
            w(f'| `{s["key"]}` | {s["status"]} | {s["done"]}/{s["stubs"]}/{s["total"]} | {s["bytes"] / 1024:.1f} | '
              f'{s["score"]} | {s["hits"]} |')
except Exception as ex:          # never lose the rest of the page over the TU summary
    w(f'- TU summary failed: {ex!r}')
w('')

# ---- frontier ------------------------------------------------------------
frontier = Counter()
for a in done:
    f = feat.get(a, {})
    for c in f.get('calls', []) + f.get('tails', []):
        if c not in done and c in feat and not in_lib(c):
            frontier[c] += 1
w('## Frontier (called by reconstructed code, not written yet)')
w('')
if not frontier:
    w('- empty: nothing reconstructed calls an unwritten function yet. Start from `main` (crt0 `_start` '
      'calls it) or the queue in `references/open-items.md`.')
else:
    w('| FS2 addr | name | call sites | provenance | state | inferred file |')
    w('|---|---|---|---|---|---|')
    for c, n in frontier.most_common(40):
        f = funcs.get(c, {})
        w(f'| `{c:08x}` | {f.get("name", "?")} | {n} | {f.get("prov", "") or "-"} | '
          f'{"stub" if c in stub else "missing"} | {f.get("file") or "-"} |')
w('')

out = yamipan.SKILL_DIR / 'references' / 'status.md'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text('\n'.join(L) + '\n', encoding='utf-8')
print(f'{out.relative_to(yamipan.REPO)}: {named}/{total} game functions named, {len(done)} reconstructed, '
      f'{len(stub)} stubs, frontier {len(frontier)}')
