#!/usr/bin/env python3
"""rx/rxsndstrmem.c against the ROM, offline.

python strmemcmp.py [scripts] [ops]

Random op scripts on the ROM's own functions (eerun.py, 0x1660e0..0x166b50) and on the host's in a small
exe compiled from src/rx/rxsndstrmem.c (work/strmemcmp/): open on EE and IOP memory up to no free slot,
close, seek, consume (exact, too much, negative), markers (active or not, pass limits, jumps, no callback
or one that says stop, that seeks, that jumps) up to an empty pool, tags, both Enable calls, exit, ids out
of range and never opened. After each op both print its result, every callback and IOP read in order, and
for the op's stream the readable size, end flag, size, the bytes at the position and all its tags. The
dispatch calls, rxSndUtGetDebugFlags, rxSysPrintf and rxSndIopRead are stand-ins on both sides.
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
WORK = HERE / 'work' / 'strmemcmp'
SRC = yamipan.REPO / 'src'
M64 = 0xffffffffffffffff
INIT, EXIT, OPEN, CLOSE, SEEK, READABLE, CONSUME, DATA, ISEND = (
    0x1660e0, 0x166178, 0x1661c0, 0x1662b0, 0x166320, 0x166370, 0x166418, 0x1665b8, 0x166650)
SETMARKERS, MARKERENABLE, SETTAGS, TAGENABLE, GETTAGS, GETSIZE = (
    0x166688, 0x166828, 0x166890, 0x166a10, 0x166a78, 0x166b18)
FAKE_CALLBACK = 0x7f0000
MEMORY = 0x4000
IOP_BASE = 0x100000
TAG_ALL = 0x1000000

HOST = r'''
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rx/rxsnd.h"
#include "rx/rxsndstrmem.h"
#include "rx/rxsndut.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"

static u_char s_bytes[0x4000];
extern "C" void rxThDisableDispatch(void) {}
extern "C" void rxThEnableDispatch(void) {}
extern "C" u_int rxSndUtGetDebugFlags(void) { return 1; }
extern "C" int rxSysPrintf(const char *fmt, ...) { printf(" print"); return 0; }
extern "C" int rxSndIopRead(void *dst, u_int iopAddr, int size)
{
    printf(" read %x %d", iopAddr, size);
    if (size > 0) memcpy(dst, s_bytes + (iopAddr - 0x100000), size);
    return 0;
}

static int callback(void *arg, RxFileMarkerDef *m)
{
    printf(" cb %d %x %d %d %d", (int)(size_t)arg, m->pos, m->count, m->max, m->user);
    int stream = m->user >> 8, how = m->user & 3;
    if (how == 2) { rxSndStrMemSeek(stream, m->pos + 0x10); return 0; }
    return how == 1;
}

static void show(int id)
{
    int readable = rxSndStrMemGetReadable(id);
    printf(" | %d %d %d", readable, rxSndStrMemIsEnd(id), rxSndStrMemGetSize(id));
    if (readable > 0) {
        const u_char *data = rxSndStrMemGetData(id);
        for (int i = 0; i < 8 && i < readable; i++) printf(" %02x", data[i]);
        printf(" %02x", data[readable - 1]);
    }
    RxFileTagHit hits[300];
    int n = rxSndStrMemGetTags(id, hits, 300, 0x1000000);
    printf(" t%d", n);
    for (int i = 0; i < n && i < 300; i++) printf(" %x:%d", hits[i].offset, hits[i].value);
}

int main(int, char **argv)
{
    for (int i = 0; i < 0x4000; i++) s_bytes[i] = (u_char)((i * i >> 3) ^ i ^ (i >> 8));
    static RxFileMarkerDef marks[400];
    static RxFileTagDef tags[400];
    rxSndStrMemInit();
    FILE *ops = fopen(argv[1], "r");
    char op[16];
    while (fscanf(ops, "%15s", op) == 1) {
        int id = 0;
        if (!strcmp(op, "exit")) { printf("exit"); rxSndStrMemExit(); printf("\n"); continue; }
        fscanf(ops, "%d", &id);
        printf("%s", op);
        if (!strcmp(op, "open")) {
            int size, iop, at;
            fscanf(ops, "%d %d %d", &size, &iop, &at);
            uintptr_t addr = iop ? 0x100000 + (size_t)at : (uintptr_t)(s_bytes + at);
            id = rxSndStrMemOpen(addr, size, iop);
            printf(" %d", id);
        } else if (!strcmp(op, "close")) {
            rxSndStrMemClose(id);
        } else if (!strcmp(op, "seek")) {
            unsigned pos; fscanf(ops, "%u", &pos); rxSndStrMemSeek(id, pos);
        } else if (!strcmp(op, "eat")) {
            int size; fscanf(ops, "%d", &size); rxSndStrMemConsume(id, size);
        } else if (!strcmp(op, "mark")) {
            int n, arg; fscanf(ops, "%d %d", &n, &arg);
            for (int i = 0; i < n; i++) {
                int active, mid, max, cb; RxFileMarkerDef *m = &marks[i]; memset(m, 0, sizeof *m);
                fscanf(ops, "%u %d %d %d %d %d %d", &m->pos, &active, &mid, &max, &m->jumpPos, &cb, &m->user);
                m->active = active; m->id = mid; m->max = max; m->callback = cb ? callback : 0;
            }
            printf(" %d", rxSndStrMemSetMarkers(id, marks, n, (void *)(size_t)arg));
        } else if (!strcmp(op, "tag")) {
            int n; fscanf(ops, "%d", &n);
            for (int i = 0; i < n; i++) {
                int active, tid; RxFileTagDef *t = &tags[i]; memset(t, 0, sizeof *t);
                fscanf(ops, "%u %d %d %d", &t->pos, &active, &tid, &t->value);
                t->active = active; t->id = tid;
            }
            printf(" %d", rxSndStrMemSetTags(id, tags, n));
        } else if (!strcmp(op, "markon")) {
            int which, active; fscanf(ops, "%d %d", &which, &active); rxSndStrMemSetMarkerEnable(id, which, active);
        } else if (!strcmp(op, "tagon")) {
            int which, active; fscanf(ops, "%d %d", &which, &active); rxSndStrMemSetTagEnable(id, which, active);
        } else {
            int max, size; fscanf(ops, "%d %d", &max, &size);
            RxFileTagHit hits[8]; memset(hits, 0xee, sizeof hits);
            int n = rxSndStrMemGetTags(id, hits, max, size);
            printf(" %d", n);
            for (int i = 0; i < 8; i++) printf(" %x:%x", hits[i].offset, hits[i].value);
        }
        show(id);
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
                    '-x', 'c++', str(WORK / 'host.cpp'), str(SRC / 'rx' / 'rxsndstrmem.c'), '-o', str(exe)],
                   check=True)
    return exe


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        self.log = ''
        for addr in (0x167890, 0x167920, 0x151f70, 0x151f80):
            ee.hooks[addr] = lambda e: 0
        ee.hook('rxSndUtGetDebugFlags', lambda e: 1)
        ee.hook('rxSysPrintf', self.printf)
        ee.hook('rxSndIopRead', self.iop_read)
        ee.hooks[FAKE_CALLBACK] = self.callback
        self.bytes = ee.alloc(MEMORY)
        ee.write(self.bytes, bytes(((i * i >> 3) ^ i ^ (i >> 8)) & 0xff for i in range(MEMORY)))
        self.defs = ee.alloc(0x14 * 400)
        self.hits = ee.alloc(8 * 300)
        ee.call(INIT)

    def printf(self, ee):
        self.log += ' print'
        return 0

    def iop_read(self, ee):
        dst, at, size = ee.g('a0') & 0xffffffff, ee.g('a1') & 0xffffffff, eerun.s32(ee.g('a2'))
        self.log += f' read {at:x} {size}'
        if size > 0:
            ee.m[dst:dst + size] = ee.m[self.bytes + at - IOP_BASE:self.bytes + at - IOP_BASE + size]
        return 0

    def callback(self, ee):
        arg, m = eerun.s32(ee.g('a0')), ee.g('a1') & 0xffffffff
        user = eerun.s32(ee.u32(m + 0x10))
        self.log += f' cb {arg} {ee.u32(m):x} {ee.rd(m + 6, 1)} {ee.rd(m + 7, 1)} {user}'
        how = user & 3
        if how == 2:
            saved = dict(ee.r)
            ee.call(SEEK, (user >> 8) & M64, ee.u32(m) + 0x10)
            ee.r = saved
            return 0
        return 1 if how == 1 else 0

    def c(self, pc, *args):
        return self.ee.call(pc, *[a & M64 for a in args])

    def show(self, i):
        ee = self.ee
        readable = self.c(READABLE, i)
        self.log += f' | {readable} {self.c(ISEND, i)} {self.c(GETSIZE, i)}'
        if readable > 0:
            data = self.c(DATA, i) & 0xffffffff
            for k in range(min(8, readable)):
                self.log += f' {ee.rd(data + k, 1):02x}'
            self.log += f' {ee.rd(data + readable - 1, 1):02x}'
        n = self.c(GETTAGS, i, self.hits, 300, TAG_ALL)
        self.log += f' t{n}'
        for k in range(min(n, 300)):
            self.log += f' {ee.u32(self.hits + k * 8):x}:{eerun.s32(ee.u32(self.hits + k * 8 + 4))}'

    def run(self, line):
        ee = self.ee
        w = line.split()
        op = w[0]
        if op == 'exit':
            self.log = 'exit'
            self.c(EXIT)
            return self.log
        i = int(w[1])
        self.log = op
        n = [int(x) for x in w[2:]]
        if op == 'open':
            size, iop, at = n
            i = self.c(OPEN, IOP_BASE + at if iop else self.bytes + at, size, iop)
            self.log += f' {i}'
        elif op == 'close':
            self.c(CLOSE, i)
        elif op == 'seek':
            self.c(SEEK, i, n[0])
        elif op == 'eat':
            self.c(CONSUME, i, n[0])
        elif op == 'mark':
            count, arg = n[0], n[1]
            at = self.defs
            for k in range(count):
                pos, active, mid, top, jump, cb, user = n[2 + k * 7:9 + k * 7]
                ee.write(at, struct.pack('<IBBBBiIi', pos, active, mid, 0, top, jump,
                                         FAKE_CALLBACK if cb else 0, user))
                at += 0x14
            self.log += f' {self.c(SETMARKERS, i, self.defs, count, arg)}'
        elif op == 'tag':
            count = n[0]
            at = self.defs
            for k in range(count):
                pos, active, tid, value = n[1 + k * 4:5 + k * 4]
                ee.write(at, struct.pack('<IBBHi', pos, active, tid, 0, value))
                at += 0xc
            self.log += f' {self.c(SETTAGS, i, self.defs, count)}'
        elif op == 'markon':
            self.c(MARKERENABLE, i, n[0], n[1])
        elif op == 'tagon':
            self.c(TAGENABLE, i, n[0], n[1])
        else:
            for k in range(0x40):
                ee.wr(self.hits + k, 1, 0xee)
            count = self.c(GETTAGS, i, self.hits, n[0], n[1])
            self.log += f' {count}'
            for k in range(8):
                self.log += f' {ee.u32(self.hits + k * 8):x}:{ee.u32(self.hits + k * 8 + 4):x}'
        self.show(i)
        return self.log


def pick_id(rng):
    return rng.choice((1, 1, 1, 2, 2, 3, 4, 5, 0, 63, -1, 64))


def pick_pos(rng, top=0x3000):
    return rng.randrange(0, top, 0x10) + rng.choice((0, 0, 0, 0, 1))


def marks(rng, i):
    count = rng.choice((0, 1, 2, 3, 5, 8, 8, 300))
    rows = []
    for _ in range(count):
        jump = rng.choice((-1, -1, 0, 0, pick_pos(rng), 0x3800))
        how = rng.randrange(3)
        pos = pick_pos(rng, 0x1000)
        rows.append(f'{pos} {rng.choice((1, 1, 1, 1, 0, 2))} {rng.randrange(4)} '
                    f'{rng.choice((0, 0, 1, 2, 3))} {jump} {rng.randrange(5) != 0:d} {(i << 8) | how}')
        if how == 2 and count < 300:    # where its seek lands: the pass must see the new position
            rows.append(f'{pos + 0x10} 1 0 0 {jump} 1 {(i << 8) | 1}')
    return f'mark {i} {len(rows)} {rng.randrange(1, 100)} ' + ' '.join(rows)


def script(rng, n):
    ops = []
    while len(ops) < n:
        r = rng.random()
        i = pick_id(rng)
        if r < 0.03:
            # a stream played through its markers: reads stop exactly on them
            size = rng.choice((0x1000, 0x2000, 0x3000))
            iop = rng.randrange(2)
            ops.append(f'close {i}')
            ops.append(f'open 0 {size} {iop} {rng.randrange(0, 0x800, 4)}')
            ops.append(marks(rng, i))
            ops.extend(f'eat {i} {rng.choice((0x7fffffff, 0x7fffffff, 0x10, 0x40))}'
                       for _ in range(rng.randrange(4, 30)))
        elif r < 0.10:
            size = rng.choice((0, 0x10, 0x100, 0x800, 0x1000, 0x1010, 0x2000, 0x3000))
            ops.append(f'open 0 {size} {rng.randrange(2)} {rng.randrange(0, 0x800, 4)}')
        elif r < 0.105:
            ops.extend(['open 0 256 0 0'] * rng.randrange(20, 70))
            ops.extend(f'close {k}' for k in range(1, 64))
        elif r < 0.16:
            ops.append(f'close {i}')
        elif r < 0.25:
            ops.append(f'seek {i} {pick_pos(rng, 0x3800)}')
        elif r < 0.55:
            ops.append(f'eat {i} {rng.choice((0x10, 0x40, 0x100, 0x1000, 0x7fffffff, -1, 0, rng.randrange(0x400)))}')
        elif r < 0.68:
            ops.append(marks(rng, i))
        elif r < 0.82:
            count = rng.choice((0, 1, 2, 4, 8, 8, 300))
            rows = [f'{pick_pos(rng, 0x1000)} {rng.choice((1, 1, 1, 0, 2))} {rng.randrange(4)} {rng.randrange(1000)}'
                    for _ in range(count)]
            ops.append(f'tag {i} {count} ' + ' '.join(rows))
        elif r < 0.86:
            ops.append(f'markon {i} {rng.randrange(4)} {rng.choice((0, 1, 2))}')
        elif r < 0.90:
            ops.append(f'tagon {i} {rng.randrange(4)} {rng.choice((0, 1, 2))}')
        elif r < 0.997:
            ops.append(f'get {i} {rng.choice((0, 1, 2, 8))} {rng.choice((0, 0x10, 0x100, 0x1000))}')
        else:
            ops.append('exit')
    return ops


def main(argv):
    scripts = int(argv[1]) if len(argv) > 1 else 40
    n = int(argv[2]) if len(argv) > 2 else 500
    exe = build()
    rng = random.Random(1)
    bad = total = cbs = full = ends = 0
    for s in range(scripts):
        ops = script(rng, n)
        (WORK / 'ops.txt').write_text('\n'.join(ops) + '\n')
        out = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                             check=True).stdout.splitlines()
        rom = Rom()
        for op, host in zip(ops, out):
            got = rom.run(op)
            total += 1
            cbs += got.count(' cb ')
            ends += got.split(' | ')[-1].split()[1:2] == ['1']
            full += ' print' in got
            if got != host:
                bad += 1
                if bad <= 6:
                    print(f'{op[:120]}\n  rom  {got[:240]}\n  host {host[:240]}')
        if len(out) != len(ops):
            print('host stopped early', len(out), len(ops))
            bad += 1
    print(f'{scripts} scripts, {total} ops, {cbs} callbacks, {full} opens with no slot, {ends} ops on an ended stream, {bad} differences')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
