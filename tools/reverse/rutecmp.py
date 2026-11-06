#!/usr/bin/env python3
"""game/routeplayer.cpp against the ROM, on rute files from the disc (offline).

python rutecmp.py <dir of rute files> [queries per file] [--files N]

For each file: builds the ROM's record on eerun.py (0x257fa0) and the host's RouteFile in a small exe
compiled from src/game/routeplayer.cpp (work/rutecmp/), then asks both the same questions and compares:
maxStepCount, the listed areas, the names, every (area, area, crossing) of the file by crossingPath,
areaCrossings, and random points near nodes by areaPath and wayToCrossing. Prints the first differences
of each kind and a count. Lengths compare within 0.01 (the host does not round like the EE). The files
come from any scan for the magic "ord\\0" (raw and inside SLDI packs: sssall.sldi); no file is stored.
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
WORK = HERE / 'work' / 'rutecmp'
SRC = yamipan.REPO / 'src'
TOL = 0.01
COVERED = {}    # (question, kinds of leg in the ROM's answer): how many, so a clean run can be judged

HOST = r'''
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "game/routeplayer.h"
float planarDistance(const float *a, const float *b)
{ return sqrtf((b[0]-a[0])*(b[0]-a[0]) + (b[1]-a[1])*(b[1]-a[1])); }
int rxSysPrintf(const char *, ...) { return 0; }
static void show(const RoutePath &p, float r)
{
    printf("%.5f %.5f %d", r, p.length, (int)p.steps.size());
    for (const RouteLeg &l : p.steps) printf(" %d %d", l.kind, l.index);
    printf("\n");
}
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    void *bytes = malloc(n); fread(bytes, 1, n, f); fclose(f);
    RouteFile core(nullptr, bytes);
    printf("%d\n", core.maxStepCount());
    printf("%d", (int)core.m_areas.size()); for (short a : core.m_areas) printf(" %d", a); printf("\n");
    printf("%d", (int)core.m_names.size());
    for (const char *s : core.m_names) printf(" %d", (int)(s - (const char *)bytes)); printf("\n");
    FILE *q = fopen(argv[2], "r"); char kind[8];
    while (fscanf(q, "%7s", kind) == 1) {
        RoutePath out;
        if (kind[0] == 'c') { int a, b, c, s; fscanf(q, "%d %d %d %d", &a, &b, &c, &s);
            float r = core.crossingPath(out, a, b, c, s); show(out, r); }
        else if (kind[0] == 'x') { int a, b; fscanf(q, "%d %d", &a, &b); std::vector<short> v;
            core.areaCrossings(v, a, b); printf("%d", (int)v.size()); for (short s : v) printf(" %d", s); printf("\n"); }
        else if (kind[0] == 'a') { float p[3], t[3]; int a;
            fscanf(q, "%f %f %f %f %f %f %d", p, p+1, p+2, t, t+1, t+2, &a);
            float r = core.m_search->areaPath(out, p, t, a); show(out, r); }
        else if (kind[0] == 'w') { float p[3]; int a, c; fscanf(q, "%f %f %f %d %d", p, p+1, p+2, &a, &c);
            float r = core.wayToCrossing(out, p, a, c); show(out, r); }
    }
    return 0;
}
'''


def build():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    cmd = [str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC), str(WORK / 'host.cpp'),
           str(SRC / 'game' / 'routeplayer.cpp'), '-o', str(exe)]
    subprocess.run(cmd, check=True)
    return exe


class Rom:
    def __init__(self, data):
        self.ee = eerun.Ee()
        self.file = self.ee.alloc(len(data))
        self.ee.write(self.file, data)
        self.core = self.ee.alloc(0x24)
        self.ee.call(0x257fa0, self.core, 0, self.file)
        self.search = self.ee.u32(self.core)
        self.path = self.ee.alloc(0x10)
        self.mark = self.ee.heap

    def vec(self, at):      # MSL vector: cap, size, data
        ee = self.ee
        return ee.u32(at + 4), ee.u32(at + 8)

    def steps(self):
        count, data = self.vec(self.path + 4)
        return [(self.ee.s16(data + i * 4), self.ee.s16(data + i * 4 + 2)) for i in range(count)]

    def clear(self):
        ee = self.ee
        for i in range(4):
            ee.wr(self.path + i * 4, 4, 0)
        ee.heap = self.mark

    def show(self, r):
        return r, self.ee.float(self.path), self.steps()

    def floats(self, values):
        at = self.ee.alloc(0x10)
        for i, v in enumerate(values):
            self.ee.setfloat(at + i * 4, v)
        return at

    def crossing(self, a, b, c, s):
        self.clear()
        return self.show(self.ee.callf(0x2578f0, self.core, self.path, a, b, c, s))

    def crossings(self, a, b):
        self.clear()
        out = self.ee.alloc(0xc)
        self.ee.call(0x257db0, self.core, out, a, b)
        count, data = self.vec(out)
        return [self.ee.s16(data + i * 2) for i in range(count)]

    def area(self, p, t, a):
        self.clear()
        return self.show(self.ee.callf(0x253420, self.search, self.path, self.floats(p), self.floats(t), a))

    def way(self, p, a, c):
        self.clear()
        return self.show(self.ee.callf(0x257680, self.core, self.path, self.floats(p), a, c))


def nodes_of(data):
    nodes = struct.unpack_from('<I', data, 4)[0]
    count = struct.unpack_from('<I', data, nodes)[0]
    out = []
    for i in range(count):
        rec = nodes + 8 + i * 0x20
        out.append((struct.unpack_from('<h', data, rec)[0], struct.unpack_from('<3f', data, rec + 0x14)))
    return out


def triples(data):
    """Every (area, area, crossing) of the header's table +0x1c."""
    w = lambda o: struct.unpack_from('<I', data, o)[0]
    h = lambda o: struct.unpack_from('<h', data, o)[0]
    table = w(0x1c)
    for i in range(w(table)):
        rec = table + 4 + i * 8
        rows = w(rec + 4)
        for j in range(w(rows)):
            row = rows + 4 + j * 8
            ways = w(row + 4)
            for k in range(w(ways)):
                yield h(rec), h(row), h(ways + 4 + k * 4)


def same(rom, host):
    r, length, steps = rom
    fields = host.split()
    hr, hl, n = float(fields[0]), float(fields[1]), int(fields[2])
    legs = [(int(fields[3 + i * 2]), int(fields[4 + i * 2])) for i in range(n)]
    return abs(r - hr) <= TOL * max(1, abs(r)) and abs(length - hl) <= TOL * max(1, abs(length)) and legs == steps


def compare(path, exe, queries, rng):
    data = Path(path).read_bytes()
    rom = Rom(data)
    ee = rom.ee
    nodes = nodes_of(data)
    w = lambda o: struct.unpack_from('<I', data, o)[0]
    wanted = sorted({struct.unpack_from('<h', data, w(0x24) + 4 + i * 8)[0] for i in range(w(w(0x24)))})
    all3 = list(triples(data))
    picked = rng.sample(all3, min(len(all3), queries))
    sets = struct.unpack_from('<I', data, 0x2c)[0]
    nsets = struct.unpack_from('<I', data, sets)[0] if sets else 0

    lines = []
    asks = []
    for a, b, c in picked:
        s = rng.randint(0, nsets)
        lines.append(f'c {a} {b} {c} {s}')
        asks.append(('crossingPath', (a, b, c, s), rom.crossing))
        lines.append(f'x {a} {b}')
        asks.append(('areaCrossings', (a, b), rom.crossings))
    near = lambda p: [v + rng.uniform(-400, 400) for v in p]
    for _ in range(queries):
        a = rng.choice(wanted)
        inside = [p for n, p in nodes if n == a]
        p = [float(v) for v in near(rng.choice(inside))]
        t = [float(v) for v in near(rng.choice(inside))]
        lines.append('a ' + ' '.join(f'{v:.6f}' for v in p + t) + f' {a}')
        asks.append(('areaPath', (p, t, a), rom.area))
    for a, b, c in picked:
        inside = [p for n, q in [(x, y) for x, y in nodes] for p in [q] if n == a] or [nodes[0][1]]
        p = [float(v) for v in near(rng.choice(inside))]
        lines.append('w ' + ' '.join(f'{v:.6f}' for v in p) + f' {a} {c}')
        asks.append(('wayToCrossing', (p, a, c), rom.way))

    (WORK / 'q.txt').write_text('\n'.join(lines) + '\n')
    out = subprocess.run([str(exe), path, str(WORK / 'q.txt')], capture_output=True, text=True, check=True).stdout
    rows = out.splitlines()

    counts = {}
    shown = {}

    def note(kind, ok, text):
        c = counts.setdefault(kind, [0, 0])
        c[0] += 1
        if not ok:
            c[1] += 1
            if shown.get(kind, 0) < 3:
                shown[kind] = shown.get(kind, 0) + 1
                print(f'  DIFF {kind}: {text}')

    note('maxStepCount', int(rows[0]) == ee.call(0x258110, rom.file), rows[0])
    areas = [ee.s16(ee.u32(rom.core + 0x14) + i * 2) for i in range(ee.u32(rom.core + 0x10))]
    note('areas', [int(v) for v in rows[1].split()[1:]] == areas, rows[1][:60])
    names = [ee.u32(ee.u32(rom.core + 0x20) + i * 4) - rom.file for i in range(ee.u32(rom.core + 0x1c))]
    note('names', [int(v) for v in rows[2].split()[1:]] == names, rows[2][:60])

    for (kind, args, fn), row in zip(asks, rows[3:]):
        got = fn(*args)
        if kind == 'areaCrossings':
            note(kind, [int(v) for v in row.split()[1:]] == got, f'{args} rom {got[:12]} host {row[:60]}')
        else:
            note(kind, same(got, row), f'{args}\n    rom  {got[0]:.3f} {got[1]:.3f} {got[2][:14]}\n    host {row[:110]}')
            shape = 'no way' if got[1] < 0 or got[0] < 0 else 'no steps' if not got[2] else 'legs ' + ''.join(sorted({str(k) for k, _ in got[2]}))
            COVERED[(kind, shape)] = COVERED.get((kind, shape), 0) + 1
    print(' ', Path(path).name[-28:], {k: f'{v[0] - v[1]}/{v[0]}' for k, v in counts.items()}, f'{ee.count} EE instructions', flush=True)
    return sum(v[1] for v in counts.values())


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    limit = None
    if '--files' in argv:
        at = argv.index('--files')
        limit = int(argv[at + 1])
        del argv[at:at + 2]
    queries = int(argv[2]) if len(argv) > 2 else 40
    exe = build()
    rng = random.Random(1)
    seen = {}
    for path in sorted(Path(argv[1]).iterdir()):
        head = path.read_bytes()[:0x3c]     # the same stage on several ROMs
        seen.setdefault(head, path)
    files = list(seen.values())[:limit]
    bad = 0
    for path in files:
        bad += compare(str(path), exe, queries, rng)
    for key in sorted(COVERED):
        print(' ', key[0], key[1], COVERED[key])
    print(f'{len(files)} files, {bad} differences')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
