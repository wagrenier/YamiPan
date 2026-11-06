#!/usr/bin/env python3
"""game/rumblelist.cpp against the ROM, on the disc's rumble file (offline).

python rumblecmp.py [scripts] [ops]

Finds the file on the disc (ROM.000, the entry names), then plays random scripts on the ROM's own
code (eerun.py, 0x2bc100..) and on the host's in a small exe compiled from src/game/rumblelist.cpp
(work/rumblecmp/): add, removeNamed, isPlaying, removeUnlooped, update with whole and fractional
frames, sources 0 / 1 / 2. After each op both print what it returned: motors, events, sources, and
the tracks left with their names, timers, finished bytes. The first differences and a count go out.
"""
import mmap
import os
import random
import struct
import subprocess
import sys
from pathlib import Path

import eerun
import yamipan

HERE = Path(__file__).parent
WORK = HERE / 'work' / 'rumblecmp'
SRC = yamipan.REPO / 'src'
MAX = 8
FAKE_SOURCE = {0: 0, 1: 0x1000, 2: 0x2000}

HOST = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/rumblelist.h"
static StageObject *who(int s) { return (StageObject *)(size_t)(s * 0x1000); }
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    void *bytes = malloc(n); fread(bytes, 1, n, f); fclose(f);
    RumbleList list; list.data = (const RumbleFile *)bytes;
    FILE *ops = fopen(argv[2], "r");
    char op[8], name[32]; int s; float frames;
    while (fscanf(ops, "%7s", op) == 1) {
        if (!strcmp(op, "add")) { fscanf(ops, "%31s %d", name, &s); list.addNamed(name, who(s)); printf("add"); }
        else if (!strcmp(op, "del")) { fscanf(ops, "%31s", name); printf("del %d", list.removeNamed(name)); }
        else if (!strcmp(op, "has")) { fscanf(ops, "%31s", name); printf("has %d", list.isPlaying(name)); }
        else if (!strcmp(op, "cut")) { list.removeUnlooped(); printf("cut"); }
        else {
            fscanf(ops, "%f %d", &frames, &s);
            u_char a = 0, b = 0; char ev[8]; StageObject *src[8];
            list.update(frames, &a, &b, ev, src, 8, who(s));
            printf("up %d %d", a, b);
            for (int i = 0; i < 8; i++) printf(" %d", (u_char)ev[i]);
            for (int i = 0; i < 8; i++) printf(" %d", (int)((size_t)src[i] / 0x1000));
        }
        printf(" |");
        for (RumbleTrack *t = list.head; t; t = t->next) {
            printf(" %s %d", t->name, t->finished);
            for (auto &c : t->channels) printf(" %.4f", c.timer);
        }
        printf("\n");
    }
    return 0;
}
'''


def find_file():
    root = Path('C:/Users/willi/Documents/Decomp/Forbidden Siren 2/ROM.000')
    with open(root, 'rb') as h:
        m = mmap.mmap(h.fileno(), 0, access=mmap.ACCESS_READ)
        name = m.find(b'PETITJACK\0')
        at = m.rfind(b'sv2S', name - 0x1000, name)
        count = struct.unpack_from('<I', m, at + 8)[0]
        last = struct.unpack_from('<I', m, at + 0xc + (count - 1) * 4)[0]
        data = bytes(m[at:at + last + 0x100])
    names = []
    for i in range(count):
        o = struct.unpack_from('<I', data, 0xc + i * 4)[0]
        names.append(data[o + 8:o + 0x18].split(b'\0')[0].decode())
    return data, names


def build():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC),
                    str(WORK / 'host.cpp'), str(SRC / 'game' / 'rumblelist.cpp'), '-o', str(exe)], check=True)
    return exe


class Rom:
    def __init__(self, data):
        ee = self.ee = eerun.Ee()
        ee.hook('memset', self.memset)      # the libc's use MMI ops eerun does not have
        ee.hook('memcpy', self.memcpy)
        self.file = ee.alloc(len(data))
        ee.write(self.file, data)
        self.list = ee.alloc(0xc)
        ee.call(0x2bc770, self.list)
        ee.wr(self.list, 4, self.file)
        self.text = ee.alloc(0x20)
        self.out = ee.alloc(0x40)

    @staticmethod
    def memset(ee):
        for i in range(ee.g('a2')):
            ee.wr(ee.g('a0') + i, 1, ee.g('a1') & 0xff)
        return ee.g('a0')

    @staticmethod
    def memcpy(ee):
        for i in range(ee.g('a2')):
            ee.wr(ee.g('a0') + i, 1, ee.rd(ee.g('a1') + i, 1))
        return ee.g('a0')

    def name(self, text):
        self.ee.write(self.text, text.encode() + b'\0')
        return self.text

    def state(self):
        ee = self.ee
        row = ' |'
        t = ee.u32(self.list + 4)
        while t:
            at = ee.u32(t + 8)
            name = bytes(ee.rd(at + i, 1) for i in range(0x10)).split(b'\0')[0].decode()
            row += f' {name} {ee.rd(t + 0x58, 1)}'
            for c in range(3):
                row += f' {eerun.f32(ee.u32(t + 0x10 + c * 0x18 + 0x10)):.4f}'
            t = ee.u32(t)
        return row

    def run(self, op):
        ee = self.ee
        w = op.split()
        if w[0] == 'add':
            ee.call(0x2bc3c0, self.list, self.name(w[1]), FAKE_SOURCE[int(w[2])])
            head = 'add'
        elif w[0] == 'del':
            head = f'del {eerun.s32(ee.call(0x2bc4f0, self.list, self.name(w[1])))}'
        elif w[0] == 'has':
            head = f'has {eerun.s32(ee.call(0x2bc690, self.list, self.name(w[1])))}'
        elif w[0] == 'cut':
            ee.call(0x2bc5d0, self.list)
            head = 'cut'
        else:
            for i in range(0x40):
                ee.wr(self.out + i, 1, 0xee)
            ev, src = self.out + 2, self.out + 0x10
            ee.f[12] = eerun.b32(float(w[1]))
            ee.call(0x2bc100, self.list, self.out, self.out + 1, ev, src, MAX, FAKE_SOURCE[int(w[2])])
            head = f'up {ee.rd(self.out, 1)} {ee.rd(self.out + 1, 1)}'
            head += ''.join(f' {ee.rd(ev + i, 1)}' for i in range(8))
            head += ''.join(f' {ee.u32(src + i * 4) // 0x1000}' for i in range(8))
        return head + self.state()


def script(names, rng, n):
    ops = []
    for _ in range(n):
        r = rng.random()
        if r < 0.12:
            ops.append(f'add {rng.choice(names + ["NOSUCH"])} {rng.randrange(3)}')
        elif r < 0.15:
            ops.append(f'del {rng.choice(names)}')
        elif r < 0.18:
            ops.append(f'has {rng.choice(names)}')
        elif r < 0.195:
            ops.append('cut')
        else:
            ops.append(f'up {rng.choice((1.0, 1.0, 1.0, 0.5, 2.0, 1.25, 7.0, 45.0))} {rng.randrange(3)}')
    return ops


def main(argv):
    scripts = int(argv[1]) if len(argv) > 1 else 40
    n = int(argv[2]) if len(argv) > 2 else 600
    data, names = find_file()
    print(f'{len(names)} tracks: {" ".join(names)}')
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'rumble.bin').write_bytes(data)
    exe = build()
    rng = random.Random(1)
    bad = total = fired = 0
    for s in range(scripts):
        ops = script(names, rng, n)
        (WORK / 'ops.txt').write_text('\n'.join(ops) + '\n')
        out = subprocess.run([str(exe), str(WORK / 'rumble.bin'), str(WORK / 'ops.txt')], capture_output=True,
                             text=True, check=True).stdout.splitlines()
        rom = Rom(data)
        for op, host in zip(ops, out):
            got = rom.run(op)
            total += 1
            if op.startswith('up') and got.split(' |')[0].split()[3:11] != ['0'] * 8:
                fired += 1
            if got != host:
                bad += 1
                if bad <= 6:
                    print(f'{op}\n  rom  {got[:200]}\n  host {host[:200]}')
        if len(out) != len(ops):
            print('host stopped early', len(out), len(ops))
            bad += 1
    print(f'{scripts} scripts, {total} ops, {fired} updates with events, {bad} differences')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
