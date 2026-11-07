#!/usr/bin/env python3
"""game/mslsort.h against the ROM (offline).

python sortcmp.py [lists] [--plant]

Random lists on the ROM's own sort 0x192150 (eerun.py) and on the host's template in a small exe
(work/sortcmp/), one after the other in the same run, so the pivot's counter 0x31a8f0 carries from
list to list on both sides. Each element is {id, key} as the effects' pair; the keys are drawn from
narrow and wide ranges, all equal, sorted and reversed, because ties are what this code decides.
Every list's ids are compared in order. --plant compiles the host with the counter moving one step
per pass instead of two, to prove the tool sees it.
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

ROM_SORT = 0x192150
ROM_SHIFT = 0x31a8f0

HOST = r'''
#include <stdio.h>
#include <vector>
#include "mslsort.h"
struct Entry { int id; int key; };
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    const auto less = [](const Entry &a, const Entry &b) { return a.key < b.key; };
    int n;
    while (fscanf(f, "%d", &n) == 1) {
        std::vector<Entry> list((size_t)n);
        for (int i = 0; i < n; i++) {
            list[(size_t)i].id = i;
            fscanf(f, "%d", &list[(size_t)i].key);
        }
        mslSort(list.data(), list.data() + n, less);
        for (const Entry &e : list)
            printf("%d ", e.id);
        printf("\n");
    }
    return 0;
}
'''

PLANT = ('shift = shift + 1 > kShiftHigh ? kShiftLow : shift + 1;\n        const int farSample',
         'const int farSample')


def build(plant):
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
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


def rom_sort(ee, keys):
    base = ee.alloc(8 * len(keys) + 8)
    ee.write(base, b''.join(struct.pack('<ii', i, k) for i, k in enumerate(keys)))
    ee.call(ROM_SORT, base, base + 8 * len(keys), 0)
    return [eerun.s32(ee.u32(base + 8 * i)) for i in range(len(keys))]


def main(argv):
    plant = '--plant' in argv
    args = [a for a in argv[1:] if not a.startswith('--')]
    lists = int(args[0]) if args else 400
    exe = build(plant)
    rng = random.Random(ROM_SORT)
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
        rom = rom_sort(ee, keys)
        host = [int(x) for x in line.split()]
        elements += len(keys)
        long_lists += len(keys) >= 21
        if [keys[x] for x in rom] != sorted(keys):
            print(f'list {i}: the ROM left it unsorted')
        if rom != host:
            bad += 1
            if bad <= 3:
                first = next(j for j, (a, b) in enumerate(zip(rom, host)) if a != b)
                print(f'list {i} ({len(keys)} elements) differs at {first}: rom {rom[first:first + 6]} '
                      f'host {host[first:first + 6]}')
    print(f'{lists} lists, {long_lists} through the quicksort, {elements} elements, {bad} lists differ; '
          f'counter ends at {eerun.s32(ee.u32(ROM_SHIFT))}')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
