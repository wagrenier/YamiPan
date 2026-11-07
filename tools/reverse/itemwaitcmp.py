#!/usr/bin/env python3
"""StageCharacter::putSmoke and throwCarried against the ROM (offline).

python itemwaitcmp.py [scripts] [ops] [--plant]

The two script waits of game/playermotion.cpp (the block between "itemwait begin" and "itemwait end")
beside the ROM's 0x218850 and 0x218b50 on eerun.py. Random op scripts call them frame after frame while
the motion history moves under them: the right motion started, ended, given way, pushed out of the ring,
the wrong one on top, frames near the 30 frame limit and past bit 28, the yield forced to any state.
findCarried, isControlled, the mission's frame count and the brain's askThrow are stand-ins on both sides
and log their calls; askNothing, wantThrow and scriptYield are the ROM's own. After each op both print the
answer, the yield, the wants and the log. --plant compiles the host with three bugs, to prove it sees them.

Not compared: a null gRunningScript or an empty history read through address 0: the ROM reads low memory
there and the host takes it as nothing (an empty history is in the scripts, low memory reads as 0).
"""
import os
import random
import subprocess
import sys
from pathlib import Path

import eerun
import yamipan

HERE = Path(__file__).parent
WORK = HERE / 'work' / 'itemwaitcmp'
SRC = yamipan.REPO / 'src'

PUT, THROW, FIND, CONTROLLED, FRAMES, ASK = 0x218850, 0x218b50, 0x216310, 0x215270, 0x196340, 0x1db6f0
RUNNER_POINTER = eerun.GP - 0x7a58
BRAIN = 0x5000
NAMES = ('I_HATUEN', 'I_KANTSUME', 'I_NONE')
ITEMS = (0x1000, 0x2000, 0)
PICK_LOW, THROW_MOTION = 0x12, 0x9d
PLANTS = (('since + kItemWaitFrames <', 'since + kItemWaitFrames <='),
          ('? ItemWait_Done : ItemWait_Failed', '? ItemWait_Done : ItemWait_Asked'),
          ('since = (u_int)gStageScene->getMissionFrames();', 'since = 0; gStageScene->getMissionFrames();'))

HOST = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#define private public
#define protected public
#include "game/gamehooks.h"
#include "game/characterbrain.h"
#include "game/characterroute.h"
#include "game/scriptrunner.h"
#include "game/stageitem.h"
#include "game/stagescene.h"
#undef private
#undef protected

static std::string gLog;
static std::map<std::string, unsigned> gCarried;
static unsigned gFrame, gControlled, gAsk;
StageScene *gStageScene;
ScriptRunner *gRunningScript;

static void logf(const char *format, unsigned a = 0, unsigned b = 0, unsigned c = 0, unsigned d = 0,
                 unsigned e = 0, unsigned f = 0)
{
    char line[128];
    sprintf(line, format, a, b, c, d, e, f);
    gLog += line;
}
static unsigned low(const void *p) { return (unsigned)(uintptr_t)p; }

void scriptYield(int a, int b) { gRunningScript->m_yieldA = a; gRunningScript->m_yieldB = b; }
int StageScene::getMissionFrames() { logf(" frames"); return (int)gFrame; }
StageObject *StageCharacter::findCarried(const char *name)
{
    gLog += " find "; gLog += name;
    return (StageObject *)(uintptr_t)gCarried[name];
}
bool StageCharacter::isControlled() const { logf(" controlled"); return gControlled != 0; }
bool CharacterBrain::askThrow(void *item, void *vector, bool retry)
{
    unsigned v[4];
    memcpy(v, vector, 16);
    logf(" ask %x %x %x %x %x %x", low(this), low(item), v[0], v[1], v[2], v[3]);
    logf(" %x", retry);
    return gAsk != 0;
}

constexpr int kMotionPickLow = 0x12;
constexpr int kMotionThrow = 0x9d;
#include "block.inc"

int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    char op[16], name[32];
    StageCharacter *ch = (StageCharacter *)calloc(1, sizeof(StageCharacter));
    gRunningScript = (ScriptRunner *)calloc(1, sizeof(ScriptRunner));
    gStageScene = (StageScene *)calloc(1, sizeof(StageScene));
    ch->m_brain = (CharacterBrain *)(uintptr_t)0x5000;
    while (fscanf(f, "%15s", op) == 1) {
        gLog.clear();
        unsigned a = 0, b = 0, c = 0, d = 0, e = 0;
        int answer = 0;
        if (!strcmp(op, "put")) { fscanf(f, "%x", &a); answer = ch->putSmoke(a != 0); }
        else if (!strcmp(op, "throw")) {
            unsigned v[4];
            fscanf(f, "%31s %x %x %x %x %x", name, &v[0], &v[1], &v[2], &v[3], &a);
            alignas(16) unsigned aligned[4];
            memcpy(aligned, v, 16);
            answer = ch->throwCarried(name, (const float *)aligned, a != 0);
        }
        else if (!strcmp(op, "env")) { fscanf(f, "%x %x %x", &gFrame, &gControlled, &gAsk); }
        else if (!strcmp(op, "carry")) { fscanf(f, "%31s %x", name, &a); gCarried[name] = a; }
        else if (!strcmp(op, "yield")) {
            fscanf(f, "%x %x", &a, &b);
            gRunningScript->m_yieldA = (int)a; gRunningScript->m_yieldB = (int)b;
        }
        else if (!strcmp(op, "hist")) {
            CharacterBlock7c8 &h = ch->m_block7c8;
            fscanf(f, "%x %x", &a, &b);
            h.next = (int)a; h.count = b;
            for (int i = 0; i < kMotionHistory; i++) {
                fscanf(f, "%x %x", &c, &d);
                h.entries[i].id = (int)c; h.entries[i].events = d;
            }
        }
        else if (!strcmp(op, "clear")) {
            ch->m_wants.putItem = 0; ch->m_wants.throwItem = 0; ch->m_putItem = nullptr; ch->m_throwItem = nullptr;
            memset(ch->m_wants.throwVector, 0, 16);
        }
        unsigned v[4];
        memcpy(v, ch->m_wants.throwVector, 16);
        printf("%d %x %x %x %x %x %x %x %x %x %x |%s\n", answer, (unsigned)gRunningScript->m_yieldA,
               (unsigned)gRunningScript->m_yieldB, ch->m_wants.putItem, low(ch->m_putItem), ch->m_wants.throwItem,
               low(ch->m_throwItem), v[0], v[1], v[2], v[3], gLog.c_str());
    }
    return 0;
}
'''


def block(plant):
    text = (SRC / 'game' / 'playermotion.cpp').read_text(encoding='utf-8')
    at = text.index('// itemwait begin')
    text = text[at:text.index('// itemwait end')]
    if plant:
        for bug, wrong in PLANTS:
            assert bug in text, bug
            text = text.replace(bug, wrong)
    return text


def build(plant):
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    (WORK / 'block.inc').write_text(block(plant), encoding='utf-8')
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC), '-I',
                    str(SRC / 'sdk'), str(WORK / 'host.cpp'), '-o', str(exe)], check=True)
    return exe


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        self.log = []
        self.frame = self.controlled = self.ask = 0
        self.carried = {}
        self.ch = ee.alloc(0x1000)
        self.runner = ee.alloc(0x100)
        self.vector = ee.alloc(0x10)
        self.text = {n: ee.alloc(0x20) for n in NAMES}
        for n, at in self.text.items():
            ee.write(at, n.encode() + b'\0')
        ee.wr(RUNNER_POINTER, 4, self.runner)
        ee.wr(self.ch + 0x220, 4, BRAIN)
        ee.hooks[FIND] = self.find
        ee.hooks[CONTROLLED] = self.is_controlled
        ee.hooks[FRAMES] = self.frames
        ee.hooks[ASK] = self.ask_throw

    def find(self, ee):
        at = ee.g('a1') & 0xffffffff
        name = bytes(ee.m[at:ee.m.index(0, at)]).decode('ascii')
        self.log.append(f'find {name}')
        return self.carried.get(name, 0)

    def is_controlled(self, ee):
        self.log.append('controlled')
        return self.controlled

    def frames(self, ee):
        self.log.append('frames')
        return self.frame | 0xffffffff00000000 if self.frame >> 31 else self.frame

    def ask_throw(self, ee):
        v = [ee.u32((ee.g('a2') & 0xffffffff) + 4 * i) for i in range(4)]
        words = (ee.g('a0') & 0xffffffff, ee.g('a1') & 0xffffffff, *v)
        self.log.append('ask ' + ' '.join(f'{x:x}' for x in words) + f' {ee.g("a3") & 0xffffffff:x}')
        return self.ask

    def run(self, op):
        w = op.split()
        self.log = []
        name, ee, ch = w[0], self.ee, self.ch
        answer = 0
        if name == 'put':
            answer = ee.call(PUT, ch, int(w[1], 16))
        elif name == 'throw':
            for i in range(4):
                ee.wr(self.vector + 4 * i, 4, int(w[2 + i], 16))
            answer = ee.call(THROW, ch, self.text[w[1]], self.vector, int(w[6], 16))
        elif name == 'env':
            self.frame, self.controlled, self.ask = (int(x, 16) for x in w[1:])
        elif name == 'carry':
            self.carried[w[1]] = int(w[2], 16)
        elif name == 'yield':
            ee.wr(self.runner + 0xac, 4, int(w[1], 16))
            ee.wr(self.runner + 0xb0, 4, int(w[2], 16))
        elif name == 'hist':
            ee.wr(ch + 0x7f8, 4, int(w[1], 16))
            ee.wr(ch + 0x7fc, 4, int(w[2], 16))
            for i in range(4):
                ee.wr(ch + 0x7c8 + 0xc * i, 4, int(w[3 + 2 * i], 16))
                ee.wr(ch + 0x7cc + 0xc * i, 4, int(w[4 + 2 * i], 16))
        elif name == 'clear':
            ee.wr(ch + 0x2d2, 1, 0)
            ee.wr(ch + 0x2d4, 4, 0)
            ee.wr(ch + 0x2da, 1, 0)
            ee.wr(ch + 0x2dc, 4, 0)
            for i in range(4):
                ee.wr(ch + 0x2e0 + 4 * i, 4, 0)
        fields = [ee.u32(self.runner + 0xac), ee.u32(self.runner + 0xb0), ee.rd(ch + 0x2d2, 1), ee.u32(ch + 0x2d4),
                  ee.rd(ch + 0x2da, 1), ee.u32(ch + 0x2dc)] + [ee.u32(ch + 0x2e0 + 4 * i) for i in range(4)]
        log = ''.join(' ' + x for x in self.log)
        return f'{answer & 0xff} ' + ' '.join(f'{x:x}' for x in fields) + f' |{log}'


class History:
    """The ring as noteMotion keeps it; each change goes out as a hist op."""
    def __init__(self):
        self.next = self.count = 0
        self.rows = [[0, 0] for _ in range(4)]

    def start(self, motion):
        self.rows[self.next] = [motion, 1]
        self.next = (self.next + 1) & 3
        self.count = min(self.count + 1, 4)

    def mark(self, motion, event):
        for i in range(self.count):
            row = self.rows[(self.next - i + 3) & 3]
            if row[0] == motion:
                row[1] |= 1 << event
                return

    def op(self):
        return f'hist {self.next:x} {self.count:x} ' + ' '.join(f'{a:x} {b:x}' for a, b in self.rows)


def wait(rng, ops, frame):
    """One native from its first call to some end, the motion playing out beneath it."""
    throwing = rng.random() < 0.6
    motion = THROW_MOTION if throwing else PICK_LOW
    name = rng.choice(NAMES) if throwing else 'I_HATUEN'
    vector = [rng.choice((0, 0x3f800000, 0xc2c80000, rng.getrandbits(32))) for _ in range(4)]
    retry = rng.randrange(2)
    controlled = rng.randrange(2) if rng.random() < 0.6 else 1
    ask = rng.randrange(2)
    history = History()
    if rng.random() < 0.8:                      # older motions in the ring
        for _ in range(rng.randrange(1, 5)):
            history.start(rng.choice((0, 1, 0x20, motion)))
            history.mark(history.rows[(history.next + 3) & 3][0], rng.choice((2, 3, 4)))
    ops.append(history.op())
    ops.append('clear')
    start_at = rng.choice((1, 1, 2, 3, 28, 29, 30, 31, 32, 40))
    frame_step = rng.choice((1, 1, 1, 2))
    plays = rng.randrange(1, 12)
    ending = rng.random() < 0.7
    for i in range(rng.randrange(3, 60)):
        ops.append(f'env {frame & 0xffffffff:x} {controlled:x} {ask:x}')
        if throwing:
            ops.append(f'throw {name} ' + ' '.join(f'{x:x}' for x in vector) + f' {retry:x}')
        else:
            ops.append(f'put {retry:x}')
        frame += frame_step
        if rng.random() < 0.05:
            ask ^= 1
        if i + 1 == start_at:
            history.start(motion)
            ops.append(history.op())
        if i + 1 == start_at + plays:
            kind = rng.random()
            if ending:
                history.mark(motion, 2)
            if kind < 0.6:
                history.start(0)                # it gave way to the stand
            elif kind < 0.8:
                for _ in range(4):
                    history.start(rng.choice((0, 1, 2)))
            ops.append(history.op())
    return frame


def script(rng, n):
    ops = []
    frame = rng.choice((0, 5, 100, 3000, 0x0fffffe0, 0x10000000, 0x7fffff00, 0xffffff00))
    for name, item in zip(NAMES, ITEMS):
        ops.append(f'carry {name} {item:x}')
    while len(ops) < n:
        kind = rng.random()
        if kind < 0.85:
            ops.append(f'yield 0 0')
            frame = wait(rng, ops, frame)
        elif kind < 0.92:
            ops.append(f'carry I_HATUEN {rng.choice((0, 0x1000)):x}')
        else:
            state = rng.randrange(16)
            since = rng.choice((0, frame & 0xfffffff, rng.getrandbits(28)))
            ops.append(f'yield {rng.choice((0, 0x104, 0x105, 1)):x} {(since << 4 | state):x}')
            ops.append(f'env {frame & 0xffffffff:x} 1 0')
            ops.append('put 1' if rng.random() < 0.5 else f'throw I_HATUEN 0 0 0 0 1')
    return ops


def main(argv):
    plant = '--plant' in argv
    numbers = [int(x) for x in argv[1:] if not x.startswith('--')]
    scripts = numbers[0] if numbers else 40
    n = numbers[1] if len(numbers) > 1 else 2000
    exe = build(plant)
    rng = random.Random(0x218850)
    bad = total = 0
    seen = {}
    for s in range(scripts):
        ops = script(rng, n)
        (WORK / 'ops.txt').write_text('\n'.join(ops) + '\n')
        out = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                             check=True).stdout.splitlines()
        rom = Rom()
        for op, host in zip(ops, out):
            got = rom.run(op)
            total += 1
            if op.startswith(('put', 'throw')):
                head = got.split()
                key = 'true' if head[0] == '1' else f'state {int(head[2], 16) & 0xf}' if head[1] != '0' else 'false'
                key = op.split()[0] + ' ' + key
                seen[key] = seen.get(key, 0) + 1
            if got != host:
                bad += 1
                if bad <= 6:
                    print(f'{op}\n  rom  {got[:200]}\n  host {host[:200]}')
        if len(out) != len(ops):
            print('host stopped early', len(out), len(ops))
            bad += 1
    print(f'{scripts} scripts, {total} ops, {bad} differences')
    print('  ' + ', '.join(f'{k}: {seen[k]}' for k in sorted(seen)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
