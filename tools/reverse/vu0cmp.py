#!/usr/bin/env python3
"""game/vu0math.cpp against the ROM (offline).

python vu0cmp.py [rounds per function]

Every function of 0x135990..0x1366c0 on the same random vectors, matrices and angles: the ROM's own
code on eerun.py, the host's in a small exe compiled from src/game/vu0math.cpp (work/vu0cmp/). The
inputs include a zero vector, equal points and out on top of an input, and angles past -pi..pi. Each
result lane is compared by its bits: the table gives the worst distance in units in the last place.
vecCross's w is not compared: the ROM's is whatever vf6 held.
"""
import os
import random
import struct
import subprocess
import sys
from pathlib import Path

import eerun
import yamipan

HERE = Path(__file__).parent
WORK = HERE / 'work' / 'vu0cmp'
SRC = yamipan.REPO / 'src'
V0, V1, M0, M1, OUT = 0x1000000, 0x1000040, 0x1000080, 0x1000100, 0x1000180
ADDR = {'V0': V0, 'V1': V1, 'M0': M0, 'M1': M1, 'OUT': OUT}
SENTINEL = 0x7f7f1234
PI = 3.14159274

# name, ROM address, a0.. of the ROM, the host's statement, what OUT starts as, the inputs, lanes kept.
# Inputs: 'v' vectors, 'a' angles of -pi..pi in V0, 'A' angles past it, 'z' a zero vector, 'e' V1 = V0.
CASES = [
    ('rotateByAngles', 0x135990, ['OUT', 'M0', 'V0'], 'rotateByAngles(O, M0, V0);', None, 'a'),
    ('rotateByAngles in place', 0x135990, ['OUT', 'OUT', 'V0'], 'rotateByAngles(O, O, V0);', 'M0', 'a'),
    ('rotationMatrixZxy', 0x1359f0, ['OUT', 'V0'], 'rotationMatrixZxy(O, V0);', None, 'a'),
    ('rotationMatrixXyzRegs', 0x135a90, ['V0'], 'rotationMatrixXyzRegs(O, V0);', None, 'a'),
    ('rotationMatrixXyz', 0x135b90, ['OUT', 'V0'], 'rotationMatrixXyz(O, V0);', None, 'a'),
    ('rotationMatrixXyz past pi', 0x135b90, ['OUT', 'V0'], 'rotationMatrixXyz(O, V0);', None, 'A'),
    ('sinCosVec', 0x135c30, ['V0'], 'sinCosVec(O[0], O[1], V0);', None, 'A'),
    ('sinCosPair', 0x135d50, ['OUT'], 'sinCosPair(s, O[0]);', None, 'a'),
    ('wrapAngleVec', 0x135e20, ['OUT', 'V0'], 'wrapAngleVec(O[0], V0);', None, 'A'),
    ('wrapAngleVec in range', 0x135e20, ['OUT', 'V0'], 'wrapAngleVec(O[0], V0);', None, 'a'),
    ('wrapAngleLane', 0x135ec0, [], 'r = wrapAngleLane(s);', None, 'A'),
    ('vecDistance', 0x135f20, ['V0', 'V1'], 'r = vecDistance(V0, V1);', None, 'v'),
    ('vecDistanceSq', 0x135f70, ['V0', 'V1'], 'r = vecDistanceSq(V0, V1);', None, 'v'),
    ('addScaledVec4', 0x135fa0, ['OUT', 'V0', 'V1'], 'addScaledVec4(s, O[0], V0, V1);', None, 'v'),
    ('addScaledVec3', 0x135fc0, ['OUT', 'V0', 'V1'], 'addScaledVec3(s, O[0], V0, V1);', None, 'v'),
    ('setVecLength', 0x135fe0, ['OUT', 'V0'], 'setVecLength(s, O[0], V0);', None, 'v'),
    ('setVecLength zero', 0x135fe0, ['OUT', 'V0'], 'setVecLength(s, O[0], V0);', None, 'z'),
    ('vecDirection', 0x136020, ['OUT', 'V0', 'V1'], 'vecDirection(O[0], V0, V1);', None, 'v'),
    ('vecDirection same', 0x136020, ['OUT', 'V0', 'V1'], 'vecDirection(O[0], V0, V1);', None, 'e'),
    ('scaleVec3', 0x136070, ['OUT', 'V0'], 'scaleVec3(s, O[0], V0);', None, 'v'),
    ('rotTransPers 0', 0x136090, ['OUT', 'M0', 'V0', 0],
     'rotTransPers((int *)O[0], M0, V0, 0);', None, 'v'),
    ('rotTransPers 1', 0x136090, ['OUT', 'M0', 'V0', 1],
     'rotTransPers((int *)O[0], M0, V0, 1);', None, 'v'),
    ('rotateMatrixY', 0x1360e0, ['OUT', 'M0'], 'rotateMatrixY(s, O, M0);', None, 'a'),
    ('rotateMatrixY past pi', 0x1360e0, ['OUT', 'M0'], 'rotateMatrixY(s, O, M0);', None, 'A'),
    ('rotateMatrixX', 0x1361c0, ['OUT', 'M0'], 'rotateMatrixX(s, O, M0);', None, 'a'),
    ('rotateMatrixX past pi', 0x1361c0, ['OUT', 'M0'], 'rotateMatrixX(s, O, M0);', None, 'A'),
    ('rotateMatrixZ', 0x1362a0, ['OUT', 'M0'], 'rotateMatrixZ(s, O, M0);', None, 'a'),
    ('rotateMatrixZ in place', 0x1362a0, ['OUT', 'OUT'], 'rotateMatrixZ(s, O, O);', 'M0', 'a'),
    ('identityMatrix', 0x136400, ['OUT'], 'identityMatrix(O);', None, 'v'),
    ('copyMatrix', 0x136430, ['OUT', 'M0'], 'copyMatrix(O, M0);', None, 'v'),
    ('translateMatrix', 0x136460, ['OUT', 'M0', 'V0'], 'translateMatrix(O, M0, V0);', None, 'v'),
    ('translateMatrix in place', 0x136460, ['OUT', 'OUT', 'V0'], 'translateMatrix(O, O, V0);', 'M0', 'v'),
    ('scaleVec4', 0x136490, ['OUT', 'V0'], 'scaleVec4(s, O[0], V0);', None, 'v'),
    ('blendVec4', 0x1364b0, ['OUT', 'V0', 'V1'], 'blendVec4(s, O[0], V0, V1);', None, 'v'),
    ('divideVec4', 0x1364e0, ['OUT', 'V0'], 'divideVec4(s, O[0], V0);', None, 'v'),
    ('divideVec4 by zero', 0x1364e0, ['OUT', 'V0'], 'divideVec4(0.0f, O[0], V0);', None, 'z'),
    ('invertRigidMatrix', 0x136500, ['OUT', 'M0'], 'invertRigidMatrix(O, M0);', None, 'v'),
    ('invertRigidMatrix in place', 0x136500, ['OUT', 'OUT'], 'invertRigidMatrix(O, O);', 'M0', 'v'),
    ('vecNormalize', 0x136560, ['OUT', 'V0'], 'vecNormalize(O[0], V0);', None, 'v'),
    ('vecNormalize in place', 0x136560, ['OUT', 'OUT'], 'vecNormalize(O[0], O[0]);', 'V0', 'v'),
    ('vecNormalize zero', 0x136560, ['OUT', 'V0'], 'vecNormalize(O[0], V0);', None, 'z'),
    ('vecDot', 0x1365a0, ['V0', 'V1'], 'r = vecDot(V0, V1);', None, 'v'),
    ('vecCross', 0x1365d0, ['OUT', 'V0', 'V1'], 'vecCross(O[0], V0, V1);', None, 'v'),
    ('multiplyTurn', 0x1365f0, ['OUT', 'M0', 'M1'], 'multiplyTurn(O, M0, M1);', None, 'v'),
    ('multiplyMatrix', 0x136640, ['OUT', 'M0', 'M1'], 'multiplyMatrix(O, M0, M1);', None, 'v'),
    ('multiplyMatrix out a', 0x136640, ['OUT', 'OUT', 'M1'], 'multiplyMatrix(O, O, M1);', 'M0', 'v'),
    ('multiplyMatrix out b', 0x136640, ['OUT', 'M0', 'OUT'], 'multiplyMatrix(O, M0, O);', 'M1', 'v'),
    ('applyMatrix', 0x136690, ['OUT', 'M0', 'V0'], 'applyMatrix(O[0], M0, V0);', None, 'v'),
    ('applyMatrix in place', 0x136690, ['OUT', 'M0', 'OUT'], 'applyMatrix(O[0], M0, O[0]);', 'V0', 'v'),
]
START = {None: '', 'V0': 'memcpy(O, V0, 16);', 'V1': 'memcpy(O, V1, 16);',
         'M0': 'memcpy(O, M0, 64);', 'M1': 'memcpy(O, M1, 64);'}

HOST = r'''
#include <stdio.h>
#include <string.h>
#include "game/vu0math.h"
static sceVu0FVECTOR V0, V1;
static sceVu0FMATRIX M0, M1, O;
static float word() { unsigned b; if (scanf("%x", &b) != 1) return 0; float f; memcpy(&f, &b, 4); return f; }
static void put(float f) { unsigned b; memcpy(&b, &f, 4); printf("%x ", b); }
int main()
{
    int id;
    while (scanf("%d", &id) == 1) {
        float s = word();
        for (int i = 0; i < 4; i++) V0[i] = word();
        for (int i = 0; i < 4; i++) V1[i] = word();
        for (int i = 0; i < 16; i++) M0[i / 4][i % 4] = word();
        for (int i = 0; i < 16; i++) M1[i / 4][i % 4] = word();
        for (int i = 0; i < 16; i++) O[i / 4][i % 4] = word();
        float r = 0;
        switch (id) {
%s
        }
        put(r);
        for (int i = 0; i < 16; i++) put(O[i / 4][i % 4]);
        printf("\n");
    }
    return 0;
}
'''


def bits(value):
    return eerun.b32(value)


def inputs(rng, kind):
    """s, V0, V1, M0, M1 as bit words."""
    wide = lambda: rng.uniform(-1000.0, 1000.0)
    v0 = [wide() for _ in range(4)]
    v1 = [wide() for _ in range(4)]
    s = rng.uniform(-10.0, 10.0)
    if kind == 'a':
        v0 = [rng.uniform(-PI, PI) for _ in range(4)]
        s = rng.uniform(-PI, PI)
    elif kind == 'A':
        v0 = [rng.uniform(-40.0, 40.0) for _ in range(4)]
        s = rng.uniform(-40.0, 40.0)
    elif kind == 'z':
        v0 = [0.0] * 4
    elif kind == 'e':
        v1 = list(v0)
    m0 = [wide() for _ in range(16)]
    m1 = [wide() for _ in range(16)]
    return [bits(x) for x in [s] + v0 + v1 + m0 + m1]


def rom_one(ee, case, data):
    name, addr, args, _, start, _ = case
    s, v0, v1, m0, m1 = data[0], data[1:5], data[5:9], data[9:25], data[25:41]
    for at, words in ((V0, v0), (V1, v1), (M0, m0), (M1, m1)):
        ee.write(at, struct.pack('<%dI' % len(words), *words))
    begin = [SENTINEL] * 16
    if start:
        source = {'V0': v0, 'V1': v1, 'M0': m0, 'M1': m1}[start]
        begin[:len(source)] = source
    ee.write(OUT, struct.pack('<16I', *begin))
    # What a call left in the vf's upper lanes can be junk bits: sign-extended 64-bit moves through
    # qmtc2. Harmless on a VU, NaN here.
    ee.vf[1:] = [[0.0, 0.0, 0.0, 0.0] for _ in range(31)]
    ee.f[12] = 0 if name == 'divideVec4 by zero' else s
    regs = [ADDR[a] if isinstance(a, str) else a for a in args]
    if name == 'sinCosPair':
        regs = [OUT]
    ee.call(addr, *regs)
    out = list(struct.unpack('<16I', bytes(ee.m[OUT:OUT + 64])))
    r = ee.f[0] if name in FLOATS else 0
    if name == 'rotationMatrixXyzRegs':
        out = [bits(x) for row in ee.vf[4:8] for x in row]
    elif name == 'sinCosVec':
        out[:6] = [ee.f[1], ee.f[3], ee.f[5], SENTINEL, ee.f[2], ee.f[4]]
        out[6] = ee.f[6]
    return r, out


FLOATS = {'wrapAngleLane', 'vecDistance', 'vecDistanceSq', 'vecDot'}


def ulps(a, b):
    def line(x):
        return x & 0x7fffffff if not x >> 31 else -(x & 0x7fffffff)
    return abs(line(a) - line(b))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    WORK.mkdir(parents=True, exist_ok=True)
    cases = '\n'.join(f'        case {i}: {START[c[4]]} {c[3]} break;' for i, c in enumerate(CASES))
    source = WORK / 'host.cpp'
    source.write_text(HOST.replace('%s', cases))
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC), str(source),
                    str(SRC / 'game' / 'vu0math.cpp'), '-o', str(exe)], check=True)
    rng = random.Random(0x135990)
    jobs = []
    for i, case in enumerate(CASES):
        for _ in range(rounds):
            jobs.append((i, inputs(rng, case[5])))
    text = []
    for i, data in jobs:
        case = CASES[i]
        begin = [SENTINEL] * 16
        if case[4]:
            at = {'V0': 1, 'V1': 5, 'M0': 9, 'M1': 25}[case[4]]
            count = 4 if case[4][0] == 'V' else 16
            begin[:count] = data[at:at + count]
        text.append(' '.join([str(i)] + ['%x' % x for x in data + begin]))
    done = subprocess.run([str(exe)], input='\n'.join(text) + '\n', capture_output=True, text=True, check=True)
    lines = done.stdout.split('\n')
    ee = eerun.Ee()
    worst = {}
    differing = {}
    shown = 0
    for job, (i, data) in enumerate(jobs):
        case = CASES[i]
        r, out = rom_one(ee, case, data)
        host = [int(x, 16) for x in lines[job].split()]
        pairs = list(zip([r] + out, host))
        if case[0] == 'vecCross':
            pairs[4] = (0, 0)
        integers = case[0].startswith('rotTransPers')
        distance = max((abs(eerun.s32(a) - eerun.s32(b)) if integers else ulps(a, b)) for a, b in pairs)
        worst[i] = max(worst.get(i, 0), distance)
        if distance:
            differing[i] = differing.get(i, 0) + 1
            if distance > 2 and shown < 6:
                shown += 1
                print(f'{case[0]}: rom {[hex(a) for a, _ in pairs[:9]]}\n   host {[hex(b) for _, b in pairs[:9]]}')
    for i, case in enumerate(CASES):
        print(f'{case[0]:30s} 0x{case[1]:06x}  {rounds} rounds, {differing.get(i, 0):5d} differ, '
              f'worst {worst.get(i, 0)} ulp')
    total = sum(differing.values())
    print(f'{len(CASES)} cases, {len(jobs)} calls: {total} differ, worst {max(worst.values())} ulp')


if __name__ == "__main__":
    main()
