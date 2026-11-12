"""Recover a table that FS2 fills at start-up from a static initializer.

GCC turns a C++ aggregate whose initializer is not constant (u_long GS register fields, SCE_GS_SET_*
macros, addresses of other objects) into straight-line code in a file's static-initialization function
(GCC 2: __static_initialization_and_destruction_0 called by _GLOBAL_$I$<file>, listed in .ctors):
li/lui/ori/addiu constants, ld from rodata, and sb/sh/sw/sd stores. This tool runs that code
symbolically (no branches, no calls) and prints the stored bytes as hex rows.

python sinit.py <lo> <hi> <base> <size> [--a0 N] [--a1 N]
    <lo> <hi>   the initializer from its entry up to its epilogue; branches (the init/priority
                tests) and sp-relative saves are skipped, so only the init path's stores count
    <base>      the table's address, <size> its byte size
    --a0/--a1   values of a0/a1 the code reads (default 1 and 0xffff: init, priority)

A table built on the stack and copied (ld/sd loops, calls) needs mipsemu.Emu with a byte-level memory
instead (hazards.md). In ReiPan this recovered FF3's sprite tables (33 rows, then 247 HUD rows).
"""
import re
import struct
import sys

import yamipan
from ghidra import disasm


def elf_reader(path):
    data = open(path, 'rb').read()
    phoff = struct.unpack_from('<I', data, 0x1c)[0]
    segs = [struct.unpack_from('<8I', data, phoff + i * 32) for i in range(struct.unpack_from('<H', data, 0x2c)[0])]

    def rd(a, n):
        for _t, off, va, _pa, fs, *_ in segs:
            if va <= a < va + fs:
                return data[off + a - va:off + a - va + n]
        raise SystemExit(f'0x{a:x} is not in a loaded segment')
    return rd


def num(x):
    x = x.strip()
    return int(x, 16) if re.match(r'-?0x', x, re.I) else int(x)


def run(lines, rd, a0, a1):
    R = {'zero': 0, 'a0': a0, 'a1': a1, 'sp': 0x70000000}
    mem = {}
    for mnem, ops in lines:
        a = [o.strip() for o in ops.split(',')] if ops else []

        def ea(o):
            m = re.match(r'(-?(?:0x)?[0-9a-f]+)\((\w+)\)', o)
            return (R[m.group(2)] + num(m.group(1))) & 0xffffffff
        if mnem.startswith(('b', 'j')) or (len(a) > 1 and a[-1].endswith('(sp)')):
            continue                # the init/priority tests and the register saves
        if mnem == 'li':
            R[a[0]] = num(a[1]) & 0xffffffffffffffff
        elif mnem == 'lui':
            R[a[0]] = (num(a[1]) << 16) & 0xffffffff
        elif mnem == 'ori':
            R[a[0]] = R[a[1]] | num(a[2])
        elif mnem in ('addiu', 'daddiu'):
            R[a[0]] = (R[a[1]] + num(a[2])) & 0xffffffff
        elif mnem == 'move':
            R[a[0]] = R[a[1]]
        elif mnem in ('mtlo', 'mthi', 'mtlo1', 'mthi1'):
            R[mnem[2:]] = R[a[0]]
        elif mnem in ('mflo', 'mfhi', 'mflo1', 'mfhi1'):
            R[a[0]] = R[mnem[2:]]
        elif mnem in ('lw', 'ld', 'lh', 'lhu', 'lb', 'lbu'):
            n = {'lw': 4, 'ld': 8, 'lh': 2, 'lhu': 2, 'lb': 1, 'lbu': 1}[mnem]
            R[a[0]] = int.from_bytes(rd(ea(a[1]), n), 'little')
        elif mnem in ('sb', 'sh', 'sw', 'sd'):
            n = {'sb': 1, 'sh': 2, 'sw': 4, 'sd': 8}[mnem]
            v, at = R[a[0]], ea(a[1])
            for i in range(n):
                mem[at + i] = (v >> (8 * i)) & 0xff
        elif mnem in ('nop',):
            pass
        else:
            raise SystemExit(f'unhandled instruction: {mnem} {ops} (straight-line code only)')
    return mem


def main(argv):
    def opt(name, default):
        if name in argv:
            i = argv.index(name)
            v = argv[i + 1]
            del argv[i:i + 2]
            return num(v)
        return default
    a0, a1 = opt('--a0', 1), opt('--a1', 0xffff)
    lo, hi, base, size = (num(x) for x in argv[:4])
    lines = [(i['mnemonic'].lstrip('_'), i['operands']) for i in disasm(lo, hi)]
    mem = run(lines, elf_reader(yamipan.ELF), a0, a1)
    buf = bytes(mem.get(a, 0) for a in range(base, base + size))
    unset = sum(1 for a in range(base, base + size) if a not in mem)
    print(f'// 0x{base:x}..0x{base + size:x}: {size - unset} bytes stored, {unset} left zero (padding?)')
    for off in range(0, size, 16):
        print(f'{base + off:08x}: ' + buf[off:off + 16].hex(' '))


if __name__ == '__main__':
    main(sys.argv[1:])
