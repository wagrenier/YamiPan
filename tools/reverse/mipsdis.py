"""Offline, read-only R5900 disassembler over the FS2 ELF (no Ghidra needed).

Carried over from ReiPan, where it was verified against Ghidra's listing (same instructions; it prints
delay slots without Ghidra's `_` and `li` as `addiu rt,zero,imm`).

    python mipsdis.py <lo> <hi>          disassemble [lo, hi) (hex addresses)
    python mipsdis.py <lo> +<len>        same, by length

Use it where Ghidra has no instructions (code after a tail call that no function covers, inline
virtuals and hooks only reached through data pointers): GhidraMCP's disassemble_bytes writes
instructions into the program unless dry_run is in the query string. Covers the integer, COP1 and
common MMI encodings and VU0 macro mode the game code uses; anything else prints as `.word`. jal/j targets and
gp-relative addresses are annotated with the cached function names (work/feat_fs2.json).
"""
import struct
import sys

import yamipan
from elf32 import Elf

R = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
     's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra']

SPECIAL = {0x00: 'sll', 0x02: 'srl', 0x03: 'sra', 0x04: 'sllv', 0x06: 'srlv', 0x07: 'srav',
           0x08: 'jr', 0x09: 'jalr', 0x0a: 'movz', 0x0b: 'movn', 0x0c: 'syscall', 0x0d: 'break',
           0x0f: 'sync', 0x10: 'mfhi', 0x11: 'mthi', 0x12: 'mflo', 0x13: 'mtlo', 0x14: 'dsllv',
           0x16: 'dsrlv', 0x17: 'dsrav', 0x18: 'mult', 0x19: 'multu', 0x1a: 'div', 0x1b: 'divu',
           0x20: 'add', 0x21: 'addu', 0x22: 'sub', 0x23: 'subu', 0x24: 'and', 0x25: 'or',
           0x26: 'xor', 0x27: 'nor', 0x28: 'mfsa', 0x29: 'mtsa', 0x2a: 'slt', 0x2b: 'sltu',
           0x2c: 'dadd', 0x2d: 'daddu', 0x2e: 'dsub', 0x2f: 'dsubu', 0x38: 'dsll', 0x3a: 'dsrl',
           0x3b: 'dsra', 0x3c: 'dsll32', 0x3e: 'dsrl32', 0x3f: 'dsra32'}
IMM = {0x08: 'addi', 0x09: 'addiu', 0x0a: 'slti', 0x0b: 'sltiu', 0x0c: 'andi', 0x0d: 'ori',
       0x0e: 'xori', 0x18: 'daddi', 0x19: 'daddiu'}
MEM = {0x1a: 'ldl', 0x1b: 'ldr', 0x1e: 'lq', 0x1f: 'sq', 0x20: 'lb', 0x21: 'lh', 0x22: 'lwl',
       0x23: 'lw', 0x24: 'lbu', 0x25: 'lhu', 0x26: 'lwr', 0x27: 'lwu', 0x28: 'sb', 0x29: 'sh',
       0x2a: 'swl', 0x2b: 'sw', 0x2c: 'sdl', 0x2d: 'sdr', 0x2e: 'swr', 0x31: 'lwc1', 0x37: 'ld',
       0x39: 'swc1', 0x3f: 'sd', 0x36: 'lqc2', 0x3e: 'sqc2', 0x2f: 'cache', 0x33: 'pref'}
BRANCH = {0x04: 'beq', 0x05: 'bne', 0x06: 'blez', 0x07: 'bgtz', 0x14: 'beql', 0x15: 'bnel',
          0x16: 'blezl', 0x17: 'bgtzl'}
REGIMM = {0x00: 'bltz', 0x01: 'bgez', 0x02: 'bltzl', 0x03: 'bgezl', 0x10: 'bltzal', 0x11: 'bgezal'}
FMT_S = {0x00: 'add.s', 0x01: 'sub.s', 0x02: 'mul.s', 0x03: 'div.s', 0x04: 'sqrt.s', 0x05: 'abs.s',
         0x06: 'mov.s', 0x07: 'neg.s', 0x16: 'rsqrt.s', 0x18: 'adda.s', 0x19: 'suba.s',
         0x1a: 'mula.s', 0x1c: 'madd.s', 0x1d: 'msub.s', 0x1e: 'madda.s', 0x1f: 'msuba.s',
         0x24: 'cvt.w.s', 0x28: 'max.s', 0x29: 'min.s', 0x30: 'c.f.s', 0x32: 'c.eq.s',
         0x34: 'c.lt.s', 0x36: 'c.le.s'}
MMI = {0x00: 'madd', 0x01: 'maddu', 0x04: 'plzcw', 0x10: 'mfhi1', 0x11: 'mthi1', 0x12: 'mflo1',
       0x13: 'mtlo1', 0x18: 'mult1', 0x19: 'multu1', 0x1a: 'div1', 0x1b: 'divu1'}


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def decode_cop2(w):
    """VU0 macro mode (COP2) and the EE <-> VU moves; None when it is neither."""
    if w >> 26 != 0x12:
        return None
    rs = (w >> 21) & 0x1f
    ft, fs, fd = (w >> 16) & 0x1f, (w >> 11) & 0x1f, (w >> 6) & 0x1f
    if rs < 0x10:
        name = {1: 'qmfc2', 2: 'cfc2', 5: 'qmtc2', 6: 'ctc2'}.get(rs)
        return f'{name} {R[ft]},vf{fs}' if name else None
    dest = ''.join(c for c, bit in zip('xyzw', (8, 4, 2, 1)) if (rs & 0xf) & bit)
    op = w & 0x3f
    bc = 'xyzw'
    three = {0x1c: 'vmulq', 0x1d: 'vmaxi', 0x1e: 'vmuli', 0x1f: 'vminii', 0x20: 'vaddq',
             0x21: 'vmaddq', 0x22: 'vaddi', 0x23: 'vmaddi', 0x24: 'vsubq', 0x25: 'vmsubq',
             0x26: 'vsubi', 0x27: 'vmsubi', 0x28: 'vadd', 0x29: 'vmadd', 0x2a: 'vmul',
             0x2b: 'vmax', 0x2c: 'vsub', 0x2d: 'vmsub', 0x2e: 'vopmsub', 0x2f: 'vmini'}
    if op < 0x1c:
        base = ('vadd', 'vsub', 'vmadd', 'vmsub', 'vmax', 'vmini', 'vmul')[op >> 2]
        return f'{base}{bc[op & 3]}.{dest} vf{fd},vf{fs},vf{ft}{bc[op & 3]}'
    if op in three:
        return f'{three[op]}.{dest} vf{fd},vf{fs},vf{ft}'
    if op >= 0x3c:
        fn = (w & 3) | (((w >> 6) & 0x1f) << 2)
        if fn < 0x10 or 0x18 <= fn < 0x1c:
            base = {0: 'vadda', 1: 'vsuba', 2: 'vmadda', 3: 'vmsuba', 6: 'vmula'}[fn >> 2]
            return f'{base}{bc[fn & 3]}.{dest} acc,vf{fs},vf{ft}{bc[fn & 3]}'
        two = {0x10: 'vitof0', 0x11: 'vitof4', 0x12: 'vitof12', 0x13: 'vitof15', 0x14: 'vftoi0',
               0x15: 'vftoi4', 0x16: 'vftoi12', 0x17: 'vftoi15', 0x1d: 'vabs', 0x30: 'vmove',
               0x31: 'vmr32'}
        if fn in two:
            return f'{two[fn]}.{dest} vf{ft},vf{fs}'
        acc = {0x1c: 'vmulaq', 0x1e: 'vmulai', 0x20: 'vaddaq', 0x21: 'vmaddaq', 0x22: 'vaddai',
               0x23: 'vmaddai', 0x24: 'vsubaq', 0x25: 'vmsubaq', 0x26: 'vsubai', 0x27: 'vmsubai',
               0x28: 'vadda', 0x29: 'vmadda', 0x2a: 'vmula', 0x2c: 'vsuba', 0x2d: 'vmsuba',
               0x2e: 'vopmula'}
        if fn in acc:
            return f'{acc[fn]}.{dest} acc,vf{fs},vf{ft}'
        if fn == 0x2f:
            return 'vnop'
        if fn in (0x38, 0x39, 0x3a):
            fsf, ftf = bc[(w >> 21) & 3], bc[(w >> 23) & 3]
            name = {0x38: 'vdiv', 0x39: 'vsqrt', 0x3a: 'vrsqrt'}[fn]
            return f'{name} Q,vf{fs}{fsf},vf{ft}{ftf}'
        if fn == 0x3b:
            return 'vwaitq'
    return None


def decode(pc, w, names, gp):
    op, rs, rt, rd = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
    sa, fn, imm = (w >> 6) & 31, w & 63, w & 0xffff
    note = ''
    if w == 0:
        return 'nop', ''
    if op == 0:
        m = SPECIAL.get(fn)
        if m is None:
            return f'.word 0x{w:08x}', ''
        if m in ('sll', 'srl', 'sra', 'dsll', 'dsrl', 'dsra', 'dsll32', 'dsrl32', 'dsra32'):
            return f'{m} {R[rd]},{R[rt]},{sa}', ''
        if m in ('sllv', 'srlv', 'srav', 'dsllv', 'dsrlv', 'dsrav'):
            return f'{m} {R[rd]},{R[rt]},{R[rs]}', ''
        if m == 'jr':
            return f'jr {R[rs]}', ''
        if m == 'jalr':
            return f'jalr {R[rd]},{R[rs]}', ''
        if m in ('mfhi', 'mflo', 'mfsa'):
            return f'{m} {R[rd]}', ''
        if m in ('mthi', 'mtlo', 'mtsa'):
            return f'{m} {R[rs]}', ''
        if m in ('mult', 'multu', 'div', 'divu'):
            return (f'{m} {R[rd]},{R[rs]},{R[rt]}' if rd else f'{m} {R[rs]},{R[rt]}'), ''
        if m in ('syscall', 'break', 'sync'):
            return m, ''
        if m in ('addu', 'daddu', 'or') and rt == 0:
            return f'move {R[rd]},{R[rs]}', ''
        return f'{m} {R[rd]},{R[rs]},{R[rt]}', ''
    if op == 1:
        m = REGIMM.get(rt)
        if m is None:
            return f'.word 0x{w:08x}', ''
        return f'{m} {R[rs]},0x{pc + 4 + s16(imm) * 4:08x}', ''
    if op in (2, 3):
        t = (pc & 0xf0000000) | ((w & 0x3ffffff) << 2)
        return f'{"j" if op == 2 else "jal"} 0x{t:08x}', names.get(f'{t:08x}', '')
    if op in BRANCH:
        m, t = BRANCH[op], pc + 4 + s16(imm) * 4
        if op in (6, 7, 0x16, 0x17):
            return f'{m} {R[rs]},0x{t:08x}', ''
        if op == 4 and rs == 0 and rt == 0:
            return f'b 0x{t:08x}', ''
        return f'{m} {R[rs]},{R[rt]},0x{t:08x}', ''
    if op in IMM:
        m = IMM[op]
        v = imm if m in ('andi', 'ori', 'xori') else s16(imm)
        if rs == 28 and gp is not None:
            a = (gp + s16(imm)) & 0xffffffff
            note = f'&0x{a:08x} ' + names.get(f'{a:08x}', '')
        return f'{m} {R[rt]},{R[rs]},{v:#x}' if v >= 0 else f'{m} {R[rt]},{R[rs]},-{-v:#x}', note.strip()
    if op == 0x0f:
        return f'lui {R[rt]},0x{imm:x}', ''
    if op in MEM:
        m = MEM[op]
        if rs == 28 and gp is not None:
            note = f'0x{(gp + s16(imm)) & 0xffffffff:08x}'
        reg = f'f{rt}' if m in ('lwc1', 'swc1') else R[rt]
        off = s16(imm)
        return f'{m} {reg},{"-" if off < 0 else ""}{abs(off):#x}({R[rs]})', note
    if op == 0x11:  # COP1
        if rs == 0:
            return f'mfc1 {R[rt]},f{rd}', ''
        if rs == 4:
            return f'mtc1 {R[rt]},f{rd}', ''
        if rs == 2:
            return f'cfc1 {R[rt]},${rd}', ''
        if rs == 6:
            return f'ctc1 {R[rt]},${rd}', ''
        if rs == 8:
            t = pc + 4 + s16(imm) * 4
            return f'{["bc1f", "bc1t", "bc1fl", "bc1tl"][rt & 3]} 0x{t:08x}', ''
        fs, ft, fd = rd, rt, sa
        if rs == 0x10 and fn in FMT_S:
            m = FMT_S[fn]
            if m.startswith('c.'):
                return f'{m} f{fs},f{ft}', ''
            if m in ('mov.s', 'neg.s', 'abs.s', 'sqrt.s', 'cvt.w.s'):
                return f'{m} f{fd},f{fs}' if m != 'sqrt.s' else f'{m} f{fd},f{ft}', ''
            if m in ('adda.s', 'suba.s', 'mula.s', 'madda.s', 'msuba.s'):
                return f'{m} f{fs},f{ft}', ''
            return f'{m} f{fd},f{fs},f{ft}', ''
        if rs == 0x14 and fn == 0x20:
            return f'cvt.s.w f{fd},f{fs}', ''
        return f'.word 0x{w:08x}', 'cop1'
    if op == 0x1c:  # MMI
        m = MMI.get(fn)
        if m:
            return f'{m} {R[rd]},{R[rs]},{R[rt]}', ''
        return f'.word 0x{w:08x}', 'mmi'
    text = decode_cop2(w)
    if text:
        return text, ''
    return f'.word 0x{w:08x}', ''


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    lo = int(argv[0], 16)
    hi = lo + int(argv[1][1:], 16) if argv[1].startswith('+') else int(argv[1], 16)
    elf = Elf(yamipan.ELF)
    names = yamipan.cached_names()
    for pc in range(lo & ~3, hi, 4):
        w = elf.u32(pc)
        if w is None:
            print(f'{pc:08x}: <unmapped>')
            continue
        if f'{pc:08x}' in names:
            print(f'{names[f"{pc:08x}"]}:')
        text, note = decode(pc, w, names, elf.gp)
        print(f'{pc:08x}: {text:<32}' + (f' ; {note}' if note else ''))


if __name__ == '__main__':
    main(sys.argv[1:])
