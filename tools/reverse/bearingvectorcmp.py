#!/usr/bin/env python3
"""The MSL vectors of the jack's bearings and the ripple sheet's lists against the host (offline).

python bearingvectorcmp.py [ops] [--plant]

game/stageparty.cpp keeps std::vector where the ROM has its own instances, so what is compared is the
promise: one random script of reserve, insert and clear on the ROM's BearingVector (0x206890,
0x206d60, 0x206f70, eerun.py) and on std::vector in a small exe (work/bearingvectorcmp/), count and
every element as bits after each op. insert goes anywhere with any n, and half of the time its value
points into the vector itself, the case the ROM's code has a branch for. Capacity is not compared:
the two grow differently and no caller can tell. The dtors, 0x206f80 and RippleLists' 0x206f00 (with
0x206ff0, 0x207090, 0x207100), are run on built vectors for what they free, with both delete flags.
--plant makes the host ignore the position, to prove the tool sees it.
"""
import os
import random
import subprocess
import sys
from pathlib import Path

import eerun
import yamipan

HERE = Path(__file__).parent
WORK = HERE / 'work' / 'bearingvectorcmp'

ROM_INSERT, ROM_RESERVE, ROM_CLEAR, ROM_DTOR = 0x206890, 0x206d60, 0x206f70, 0x206f80
ROM_LISTS_DTOR = 0x206f00
MAX_COUNT = 48

HOST = r'''
#include <stdio.h>
#include <vector>
struct Bearing { unsigned angle, distance, object; };
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    std::vector<Bearing> v;
    char op;
    while (fscanf(f, " %c", &op) == 1) {
        unsigned a, b, c, d, e;
        if (op == 'r') {
            fscanf(f, "%u", &a);
            v.reserve(a);
        } else if (op == 'c') {
            v.clear();
        } else if (op == 'i') {
            fscanf(f, "%u %u %x %x %x", &a, &b, &c, &d, &e);
            v.insert(v.begin() + POSITION, b, Bearing{c, d, e});
        } else {
            fscanf(f, "%u %u %u", &a, &b, &c);
            v.insert(v.begin() + POSITION, b, v[c]);
        }
        printf("%u", (unsigned)v.size());
        for (const Bearing &x : v)
            printf(" %x %x %x", x.angle, x.distance, x.object);
        printf("\n");
    }
    return 0;
}
'''


def build(plant):
    WORK.mkdir(parents=True, exist_ok=True)
    code = HOST.replace('POSITION', 'v.size()' if plant else 'a')
    (WORK / 'host.cpp').write_text(code)
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w',
                    str(WORK / 'host.cpp'), '-o', str(exe)], check=True)
    return exe


def script(rng, count):
    ops, size = [], 0
    for _ in range(count):
        roll = rng.random()
        if size > MAX_COUNT or roll < 0.03:
            ops.append(('c',))
            size = 0
        elif roll < 0.10:
            ops.append(('r', rng.choice((0, 1, 4, size, size + 1, size * 2, MAX_COUNT * 3))))
        else:
            pos = rng.choice((size, size, 0, rng.randint(0, size)))
            n = rng.choice((0, 1, 1, 1, 2, 3, 5, 9, 20))
            if size and rng.random() < 0.5:
                ops.append(('a', pos, n, rng.randrange(size)))
            else:
                ops.append(('i', pos, n) + tuple(rng.getrandbits(32) for _ in range(3)))
            size += n
    return ops


def row(ee, v):
    count = ee.u32(v + 4)
    data = ee.u32(v + 8)
    return ' '.join([str(count)] + [f'{ee.u32(data + i * 4):x}' for i in range(count * 3)])


def dtors(ee, freed):
    """What the two dtors free, against what each vector owns. Returns the mistakes."""
    bad = 0
    for flag in (0, 1):
        v = ee.alloc(12)
        data = ee.alloc(36)
        ee.wr(v, 4, 3), ee.wr(v + 4, 4, 2), ee.wr(v + 8, 4, data)
        freed.clear()
        ee.call(ROM_DTOR, v, flag)
        bad += freed != ([data, v] if flag else [data]) or ee.u32(v + 4) != 0
        lists = ee.alloc(12)
        inner = ee.alloc(36)
        blocks = [ee.alloc(16) for _ in range(3)]
        for i, block in enumerate(blocks):
            ee.wr(inner + i * 12, 4, 1), ee.wr(inner + i * 12 + 4, 4, 1), ee.wr(inner + i * 12 + 8, 4, block)
        ee.wr(lists, 4, 3), ee.wr(lists + 4, 4, 3), ee.wr(lists + 8, 4, inner)
        freed.clear()
        ee.call(ROM_LISTS_DTOR, lists, flag)
        want = blocks[::-1] + [inner] + ([lists] if flag else [])
        bad += freed != want or ee.u32(lists + 4) != 0
    # A vector that never held anything frees nothing.
    v = ee.alloc(12)
    freed.clear()
    ee.call(ROM_DTOR, v, 0)
    bad += freed != []
    return bad


def main(argv):
    plant = '--plant' in argv
    args = [a for a in argv[1:] if not a.startswith('--')]
    count = int(args[0]) if args else 6000
    exe = build(plant)
    ops = script(random.Random(ROM_INSERT), count)
    (WORK / 'ops.txt').write_text(''.join(
        ' '.join(f'{x:x}' if op[0] == 'i' and i > 2 else str(x) for i, x in enumerate(op)) + '\n'
        for op in ops))
    out = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                         timeout=60).stdout.splitlines()
    if len(out) != count:
        print('host stopped early', len(out), count)
        return 1
    ee = eerun.Ee()
    freed = []

    def delete(e):
        freed.append(e.g('a0'))
        return 0

    ee.hook('operator_delete', delete)
    v = ee.alloc(12)
    value = ee.alloc(12)
    ee.call(ROM_CLEAR, v)
    bad = aliased = in_place = elements = 0
    for i, op in enumerate(ops):
        data, size = ee.u32(v + 8), ee.u32(v + 4)
        if op[0] == 'r':
            ee.call(ROM_RESERVE, v, op[1])
        elif op[0] == 'c':
            ee.call(ROM_CLEAR, v)
        elif op[0] == 'i':
            for w in range(3):
                ee.wr(value + w * 4, 4, op[3 + w])
            ee.call(ROM_INSERT, v, data + op[1] * 12, op[2], value)
        else:
            aliased += 1
            in_place += op[2] != 0 and size + op[2] <= ee.u32(v)
            ee.call(ROM_INSERT, v, data + op[1] * 12, op[2], data + op[3] * 12)
        rom = row(ee, v)
        elements += ee.u32(v + 4)
        if rom != out[i]:
            bad += 1
            if bad <= 3:
                print(f'op {i} {op}:\n rom  {rom[:200]}\n host {out[i][:200]}')
    wrong = dtors(ee, freed)
    print(f'{count} ops, {elements} elements compared, {aliased} inserts of a value inside the vector '
          f'({in_place} in place), {bad} differ; dtors {wrong} wrong')
    return 1 if bad or wrong else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
