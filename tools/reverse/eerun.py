#!/usr/bin/env python3
"""Run the ROM's own functions on a small EE interpreter, to compare them with a reconstruction.

mipsemu.py is the leaf tool: integers only, memory by (address, size). This one is for code that calls
code: byte-addressed memory holding the ELF, 128-bit registers, the COP1 FPU, the VU0 macro ops seen so
far, real jal / jr (any address is disassembled on first use, mipsdis over the ELF) and a bump heap
behind operator_new. Nothing here knows a game structure; a comparison is a small script:

    from eerun import Ee
    ee = Ee()
    file = ee.alloc(len(data)); ee.write(file, data)
    core = ee.alloc(0x24)
    ee.call(0x257fa0, core, 0, file)        # integer args in a0..; v0 back
    f0 = ee.callf(0x253420, ...)            # the same, f0 back as a float

Used by rutecmp.py (RoutePlayer against game/routeplayer.cpp). An op it does not know raises with its
address: add it to step(). Single precision is rounded with struct, like the EE's; denormals are not
flushed, overflow is clamped to the largest float only where a result is stored, and div.s by 0 gives
the largest float, as the EE. gp is the ELF's (.reginfo).
"""
import math
import re
import struct

import ghidra
import mipsemu
import yamipan

M32 = 0xffffffff
M64 = 0xffffffffffffffff
M128 = (1 << 128) - 1
GP = 0x322870
RETURN = 0xfffffff0
GPR = ('zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra').split()
EA = re.compile(r'(-?0x[0-9a-f]+|-?\d+)\((\w+)\)')
FIELD = {'x': 0, 'y': 1, 'z': 2, 'w': 3}


def s32(x):
    x &= M32
    return x - (1 << 32) if x & 0x80000000 else x


def s64(x):
    x &= M64
    return x - (1 << 64) if x & (1 << 63) else x


def f32(bits):
    return struct.unpack('<f', struct.pack('<I', bits & M32))[0]


def b32(value):
    try:
        return struct.unpack('<I', struct.pack('<f', value))[0]
    except OverflowError:
        return 0x7f7fffff if value > 0 else 0xff7fffff


def round32(value):
    return f32(b32(value))


def vudiv(top, bottom):
    """The VU's div: by zero, the largest float of the two signs."""
    if bottom == 0.0:
        negative = (b32(top) ^ b32(bottom)) >> 31
        return f32(0xff7fffff if negative else 0x7f7fffff)
    return round32(top / bottom)


class Ee:
    def __init__(self, size=0x2000000):
        self.m = bytearray(size)
        elf = mipsemu.Elf()
        for va, off, fsz in elf.segs:
            self.m[va:va + fsz] = elf.data[off:off + fsz]
        self.heap = 0x1000000
        self.ins = {}
        self.hooks = {}
        self.r = {}
        self.f = [0] * 32       # bits: code moves pointers through them
        self.vf = [[0.0, 0.0, 0.0, 0.0] for _ in range(32)]
        self.vf[0][3] = 1.0
        self.acc = 0.0
        self.vacc = [0.0, 0.0, 0.0, 0.0]
        self.vstatus = 0        # the sticky flags that are kept: 0x80 sign
        self.vclip = 0          # vi18: six bits per VCLIP, the last at the bottom
        self.q = 0.0
        self.cc = False
        self.hi = 0
        self.lo = 0
        self.count = 0
        for name in ('operator_new', 'operator_new[]'):
            self.hook(name, lambda ee: ee.alloc(ee.g('a0')))
        for name in ('operator_delete', 'operator_delete[]'):
            self.hook(name, lambda ee: 0)

    def hook(self, name, fn):
        for addr, row in yamipan.functions().items():
            if row.get('name') == name:
                self.hooks[addr] = fn

    # memory -----------------------------------------------------------
    def alloc(self, n):
        at = self.heap
        self.heap = (self.heap + n + 0xf) & ~0xf
        self.m[at:self.heap] = bytes(self.heap - at)
        return at

    def write(self, addr, data):
        self.m[addr:addr + len(data)] = data

    def rd(self, addr, n):
        return int.from_bytes(self.m[addr & 0x1ffffff:(addr & 0x1ffffff) + n], 'little')

    def wr(self, addr, n, value):
        self.m[addr & 0x1ffffff:(addr & 0x1ffffff) + n] = (value & ((1 << (n * 8)) - 1)).to_bytes(n, 'little')

    def u32(self, addr):
        return self.rd(addr, 4)

    def s16(self, addr):
        v = self.rd(addr, 2)
        return v - 0x10000 if v & 0x8000 else v

    def float(self, addr):
        return f32(self.rd(addr, 4))

    def setfloat(self, addr, value):
        self.wr(addr, 4, b32(value))

    # running ----------------------------------------------------------
    def g(self, n):
        return 0 if n == 'zero' else self.r.get(n, 0)

    def s(self, n, v):
        if n != 'zero':
            self.r[n] = v

    def ea(self, arg):
        m = EA.match(arg)
        return (int(m.group(1), 0) + self.g(m.group(2))) & M32

    def decode(self, pc):
        for row in ghidra.disasm(pc, pc + 0x400):
            args = [a.strip() for a in row['operands'].split(',')] if row['operands'] else []
            self.ins.setdefault(int(row['address'], 16), (row['mnemonic'].lstrip('_'), args))
        return self.ins[pc]

    def call(self, pc, *args, maxsteps=20000000):
        self.r = {'sp': 0x1ff0000 - 0x1000, 'gp': GP, 'ra': RETURN}
        for name, value in zip(('a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3'), args):
            self.r[name] = value
        for _ in range(maxsteps):
            if pc == RETURN:
                return s32(self.g('v0'))
            pc = self.step(pc)
        raise RuntimeError('no return after %d steps' % maxsteps)

    def callf(self, pc, *args):
        self.call(pc, *args)
        return f32(self.f[0])

    def slot(self, pc):
        self.step(pc)

    def step(self, pc):
        self.count += 1
        if pc in self.hooks:
            self.r['v0'] = self.hooks[pc](self)
            return self.g('ra') & M32
        ins = self.ins.get(pc) or self.decode(pc)
        op, a = ins
        g = self.g
        if op in ('beq', 'bne', 'beql', 'bnel'):
            taken = (g(a[0]) & M64) == (g(a[1]) & M64)
            if op.startswith('bne'):
                taken = not taken
            return self.branch(pc, taken, int(a[2], 0), op.endswith('l'))
        if op in ('bgez', 'bltz', 'blez', 'bgtz', 'bgezl', 'bltzl', 'blezl', 'bgtzl'):
            v = s64(g(a[0]))
            taken = {'bgez': v >= 0, 'bltz': v < 0, 'blez': v <= 0, 'bgtz': v > 0}[op[:4]]
            return self.branch(pc, taken, int(a[1], 0), op.endswith('l'))
        if op in ('bc1t', 'bc1f', 'bc1tl', 'bc1fl'):
            taken = self.cc if op[3] == 't' else not self.cc
            return self.branch(pc, taken, int(a[0], 0), op.endswith('l'))
        if op == 'b':
            self.step(pc + 4)
            return int(a[0], 0)
        if op == 'j':
            self.step(pc + 4)
            return int(a[0], 0)
        if op == 'jr':
            tgt = g(a[0]) & M32
            self.step(pc + 4)
            return tgt
        if op == 'jal':
            self.r['ra'] = pc + 8
            self.step(pc + 4)
            return int(a[0], 0)
        if op == 'jalr':
            tgt = g(a[-1]) & M32
            self.r['ra'] = pc + 8
            self.step(pc + 4)
            return tgt
        self.exec1(op, a, pc)
        return pc + 4

    def branch(self, pc, taken, tgt, likely):
        if taken or not likely:
            self.step(pc + 4)
        return tgt if taken else pc + 8

    def unaligned64(self, op, a):
        """A word of a struct copy that may straddle eight bytes (little-endian)."""
        addr = self.ea(a[1])
        at, low = addr & ~7, (addr & 7) * 8
        mem, reg = self.rd(at, 8), self.g(a[0]) & M64
        if op == 'ldl':
            self.s(a[0], s64((mem << (56 - low)) | (reg & ((1 << (56 - low)) - 1))))
        elif op == 'ldr':
            self.s(a[0], s64((mem >> low) | (reg & ~(M64 >> low))))
        elif op == 'sdl':
            self.wr(at, 8, (mem & ~(M64 >> (56 - low)) & M64) | (reg >> (56 - low)))
        else:
            self.wr(at, 8, (mem & ((1 << low) - 1)) | ((reg << low) & M64))

    def exec1(self, op, a, pc):
        g, s = self.g, self.s
        imm = lambda x: int(x, 0)
        if op == 'nop': return
        if op == 'lui': s(a[0], s32(imm(a[1]) << 16))
        elif op == 'addi': s(a[0], s32(g(a[1]) + imm(a[2])))
        elif op == 'move': s(a[0], g(a[1]))
        elif op == 'andi': s(a[0], g(a[1]) & imm(a[2]))
        elif op == 'ori': s(a[0], g(a[1]) | imm(a[2]))
        elif op == 'xori': s(a[0], g(a[1]) ^ imm(a[2]))
        elif op == 'and': s(a[0], g(a[1]) & g(a[2]))
        elif op == 'or': s(a[0], g(a[1]) | g(a[2]))
        elif op == 'xor': s(a[0], g(a[1]) ^ g(a[2]))
        elif op == 'nor': s(a[0], ~(g(a[1]) | g(a[2])))
        elif op == 'addiu': s(a[0], s32(g(a[1]) + imm(a[2])))
        elif op == 'daddiu': s(a[0], s64(g(a[1]) + imm(a[2])))
        elif op == 'addu': s(a[0], s32(g(a[1]) + g(a[2])))
        elif op == 'daddu': s(a[0], s64(g(a[1]) + g(a[2])))
        elif op == 'subu': s(a[0], s32(g(a[1]) - g(a[2])))
        elif op == 'dsubu': s(a[0], s64(g(a[1]) - g(a[2])))
        elif op == 'negu': s(a[0], s32(-g(a[1])))
        elif op == 'sll': s(a[0], s32(g(a[1]) << imm(a[2])))
        elif op == 'sllv': s(a[0], s32(g(a[1]) << (g(a[2]) & 31)))
        elif op == 'sra': s(a[0], s32(g(a[1])) >> imm(a[2]))
        elif op == 'srl': s(a[0], s32((g(a[1]) & M32) >> imm(a[2])))
        elif op == 'dsll': s(a[0], s64(g(a[1]) << imm(a[2])))
        elif op == 'dsrl': s(a[0], (g(a[1]) & M64) >> imm(a[2]))
        elif op == 'dsra': s(a[0], s64(g(a[1])) >> imm(a[2]))
        elif op == 'dsll32': s(a[0], s64(g(a[1]) << (32 + imm(a[2]))))
        elif op == 'dsrl32': s(a[0], (g(a[1]) & M64) >> (32 + imm(a[2])))
        elif op == 'dsra32': s(a[0], s64(g(a[1])) >> (32 + imm(a[2])))
        elif op == 'slti': s(a[0], int(s64(g(a[1])) < imm(a[2])))
        elif op == 'sltiu': s(a[0], int((g(a[1]) & M64) < (imm(a[2]) & M64)))
        elif op == 'slt': s(a[0], int(s64(g(a[1])) < s64(g(a[2]))))
        elif op == 'sltu': s(a[0], int((g(a[1]) & M64) < (g(a[2]) & M64)))
        elif op == 'movz':
            if g(a[2]) & M64 == 0: s(a[0], g(a[1]))
        elif op == 'movn':
            if g(a[2]) & M64 != 0: s(a[0], g(a[1]))
        elif op == 'plzcw':                     # the leading bits equal to the sign, less one, in each word
            def sign_bits(word):
                word = word & M32
                top = word >> 31
                n = 0
                while n < 32 and (word >> (31 - n)) & 1 == top:
                    n += 1
                return n - 1
            s(a[0], sign_bits(g(a[1])) | sign_bits(g(a[1]) >> 32) << 32)
        elif op == 'mult' and len(a) == 3: s(a[0], s32(s32(g(a[1])) * s32(g(a[2]))))
        elif op == 'mult':
            product = s32(g(a[0])) * s32(g(a[1]))
            self.lo, self.hi = s32(product), s32(product >> 32)
        elif op == 'div':
            x, y = s32(g(a[-2])), s32(g(a[-1]))
            if y:
                q = int(x / y)
                self.lo, self.hi = q, x - q * y
        elif op == 'mfhi': s(a[0], self.hi)
        elif op == 'mflo': s(a[0], self.lo)
        elif op == 'lw': s(a[0], s32(self.rd(self.ea(a[1]), 4)))
        elif op == 'lwu': s(a[0], self.rd(self.ea(a[1]), 4))
        elif op == 'lbu': s(a[0], self.rd(self.ea(a[1]), 1))
        elif op == 'lhu': s(a[0], self.rd(self.ea(a[1]), 2))
        elif op == 'lb':
            v = self.rd(self.ea(a[1]), 1)
            s(a[0], v - 0x100 if v & 0x80 else v)
        elif op == 'lh': s(a[0], self.s16(self.ea(a[1])))
        elif op == 'ld': s(a[0], s64(self.rd(self.ea(a[1]), 8)))
        elif op == 'lq': s(a[0], self.rd(self.ea(a[1]) & ~0xf, 16))
        elif op == 'sb': self.wr(self.ea(a[1]), 1, g(a[0]))
        elif op == 'sh': self.wr(self.ea(a[1]), 2, g(a[0]))
        elif op == 'sw': self.wr(self.ea(a[1]), 4, g(a[0]))
        elif op == 'sd': self.wr(self.ea(a[1]), 8, g(a[0]))
        elif op in ('ldl', 'ldr', 'sdl', 'sdr'): self.unaligned64(op, a)
        elif op == 'sq': self.wr(self.ea(a[1]) & ~0xf, 16, g(a[0]) & M128)
        else:
            self.cop(op, a, pc)

    def cop(self, op, a, pc):
        g, s = self.g, self.s
        F = lambda n: f32(self.f[int(n[1:])])

        def setf(n, value):
            self.f[int(n[1:])] = b32(value)

        if op == 'lwc1': self.f[int(a[0][1:])] = self.rd(self.ea(a[1]), 4)
        elif op == 'swc1': self.wr(self.ea(a[1]), 4, self.f[int(a[0][1:])])
        elif op == 'mtc1': self.f[int(a[1][1:])] = g(a[0]) & M32
        elif op == 'mfc1': s(a[0], s32(self.f[int(a[1][1:])]))
        elif op == 'mov.s': self.f[int(a[0][1:])] = self.f[int(a[1][1:])]
        elif op == 'cvt.s.w': setf(a[0], float(s32(self.f[int(a[1][1:])])))
        elif op == 'cvt.w.s': self.f[int(a[0][1:])] = max(-0x80000000, min(0x7fffffff, int(F(a[1])))) & M32
        elif op == '.word' and int(a[0], 16) & 0xfc0007ff == 0x70000389:     # pcpyld: mipsdis has no MMI
            word = int(a[0], 16)
            rs, rt, rd = (GPR[(word >> shift) & 0x1f] for shift in (21, 16, 11))
            s(rd, (g(rs) & M64) << 64 | (g(rt) & M64))
        elif op == '.word' and int(a[0], 16) & 0xfc1f07ff == 0x70000628:     # paddub rd, rs, zero: a 128-bit move
            word = int(a[0], 16)
            s(GPR[(word >> 11) & 0x1f], g(GPR[(word >> 21) & 0x1f]))
        elif op == '.word' and int(a[0], 16) & 0xfe0007ff == 0x4a0001ff:     # vclipw: mipsdis has no such op
            word = int(a[0], 16)
            fs, w = self.vf[(word >> 11) & 0x1f], abs(self.vf[(word >> 16) & 0x1f][3])
            flags = 0
            for i in range(3):
                flags |= (1 << (i * 2) if fs[i] > w else 0) | (2 << (i * 2) if fs[i] < -w else 0)
            self.vclip = ((self.vclip << 6) | flags) & 0xffffff
        elif op == '.word' and int(a[0], 16) >> 26 == 0x1c:
            self.mmi(int(a[0], 16), pc)
        elif op == 'pref':
            pass
        elif op == 'div1':
            n, d = s32(g(a[1])), s32(g(a[2]))
            q = abs(n) // abs(d) if d else 0
            self.lo1 = -q if (n < 0) != (d < 0) else q
        elif op == 'mflo1': s(a[0], self.lo1)
        elif op == 'mult1': s(a[0], s32(s32(g(a[1])) * s32(g(a[2]))))
        elif op == 'neg.s': self.f[int(a[0][1:])] = self.f[int(a[1][1:])] ^ 0x80000000
        elif op == 'abs.s': self.f[int(a[0][1:])] = self.f[int(a[1][1:])] & 0x7fffffff
        elif op == 'add.s': setf(a[0], F(a[1]) + F(a[2]))
        elif op == 'sub.s': setf(a[0], F(a[1]) - F(a[2]))
        elif op == 'mul.s': setf(a[0], F(a[1]) * F(a[2]))
        elif op == 'div.s':     # by 0: the largest float, with the sign of the two
            setf(a[0], F(a[1]) / F(a[2]) if F(a[2]) else math.copysign(1.0, F(a[1])) * math.copysign(f32(0x7f7fffff), F(a[2])))
        elif op == 'sqrt.s': setf(a[0], math.sqrt(abs(F(a[1]))))
        elif op == 'mula.s': self.acc = round32(F(a[0]) * F(a[1]))
        elif op == 'adda.s': self.acc = round32(F(a[0]) + F(a[1]))
        elif op == 'suba.s': self.acc = round32(F(a[0]) - F(a[1]))
        elif op == 'madda.s': self.acc = round32(self.acc + round32(F(a[0]) * F(a[1])))
        elif op == 'madd.s': setf(a[0], self.acc + round32(F(a[1]) * F(a[2])))
        elif op == 'msub.s': setf(a[0], self.acc - round32(F(a[1]) * F(a[2])))
        elif op == 'c.lt.s': self.cc = F(a[0]) < F(a[1])
        elif op == 'c.le.s': self.cc = F(a[0]) <= F(a[1])
        elif op == 'c.eq.s': self.cc = F(a[0]) == F(a[1])
        elif op == 'lqc2':
            at = self.ea(a[1]) & ~0xf
            self.vf[self.vnum(a[0])] = [self.float(at + i * 4) for i in range(4)]
        elif op == 'sqc2':
            at = self.ea(a[1]) & ~0xf
            for i in range(4):
                self.setfloat(at + i * 4, self.vf[self.vnum(a[0])][i])
        elif op == 'qmfc2':
            v = self.vf[self.vnum(a[1])]
            bits = 0
            for i in range(4):
                bits |= b32(v[i]) << (i * 32)
            s(a[0], bits)
        elif op == 'qmtc2':
            bits = g(a[0]) & M128
            self.vf[self.vnum(a[1])] = [f32(bits >> (i * 32)) for i in range(4)]
        elif op == 'vrsqrt':
            self.q = vudiv(self.vcomp(a[1]), math.sqrt(abs(self.vcomp(a[2]))))
        elif op == 'vdiv':
            self.q = vudiv(self.vcomp(a[1]), self.vcomp(a[2]))
        elif op == 'vsqrt':
            self.q = round32(math.sqrt(abs(self.vcomp(a[2]))))
        elif op in ('vwaitq', 'vnop'):
            pass
        elif op == 'ctc2':
            self.vstatus = g(a[0]) & 0xfff
        elif op == 'cfc2':      # vi18: the clip flags
            s(a[0], self.vclip if a[1].endswith('18') else self.vstatus)
        elif op.startswith('v'):
            self.vop(op, a, pc)
        else:
            raise RuntimeError('unsupported op %s %s at 0x%x' % (op, a, pc))

    def vnum(self, name):
        return int(name[2:4].rstrip('xyzw')) if name.startswith('vf') else GPR.index(name)

    def vcomp(self, name):
        return self.vf[self.vnum(name)][FIELD[name[-1]]]

    def mmi(self, word, pc):
        """The 128-bit MMI ops seen so far: mipsdis prints them as .word."""
        rs, rt, rd = (GPR[(word >> shift) & 0x1f] for shift in (21, 16, 11))
        key = (word & 0x3f, (word >> 6) & 0x1f)
        a, b = self.g(rs) & M128, self.g(rt) & M128
        lanes = lambda v: [(v >> (i * 32)) & M32 for i in range(4)]
        join = lambda w: sum(x << (i * 32) for i, x in enumerate(w))
        la, lb = lanes(a), lanes(b)
        if key == (0x08, 0x02):                 # pcgtw
            out = join([M32 if s32(x) > s32(y) else 0 for x, y in zip(la, lb)])
        elif key == (0x09, 0x12):               # pand
            out = a & b
        elif key == (0x29, 0x12):               # por
            out = a | b
        elif key == (0x29, 0x13):               # pnor
            out = ~(a | b) & M128
        elif key == (0x08, 0x12):               # pextlw
            out = join([lb[0], la[0], lb[1], la[1]])
        elif key == (0x28, 0x12):               # pextuw
            out = join([lb[2], la[2], lb[3], la[3]])
        elif key == (0x29, 0x0e):               # pcpyud
            out = (a >> 64) | ((b >> 64) << 64)
        else:
            raise RuntimeError('unsupported MMI word 0x%08x at 0x%x' % (word, pc))
        self.s(rd, out)

    def vset(self, dest, fields, values):
        fd = list(self.vacc if dest == 'acc' else self.vf[self.vnum(dest)])
        for field in fields:
            i = FIELD[field]
            fd[i] = values[i]
            if dest != 'acc' and b32(values[i]) >> 31:
                self.vstatus |= 0x80
        if dest == 'acc':
            self.vacc = fd
        elif dest != 'vf0':
            self.vf[self.vnum(dest)] = fd

    def vbroad(self, name, form):
        """What a VU op's ft is: a register, one lane of it broadcast, or Q."""
        if form == 'q':
            return [self.q] * 4
        return [self.vcomp(name)] * 4 if form else self.vf[self.vnum(name)]

    def vop(self, op, a, pc):
        m = re.fullmatch(r'v(msub|madd|mul|add|sub)(a)?(x|y|z|w|q)?\.([xyzw]+)', op)
        if m is not None:
            kind, _, form, fields = m.groups()
            fs, ft = self.vf[self.vnum(a[1])], self.vbroad(a[2], form)
            acc = self.vacc
            do = {'add': lambda i: fs[i] + ft[i], 'sub': lambda i: fs[i] - ft[i], 'mul': lambda i: fs[i] * ft[i],
                  'madd': lambda i: acc[i] + round32(fs[i] * ft[i]),
                  'msub': lambda i: acc[i] - round32(fs[i] * ft[i])}[kind]
            self.vset(a[0], fields, [round32(do(i)) if 'xyzw'[i] in fields else 0.0 for i in range(4)])
            return
        m = re.fullmatch(r'v(move|mr32|max|ftoi0|ftoi4|itof0|opmula|opmsub)\.([xyzw]+)', op)
        if m is None:
            raise RuntimeError('unsupported VU op %s at 0x%x' % (op, pc))
        kind, fields = m.groups()
        fs = self.vf[self.vnum(a[1])]
        if kind == 'move':
            out = list(fs)
        elif kind == 'mr32':
            out = [fs[1], fs[2], fs[3], fs[0]]
        elif kind == 'max':
            out = [max(x, y) for x, y in zip(fs, self.vf[self.vnum(a[2])])]
        elif kind in ('ftoi0', 'ftoi4'):
            # An integer in a vf is kept as the float with its bits.
            scale = 16.0 if kind == 'ftoi4' else 1.0
            out = [f32(max(-0x80000000, min(0x7fffffff, int(x * scale))) & M32) for x in fs]
        elif kind == 'itof0':
            out = [round32(float(s32(b32(x)))) for x in fs]
        else:
            ft = self.vf[self.vnum(a[2])]
            cross = [round32(fs[1] * ft[2]), round32(fs[2] * ft[0]), round32(fs[0] * ft[1]), 0.0]
            out = cross if kind == 'opmula' else [round32(self.vacc[i] - cross[i]) for i in range(4)]
        self.vset(a[0], fields, out)
