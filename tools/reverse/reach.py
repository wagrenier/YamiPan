"""What is left under a function: its unwritten game callees, transitively.

python reach.py <name|addr> [...]            # one summary line per root
python reach.py <name|addr> --list           # and every unwritten function, largest first
python reach.py --slots <vtable addr> <n>    # the n code pointers of a vtable, as roots

Written = an `FS2 0x...` annotation in src/ that is not a stub. Library code does not count. Offline
(feat_fs2.json, yamipan_functions.txt). The graph has direct calls only: a virtual slot or a script
native is an edge it cannot see, so give those as roots yourself (--slots, sssnatives.py); the numbers
are a floor, for comparing roots, not an estimate of work.
"""
import json
import re
import struct
import sys
from collections import Counter

import yamipan

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

B = {f['addr']: f for f in json.load(open(yamipan.WORK / 'feat_fs2.json', encoding='utf-8'))['functions']}
funcs = yamipan.functions()
names = {a: f['name'] for a, f in funcs.items()}
by_name = {n: a for a, n in names.items()}
LIB = {'lib', 'lib?', 'lib-ctx', 'sym'}

SRC = yamipan.SRC
stubs, written = set(), set()
for path in list(SRC.rglob('*.cpp')) + list(SRC.rglob('*.c')) + list(SRC.rglob('*.h')) + list(SRC.rglob('*.inc')):
    text = path.read_text(encoding='utf-8', errors='replace')
    for m in re.finditer(r'YAMIPAN_(?:STUB|BRIDGE)\("[^"]+", 0x([0-9a-f]+)\)', text):
        stubs.add(int(m.group(1), 16))
    for m in re.finditer(r'FS2 0x([0-9a-f]{6,8})', text):
        written.add(int(m.group(1), 16))
written -= stubs


def is_left(a):
    return a in B and a not in written and funcs.get(a, {}).get('prov', '') not in LIB


def closure(root):
    seen, todo = set(), [root]
    while todo:
        a = todo.pop()
        if a in seen or a not in B:
            continue
        seen.add(a)
        if a != root and not is_left(a):
            continue        # a written function's own callees were counted when it was written
        todo.extend(B[a]['calls'] + B[a].get('tails', []))
    return {a for a in seen if is_left(a)}


def resolve(text):
    if text in by_name:
        return by_name[text]
    return int(text, 16)


def slots(addr, count):
    data = yamipan.ELF.read_bytes()
    phoff = struct.unpack_from('<I', data, 0x1c)[0]
    segs = [struct.unpack_from('<8I', data, phoff + i * 32) for i in range(struct.unpack_from('<H', data, 0x2c)[0])]
    for s in segs:
        if s[2] <= addr < s[2] + s[4]:
            base = addr - s[2] + s[1]
            return [v for v in struct.unpack_from(f'<{count}I', data, base) if v in B]
    return []


def main():
    args = sys.argv[1:]
    show = '--list' in args
    if show:
        args.remove('--list')
    roots = []
    if '--slots' in args:
        i = args.index('--slots')
        roots += slots(int(args[i + 1], 16), int(args[i + 2], 0))
        del args[i:i + 3]
    roots += [resolve(a) for a in args]
    if not roots:
        raise SystemExit(__doc__)
    union = set()
    for r in roots:
        c = closure(r)
        union |= c
        size = sum(B[a]['size'] for a in c)
        state = 'written' if r in written else 'stub' if r in stubs else 'unwritten'
        print(f'{r:08x} {names.get(r, "?"):42s} {state:9s} {len(c):5d} functions {size:7d} instructions')
    print(f'-- union: {len(union)} functions, {sum(B[a]["size"] for a in union)} instructions, '
          f'{sum(1 for a in union if yamipan.GENERIC_NAME.match(names.get(a, "FUN_")))} unnamed')
    if show:
        blocks = Counter()
        for a in union:
            blocks[a >> 16] += B[a]['size']
        print('by 64 KB block: ' + ', '.join(f'{k << 16:08x}:{v}' for k, v in sorted(blocks.items())))
        for a in sorted(union, key=lambda x: -B[x]['size'])[:60]:
            print(f'  {a:08x} {names.get(a, "?"):42s} {B[a]["size"]:5d}i')


if __name__ == '__main__':
    main()
