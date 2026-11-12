#!/usr/bin/env python3
"""game/mslsort.h against the ROM (offline).

python sortcmp.py [lists] [--instance effects|picknamed|spawnpoints] [--plant]

Random lists on one of the ROM's own sort instances (eerun.py) and on the host's template in a small
exe (work/sortcmp/), one after the other in the same run, so the instance's pivot counter carries
from list to list on both sides. The instances:
  effects      0x192150, counter 0x31a8f0: {id, int key} (8 bytes), stageparticle.cpp
  picknamed    0x2a0040, counter 0x31ac50: {id, float key} (8 bytes), scriptPickNamed
  spawnpoints  0x29fdb0, counter 0x31ac54: {id, float key, byte} (12 bytes), missionPickSpawnPoint;
               the byte (id & 0xff) is compared too, so it must travel with its record
The keys are drawn from narrow and wide ranges, all equal, sorted and reversed, because ties are what
this code decides (float keys are the same integers, exact). Every list's ids are compared in order.
--plant compiles the host with the counter moving one step per pass instead of two, to prove the
tool sees it.
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
WORK = HERE / 'work' / 'sortcmp'
SRC = yamipan.REPO / 'src'

INSTANCES = {
    # name: (sort, counter, stride, key type, has byte)
    'effects': (0x192150, 0x31a8f0, 8, 'int', False),
    'picknamed': (0x2a0040, 0x31ac50, 8, 'float', False),
    'spawnpoints': (0x29fdb0, 0x31ac54, 12, 'float', True),
}

HOST = r'''
#include <stdio.h>
#include <vector>
#include "mslsort.h"
struct Entry { int id; KEY key; unsigned char tag; };
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    const auto less = [](const Entry &a, const Entry &b) { return a.key < b.key; };
    int n;
    while (fscanf(f, "%d", &n) == 1) {
        std::vector<Entry> list((size_t)n);
        for (int i = 0; i < n; i++) {
            int key;
            list[(size_t)i].id = i;
            list[(size_t)i].tag = (unsigned char)i;
            fscanf(f, "%d", &key);
            list[(size_t)i].key = (KEY)key;
        }
        mslSort(list.data(), list.data() + n, less);
        for (const Entry &e : list)
            printf("%d:%d ", e.id, e.tag);
        printf("\n");
    }
    return 0;
}
'''

PLANT = ('shift = shift + 1 > kShiftHigh ? kShiftLow : shift + 1;\n        const int farSample',
         'const int farSample')


def build(plant, key):
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(f'#define KEY {key}' + chr(10) + HOST)
    header = (SRC / 'game' / 'mslsort.h').read_text(encoding='utf-8')
    if plant:
        if PLANT[0] not in header:
            raise SystemExit('plant text not found')
        header = header.replace(*PLANT)
    (WORK / 'mslsort.h').write_text(header, encoding='utf-8')
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(WORK),
                    str(WORK / 'host.cpp'), '-o', str(exe)], check=True)
    return exe


def keys_for(rng):
    n = rng.choice((0, 1, 2, 3, 5, 12, 20, 21, 22, 30, 64, 200, rng.randrange(0, 400)))
    kind = rng.randrange(6)
    if kind == 0:
        return [7] * n
    if kind == 1:
        return [rng.randrange(0, 4) for _ in range(n)]
    if kind == 2:
        return [rng.randrange(-30, 30) for _ in range(n)]
    if kind == 3:
        return sorted(rng.randrange(-2000, 2000) for _ in range(n))
    if kind == 4:
        return sorted((rng.randrange(0, 40) for _ in range(n)), reverse=True)
    return [0 if rng.random() < 0.4 else rng.randrange(0, 5000) for _ in range(n)]


def rom_sort(ee, inst, keys):
    sort, _, stride, key, has_byte = inst
    base = ee.alloc(stride * len(keys) + stride)
    rows = []
    for i, k in enumerate(keys):
        row = struct.pack('<i', i) + (struct.pack('<f', float(k)) if key == 'float' else struct.pack('<i', k))
        if stride == 12:
            row += struct.pack('<I', i & 0xff if has_byte else 0)
        rows.append(row)
    ee.write(base, b''.join(rows))
    ee.call(sort, base, base + stride * len(keys), 0)
    out = []
    for i in range(len(keys)):
        at = base + stride * i
        ident = eerun.s32(ee.u32(at))
        tag = ee.u32(at + 8) & 0xff if has_byte else ident & 0xff
        out.append((ident, tag))
    return out


def main(argv):
    plant = '--plant' in argv
    name = 'effects'
    if '--instance' in argv:
        name = argv[argv.index('--instance') + 1]
        argv = argv[:argv.index('--instance')] + argv[argv.index('--instance') + 2:]
    if name not in INSTANCES:
        raise SystemExit(f'--instance: one of {", ".join(INSTANCES)}')
    inst = INSTANCES[name]
    rom_sort_at, rom_shift = inst[0], inst[1]
    args = [a for a in argv[1:] if not a.startswith('--')]
    lists = int(args[0]) if args else 400
    exe = build(plant, inst[3])
    rng = random.Random(rom_sort_at)
    ee = eerun.Ee()
    all_keys = [keys_for(rng) for _ in range(lists)]
    (WORK / 'lists.txt').write_text(''.join(f'{len(k)} ' + ' '.join(map(str, k)) + '\n' for k in all_keys))
    out = subprocess.run([str(exe), str(WORK / 'lists.txt')], capture_output=True, text=True,
                         check=True).stdout.splitlines()
    if len(out) != lists:
        print('host stopped early', len(out), lists)
        return 1
    bad = elements = long_lists = 0
    for i, (keys, line) in enumerate(zip(all_keys, out)):
        rom = rom_sort(ee, inst, keys)
        host = [tuple(int(y) for y in x.split(':')) for x in line.split()]
        elements += len(keys)
        long_lists += len(keys) >= 21
        if [keys[x] for x, _ in rom] != sorted(keys):
            print(f'list {i}: the ROM left it unsorted')
        if rom != host:
            bad += 1
            if bad <= 3:
                first = next(j for j, (a, b) in enumerate(zip(rom, host)) if a != b)
                print(f'list {i} ({len(keys)} elements) differs at {first}: rom {rom[first:first + 6]} '
                      f'host {host[first:first + 6]}')
    print(f'{name}: {lists} lists, {long_lists} through the quicksort, {elements} elements, {bad} lists differ; '
          f'counter ends at {eerun.s32(ee.u32(rom_shift))}')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
