#!/usr/bin/env python3
"""findWayOut (game/characterroute.cpp) against the ROM's 0x227f30, on rute files from the disc (offline).

python wayoutcmp.py [files] [questions per file]

Finds rute files that have area lists (header +0x34) in the ROM.* packs, builds the ROM's records on
eerun.py as rutecmp.py does, gives 0x227f30 a scene and a character of its own (only +0x150 and the
ground's +0x50 are read), and asks it and the host the same questions: from a point near the nodes of a
listed area, which way out lies most against a threat. The host's function is cut out of the source
and compiled with routeplayer.cpp (work/rutecmp/wayout.cpp), so nothing here is a copy.

A last leg of kind chain makes the ROM read one past the node list (the comment on the function); the
host takes the last node, so such an answer would show as a difference. None did so far.
"""
import math
import mmap
import random
import re
import struct
import subprocess
import sys
import os
from pathlib import Path

import eerun
import rutecmp
import sssall
import yamipan

WORK = rutecmp.WORK
SRC = yamipan.REPO / 'src'
TOL = 0.01

HOST = r'''
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "game/routeplayer.h"
#include "game/mathutil.h"
#include "game/vu0math.h"
float planarDistance(const float *a, const float *b)
{ return sqrtf((b[0]-a[0])*(b[0]-a[0]) + (b[1]-a[1])*(b[1]-a[1])); }
int rxSysPrintf(const char *, ...) { return 0; }
struct Ground { int floorAttr; };
struct Who { struct { float position[4]; } m_xform; Ground *m_ground; };
static RoutePlayer *gRoutes;
%s
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    void *bytes = malloc(n); fread(bytes, 1, n, f); fclose(f);
    gRoutes = new RoutePlayer(bytes);
    FILE *q = fopen(argv[2], "r");
    int area; float p[3], t[3], l[4];
    while (fscanf(q, "%%d %%f %%f %%f %%f %%f %%f %%f %%f %%f %%f", &area, p, p+1, p+2, t, t+1, t+2, l, l+1, l+2, l+3) == 11) {
        Ground g = { area }; Who w; w.m_ground = &g;
        for (int i = 0; i < 3; i++) w.m_xform.position[i] = p[i];
        float out[4] = {};
        const bool r = findWayOut(out, &w, t, l);
        printf("%%d %%.5f %%.5f %%.5f\n", r, out[0], out[1], out[2]);
    }
    return 0;
}
'''


def host_source():
    text = (SRC / 'game' / 'characterroute.cpp').read_text(encoding='utf-8').replace('\r\n', '\n')
    found = re.search(r'bool findWayOut\(float \*out, StageCharacter \*who[^;{]*\{.*?return best <= kMathPi;\n}\n', text, re.S)
    body = found.group(0).replace('StageCharacter *who', 'Who *who').replace('gStageScene->routePlayer', 'gRoutes')
    return HOST.replace('%s', body).replace('%%', '%')


def build():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'wayout.cpp').write_text(host_source())
    exe = WORK / 'wayout.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC),
                    str(WORK / 'wayout.cpp'), str(SRC / 'game' / 'routeplayer.cpp'), '-o', str(exe)], check=True)
    return exe


def listed_areas(data):
    """(area, its list) of header +0x34; only a file with sets has them relocated."""
    w = lambda o: struct.unpack_from('<I', data, o)[0]
    h = lambda o: struct.unpack_from('<h', data, o)[0]
    if w(0x2c) == 0 or w(0x34) == 0 or w(0x34) + 4 > len(data):
        return []
    table = w(0x34)
    out = []
    for i in range(w(table)):
        rec = table + 4 + i * 8
        out.append((h(rec), w(w(rec + 4))))
    return out


def rute_files(limit):
    """Rute files with lists, from the packs: the bytes from the magic to the end of the pack."""
    seen = {}
    for path in sorted(Path(yamipan.GAME_DIR).iterdir()):
        if not re.match(r'ROMD?\.\d+$', path.name):
            continue
        with open(path, 'rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as d:
            at = d.find(b'SLDI\x01\0\0\0')
            while at >= 0:
                data = sssall.sldi(d, at, len(d))
                if data is not None:
                    data = bytes(data)
                    pos = data.find(b'ord\0')
                    while pos >= 0:
                        rute = data[pos:]
                        if pos % 4 == 0 and len(rute) > 0x3c and rute[:0x3c] not in seen:
                            try:
                                lists = listed_areas(rute)
                            except struct.error:
                                lists = []
                            if any(n for _, n in lists):
                                seen[rute[:0x3c]] = rute
                                print(f'  {path.name}@{at:#x}+{pos:#x}: {len(lists)} lists', flush=True)
                                if len(seen) >= limit:
                                    return list(seen.values())
                        pos = data.find(b'ord\0', pos + 4)
                at = d.find(b'SLDI\x01\0\0\0', at + 4)
    return list(seen.values())


def normalize(ee):
    out, src = ee.g('a0'), ee.g('a1')
    v = [ee.float(src + i * 4) for i in range(3)]
    length = math.sqrt(sum(x * x for x in v))
    for i in range(3):
        ee.setfloat(out + i * 4, v[i] / length if length > 0.0 else 0.0)
    return 0


class RomWay:
    def __init__(self, data):
        self.rom = rutecmp.Rom(data)
        ee = self.ee = self.rom.ee
        ee.hooks[0x136560] = normalize      # eerun has no vsqrt
        player = ee.alloc(0xc)
        ee.wr(player + 4, 4, self.rom.core)
        scene = ee.alloc(0x100)
        ee.wr(scene + 0x4c, 4, player)
        ee.wr(eerun.GP - 0x7a7c, 4, scene)
        self.ground = ee.alloc(0x60)
        self.who = ee.alloc(0x400)
        ee.wr(self.who + 0x228, 4, self.ground)
        self.mark = ee.heap

    def ask(self, area, pos, threat, last):
        ee = self.ee
        ee.heap = self.mark
        ee.wr(self.ground + 0x50, 4, area & 0xffffffff)
        for i, v in enumerate(pos):
            ee.setfloat(self.who + 0x150 + i * 4, v)
        out = self.rom.floats([0.0, 0.0, 0.0, 0.0])
        r = ee.call(0x227f30, out, self.who, self.rom.floats(threat), self.rom.floats(last))
        return r & 0xff, [ee.float(out + i * 4) for i in range(3)]


def compare(data, index, exe, queries, rng):
    path = WORK / f'wayout_{index}.bin'
    path.write_bytes(data)
    rom = RomWay(data)
    nodes = rutecmp.nodes_of(data)
    lists = [a for a, n in listed_areas(data) if n]
    near = lambda p: [float(v) + rng.uniform(-600, 600) for v in p]
    rows = []
    for _ in range(queries):
        a = rng.choice(lists)
        inside = [p for n, p in nodes if n == a] or [rng.choice(nodes)[1]]
        rows.append((a, near(rng.choice(inside)), near(rng.choice(inside)), [0.0, 0.0, 0.0, 666.0]))
    (WORK / 'wayout_q.txt').write_text(''.join(
        f'{a} ' + ' '.join(f'{v:.6f}' for v in p + t + l) + '\n' for a, p, t, l in rows))
    out = subprocess.run([str(exe), str(path), str(WORK / 'wayout_q.txt')], capture_output=True, text=True,
                         check=True).stdout.splitlines()
    good = bad = yes = 0
    for row, line in zip(rows, out):
        a, p, t, l = row
        r, place = rom.ask(a, p, t, l)
        fields = line.split()
        hr, hp = int(fields[0]), [float(v) for v in fields[1:]]
        same = hr == r and (r == 0 or all(abs(x - y) <= TOL for x, y in zip(place, hp)))
        good += same
        bad += not same
        yes += r != 0
        if not same and bad <= 3:
            print(f'  DIFF area {a}: rom {r} {place} host {hr} {hp}')
    print(f'  file {index}: {good}/{good + bad} same, {yes} found a way, {len(lists)} lists, '
          f'{rom.ee.count} EE instructions', flush=True)
    return bad


def main(argv):
    limit = int(argv[1]) if len(argv) > 1 else 6
    queries = int(argv[2]) if len(argv) > 2 else 200
    exe = build()
    rng = random.Random(1)
    files = rute_files(limit)
    bad = sum(compare(data, i, exe, queries, rng) for i, data in enumerate(files))
    print(f'{len(files)} files, {bad} differences')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
