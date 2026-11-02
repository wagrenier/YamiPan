"""Stage everything needed to reconstruct FS2 functions into work/stage/<label>/.

python stage.py DogActor::update 0x0010eebc        # functions by name or address
python stage.py file:dog/dogactor.cpp              # an inferred source file (yamipan_functions.txt)
python stage.py class:DogActor                     # a class's virtuals, ctors / dtors and named methods
python stage.py range:0010e000-00110000            # an address range
options: --asm (add disassembly)  --label NAME

One markdown file per function plus index.md:
  - name (demangled signature when the ELF names it), address, size, provenance, inferred file
  - the class it belongs to (vtable slot / ctor / Class::), with the vptr offset
  - callers, callees in call order (tail jumps marked), strings (asserts give field names), floats
  - the decompilation (Ghidra), optionally the disassembly
Needs Ghidra (GhidraMCP 7.0.0) with the FS2 program open.
"""
import json
import re
import struct
import sys
from collections import defaultdict

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

args = [a for a in sys.argv[1:] if not a.startswith('--')]
want_asm = '--asm' in sys.argv
label = None
if '--label' in sys.argv:
    label = sys.argv[sys.argv.index('--label') + 1]
    args = [a for a in args if a != label]

feat = {f['addr']: f for f in json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
funcs = yamipan.functions()
by_name = defaultdict(list)
for a, f in funcs.items():
    by_name[f['name']].append(a)
classes = yamipan.load_json(yamipan.WORK / 'classes.json', [])
symbols = {s['addr']: s for s in yamipan.load_json(yamipan.WORK / 'sections.json', {}).get('functions', [])}
role = defaultdict(list)                   # addr -> ['DogActor slot 3 (+0x18)', 'DogActor ctor/dtor']
for c in classes:
    for v in c['vtables']:
        for s in v['slots']:
            if s['addr']:
                role[s['addr']].append(f'{c["name"]} virtual slot {s["index"]} (+{s["offset"]:#x})')
        for u in v['users']:
            role[int(u.rsplit('@', 1)[1], 16)].append(f'{c["name"]} ctor/dtor (stores its vtable)')
vptr = {c['name']: c['vptr_offset'] for c in classes}
callers = defaultdict(list)
for a, f in feat.items():
    for c in dict.fromkeys(f['calls'] + f.get('tails', [])):
        callers[c].append(a)
nm = lambda a: funcs.get(a, {}).get('name') or feat.get(a, {}).get('name') or f'FUN_{a:08x}'

# ---- resolve targets ---------------------------------------------------------
targets = []
for t in args:
    if t.startswith('file:'):
        key = t[5:]
        targets += sorted(a for a, f in funcs.items() if f['file'] and f['file'].endswith(key))
    elif t.startswith('class:'):
        cls = t[6:]
        targets += sorted(a for a in funcs if nm(a).startswith(cls + '::') or any(r.startswith(cls + ' ') for r in role[a]))
    elif t.startswith('range:'):
        lo, hi = (int(x, 16) for x in t[6:].split('-'))
        targets += sorted(a for a in funcs if lo <= a < hi)
    elif re.fullmatch(r'(0x)?[0-9a-fA-F]{6,8}', t):
        targets.append(int(t, 16))
    elif t in by_name:
        targets += by_name[t]
    else:
        raise SystemExit(f'unknown target {t!r}')
targets = list(dict.fromkeys(targets))
if not targets:
    raise SystemExit('nothing to stage')
label = label or re.sub(r'[^\w.-]+', '_', args[0])[:40]
out_dir = yamipan.WORK / 'stage' / label
out_dir.mkdir(parents=True, exist_ok=True)


def ghidra(endpoint, address):
    if endpoint == 'disassemble_function':
        return yamipan.disassemble_function(address)
    return yamipan.decompile(address)


def fl(v):
    return f'{struct.unpack("<f", struct.pack("<I", v & 0xffffffff))[0]:g}'


index = [f'# Stage: {label}', '', f'{len(targets)} functions. Read each file, then reconstruct into the '
         'listed source file (see references/reconstruction.md).', '',
         '| FS2 addr | name | size | provenance | class role | inferred file |', '|---|---|---|---|---|---|']
for a in targets:
    f, ft = funcs.get(a, {}), feat.get(a, {})
    name = nm(a)
    sym = symbols.get(a)
    cls = name.split('::')[0] if '::' in name else (role[a][0].split(' ')[0] if role[a] else None)
    tails = set(ft.get('tails', []))
    doc = [f'# {name}  (FS2 0x{a:08x})', '',
           f'- size: {ft.get("size", 0) * 4 or f.get("size", 0):#x} bytes, provenance: {f.get("prov") or "unnamed"}',
           f'- inferred file: `{f.get("file") or "(unassigned)"}` -> `{yamipan.src_path_for(f.get("file"))}`']
    if sym:
        doc.append(f'- ELF symbol `{sym["symbol"]}` = `{sym["display"]}` (exact signature)')
    if role[a]:
        doc.append('- class role: ' + '; '.join(role[a]))
    if cls and vptr.get(cls) is not None:
        doc.append(f'- `{cls}`: vptr at +{vptr[cls]:#x} (`python rtti.py --class {cls}`)')
    doc += [f'- callers ({len(callers[a])}): ' + ', '.join(sorted({nm(c) for c in callers[a]}))[:600],
            '- callees (call order): ' + ', '.join(nm(c) for c in ft.get('calls', []))
            + (' ; tail jumps: ' + ', '.join(nm(c) for c in tails) if tails else '')]
    if ft.get('strings'):
        doc.append('- strings: ' + repr(list(dict.fromkeys(ft['strings']))[:12]))
    if ft.get('floats'):
        doc.append('- float constants: ' + ', '.join(sorted({fl(v) for v in ft['floats']})))
    doc += ['', '## FS2 decompilation', '', '```c', ghidra('decompile_function', f'0x{a:08x}'), '```']
    if want_asm:
        doc += ['', '## FS2 disassembly', '', '```', ghidra('disassemble_function', f'0x{a:08x}'), '```']
    (out_dir / f'{a:08x}_{re.sub(r"[^\w]+", "_", name)[:60]}.md').write_text('\n'.join(doc) + '\n', encoding='utf-8')
    index.append(f'| `{a:08x}` | {name} | {ft.get("size", 0) * 4:#x} | {f.get("prov") or "-"} | '
                 f'{"; ".join(role[a])[:60] or "-"} | {f.get("file") or "-"} |')
(out_dir / 'index.md').write_text('\n'.join(index) + '\n', encoding='utf-8')
print(f'staged {len(targets)} functions -> {out_dir.relative_to(yamipan.REPO)}')
