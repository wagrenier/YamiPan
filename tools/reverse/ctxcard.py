"""Context cards for naming unnamed FS2 functions.

python ctxcard.py --top 20 [--skip 0] [--lines 40] [--lib]   most-called unnamed game functions, not yet carded
python ctxcard.py 0010eebc 0011023c ...                       specific functions

Each card: call sites and named callers, callees (tail jumps marked), strings (asserts carry field and
member names: "m_main.m_no < m_max_motion"), floats, the class role (vtable slot / ctor-dtor, rtti.py),
the nearest named neighbours with their inferred file, and the decompilation (truncated). Write the
names into a batch JSON for name_ctx.py.
"""
import json
import struct
import sys
from bisect import bisect_left
from collections import Counter, defaultdict

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
args = [a for a in sys.argv[1:] if a != '--lib']


def opt(name, default):
    if name in args:
        i = args.index(name)
        v = args[i + 1]
        del args[i:i + 2]
        return int(v)
    return default


top, skip, max_lines = opt('--top', 0), opt('--skip', 0), opt('--lines', 40)
B = {f['addr']: f for f in json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
funcs = yamipan.functions()
names = {a: f['name'] for a, f in funcs.items()}
if yamipan.ghidra_up():                     # Ghidra's newest names, where they say more than FUN_
    names.update({int(a, 16): n for a, n in yamipan.function_names().items() if not yamipan.GENERIC_NAME.match(n)})
role = defaultdict(list)
for c in yamipan.load_json(yamipan.WORK / 'classes.json', []):
    for v in c['vtables']:
        for s in v['slots']:
            if s['addr']:
                role[s['addr']].append(f'{c["name"]} slot {s["index"]}')
        for u in v['users']:
            role[int(u.rsplit('@', 1)[1], 16)].append(f'{c["name"]} ctor/dtor')
sites = Counter(c for f in B.values() for c in f['calls'] + f.get('tails', []))
callers = defaultdict(list)
for f in B.values():
    for c in set(f['calls'] + f.get('tails', [])):
        callers[c].append(f['addr'])
generic = lambda a: yamipan.GENERIC_NAME.match(names.get(a, 'FUN_'))

SEEN = yamipan.WORK / 'batches' / 'seen.json'           # carded and deliberately left unnamed
seen = set(yamipan.load_json(SEEN, []))
if top:
    def has_evidence(f):
        return f['calls'] or f['strings'] or f['floats'] or f['fptrs'] or f['size'] >= 12 or role[f['addr']]
    pick = [a for a, _ in sites.most_common() if a in B and generic(a) and '%08x' % a not in seen
            and has_evidence(B[a]) and ('--lib' in sys.argv or not yamipan.in_lib(a))][skip:skip + top]
    yamipan.save_json(SEEN, sorted(seen | {'%08x' % a for a in pick}))
else:
    pick = [int(x, 16) for x in args]


def fl(v):
    return f'{struct.unpack("<f", struct.pack("<I", v & 0xffffffff))[0]:g}'


srt = sorted(names)


def neighbours(a):
    i = bisect_left(srt, a)
    before = next((x for x in reversed(srt[:i]) if not generic(x)), None)
    after = next((x for x in srt[i + 1:] if not generic(x)), None)
    return [f'{names[x]}@{x:08x} [{funcs.get(x, {}).get("file") or "?"}]' for x in (before, after) if x is not None]


for a in pick:
    f = B.get(a)
    if f is None:
        print(f'##### {a:08x}: not a function start (python ghidra.py func {a:08x})')
        continue
    named_callers = sorted({names.get(c, f'FUN_{c:08x}') for c in callers.get(a, []) if not generic(c)})
    callees = [names.get(c, hex(c)) for c in f['calls']]
    print(f'##### {a:08x} {names.get(a)}  size={f["size"]} sites={sites[a]} callers={len(callers.get(a, []))}'
          f'  file={funcs.get(a, {}).get("file") or "?"}')
    if role[a]:
        print('  class:', '; '.join(role[a]))
    print('  callers:', ', '.join(named_callers[:6]) + (' ...' if len(named_callers) > 6 else ''))
    print('  near:', ' | '.join(neighbours(a)))
    if callees:
        print('  calls:', ', '.join(dict.fromkeys(callees))[:300])
    if f.get('tails'):
        print('  tail jumps:', ', '.join(names.get(c, hex(c)) for c in f['tails']))
    if f['strings']:
        print('  str:', [s[:60] for s in dict.fromkeys(f['strings'])][:8])
    if f['floats']:
        print('  flt:', sorted({fl(v) for v in f['floats']})[:8])
    if yamipan.ghidra_up():
        body = [l for l in yamipan.decompile(a).split('\n') if l.strip() and not l.strip().startswith('/*')]
        for l in body[:max_lines]:
            print('  |', l[:150])
        if len(body) > max_lines:
            print(f'  | ... {len(body) - max_lines} more lines')
