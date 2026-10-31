"""FS2's C++ classes from their vtables (Metrowerks CodeWarrior ABI): slots, bases, ctors/dtors, vptr offset.

python rtti.py [--quiet]       summary table; writes work/classes.json (refresh.py runs it quietly)
python rtti.py --class NAME    one class in detail (substring match)
python rtti.py --vtable ADDR   which class owns a vtable (its start or any slot target)
python rtti.py --missing       slot targets Ghidra has no function for -> data/missing_functions.json
                               (merged; create them with `ghidra.py mkfunc` on 5.14.2, or F in Ghidra)

The ELF is stripped, so vtables are found from the code that installs them (rewritten for CodeWarrior at
bootstrap 4, 2026-10-31; RosePan's GCC 2 version read named linkonce sections FS2 does not have):
  vtable: 4-byte words {type_info *ti, int this_offset, void *fn[]}; ti is 0 for every game class (RTTI
    is emitted only for MSL's exception classes), this_offset is 0 in a primary table and minus the
    base's offset in a secondary one. A virtual call loads fn from vptr + 8 + 4*i. Tables are packed
    one after another in .data from ~0x310fe0, zero-padded; slot lists end at the first word that is not
    a function start.
  the vptr points at the table START (the ti word). It need not be at offset 0: `main` installs
    0x311030 / 0x311020 at object+0x120 (architecture.md "Binary and toolchain"); the `sw vt, OFF(this)`
    stores in ctors / dtors give it.
  type_info: {const char *name, const void *bases}; bases is a list {type_info *, offset, ...} ended by 0.
  Every lui/addiu (or ori) pair that builds an address which is then stored with sw is a candidate
  table; it is kept when its words look like one. Tables no code stores (abstract bases whose ctors were
  inlined away) are found by shape between them ({0 or ti, 0, fn}) and marked "stored": false.
Class names: the type_info name when there is one, else data/class_names.json ({"<vtable hex>": "Name"},
  filled as sessions learn them), else Vt_<vtable hex>.
Bases: the type_info's list, else a table co-stored at the same offset in one function (a ctor / dtor
  installing base then derived, or derived then base) that has no more slots -- the shorter one is the
  base; equal lengths stay unresolved under "related".
"""
import json
import struct
import sys
from collections import Counter, defaultdict

import yamipan
from elf32 import Elf

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
OUT = yamipan.WORK / 'classes.json'
MISSING = yamipan.DATA / 'missing_functions.json'
NAMES = yamipan.DATA / 'class_names.json'
DATA_LO, DATA_HI = 0x2f4d20, 0x31ad80        # .data / .rodata (architecture.md "Address map")
TEXT_LO, TEXT_HI = 0x100008, 0x2ec900


def scan(elf, lo, hi, in_data):
    """({address built and stored: [(store offset, insn index)]}) for the function body [lo, hi)."""
    raw = elf.read(lo, hi - lo) or b''
    words = struct.unpack('<%dI' % (len(raw) // 4), raw[:len(raw) // 4 * 4])
    val, stores = {}, defaultdict(list)
    for k, w in enumerate(words):
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        imm = w & 0xffff
        simm = imm - 0x10000 if imm & 0x8000 else imm
        if op == 15:                                         # lui
            val[rt] = imm << 16
            continue
        if op in (9, 25, 13) and rs in val:                  # addiu / daddiu / ori: %lo
            val[rt] = (val[rs] | imm) if op == 13 else (val[rs] + simm) & 0xffffffff
            continue
        if op == 43 and in_data(val.get(rt)):                # sw vt, off(base)
            stores[val[rt]].append((rs, simm, k))
            continue
        if op == 0:
            val.pop((w >> 11) & 31, None)
        elif op == 3:
            val.pop(31, None)
        elif op in (9, 25, 13, 32, 33, 35, 36, 37, 10, 11, 12, 14, 55):
            val.pop(rt, None)
    return stores


def load():
    elf = Elf(str(yamipan.ELF))
    feats = json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']
    bodies = {f['addr']: f['addr'] + 4 * f['size'] for f in feats}
    starts = set(bodies)
    names = {int(a, 16): n for a, n in yamipan.cached_names().items()}
    known = {int(a, 16): n for a, n in yamipan.load_json(NAMES, {}).items()}
    raw = elf.read(DATA_LO, DATA_HI - DATA_LO)
    W = struct.unpack('<%dI' % (len(raw) // 4), raw)
    word = lambda a: W[(a - DATA_LO) // 4] if DATA_LO <= a < DATA_HI - 3 else None

    def typeinfo(a):
        """(name, [base type_info addrs]) when a looks like a CodeWarrior type_info, else None."""
        if not a or word(a) is None:
            return None
        s = elf.read(word(a), 64) if DATA_LO <= (word(a) or 0) < DATA_HI else None
        if not s or b'\0' not in s:
            return None
        nm = s.split(b'\0')[0]
        if not nm or not all(32 < c < 127 for c in nm):
            return None
        bases, b = [], word(a + 4)
        while b and word(b):
            bases.append(word(b))
            b += 8
        return nm.decode(), bases

    def is_code(x):
        return x is not None and TEXT_LO <= x < TEXT_HI and not x % 4

    def slots_at(v, stop):
        out, a = [], v + 8
        while a < stop and is_code(word(a)):
            out.append(word(a))
            a += 4
        return out

    def shaped(v):
        ti, off = word(v), word(v + 4)
        if ti is None or off is None:
            return False
        off = off - (1 << 32) if off & 0x80000000 else off
        return (ti == 0 or typeinfo(ti)) and -0x10000 < off <= 0 and is_code(word(v + 8))

    # 1. tables built and stored by code
    stores, users = defaultdict(list), defaultdict(set)
    seq = defaultdict(list)                           # function -> [(base reg, offset, insn, vtable)]
    for lo, hi in bodies.items():
        for v, st in scan(elf, lo, hi, lambda x: x is not None and DATA_LO <= x < DATA_HI).items():
            if v % 4 or not shaped(v):
                continue
            users[v].add(lo)
            for rs, off, k in st:
                if rs not in (29, 30):                # sp / fp: a stack object, its offset says nothing
                    stores[v].append(off)
                seq[lo].append((rs, off, k, v))
    stored = set(users)
    # 2. tables by shape in the table region, between the stored ones
    region_lo, region_hi = (min(stored) - 0x40, max(stored) + 0x400) if stored else (0, 0)
    found = set(stored)
    for a in range(region_lo & ~3, region_hi, 4):
        if a not in found and shaped(a) and not any(f < a < f + 8 for f in found):
            found.add(a)
    order = sorted(found)
    nxt = {v: (order[i + 1] if i + 1 < len(order) else DATA_HI) for i, v in enumerate(order)}

    # bases from co-stored tables
    co = defaultdict(set)
    for f, ss in seq.items():
        by_slot = defaultdict(list)
        for rs, off, k, v in sorted(ss, key=lambda s: s[2]):
            by_slot[(rs, off)].append(v)
        for vs in by_slot.values():
            for x in set(vs):
                co[x].update(set(vs) - {x})

    tables = {}
    for v in order:
        ti = word(v)
        off = word(v + 4)
        tables[v] = {'addr': v, 'typeinfo': ti or None, 'this_offset': off - (1 << 32) if off & 0x80000000 else off,
                     'stored': v in stored, 'slots': slots_at(v, nxt[v])}
    cname = {}
    for v, t in tables.items():
        tin = typeinfo(t['typeinfo']) if t['typeinfo'] else None
        cname[v] = known.get(v) or (tin[0] if tin else f'Vt_{v:08x}')
    ti_class = {t['typeinfo']: cname[v] for v, t in tables.items() if t['typeinfo'] and not t['this_offset']}
    out = []
    for v, t in tables.items():
        tin = typeinfo(t['typeinfo']) if t['typeinfo'] else None
        bases, related = [], []
        if tin:
            bases = [ti_class.get(b) or typeinfo(b)[0] for b in tin[1] if typeinfo(b)]
        else:
            for x in sorted(co[v]):
                if len(tables[x]['slots']) < len(t['slots']):
                    bases.append(cname[x])
                elif len(tables[x]['slots']) == len(t['slots']):
                    related.append(cname[x])
        slots = [{'index': i, 'offset': 8 + 4 * i, 'delta': 0, 'addr': a,
                  'name': names.get(a, f'FUN_{a:08x}')} for i, a in enumerate(t['slots'])]
        offs = Counter(stores[v])
        out.append({'name': cname[v], 'bases': bases, 'related': related,
                    'vtables': [{'addr': v, 'for': [], 'typeinfo': t['typeinfo'], 'this_offset': t['this_offset'],
                                 'stored': t['stored'], 'slots': slots,
                                 'users': sorted(f'{names.get(u, f"FUN_{u:08x}")}@{u:08x}' for u in users[v]),
                                 'vptr_offsets': sorted(offs.items())}],
                    'vptr_offset': offs.most_common(1)[0][0] if offs else None})
    return sorted(out, key=lambda c: c['vtables'][0]['addr'])


def main(argv):
    classes = load()
    yamipan.save_json(OUT, classes)
    if '--class' in argv:
        q = argv[argv.index('--class') + 1].lower()
        for c in classes:
            if q not in c['name'].lower():
                continue
            print(f'{c["name"]}' + (f' : {", ".join(c["bases"])}' if c['bases'] else '')
                  + (f'   (same depth: {", ".join(c["related"])})' if c['related'] else '')
                  + (f'   vptr at +{c["vptr_offset"]:#x}' if c['vptr_offset'] is not None else '   vptr offset unknown'))
            for v in c['vtables']:
                print(f'  vtable {v["addr"]:#x}' + ('' if v['stored'] else ' (by shape: no code stores it)')
                      + (f' this {v["this_offset"]:+#x}' if v['this_offset'] else '')
                      + f'; vptr stores {v["vptr_offsets"]}; ctors/dtors: {", ".join(v["users"]) or "-"}')
                for s in v['slots']:
                    print(f'    slot {s["index"]:2} +{s["offset"]:#05x} {s["addr"]:08x} {s["name"]}')
        return 0
    if '--missing' in argv:
        starts = {f['addr'] for f in json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
        miss = yamipan.load_json(MISSING, {})
        new = 0
        for c in classes:
            for v in c['vtables']:
                for s in v['slots']:
                    if s['addr'] not in starts and f'{s["addr"]:08x}' not in miss:
                        miss[f'{s["addr"]:08x}'] = f'{c["name"]} slot {s["index"]} (vtable {v["addr"]:#x})'
                        new += 1
        miss = {a: w for a, w in sorted(miss.items()) if int(a, 16) not in starts}
        yamipan.save_json(MISSING, miss)
        print(f'{len(miss)} function starts Ghidra lacks ({new} new) -> {MISSING.relative_to(yamipan.REPO)}')
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
    unstored = sum(not v['stored'] for c in classes for v in c['vtables'])
    print(f'{len(classes)} classes, {nv} vtables ({unstored} by shape only) -> {OUT.relative_to(yamipan.REPO)}')
    if '--quiet' in argv:
        return 0
    for c in classes:
        v = c['vtables'][0]
        print(f'{c["name"][:36]:36} vt {v["addr"]:#x} {len(v["slots"]):3} slots  vptr '
              + (f'+{c["vptr_offset"]:#05x}' if c['vptr_offset'] is not None else '  ?  ')
              + ('' if v['stored'] else '  (shape)')
              + (f'  : {", ".join(c["bases"])}' if c['bases'] else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
