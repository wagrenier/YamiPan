#!/usr/bin/env python3
"""game/rand48.cpp against the ROM (offline).

python rand48cmp.py [ops] [--plant]

One random script of seed and next on the ROM's own code (0x2681e0.., eerun.py) and on the host's
file in a small exe (work/rand48cmp/), starting from the constructors, the state compared as bits
after every op. The ranges are 100, what every caller in the ROM passes, then small ones, ones high
enough that the throwing-away rule decides (the rule is the point: n + 1, not Java's n - 1), negative
ones, and 0, the EE's untrapped divide. n of 0x7fffffff and -0x7fffffff are left out: they do not return, on either side.
--plant compiles the host with Java's n - 1, to prove the tool sees it.
"""
import os
import random
import subprocess
import sys
from pathlib import Path

import eerun
import yamipan

HERE = Path(__file__).parent
WORK = HERE / 'work' / 'rand48cmp'
SRC = yamipan.REPO / 'src'

ROM_NEXT = 0x2681e0
ROM_SEED = 0x268290
ROM_CTOR = 0x2682e0

HOST = r'''
#include <stdio.h>
#include "game/rand48.h"
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    Rand48 rng;
    printf("0 %llx\n", (unsigned long long)rng.m_state);
    char op;
    long long value;
    while (fscanf(f, " %c %lld", &op, &value) == 2) {
        int out = 0;
        if (op == 's')
            rng.seed((u_int)value);
        else
            out = rng.next((int)value);
        printf("%d %llx\n", out, (unsigned long long)rng.m_state);
    }
    return 0;
}
'''

PLANT = ('(u_int)n + 1', '(u_int)n - 1')

RANGES = (100, 100, 100, 100, 1, 2, 3, 7, 10, 256, 1000, 65536, 0x10000000, 0x30000000, 0x40000000,
          0x40000001, 0x55555555, 0x60000000, 0x7ffffff0, -1, -2, -100, -0x40000000, 0)


def build(plant):
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    code = (SRC / 'game' / 'rand48.cpp').read_text(encoding='utf-8')
    if plant:
        if PLANT[0] not in code:
            raise SystemExit('plant text not found')
        code = code.replace(*PLANT)
    (WORK / 'rand48.cpp').write_text(code, encoding='utf-8')
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC),
                    str(WORK / 'host.cpp'), str(WORK / 'rand48.cpp'), '-o', str(exe)], check=True)
    return exe


def main(argv):
    plant = '--plant' in argv
    args = [a for a in argv[1:] if not a.startswith('--')]
    count = int(args[0]) if args else 20000
    exe = build(plant)
    rng = random.Random(ROM_NEXT)
    ops = []
    for _ in range(count):
        if rng.random() < 0.02:
            ops.append(('s', rng.choice((0, 1, 0xffffffff, 0xdeece66d, rng.getrandbits(32)))))
        else:
            # Java's rule never accepts a draw at n = 0 or -0x40000000.
            ops.append(('n', rng.choice(RANGES[:-2] if plant else RANGES)))
    (WORK / 'ops.txt').write_text(''.join(f'{o} {v}\n' for o, v in ops))
    out = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                         timeout=60).stdout.splitlines()
    if len(out) != count + 1:
        print('host stopped early', len(out), count + 1)
        return 1
    ee = eerun.Ee()
    obj = ee.alloc(16)
    ee.call(ROM_CTOR, obj)
    bad = rejected = 0
    state = ee.rd(obj, 8)
    host = out[0].split()
    if state != int(host[1], 16):
        bad += 1
        print(f'ctor: rom {state:x} host {host[1]}')
    for i, (op, value) in enumerate(ops):
        if op == 's':
            result = 0
            ee.call(ROM_SEED, obj, value)
        else:
            before = ee.rd(obj, 8)
            result = eerun.s32(ee.call(ROM_NEXT, obj, value & 0xffffffff))
            rejected += ((before * 0x5deece66d) & 0xffffffffffff) != ee.rd(obj, 8)
        state = ee.rd(obj, 8)
        host = out[i + 1].split()
        if (result, state) != (int(host[0]), int(host[1], 16)):
            bad += 1
            if bad <= 5:
                print(f'op {i} {op} {value}: rom {result} {state:x} host {host[0]} {host[1]}')
    print(f'{count} ops, {rejected} draws where the ROM threw one away, {bad} differ')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
