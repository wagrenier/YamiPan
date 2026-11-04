"""VU microcode disassembler over the ELF: follows the VIF MPG codes of a microprogram image and prints
the micro instructions at the addresses asked for, upper | lower, until the [E] bit and its delay slot.
An image that starts with a DMA tag starts 8 bytes in: FS2's VU1 program is `vudis.py 2ec908 3f8 --all`.

    python vudis.py <image addr> <qwords> <micro addr> [...]      e.g. vudis.py 2ec908 3f8 0 2a9 2bb
    python vudis.py <image addr> <qwords> --all                     every loaded instruction

Micro addresses are instruction indices (8 bytes each), the unit vcallms takes: `vcallms 0x96` runs
the instruction printed as 0x096. Covers the opcodes i3d's micros use (FMAC
bc / q / i / plain forms, CLIP, OPMULA / OPMSUB, the integer, flag, branch and load / store lower ops);
anything else prints as raw hex. Lower integer ops print as `op id, is, it`.

Carried over from RosePan 2026-11-04 with the load / store forms and the EFU ops added.
"""
import struct
import sys

import yamipan
from elf32 import Elf

BC = 'xyzw'


def dest(w):
    d = (w >> 21) & 15
    return ''.join(c for i, c in enumerate('xyzw') if d & (8 >> i))


def upper(hi):
    ft, fs, fd, op, d = (hi >> 16) & 31, (hi >> 11) & 31, (hi >> 6) & 31, hi & 63, dest(hi)
    if (op >> 2) == 0xf:
        ext = ((hi >> 6) & 31) << 2 | (op & 3)
        acc = {0x00: 'adda', 0x04: 'suba', 0x08: 'madda', 0x0c: 'msuba', 0x18: 'mula'}
        if ext == 0x1f:
            return f'clipw.xyz vf{fs}, vf{ft}w'
        if ext == 0x2f or ext == 0x2ff & 0x7f:
            return 'nop'
        if (ext & ~3) in acc and ext < 0x1c:
            return f'{acc[ext & ~3]}{BC[ext & 3]}.{d} ACC, vf{fs}, vf{ft}'
        other = {0x10: 'itof0', 0x11: 'itof4', 0x12: 'itof12', 0x13: 'itof15', 0x14: 'ftoi0', 0x15: 'ftoi4',
                 0x16: 'ftoi12', 0x17: 'ftoi15', 0x1c: 'mulaq', 0x1d: 'abs', 0x1e: 'mulai', 0x20: 'addaq',
                 0x21: 'maddaq', 0x22: 'addai', 0x23: 'maddai', 0x28: 'adda', 0x29: 'madda', 0x2a: 'mula',
                 0x2c: 'suba', 0x2d: 'msuba', 0x2e: 'opmula'}
        if ext in other:
            return f'{other[ext]}.{d} vf{fs}, vf{ft}'
        return f'ext{ext:#x}.{d} vf{fs}, vf{ft}'
    if op < 0x1c:
        nm = {0: 'add', 1: 'sub', 2: 'madd', 3: 'msub', 4: 'max', 5: 'mini', 6: 'mul'}.get(op >> 2, '?')
        return f'{nm}{BC[op & 3]}.{d} vf{fd}, vf{fs}, vf{ft}'
    m = {0x1c: 'mulq', 0x1d: 'maxi', 0x1e: 'muli', 0x1f: 'minii', 0x20: 'addq', 0x21: 'maddq', 0x22: 'addi',
         0x23: 'maddi', 0x24: 'subq', 0x25: 'msubq', 0x26: 'subi', 0x27: 'msubi', 0x28: 'add', 0x29: 'madd',
         0x2a: 'mul', 0x2b: 'max', 0x2c: 'sub', 0x2d: 'msub', 0x2e: 'opmsub', 0x2f: 'mini'}
    return f'{m.get(op, hex(op))}.{d} vf{fd}, vf{fs}, vf{ft}'


def lower(lo):
    if lo == 0x8000033c:
        return 'nop'
    top, it, is_ = lo >> 25, (lo >> 16) & 31, (lo >> 11) & 31
    imm11 = lo & 0x7ff
    imm11 -= 0x800 if imm11 & 0x400 else 0
    if top in (0x10, 0x11, 0x12, 0x13):
        return f"{['fceq', 'fcset', 'fcand', 'fcor'][top - 0x10]} vi1, {lo & 0xffffff:#08x}"
    if top == 0x1c:
        return f'fcget vi{it}'
    t = {0x00: 'lq', 0x01: 'sq', 0x04: 'ilw', 0x05: 'isw', 0x08: 'iaddiu', 0x09: 'isubiu', 0x1a: 'fmand',
         0x16: 'fsand', 0x20: 'b', 0x21: 'bal', 0x24: 'jr', 0x25: 'jalr', 0x28: 'ibeq', 0x29: 'ibne',
         0x2c: 'ibltz', 0x2d: 'ibgtz', 0x2e: 'iblez', 0x2f: 'ibgez'}
    if top in (0x08, 0x09):
        return f'{t[top]} vi{it}, vi{is_}, {lo & 0x7fff:#x}'
    if top == 0x00:
        return f'lq.{dest(lo)} vf{it}, {imm11}(vi{is_})'
    if top == 0x01:
        return f'sq.{dest(lo)} vf{is_}, {imm11}(vi{it})'
    if top in (0x04, 0x05):
        return f'{t[top]}.{dest(lo)} vi{it}, {imm11}(vi{is_})'
    if top in t:
        return f'{t[top]} it=vi{it} is=vi{is_} imm={imm11}'
    if top == 0x40:
        op, fd = lo & 63, (lo >> 6) & 31
        if op >= 0x3c:
            ext = fd << 2 | (op & 3)
            m = {0x30: 'move', 0x31: 'mr32', 0x34: 'lqi', 0x35: 'sqi', 0x36: 'lqd', 0x37: 'sqd', 0x38: 'div',
                 0x39: 'sqrt', 0x3a: 'rsqrt', 0x3b: 'waitq', 0x3c: 'mtir', 0x3d: 'mfir', 0x3e: 'ilwr',
                 0x3f: 'iswr', 0x64: 'mfp', 0x68: 'xtop', 0x69: 'xitop', 0x6c: 'xgkick', 0x70: 'esadd',
                 0x71: 'ersadd', 0x72: 'eleng', 0x73: 'erleng', 0x74: 'eatanxy', 0x75: 'eatanxz', 0x76: 'esum',
                 0x78: 'esqrt', 0x79: 'ersqrt', 0x7a: 'einvsqrt', 0x7b: 'waitp', 0x7c: 'esin', 0x7d: 'eatan',
                 0x7e: 'eexp'}
            return f'{m.get(ext, hex(ext))}.{dest(lo)} ft={it} fs={is_}'
        if op == 0x32:
            return f'iaddi vi{it}, vi{is_}, {fd - 32 if fd & 16 else fd}'
        m = {0x30: 'iadd', 0x31: 'isub', 0x34: 'iand', 0x35: 'ior'}
        return f'{m.get(op, hex(op))} vi{fd}, vi{is_}, vi{it}'
    return f'lo {lo:08x}'


def load(elf, va, qwords):
    """The micro instructions an image's MPG codes load: {address: (lower, upper)}."""
    data = elf.read(va, qwords * 16)
    code, p = {}, 0
    while p + 4 <= len(data):
        w = struct.unpack_from('<I', data, p)[0]
        cmd = (w >> 24) & 0x7f
        p += 4
        if cmd == 0x4a:                              # MPG num, addr: num instructions, 8-aligned
            n, addr = (w >> 16) & 0xff or 256, w & 0xffff
            p += 4 if p % 8 else 0
            for i in range(n):
                code[addr + i] = struct.unpack_from('<II', data, p)
                p += 8
        elif cmd in (0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x13, 0x14, 0x15, 0x17):
            continue                                 # one-word VIF codes
        else:
            print(f'stopped at VIF code {w:#010x} (+{p - 4:#x})')
            break
    return code


def show(code, a, follow=True):
    while a in code:
        lo, hi = code[a]
        end = hi >> 30 & 1
        print(f'{a:#05x}: {upper(hi):36s} | {lower(lo)}{" [E]" if end else ""}')
        if follow and end:
            lo, hi = code[a + 1]
            print(f'{a + 1:#05x}: {upper(hi):36s} | {lower(lo)} (delay)')
            return
        a += 1


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    code = load(Elf(yamipan.ELF), int(argv[0], 16), int(argv[1], 16))
    if argv[2] == '--all':
        # every loaded run (the images leave gaps between MPG blocks)
        for a in sorted(code):
            if a - 1 not in code:
                show(code, a, follow=False)
                print('---')
        return 0
    for s in argv[2:]:
        show(code, int(s, 16))
        print('---')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
