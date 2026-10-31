"""Run small FS2 leaf functions on a tiny EE (R5900 integer subset) interpreter, to check a
C rewrite against the ROM over every input -- the switch-heavy table functions Ghidra
decompiles into goto soup (hazards.md "Switch tables").

python mipsemu.py <lo> <hi> <entry> [arg ...]          run once: a0.. = args, print v0
python mipsemu.py <lo> <hi> <entry> --grid N M [--out F] v0 for every (a0 < N, a1 < M) as JSON
                                                        (rows a0, columns a1); default work/emu_<entry>.json

<lo> <hi> is the byte range to disassemble (ghidra.disasm: GhidraMCP 5.14.2 or mipsdis.py); it must hold the entry
and everything it branches to. Loads read the ELF (yamipan.ELF); stores go to a scratch dict.
Calls (jal / jalr / tail j) and loads the ELF cannot answer go to hooks: from Python, build
Emu(listing_lines, mem_hook, call_hook) yourself. The CLI's hooks make every unknown load / call
an error. Covered: the integer ops these functions use (add/sub/logic/shift/slt/mov, loads,
stores, branches incl. likely, jr/jalr/jal/j). No FPU, no MMI: extend exec1 as needed.

How ReiPan used it (FF3, 2026-10-14): three byte-switch tables checked for all 65536 input pairs against
a MinGW gcc build of the C rewrite; a non-leaf function with Python hooks for the loads and calls:
Emu(listing(lo, hi), mem_hook, call_hook) where mem_hook answers the global pointer and the work's
fields with fake addresses and call_hook the vtable / helper calls; the C body is compiled standalone
(mock work struct) and both run over the same input grid. Its memory is keyed by (address, size):
code that stores a short and loads the word around it needs a byte-addressed subclass; run() resets
registers, so nested calls need their own stack.
"""
import json
import re
import struct
import sys

import yamipan

M32 = 0xffffffff
M64 = 0xffffffffffffffff


def s32(x):
    x &= M32
    return x - (1 << 32) if x & 0x80000000 else x


def s64(x):
    x &= M64
    return x - (1 << 64) if x & (1 << 63) else x


class Elf:
    def __init__(self, path=yamipan.ELF):
        self.data = open(path, 'rb').read()
        phoff = struct.unpack_from('<I', self.data, 0x1c)[0]
        phnum = struct.unpack_from('<H', self.data, 0x2c)[0]
        self.segs = []
        for i in range(phnum):
            t, off, va, _pa, fsz, _msz = struct.unpack_from('<6I', self.data, phoff + i * 32)
            if t == 1:
                self.segs.append((va, off, fsz))

    def read(self, addr, n):
        for va, off, fsz in self.segs:
            if va <= addr and addr + n <= va + fsz:
                return self.data[off + addr - va:off + addr - va + n]
        return None


def listing(lo, hi):
    """'addr: [_]op args' lines of the range, as ghidra.py asm prints them (GhidraMCP 5.14.2's
    disassemble_bytes, dry run, or mipsdis.py offline when 5.14.2 does not answer)."""
    from ghidra import disasm
    return [f'{i["address"]}: {i["mnemonic"]} {i["operands"]}' for i in disasm(lo, hi, chunk=0xf00)]


class Emu:
    def __init__(self, lines, mem_hook=None, call_hook=None, elf=None):
        self.ins = {}
        for line in lines:
            m = re.match(r'([0-9a-f]{8}): (_?)(\S+)\s*(.*)', line.strip())
            if m:
                args = [a.strip() for a in m.group(4).split(',')] if m.group(4) else []
                self.ins[int(m.group(1), 16)] = (m.group(3), args)
        self.mem_hook = mem_hook
        self.call_hook = call_hook
        self.elf = elf or Elf()

    def run(self, pc, args, maxsteps=100000):
        ra = 0xfffffff0
        self.r = {'zero': 0, 'sp': 0x7ff000, 'ra': ra}
        for n, v in zip(('a0', 'a1', 'a2', 'a3', 't0', 't1'), args):
            self.r[n] = v
        self.mem = {}
        for _ in range(maxsteps):
            if pc == ra:
                return s32(self.g('v0'))
            op, a = self.ins[pc]
            if op in ('beq', 'bne', 'beql', 'bnel', 'bgez', 'bltz', 'blez', 'bgtz'):
                if op.startswith('b') and op[1:3] in ('eq', 'ne'):
                    taken = (self.g(a[0]) & M32) == (self.g(a[1]) & M32)
                    if op.startswith('bne'):
                        taken = not taken
                    tgt = int(a[2], 0)
                else:
                    v = s32(self.g(a[0]))
                    taken = {'bgez': v >= 0, 'bltz': v < 0, 'blez': v <= 0, 'bgtz': v > 0}[op]
                    tgt = int(a[1], 0)
                if taken or not op.endswith('l'):
                    self.exec1(pc + 4)
                pc = tgt if taken else pc + 8
            elif op == 'b':
                self.exec1(pc + 4)
                pc = int(a[0], 0)
            elif op == 'jr':
                tgt = self.g(a[0]) & M32
                self.exec1(pc + 4)
                pc = tgt
            elif op in ('jal', 'jalr'):
                tgt = int(a[0], 0) if op == 'jal' else self.g(a[0]) & M32
                self.exec1(pc + 4)
                self.r['v0'] = self.call(tgt)
                pc += 8
            elif op == 'j':                     # tail call
                self.exec1(pc + 4)
                return s32(self.call(int(a[0], 0)))
            else:
                self.exec1(pc)
                pc += 4
        raise RuntimeError('no return after %d steps' % maxsteps)

    def call(self, tgt):
        if self.call_hook is None:
            raise RuntimeError('call 0x%x without a call hook' % tgt)
        return self.call_hook(tgt, self.r)

    def g(self, n):
        return 0 if n == 'zero' else self.r.get(n, 0)

    def s(self, n, v):
        if n != 'zero':
            self.r[n] = v

    def ea(self, arg):
        m = re.match(r'(-?0x[0-9a-f]+|-?\d+)\((\w+)\)', arg)
        return (int(m.group(1), 0) + self.g(m.group(2))) & M32

    def load(self, addr, size, signed):
        if (addr, size) in self.mem:
            return self.mem[(addr, size)]
        if self.mem_hook:
            v = self.mem_hook(addr, size)
            if v is not None:
                return v
        b = self.elf.read(addr, size)
        if b is None:
            raise RuntimeError('load 0x%x (%d bytes): not in the ELF and no hook' % (addr, size))
        return int.from_bytes(b, 'little', signed=signed)

    def exec1(self, pc):
        op, a = self.ins[pc]
        g, s, imm = self.g, self.s, (lambda x: int(x, 0))
        if op == 'nop':
            return
        if op == 'lui': s(a[0], s32(imm(a[1]) << 16))
        elif op == 'li': s(a[0], imm(a[1]))
        elif op == 'move': s(a[0], g(a[1]))
        elif op == 'andi': s(a[0], g(a[1]) & imm(a[2]))
        elif op == 'ori': s(a[0], g(a[1]) | imm(a[2]))
        elif op == 'xori': s(a[0], g(a[1]) ^ imm(a[2]))
        elif op == 'and': s(a[0], g(a[1]) & g(a[2]))
        elif op == 'or': s(a[0], g(a[1]) | g(a[2]))
        elif op == 'xor': s(a[0], g(a[1]) ^ g(a[2]))
        elif op == 'nor': s(a[0], ~(g(a[1]) | g(a[2])))
        elif op in ('addiu', 'daddiu'): s(a[0], s32(g(a[1]) + imm(a[2])))
        elif op in ('addu', 'daddu'): s(a[0], s32(g(a[1]) + g(a[2])))
        elif op in ('subu', 'dsubu'): s(a[0], s32(g(a[1]) - g(a[2])))
        elif op == 'negu': s(a[0], s32(-g(a[1])))
        elif op == 'sll': s(a[0], s32(g(a[1]) << imm(a[2])))
        elif op == 'sra': s(a[0], s32(g(a[1])) >> imm(a[2]))
        elif op == 'srl': s(a[0], s32((g(a[1]) & M32) >> imm(a[2])))
        elif op == 'dsrl32': s(a[0], (g(a[1]) & M64) >> (32 + imm(a[2])))
        elif op == 'dsll32': s(a[0], (g(a[1]) << (32 + imm(a[2]))) & M64)
        elif op == 'dsra32': s(a[0], s64(g(a[1])) >> (32 + imm(a[2])))
        elif op == 'slti': s(a[0], int(s32(g(a[1])) < imm(a[2])))
        elif op == 'sltiu': s(a[0], int((g(a[1]) & M32) < (imm(a[2]) & M32)))
        elif op == 'slt': s(a[0], int(s32(g(a[1])) < s32(g(a[2]))))
        elif op == 'sltu': s(a[0], int((g(a[1]) & M32) < (g(a[2]) & M32)))
        elif op == 'movz':
            if g(a[2]) == 0: s(a[0], g(a[1]))
        elif op == 'movn':
            if g(a[2]) != 0: s(a[0], g(a[1]))
        elif op == 'mult': s(a[0], s32(s32(g(a[1])) * s32(g(a[2]))))
        elif op == 'lw': s(a[0], s32(self.load(self.ea(a[1]), 4, True)))
        elif op == 'lbu': s(a[0], self.load(self.ea(a[1]), 1, False))
        elif op == 'lb': s(a[0], self.load(self.ea(a[1]), 1, True))
        elif op == 'lhu': s(a[0], self.load(self.ea(a[1]), 2, False))
        elif op == 'lh': s(a[0], self.load(self.ea(a[1]), 2, True))
        elif op == 'ld': s(a[0], self.load(self.ea(a[1]), 8, False))
        elif op in ('sd', 'sw', 'sh', 'sb'):
            self.mem[(self.ea(a[1]), {'sd': 8, 'sw': 4, 'sh': 2, 'sb': 1}[op])] = g(a[0])
        else:
            raise RuntimeError('unsupported op %s at 0x%x' % (op, pc))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    lo, hi, entry = (int(x, 16) for x in argv[:3])
    emu = Emu(listing(lo, hi))
    rest = argv[3:]
    if rest and rest[0] == '--grid':
        n, m = int(rest[1], 0), int(rest[2], 0)
        out = rest[rest.index('--out') + 1] if '--out' in rest else str(yamipan.WORK / f'emu_{entry:08x}.json')
        grid = [[emu.run(entry, [i, j]) for j in range(m)] for i in range(n)]
        json.dump(grid, open(out, 'w'))
        print(f'{n}x{m} results -> {out}')
    else:
        print(emu.run(entry, [int(x, 0) for x in rest]))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
