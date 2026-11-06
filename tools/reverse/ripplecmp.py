#!/usr/bin/env python3
"""game/ripplesheet.cpp against the ROM (offline).

python ripplecmp.py [frames]

Builds the sheet as SightJack's ctor does on both, the ROM's own code on eerun.py (0x2d31d0..) and the
host's in a small exe compiled from src/game/ripplesheet.cpp and mathutil.cpp (work/ripplecmp/), with
one generator behind randNext. Then frames of draw and advance with the same steps: every point's
x, y, height compared, and the ROM's packet walked quad by quad against the rule the host's
UiCanvas::rippleFrame lays the sheet by (cell, edge, which point at which corner).
"""
import os
import subprocess
import sys
from pathlib import Path

import eerun
import yamipan

HERE = Path(__file__).parent
WORK = HERE / 'work' / 'ripplecmp'
SRC = yamipan.REPO / 'src'
COLUMNS = ROWS = 15
SCRATCH = 0x70000000
STEPS = [1 / 30, 1 / 60, 0.1, 0.25, 1.0]

HOST = r'''
#include <stdio.h>
#include <stdlib.h>
#define private public
#include "game/ripplesheet.h"
#undef private
#include "yamipan/rendering/yamipan_drawui.h"
static unsigned state = 12345;
u_int randNext() { state = (state * 1103515245u + 12345u) & 0x7fffffff; return state; }
void YamiPan_DrawUiFrameRipple(const void *, const YamiPanRipple *r)
{
    for (int i = 0; i < r->columns * r->rows; i++)
        printf("%d %d %d\n", r->points[i].x, r->points[i].y, r->points[i].height);
}
int main(int, char **argv)
{
    RippleSheet sheet(15, 15, 1);
    RippleSpec &spec = sheet.spec();
    spec.speedMin = 0.3f;
    spec.speedMax = 3.0f;
    spec.setPeak(8.0f);
    sheet.restart();
    printf("%.6f %.6f\n", spec.pushOverSpeed, spec.push);
    int frames = atoi(argv[1]);
    static const float steps[] = {1.0f / 30, 1.0f / 60, 0.1f, 0.25f, 1.0f};
    for (int i = 0; i < frames; i++) {
        sheet.draw(nullptr, 0);
        sheet.advance(steps[i % 5]);
    }
    return 0;
}
'''


def host(frames):
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC),
                    str(WORK / 'host.cpp'), str(SRC / 'game' / 'ripplesheet.cpp'), str(SRC / 'game' / 'mathutil.cpp'),
                    '-o', str(exe)], check=True)
    lines = subprocess.run([str(exe), str(frames)], capture_output=True, text=True, check=True).stdout.split('\n')
    head = lines[0]
    points = [tuple(int(v) for v in row.split()) for row in lines[1:] if row]
    return head, points


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        self.state = 12345
        ee.hook('randNext', self.rand)
        env = ee.alloc(0x100)
        ee.hook('gfxGetDrawEnv', lambda e: env)
        self.buffer = ee.alloc(0x40000)
        self.packet = ee.alloc(0x20)
        self.sheet = ee.alloc(0x30)
        ee.call(0x2d31d0, self.sheet, COLUMNS, ROWS, 1)
        ee.setfloat(self.sheet + 0x20, 0.3)
        ee.setfloat(self.sheet + 0x24, 3.0)
        ee.f[12] = eerun.b32(8.0)
        ee.call(0x2d2c20, self.sheet + 0x18)
        ee.call(0x2d2f50, self.sheet)

    def rand(self, ee):
        self.state = (self.state * 1103515245 + 12345) & 0x7fffffff
        return self.state

    def spec(self):
        return f'{self.ee.float(self.sheet + 0x18):.6f} {self.ee.float(self.sheet + 0x1c):.6f}'

    def frame(self, step):
        ee = self.ee
        ee.wr(self.packet, 4, self.buffer)
        ee.call(0x2d10f0, self.sheet, self.packet, 0, 0x200, 0x200, 0x100)
        points = []
        for i in range(COLUMNS * ROWS):
            row = [eerun.s32(ee.rd(SCRATCH + i * 12 + k * 4, 4)) for k in range(3)]
            points.append(tuple(row))
        wrong = self.walk(points)
        ee.f[12] = eerun.b32(step)
        ee.call(0x2d3080, self.sheet)
        return points, wrong

    def walk(self, points):
        """The packet's quads against the rule of UiCanvas::rippleFrame."""
        ee = self.ee
        cell = 0x2000 // (COLUMNS + 1)
        at = self.buffer + 0x90
        wrong = 0

        def corner(c, r):
            if 0 < c <= COLUMNS and 0 < r <= ROWS:
                p = points[(r - 1) * COLUMNS + c - 1]
                return p[0], p[1], p[2] & 0xff
            return 0, 0, 0x80

        for r in range(ROWS + 1):
            for c in range(COLUMNS + 1):
                for k, (cc, rr) in enumerate(((c, r), (c + 1, r), (c, r + 1), (c + 1, r + 1))):
                    rgba, uv, xyz = (ee.rd(at + (k * 3 + n) * 8, 8) for n in range(3))
                    px, py, h = corner(cc, rr)
                    if (c, r, k) == (COLUMNS, 0, 2):    # the ROM's slip, kept by the host
                        h = (h + 0x80) & 0xff
                    x, y = cc * cell + px, rr * cell + py
                    if (rgba >> 24) & 0xff != h or (uv & 0xffff) != cc * cell + 8 or (uv >> 16) & 0xffff != rr * cell + 8 \
                            or (xyz & 0xffff) != x & 0xffff or (xyz >> 16) & 0xffff != y & 0xffff:
                        wrong += 1
                        if wrong <= 3:
                            print(f'cell {c},{r} corner {k}: alpha {(rgba >> 24) & 0xff:#x} expected {h:#x}, '
                                  f'uv {uv:#x} xyz {xyz:#x} expected {x:#x} {y:#x}')
                at += 12 * 8
        return wrong


def main():
    frames = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    head, hosts = host(frames)
    rom = Rom()
    print('spec host', head, '| rom', rom.spec())
    differ = off = wrong = worst = 0
    for f in range(frames):
        points, bad = rom.frame(STEPS[f % 5])
        wrong += bad
        for i, p in enumerate(points):
            q = hosts[f * COLUMNS * ROWS + i]
            if p != q:
                differ += 1
                worst = max(worst, *(abs(a - b) for a, b in zip(p, q)))
                if differ <= 5:
                    print(f'frame {f} point {i}: rom {p} host {q}')
    print(f'{frames} frames, {frames * COLUMNS * ROWS} points: {differ} differ, largest difference {worst}; '
          f'{wrong} packet corners off the host\'s rule')


main()
