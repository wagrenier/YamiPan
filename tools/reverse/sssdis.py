#!/usr/bin/env python3
"""The stage scripts ("SSS", StageScript::relocate 0x1d3a40), read statically.

python sssdis.py <script file> [--natives lo-hi] [--all]

A script file is a header (+8 entries, +0xa second table's count), 0x1c-byte entries from +0xc (name,
code size at +0x18, slot count at +0x1a), an optional table, each entry's code, the SSS\\x1a block. The
VM is 0x1d1c40: words; 0x01000000 | n calls native n (table 0x304230, index n - 0x19), which pops its
own arguments; 0x02000000 | n a slot of the current object. This walks each entry linearly with a
stack of constants and prints the native calls in the range, with the arguments it could follow
(`?` for one that came from a variable). Without --natives: the lighting natives of SceneRender.
"""
import struct
import sys

# 0x1b4.. name the scene's lighting commands (architecture.md "The lights of a stage").
LIGHT_NATIVES = {
    0x1b6: 'clearColor(set, key, r, g, b)',
    0x1b7: 'ambient(set, key, r, g, b)',
    0x1b8: 'light(set, i, key, rx, ry, rz, r, g, b)',
    0x1b9: 'modelAmbient(set, key, r, g, b)',
    0x1ba: 'modelLight(set, i, key, rx, ry, rz, r, g, b)',
    0x1bb: 'fog(set, key, near, far, nearValue, farValue, r, g, b)',
    0x1bc: 'farFog(set, i, key, near, far, nearValue, farValue, r, g, b)',
    0x1bd: 'transition0(set, key, value)',
    0x1be: 'transition1(set, key, value)',
    0x1c2: 'addSetKeys(set, time)',
    0x1c3: 'setSetLoop(set, loop)',
}
TWO_WORD = {4, 5, 7, 8, 9, 0x1b, 0x1c, 0x1f, 0x20, 0x21, 0x22, 0x26}


def entries(d):
    count, table2 = struct.unpack_from('<hh', d, 8)
    pos = 0xc + count * 0x1c + (table2 * 0x10)
    out = []
    for i in range(count):
        e = 0xc + i * 0x1c
        name = d[e:e + 0x10].split(b'\0')[0].decode('latin1')
        size, slots = struct.unpack_from('<Hh', d, e + 0x18)
        size = (size & 0xfffc) | ((size & 3) << 16)
        out.append((name, pos, size))
        pos += size + slots * 0x10
    return out


def walk(d, name, pos, size, lo, hi):
    end = pos + size - 0xc
    stack, acc, pending = [], None, 0
    p = pos
    hits = []
    while p + 4 <= end:
        w = struct.unpack_from('<I', d, p)[0]
        p += 4
        top = w >> 24
        if top == 1:
            n = w & 0xffffff
            if lo <= n <= hi:
                hits.append((n, list(stack)))
            stack = []
            acc = None
        elif top == 2:
            stack = []
            acc = None
        elif w == 8:
            v = struct.unpack_from('<i', d, p)[0]
            p += 4
            acc = v if pending == 0 else None
            pending = 0
        elif w == 0xa:
            stack.append(acc)
            acc = None
        elif 0xc <= w <= 0x12:
            pending = w
        elif w in TWO_WORD:
            p += 4
            if w == 7:
                acc = None
        elif w == 0:
            continue
    return hits


def main():
    args = sys.argv[1:]
    lo, hi = 0x1b3, 0x1c3
    if '--natives' in args:
        i = args.index('--natives')
        lo, hi = (int(x, 16) for x in args[i + 1].split('-'))
        del args[i:i + 2]
    d = open(args[0], 'rb').read()
    for name, pos, size in entries(d):
        hits = walk(d, name, pos, size, lo, hi)
        print(f'{name!r}: code {pos:#x} size {size:#x}, {len(hits)} calls')
        for n, st in hits:
            label = LIGHT_NATIVES.get(n, f'native {n:#x}')
            print(f'    {n:#x} {label}: ' + ', '.join('?' if v is None else str(v) for v in st))


if __name__ == '__main__':
    main()
