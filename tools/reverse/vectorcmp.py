#!/usr/bin/env python3
"""game/vectortrack.cpp against the ROM (offline).

python vectorcmp.py [scripts] [ops]

Random scripts on the ROM's own code (eerun.py) and on the host's in a small exe compiled from
src/game/vectortrack.cpp and vu0math.cpp (work/vectorcmp/): a track of each kind (linear, the Bezier
of 3 and of 4 lanes, angles; looping or not) gets keys with and without frames and times, played
with step, reset and keyAt while keys still arrive. After each op both print the value, the counters
and every key's frames and control points, as bits. VectorFade the same, with frames above 0 (the
ROM's 0 / 0 is the FPU's, the host's guard says what the host does). Differences are counted by
word, with the worst distance in units in the last place; the ROM's dtor is run for what it frees.
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
WORK = HERE / 'work' / 'vectorcmp'
SRC = yamipan.REPO / 'src'

FADE_CTOR, FADE_SET_TO, FADE_SET_FROM, FADE_STEP = 0x296cc0, 0x296ca0, 0x296cb0, 0x296c10
TRACK_CTOR, TRACK_DTOR, TRACK_STEP, TRACK_RESET = 0x29a580, 0x29a500, 0x299d90, 0x299f40
TRACK_ADD, TRACK_KEY_AT, KEY_CTOR, KEY_CTOR_ZERO = 0x29a370, 0x29a4c0, 0x29a6e0, 0x29a700

HOST = r'''
#include <stdio.h>
#include <string.h>
#include "game/vectortrack.h"
static float word(FILE *f) { unsigned b; fscanf(f, "%x", &b); float v; memcpy(&v, &b, 4); return v; }
static void put(float v) { unsigned b; memcpy(&b, &v, 4); printf(" %x", b); }
static void vec(const float *v) { for (int i = 0; i < 4; i++) put(v[i]); }
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    char op[8];
    VectorTrack *t = nullptr;
    VectorFade *fade = nullptr;
    while (fscanf(f, "%7s", op) == 1) {
        if (!strcmp(op, "fade")) {
            delete fade;
            fade = new VectorFade;
            sceVu0FVECTOR a, b;
            for (int i = 0; i < 4; i++) a[i] = word(f);
            for (int i = 0; i < 4; i++) b[i] = word(f);
            fscanf(f, "%d", &fade->frames);
            fade->setFrom(a);
            fade->setTo(b);
        } else if (!strcmp(op, "fstep")) {
            fade->step();
        } else {
            if (!strcmp(op, "new")) {
                delete t;
                t = new VectorTrack;
                int a, b, c;
                fscanf(f, "%d %d %d", &a, &b, &c);
                t->linear = a; t->loop = b; t->curve = c;
            } else if (!strcmp(op, "key") || !strcmp(op, "zkey")) {
                int frames, time;
                sceVu0FVECTOR v;
                fscanf(f, "%d %d", &frames, &time);
                VectorKey *key;
                if (op[0] == 'z') {
                    key = new VectorKey(frames);
                } else {
                    for (int i = 0; i < 4; i++) v[i] = word(f);
                    key = new VectorKey(v, frames);
                }
                t->addKey(key, time, true);
            } else if (!strcmp(op, "step")) {
                t->step();
            } else if (!strcmp(op, "reset")) {
                t->reset();
            } else if (!strcmp(op, "at")) {
                int i;
                fscanf(f, "%d", &i);
                VectorKey *key = t->keyAt(i);
                printf("at");
                if (key) put(key->value[0]);
                printf(" ");
            }
            printf("t %d %d %d %d %d %d", t->frame, t->count, t->index, t->done, t->linear, t->loop);
            vec(t->value);
            for (VectorKey *k = t->keys; k; k = k->next) {
                printf(" k %d", k->frames);
                vec(k->value); vec(k->ctrlOut); vec(k->ctrlIn);
            }
            printf("\n");
            continue;
        }
        printf("f %d %d", fade->frames, fade->frame);
        vec(fade->from); vec(fade->to); vec(fade->value);
        printf("\n");
    }
    return 0;
}
'''


def build():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC),
                    str(WORK / 'host.cpp'), str(SRC / 'game' / 'vectortrack.cpp'),
                    str(SRC / 'game' / 'vu0math.cpp'), '-o', str(exe)], check=True)
    return exe


class Rom:
    def __init__(self):
        self.ee = eerun.Ee()
        self.freed = []
        self.ee.hook('operator_delete', self.delete)
        self.track = self.fade = 0
        self.scratch = self.ee.alloc(0x40)

    def delete(self, ee):
        self.freed.append(ee.g('a0'))
        return 0

    def call(self, addr, *args):
        # What a call left in the vf's upper lanes can be junk bits (vu0cmp.py).
        self.ee.vf[1:] = [[0.0, 0.0, 0.0, 0.0] for _ in range(31)]
        return self.ee.call(addr, *args)

    def vector(self, at, words):
        self.ee.write(at, struct.pack('<4I', *words))

    def bits(self, at):
        return ''.join(f' {w:x}' for w in struct.unpack('<4I', bytes(self.ee.m[at:at + 16])))

    def run(self, op):
        ee = self.ee
        w = op.split()
        if w[0] == 'fade':
            self.fade = ee.alloc(0x40)
            self.call(FADE_CTOR, self.fade)
            self.vector(self.scratch, [int(x, 16) for x in w[1:5]])
            self.call(FADE_SET_FROM, self.fade, self.scratch)
            self.vector(self.scratch, [int(x, 16) for x in w[5:9]])
            self.call(FADE_SET_TO, self.fade, self.scratch)
            ee.wr(self.fade + 0x30, 4, int(w[9]))
            return self.fade_state()
        if w[0] == 'fstep':
            self.call(FADE_STEP, self.fade)
            return self.fade_state()
        head = ''
        if w[0] == 'new':
            self.track = ee.alloc(0x20)
            self.call(TRACK_CTOR, self.track)
            ee.wr(self.track + 0x10, 1, int(w[1]))
            ee.wr(self.track + 0x11, 1, int(w[2]))
            ee.wr(self.track + 0x13, 1, int(w[3]))
        elif w[0] in ('key', 'zkey'):
            key = ee.alloc(0x40)
            frames, time = int(w[1]), int(w[2])
            if w[0] == 'zkey':
                self.call(KEY_CTOR_ZERO, key, frames)
            else:
                self.vector(self.scratch, [int(x, 16) for x in w[3:7]])
                self.call(KEY_CTOR, key, self.scratch, frames)
            self.call(TRACK_ADD, self.track, key, time, 1)
        elif w[0] == 'step':
            self.call(TRACK_STEP, self.track)
        elif w[0] == 'reset':
            self.call(TRACK_RESET, self.track)
        elif w[0] == 'at':
            key = self.call(TRACK_KEY_AT, self.track, int(w[1])) & 0xffffffff
            head = 'at' + (f' {ee.u32(key):x}' if key else '') + ' '
        return head + self.state()

    def fade_state(self):
        f = self.fade
        return (f'f {eerun.s32(self.ee.u32(f + 0x30))} {eerun.s32(self.ee.u32(f + 0x34))}'
                + self.bits(f) + self.bits(f + 0x10) + self.bits(f + 0x20))

    def state(self):
        ee, t = self.ee, self.track
        row = (f't {eerun.s32(ee.u32(t + 0x14))} {ee.s16(t + 0x18)} {ee.s16(t + 0x1a)} {ee.rd(t + 0x12, 1)}'
               f' {ee.rd(t + 0x10, 1)} {ee.rd(t + 0x11, 1)}' + self.bits(t))
        k = ee.u32(t + 0x1c)
        while k:
            row += f' k {eerun.s32(ee.u32(k + 0x30))}' + self.bits(k) + self.bits(k + 0x10) + self.bits(k + 0x20)
            k = ee.u32(k + 0x3c)
        return row


def hexes(values):
    return ' '.join(f'{eerun.b32(v):x}' for v in values)


def script(rng, n):
    linear, loop, curve = rng.choice(((1, 0, 0), (1, 1, 0), (0, 0, 0), (0, 1, 0), (0, 0, 1), (0, 1, 1), (0, 0, 2),
                                      (0, 1, 2)))
    wide = 12.0 if curve == 2 else rng.choice((2.0, 255.0, 2000.0))
    ops = [f'new {linear} {loop} {curve}']
    last = [0.0] * 4
    keys = rng.randrange(0, 7)
    for i in range(n):
        r = rng.random()
        if keys and (i < 3 or r < 0.02):
            keys -= 1
            frames = rng.choice((0, 0, 1, 2, 5, 30, 100))
            time = rng.choice((-1, -1, rng.randrange(0, 40), rng.randrange(0, 400)))
            if rng.random() < 0.15:
                ops.append(f'zkey {frames} {time}')
                last = [0.0] * 4
            else:
                if rng.random() > 0.15:     # else the same value: a zero direction, a zero distance
                    last = [rng.uniform(-wide, wide) for _ in range(4)]
                ops.append(f'key {frames} {time} {hexes(last)}')
        elif r < 0.03:
            ops.append('reset')
        elif r < 0.06:
            ops.append(f'at {rng.randrange(0, 8)}')
        else:
            ops.append('step')
    return ops


def fade_script(rng):
    a = [rng.uniform(-1000.0, 1000.0) for _ in range(4)]
    b = [rng.uniform(-1000.0, 1000.0) for _ in range(4)]
    frames = rng.choice((1, 2, 10, 30, 90))
    return [f'fade {hexes(a)} {hexes(b)} {frames}'] + ['fstep'] * (frames + 3)


def ulps(a, b):
    def line(x):
        return x & 0x7fffffff if not x >> 31 else -(x & 0x7fffffff)
    return abs(line(a) - line(b))


def main(argv):
    scripts = int(argv[1]) if len(argv) > 1 else 60
    n = int(argv[2]) if len(argv) > 2 else 300
    exe = build()
    rng = random.Random(0x29a370)
    rom = Rom()
    words = bad = total = worst = shown = 0
    kinds = {}
    for s in range(scripts):
        ops = script(rng, n) + fade_script(rng)
        (WORK / 'ops.txt').write_text('\n'.join(ops) + '\n')
        out = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                             check=True).stdout.splitlines()
        if len(out) != len(ops):
            print('host stopped early', len(out), len(ops))
            bad += 1
        kinds[ops[0]] = kinds.get(ops[0], 0) + 1
        for op, host in zip(ops, out):
            got = rom.run(op)
            total += 1
            a, b = got.split(), host.split()
            if len(a) != len(b):
                bad += 1
                print(f'{op[:60]}\n  rom  {got[:160]}\n  host {host[:160]}')
                continue
            for i, (x, y) in enumerate(zip(a, b)):
                words += 1
                if x == y:
                    continue
                bad += 1
                try:
                    worst = max(worst, ulps(int(x, 16), int(y, 16)))
                except ValueError:
                    pass
                if shown < 8:
                    shown += 1
                    print(f'{ops[0]}: {op[:40]} word {i}: rom {x} host {y}')
        # The dtor frees the chain, and the track too when asked.
        rom.freed = []
        count = rom.ee.s16(rom.track + 0x18)
        rom.call(TRACK_DTOR, rom.track, 1)
        if len(rom.freed) != count + 1 or rom.freed[-1] != rom.track:
            print('dtor freed', len(rom.freed), 'of', count + 1)
            bad += 1
    print(f'{scripts} scripts, {total} ops, {words} words, {bad} differ, worst {worst} ulp')
    print('tracks:', ' '.join(f'{k[4:].replace(" ", ",")} x{v}' for k, v in sorted(kinds.items())))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
