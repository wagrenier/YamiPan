"""Module overview: every function in a FS2 address range, or the call tree under one.

python modview.py range 0010e000 00110000     # functions in [lo, hi)
python modview.py tree <name|addr> [depth]     # callees of a function, depth-first (default 2)

One line per function: address, name, provenance, size (instructions), call sites, callees, strings.
Offline (cached names, yamipan_functions.txt).
"""
import json
import sys
from collections import Counter

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

B = {f['addr']: f for f in json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
funcs = yamipan.functions()
names = {a: f['name'] for a, f in funcs.items()}
by_name = {n: a for a, n in names.items()}
SHORT = {'sym': 'S', 'lib': 'L', 'lib?': 'l', 'lib-ctx': 'C', 'game': 'I', 'native': 'N'}
sites = Counter(c for f in B.values() for c in f['calls'] + f.get('tails', []))


def line(a, indent=''):
    f = B.get(a)
    if f is None:
        return f'{indent}{a:08x} {names.get(a, "?")} (not a function start)'
    callees = ', '.join(dict.fromkeys(names.get(c, hex(c)) for c in f['calls'] + f.get('tails', [])))
    strs = (' str=' + repr([s[:30] for s in dict.fromkeys(f['strings'])][:3])) if f['strings'] else ''
    tag = SHORT.get(funcs.get(a, {}).get('prov', ''), ' ')
    return f'{indent}{a:08x} {names.get(a, "?"):40s} {tag} {f["size"]:4d}i {sites[a]:3d}x  -> {callees[:120]}{strs}'


if len(sys.argv) < 3:
    raise SystemExit(__doc__)
if sys.argv[1] == 'range':
    lo, hi = int(sys.argv[2], 16), int(sys.argv[3], 16)
    fs = [a for a in sorted(B) if lo <= a < hi]
    unnamed = sum(1 for a in fs if yamipan.GENERIC_NAME.match(names.get(a, 'FUN_')))
    print(f'{len(fs)} functions, {unnamed} unnamed   (S symbol, L/l library high/low, C lib-context, I inferred, '
          f'N native Ghidra name)')
    for a in fs:
        print(line(a))
elif sys.argv[1] == 'tree':
    root = by_name.get(sys.argv[2]) or int(sys.argv[2], 16)
    depth = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    seen = set()

    def walk(a, d):
        print(line(a, '  ' * (depth - d)))
        if d == 0 or a in seen or a not in B:
            return
        seen.add(a)
        for c in dict.fromkeys(B[a]['calls'] + B[a].get('tails', [])):
            walk(c, d - 1)

    walk(root, depth)
