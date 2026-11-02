"""Code pointers Ghidra has no function for: callbacks, thread entries, handlers, function tables.

python codeptrs.py                  list them (offline: the ELF + work/feat_fs2.json)
python codeptrs.py --missing        add the plausible starts to data/missing_functions.json
python codeptrs.py --create         create those functions in Ghidra (a write) and save

Two sources. (1) Code: a lui + addiu/ori pair forming an address in an executable section, followed
the way features.py follows constants; the next uses of that register say what it is for ('arg a0' =
passed to the next jal, 'store' = written to memory, 'jr' = jumped to). (2) Data: a 4-byte word in a
data section pointing into code (vtables are rtti.py's; switch tables point inside the function that
uses them and are skipped as 'interior').

A target is a plausible start ('start') when it follows a function end: `jr ra` + delay slot or a
`j` tail call (to a known start) + delay slot, optionally padded with nops. Adjacent data words with
two or more targets inside one function body are a switch table ('case'). Everything else is
'interior' (a loop head, a label, a merged function's start: read it).
"""
import bisect
import json
import struct
import sys
from collections import defaultdict

import yamipan
from elf32 import Elf
from features import decode, sext16

MISSING = yamipan.DATA / 'missing_functions.json'
JR_RA = 0x03e00008
ARGS = {4: 'a0', 5: 'a1', 6: 'a2', 7: 'a3', 8: 't0', 9: 't1'}


def load():
    elf = Elf(str(yamipan.ELF))
    feats = json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']
    funcs = sorted((f['addr'], f['name']) for f in feats)
    starts = {a for a, _ in funcs}
    code = [s for s in elf.sections if s['addr'] and s['flags'] & 4 and s['name'] != '.vutext']
    data = [s for s in elf.sections if s['addr'] and s['flags'] & 2 and not s['flags'] & 4
            and s['name'] != '.bss' and elf.offset(s['addr']) is not None]
    return elf, funcs, starts, code, data


def in_code(code, a):
    return a % 4 == 0 and any(s['addr'] <= a < s['addr'] + s['size'] for s in code)


def follows_end(elf, a, starts):
    """Whether a looks like the first instruction after a function end: `jr ra`, or a `j` to a known
    function start (a tail call; a `j` inside a body is a switch case jumping to the join), then its
    delay slot, then alignment nops."""
    k = a - 4
    while elf.u32(k) == 0 and a - k <= 0x40:       # alignment nops (or the delay slot's nop)
        k -= 4
    for back in (k, k - 4):                         # the return / tail jump, its delay slot after it
        w = elf.u32(back)
        if w == JR_RA or (w is not None and w >> 26 == 2
                          and ((back & 0xf0000000) | (w & 0x3ffffff) << 2) in starts):
            return True
    return False


def uses(elf, pc, reg, limit=12):
    """What the next instructions do with reg (formed at pc)."""
    for k in range(1, limit):
        w = elf.u32(pc + 4 * k)
        if w is None:
            break
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op in (43, 63, 31) and rt == reg:                    # sw / sd / sq of the value
            return 'store'
        if op == 0 and w & 63 in (8, 9) and rs == reg:
            return 'jr'
        if op == 3:                                             # jal: reg live into the call (delay slot too)
            return f'arg {ARGS[reg]}' if reg in ARGS else 'live at call'
        if op == 0 and w & 63 in (0x21, 0x2d, 0x25) and rt == 0 and rs == reg:   # move rd, reg
            reg = (w >> 11) & 31
            continue
        _, _, dest = decode(w)
        if dest == reg:
            break
    return 'other'


def scan():
    elf, funcs, starts, code, data = load()
    hits = defaultdict(list)                        # target -> [(where, how)]
    by_start = dict(funcs)
    for s in code:
        lo, hi = s['addr'], s['addr'] + s['size']
        words = struct.unpack('<%dI' % (s['size'] // 4), elf.read(lo, s['size'] // 4 * 4))
        val = {}
        for k, w in enumerate(words):
            pc = lo + 4 * k
            if pc in by_start:
                val = {}
            op, rs, rt, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xffff
            if op == 15:
                val[rt] = imm << 16
                continue
            if op in (9, 13) and rs in val and rs != 0:
                v = ((val[rs] + sext16(imm)) if op == 9 else (val[rs] | imm)) & 0xffffffff
                if in_code(code, v) and v not in starts:
                    hits[v].append((pc, uses(elf, pc, rt)))
                if rt != 0:
                    val[rt] = v
                continue
            _, _, dest = decode(w)
            if dest is not None:
                val.pop(dest, None)
    names = sorted(by_start)

    def owner_of(a):
        i = bisect.bisect_right(names, a) - 1
        return names[i] if i >= 0 else None

    cases = set()
    for s in data:
        n = s['size'] // 4
        words = struct.unpack('<%dI' % n, elf.read(s['addr'], n * 4)) + (0,)
        run = []                                    # adjacent words pointing into code
        for k, v in enumerate(words):
            if k < n and in_code(code, v):
                run.append(v)
                if v not in starts:
                    hits[v].append((s['addr'] + 4 * k, 'data word'))
                continue
            per = defaultdict(list)                 # two or more entries inside one body: a switch table
            for t in run:
                if t not in starts:
                    per[owner_of(t)].append(t)
            for ts in per.values():
                if len(ts) >= 2:
                    cases.update(ts)
            run = []
    out = []
    for t, refs in sorted(hits.items()):
        i = bisect.bisect_right(names, t) - 1
        owner = by_start[names[i]] if i >= 0 else '?'
        ref_owners = set()
        for where, _ in refs:
            j = bisect.bisect_right(names, where) - 1
            ref_owners.add(names[j] if j >= 0 else None)
        kind = 'start' if follows_end(elf, t, starts) else 'interior'
        # a table entry pointing inside the function that reads the table is a switch case
        if t in cases or (kind == 'interior' and i >= 0 and names[i] in ref_owners):
            kind = 'case'
        out.append({'addr': t, 'kind': kind, 'inside': owner, 'refs': refs})
    return out


def main(argv):
    found = scan()
    for f in found:
        refs = ', '.join(f'{w:#x} {h}' for w, h in f['refs'][:4]) + (' ...' if len(f['refs']) > 4 else '')
        print(f'{f["addr"]:08x} {f["kind"]:8} in {f["inside"]:40} <- {refs}')
    plausible = [f for f in found if f['kind'] == 'start']
    print(f'{len(found)} code pointers to non-starts: {len(plausible)} plausible starts, '
          f'{sum(f["kind"] == "interior" for f in found)} interior, {sum(f["kind"] == "case" for f in found)} switch cases')
    if '--missing' in argv or '--create' in argv:
        miss = yamipan.load_json(MISSING, {})
        for f in plausible:
            miss.setdefault(f'{f["addr"]:08x}', 'code pointer (codeptrs.py): '
                            + ', '.join(f'{w:#x} {h}' for w, h in f['refs'][:3]))
        yamipan.save_json(MISSING, dict(sorted(miss.items())))
        print(f'{len(miss)} entries in {MISSING.relative_to(yamipan.REPO)}')
    if '--create' in argv:
        miss = yamipan.load_json(MISSING, {})
        made = [a for a in list(miss) if yamipan.create_function(int(a, 16))]
        for a in made:
            miss.pop(a)
        yamipan.save_json(MISSING, miss)
        print(f'created {len(made)}, {len(miss)} left; save: {yamipan.save_program()}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
