"""RoR's C++ classes from the ELF's named vtables (GCC 2.x ABI): slots, bases, ctors/dtors, vptr offset.

python rtti.py [--quiet]       summary table; writes work/classes.json (refresh.py runs it quietly)
python rtti.py --class NAME    one class in detail (substring match)
python rtti.py --vtable ADDR   which class owns a vtable (its start or any slot target)

GCC 2.9x layout on the EE (no vtable thunks), all words little endian:
  vtable _vt$<Class> (a named .gnu.linkonce.d section): entries of 8 bytes {short delta, short index,
    void *pfn}; entry 0 holds the type_info function __tf<Class>; entry i (i >= 1) is the i-th virtual in
    declaration order, the destructor included where it is declared (one slot, no complete/deleting
    pair). A virtual call loads pfn from vptr + 8*i + 4 and adds delta (vptr + 8*i) to `this`.
    _vt$<Derived>$<Base> is a secondary table for a non-primary base (multiple inheritance).
  the vptr: the object's pointer to the vtable START (entry 0). GCC 2 places it after the data members
    of the first class in the hierarchy that declares a virtual -- NOT at offset 0 the way MSVC / MinGW
    place it -- so the stores that install it give its offset (`sw vt, OFF(this)` in ctors / dtors).
  __tf<Class>() builds the type_info node on first call; it calls the bases' __tf first, then
    __rtti_user (no base), __rtti_si (one public base) or __rtti_class (several): the bases come from it.
"""
import json
import struct
import sys
from collections import Counter, defaultdict

import yamipan
import sections
from elf32 import Elf

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
OUT = yamipan.WORK / 'classes.json'


def scan(elf, lo, hi, vt_addrs, tf_addrs):
    """(vtables materialised, {vtable: [store offsets]}, __tf functions called) in [lo, hi)."""
    raw = elf.read(lo, hi - lo) or b''
    words = struct.unpack('<%dI' % (len(raw) // 4), raw[:len(raw) // 4 * 4])
    val, refs, stores, tfs = {}, set(), defaultdict(list), []
    for k, w in enumerate(words):
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        imm = w & 0xffff
        simm = imm - 0x10000 if imm & 0x8000 else imm
        if op == 15:
            val[rt] = imm << 16
            continue
        if op in (9, 25) and rs in val:                      # addiu: %lo
            v = (val[rs] + simm) & 0xffffffff
            val[rt] = v
            if v in vt_addrs:
                refs.add(v)
            continue
        if op == 43 and val.get(rt) in vt_addrs:              # sw vt, off(base)
            stores[val[rt]].append(simm)
        if op == 3:
            t = ((lo + 4 * k) & 0xf0000000) | ((w & 0x3ffffff) << 2)
            if t in tf_addrs:
                tfs.append(t)
        if op in (0, 9, 25, 13, 32, 33, 35, 36, 37) or op == 3:
            dest = (w >> 11) & 31 if op == 0 else (31 if op == 3 else rt)
            if not (op in (9, 25) and rs in val):
                val.pop(dest, None)
    return refs, stores, tfs


def load():
    elf = Elf(str(yamipan.ELF))
    S = sections.load(elf)
    sec_name = {f['addr']: f['name'] for f in S['functions']}
    names = {int(a, 16): n for a, n in yamipan.cached_names().items()}
    names.update(sec_name)                      # the ELF's own symbols win over Ghidra's FUN_
    vt_addrs = {v['addr'] for v in S['vtables']}
    tf_of = {f['addr']: f['class'] for f in S['functions'] if f['kind'] == 'typeinfo_fn'}
    # every function body: Ghidra's (features) plus the named sections Ghidra has no function for yet
    bodies = {f['addr']: f['addr'] + 4 * f['size'] for f in
              json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
    for f in S['functions']:
        bodies[f['addr']] = f['addr'] + f['size']
    users, stores, bases = defaultdict(set), defaultdict(list), {}
    for lo, hi in bodies.items():
        refs, st, tfs = scan(elf, lo, hi, vt_addrs, set(tf_of))
        for v in refs:
            users[v].add(lo)
        for v, offs in st.items():
            stores[v] += offs
        if lo in tf_of:
            bases[tf_of[lo]] = [tf_of[t] for t in dict.fromkeys(tfs) if tf_of[t] != tf_of[lo]]
    classes = defaultdict(lambda: {'vtables': []})
    for v in sorted(S['vtables'], key=lambda v: v['addr']):
        c = classes[v['class']]
        c['name'] = v['class']
        entries = list(v['entries'])
        while len(entries) > 1 and not entries[-1]['pfn'] and not entries[-1]['delta']:
            entries.pop()                       # the table's zero terminator / padding
        slots = [{'index': i, 'offset': 8 * i, 'delta': e['delta'], 'addr': e['pfn'],
                  'name': names.get(e['pfn'], f'FUN_{e["pfn"]:08x}') if e['pfn'] else None}
                 for i, e in enumerate(entries[1:], 1)]
        c['vtables'].append({'addr': v['addr'], 'for': v['for'], 'typeinfo_fn': v['entries'][0]['pfn'],
                             'slots': slots,
                             'users': sorted(f'{names.get(u, f"FUN_{u:08x}")}@{u:08x}' for u in users[v['addr']]),
                             'vptr_offsets': sorted(Counter(stores[v['addr']]).items())})
    out = []
    for name, c in sorted(classes.items()):
        prim = next((v for v in c['vtables'] if not v['for']), c['vtables'][0])
        offs = Counter(dict(prim['vptr_offsets']))
        out.append({'name': name, 'bases': bases.get(name, []), 'vtables': c['vtables'],
                    'vptr_offset': offs.most_common(1)[0][0] if offs else None})
    return out


def main(argv):
    classes = load()
    yamipan.save_json(OUT, classes)
    if '--class' in argv:
        q = argv[argv.index('--class') + 1].lower()
        for c in classes:
            if q not in c['name'].lower():
                continue
            print(f'{c["name"]}' + (f' : {", ".join(c["bases"])}' if c['bases'] else '')
                  + (f'   vptr at +{c["vptr_offset"]:#x}' if c['vptr_offset'] is not None else '   vptr offset unknown'))
            for v in c['vtables']:
                print(f'  vtable {v["addr"]:#x}' + (f' (for base {"::".join(v["for"])})' if v['for'] else '')
                      + f'; vptr stores {v["vptr_offsets"]}; ctors/dtors: {", ".join(v["users"]) or "-"}')
                for s in v['slots']:
                    print(f'    slot {s["index"]:2} +{s["offset"]:#05x} {s["addr"] or 0:08x} {s["name"]}'
                          + (f'  (this {s["delta"]:+#x})' if s['delta'] else ''))
        return 0
    if '--vtable' in argv:
        x = int(argv[argv.index('--vtable') + 1], 16)
        for c in classes:
            for v in c['vtables']:
                if v['addr'] == x or any(s['addr'] == x for s in v['slots']):
                    hit = [s for s in v['slots'] if s['addr'] == x]
                    print(f'{x:#x}: {c["name"]} vtable {v["addr"]:#x}'
                          + (f' slot {hit[0]["index"]} (+{hit[0]["offset"]:#x})' if hit else ''))
        return 0
    nv = sum(len(c['vtables']) for c in classes)
    print(f'{len(classes)} classes, {nv} vtables -> {OUT.relative_to(yamipan.REPO)}')
    if '--quiet' in argv:
        return 0
    for c in classes:
        v = c['vtables'][0]
        print(f'{c["name"][:36]:36} vt {v["addr"]:#x} {len(v["slots"]):3} slots  vptr '
              + (f'+{c["vptr_offset"]:#05x}' if c['vptr_offset'] is not None else '  ?  ')
              + (f'  : {", ".join(c["bases"])}' if c['bases'] else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
