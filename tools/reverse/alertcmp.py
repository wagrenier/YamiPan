#!/usr/bin/env python3
"""game/stagealert.cpp against the ROM (offline).

python alertcmp.py [scripts] [ops] [--plant]

Random scripts on the ROM's own code (eerun.py, 0x263b90..0x2640e8) and on the host's in a small exe
compiled from src/game/stagealert.cpp (work/alertcmp/): show, update, flash, cry, draw, the option,
the rumble list there or not, the clock's delta (a menu's 0 among them), and now and then every field
forced, unsigned values past the sign bit and states out of range included. RumbleList, StageFade,
playQuestionVoice and the clock are stand-ins on both sides and log their calls, with float arguments
as bits. After each op both print the answer, the fields and the log; the first differences and a
count go out. --plant compiles the host with two bugs in it, to prove the tool sees them.
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
WORK = HERE / 'work' / 'alertcmp'
SRC = yamipan.REPO / 'src'

BEFORE, DRAW, UPDATE, FLASH, CRY, SHOW, DTOR, CTOR = (
    0x263b90, 0x263ba0, 0x263c10, 0x263ed0, 0x263f10, 0x263f80, 0x264000, 0x264070)
SCENE_POINTER = eerun.GP - 0x7a7c
OPTIONS = 0x5a0000 - 0x74d0 + 0x4d0
PACKET, TEXLIST = 0x1111, 0x2222

DELTAS = [eerun.b32(x) for x in (1 / 60, 1 / 30, 1 / 50, 1 / 25, 0.0, 1 / 300, 7 / 300, 0.5, 1.0)]
BIG = (0, 1, 2, 29, 30, 31, 0x7fffffff, 0x80000000, 0x80000001, 0xffffffff, 0xfffffff0, 0x1000000, 0x1000001)
PLANTS = (('ending + m_count >= kEndingFrames30', 'ending + m_count > kEndingFrames30'),
          ('who == m_shown || m_wanted != m_shown', 'who != m_shown || m_wanted != m_shown'))

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
#include "game/stagealert.h"
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/rumblelist.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#undef private

static std::string gLog;
static std::set<std::string> gRumble;
static GameOptions gOptions;
static unsigned gDelta;
StageScene *gStageScene;

static void logf(const char *format, unsigned a = 0, unsigned b = 0, unsigned c = 0, unsigned d = 0, unsigned e = 0)
{
    char line[128];
    sprintf(line, format, a, b, c, d, e);
    gLog += line;
}
static unsigned bitsOf(float value) { unsigned u; memcpy(&u, &value, 4); return u; }
static unsigned low(const void *p) { return (unsigned)(uintptr_t)p; }

GameOptions *getGameOptions() { return &gOptions; }
float Clock::getDeltaSeconds() { logf(" T"); float f; memcpy(&f, &gDelta, 4); return f; }
int RumbleList::isPlaying(const char *name) { gLog += " has "; gLog += name; return (int)gRumble.count(name); }
void RumbleList::addNamed(const char *name, StageObject *source)
{
    gLog += " add "; gLog += name; logf(" %x", low(source));
    gRumble.insert(name);
}
int RumbleList::removeNamed(const char *name) { gLog += " del "; gLog += name; gRumble.erase(name); return 1; }
StageFade::StageFade(int x, int y, int w, int h, Clock *clock) : m_x(x), m_y(y), m_w(w), m_h(h)
{
    logf(" new %x %x %x %x %x", x, y, w, h, low(clock));
}
StageFade::~StageFade() { logf(" gone"); }
void StageFade::start(float seconds, float target, float from)
{
    logf(" start %x %x %x", bitsOf(seconds), bitsOf(target), bitsOf(from));
    m_mode = StageFade_Running;
}
void StageFade::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    logf(" draw %x %x", low(packet), low(texList));
    m_mode = StageFade_Done;
}
void StageCharacter::playQuestionVoice() { logf(" voice %x", low(this)); }

int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    char op[16];
    gStageScene = (StageScene *)calloc(1, sizeof(StageScene));
    gStageScene->clock = (Clock *)(uintptr_t)0x60;
    StageAlert *alert = nullptr;
    while (fscanf(f, "%15s", op) == 1) {
        gLog.clear();
        unsigned a = 0, b = 0, c = 0, d = 0, e = 0;
        int answer = 0;
        if (!strcmp(op, "new")) { gRumble.clear(); alert = new StageAlert(); }
        else if (!strcmp(op, "delete")) { delete alert; alert = nullptr; }
        else if (!strcmp(op, "before")) alert->beforeFileEvent();
        else if (!strcmp(op, "draw")) alert->draw((sceVif1Packet *)(uintptr_t)0x1111, (TexAreaList *)(uintptr_t)0x2222);
        else if (!strcmp(op, "update")) { fscanf(f, "%x", &gDelta); answer = alert->update(); }
        else if (!strcmp(op, "flash")) answer = alert->flash();
        else if (!strcmp(op, "cry")) { fscanf(f, "%x", &a); answer = alert->cry((StageCharacter *)(uintptr_t)a); }
        else if (!strcmp(op, "show")) { fscanf(f, "%x", &a); alert->show((StageObject *)(uintptr_t)a); }
        else if (!strcmp(op, "option")) { fscanf(f, "%x", &a); gOptions.alertMode = (int)a; }
        else if (!strcmp(op, "rumble")) { fscanf(f, "%x", &a); gStageScene->rumble = (RumbleList *)(uintptr_t)a; }
        else if (!strcmp(op, "mode")) { fscanf(f, "%x", &a); alert->m_fade->m_mode = (int)a; }
        else if (!strcmp(op, "force")) {
            fscanf(f, "%x %x %x %x %x", &a, &b, &c, &d, &e);
            alert->m_state = (int)a; alert->m_frames = b; alert->m_count = c;
            alert->m_wanted = (StageObject *)(uintptr_t)d; alert->m_shown = (StageObject *)(uintptr_t)e;
        }
        if (alert == nullptr) { printf("gone |%s\n", gLog.c_str()); continue; }
        printf("%d %x %x %x %x %x %x %x |%s |", answer, (unsigned)alert->m_state, alert->m_frames, alert->m_count,
               low(alert->m_wanted), low(alert->m_shown), alert->m_fade->m_colour, (unsigned)alert->m_fade->m_mode,
               gLog.c_str());
        for (const std::string &name : gRumble) printf(" %s", name.c_str());
        printf("\n");
    }
    return 0;
}
'''


def build(plant):
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    source = (SRC / 'game' / 'stagealert.cpp').read_text(encoding='utf-8')
    if plant:
        for bug, wrong in PLANTS:
            assert bug in source, bug
            source = source.replace(bug, wrong)
    (WORK / 'stagealert.cpp').write_text(source, encoding='utf-8')
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC),
                    '-I', str(SRC / 'sdk'), str(WORK / 'host.cpp'), str(WORK / 'stagealert.cpp'), '-o', str(exe)],
                   check=True)
    return exe


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        self.log = []
        self.rumble = set()
        self.delta = 0
        self.alert = None
        self.scene = ee.alloc(0x130)
        self.table = ee.alloc(0x10)
        ee.wr(SCENE_POINTER, 4, self.scene)
        ee.wr(self.scene + 0x60, 4, 0x60)
        ee.wr(self.table + 8, 4, 0xfff000)
        ee.wr(self.table + 0xc, 4, 0xfff010)
        ee.hooks[0xfff000] = self.fade_gone
        ee.hooks[0xfff010] = self.fade_draw
        for name, fn in (('Clock::getDeltaSeconds', self.delta_seconds), ('RumbleList::isPlaying', self.is_playing),
                         ('RumbleList::addNamed', self.add), ('RumbleList::removeNamed', self.remove),
                         ('StageFade::StageFade', self.fade_new), ('StageFade::start', self.fade_start),
                         ('StageCharacter::playQuestionVoice', self.voice)):
            ee.hook(name, fn)
        ee.hook('operator_new', self.new)

    def name(self, at):
        at &= 0xffffffff
        return bytes(self.ee.m[at:self.ee.m.index(0, at)]).decode('ascii')

    def new(self, ee):
        assert ee.g('a0') == 0x3c
        return ee.alloc(0x3c)

    def delta_seconds(self, ee):
        self.log.append('T')
        ee.f[0] = self.delta
        return 0

    def is_playing(self, ee):
        name = self.name(ee.g('a1'))
        self.log.append(f'has {name}')
        return 1 if name in self.rumble else 0

    def add(self, ee):
        name = self.name(ee.g('a1'))
        self.log.append(f'add {name} {ee.g("a2") & 0xffffffff:x}')
        self.rumble.add(name)
        return 0

    def remove(self, ee):
        name = self.name(ee.g('a1'))
        self.log.append(f'del {name}')
        self.rumble.discard(name)
        return 1

    def fade_new(self, ee):
        fade = ee.g('a0') & 0xffffffff
        self.log.append('new ' + ' '.join(f'{ee.g(r) & 0xffffffff:x}' for r in ('a1', 'a2', 'a3', 't0', 't1')))
        ee.wr(fade + 4, 4, self.table)
        ee.wr(fade + 0x18, 4, 0)
        ee.wr(fade + 0x34, 4, 0)
        return fade

    def fade_gone(self, ee):
        assert ee.g('a1') & 0xffffffff == 1
        self.log.append('gone')
        return 0

    def fade_draw(self, ee):
        self.log.append(f'draw {ee.g("a1") & 0xffffffff:x} {ee.g("a2") & 0xffffffff:x}')
        ee.wr((ee.g('a0') & 0xffffffff) + 0x34, 4, 2)
        return 0

    def fade_start(self, ee):
        self.log.append(f'start {ee.f[12]:x} {ee.f[13]:x} {ee.f[14]:x}')
        ee.wr((ee.g('a0') & 0xffffffff) + 0x34, 4, 1)
        return 0

    def voice(self, ee):
        self.log.append(f'voice {ee.g("a0") & 0xffffffff:x}')
        return 0

    def run(self, op):
        w = op.split()
        self.log = []
        name, ee = w[0], self.ee
        at = self.alert
        args = [int(x, 16) for x in w[1:]]
        answer = 0
        if name == 'new':
            self.rumble = set()
            self.alert = at = ee.alloc(0x1c)
            ee.write(at, bytes([0xcc]) * 0x1c)
            ee.call(CTOR, at)
        elif name == 'delete':
            ee.call(DTOR, at, 1)
            self.alert = None
            return 'gone |' + ''.join(' ' + x for x in self.log)
        elif name == 'before':
            ee.call(BEFORE, at)
        elif name == 'draw':
            ee.call(DRAW, at, PACKET, TEXLIST)
        elif name == 'update':
            self.delta = args[0]
            answer = ee.call(UPDATE, at)
        elif name == 'flash':
            answer = ee.call(FLASH, at)
        elif name == 'cry':
            answer = ee.call(CRY, at, args[0])
        elif name == 'show':
            ee.call(SHOW, at, args[0])
        elif name == 'option':
            ee.wr(OPTIONS + 0x10, 4, args[0])
        elif name == 'rumble':
            ee.wr(self.scene + 0x30, 4, args[0])
        elif name == 'mode':
            ee.wr(ee.u32(at + 0x14) + 0x34, 4, args[0])
        elif name == 'force':
            for i, offset in enumerate((0, 4, 8, 0xc, 0x10)):
                ee.wr(at + offset, 4, args[i])
        fade = ee.u32(at + 0x14)
        state = ' '.join(f'{ee.u32(at + o):x}' for o in (0, 4, 8, 0xc, 0x10))
        log = ''.join(' ' + x for x in self.log)
        rumble = ''.join(' ' + x for x in sorted(self.rumble))
        return f'{answer} {state} {ee.u32(fade + 0x18):x} {ee.u32(fade + 0x34):x} |{log} |{rumble}'


def script(rng, n):
    who = lambda: rng.choice((0x1000, 0x2000, 0x3000))
    delta = rng.choice(DELTAS)
    ops = ['new', 'rumble 30', 'option 2']
    while len(ops) < n:
        kind = rng.random()
        if kind < 0.45:
            # A script asking every frame, then stopping.
            actor = who()
            for _ in range(rng.randrange(1, 40)):
                if rng.random() < 0.9:
                    ops.append(f'show {actor:x}')
                if rng.random() < 0.2:
                    ops.append(f'show {who():x}')
                ops.append(f'update {delta:x}')
            for _ in range(rng.randrange(1, 6)):
                ops.append(f'update {delta:x}')
        elif kind < 0.6:
            delta = rng.choice(DELTAS)
            ops.append(f'update {delta:x}')
        elif kind < 0.7:
            ops += ['flash', 'draw', 'draw']
        elif kind < 0.78:
            ops += [f'cry {who():x}', 'draw']
        elif kind < 0.84:
            ops.append(f'option {rng.randrange(3):x}')
        elif kind < 0.9:
            ops.append(f'rumble {rng.choice((0, 0x30, 0x30)):x}')
        elif kind < 0.94:
            ops.append(f'mode {rng.randrange(3):x}')
        elif kind < 0.96:
            ops.append('before')
        else:
            ops.append('force ' + ' '.join(f'{x:x}' for x in (
                rng.randrange(5), rng.choice(BIG), rng.choice(BIG), rng.choice((0, who())), rng.choice((0, who())))))
            ops.append(f'update {delta:x}')
    ops.append('delete')
    return ops


def main(argv):
    plant = '--plant' in argv
    numbers = [int(x) for x in argv[1:] if not x.startswith('--')]
    scripts = numbers[0] if numbers else 40
    n = numbers[1] if len(numbers) > 1 else 1500
    exe = build(plant)
    rng = random.Random(0x263b90)
    bad = total = started = 0
    for s in range(scripts):
        ops = script(rng, n)
        (WORK / 'ops.txt').write_text('\n'.join(ops) + '\n')
        out = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                             check=True).stdout.splitlines()
        rom = Rom()
        for op, host in zip(ops, out):
            got = rom.run(op)
            total += 1
            if ' add ALERT' in got:
                started += 1
            if got != host:
                bad += 1
                if bad <= 6:
                    print(f'{op}\n  rom  {got[:200]}\n  host {host[:200]}')
        if len(out) != len(ops):
            print('host stopped early', len(out), len(ops))
            bad += 1
    print(f'{scripts} scripts, {total} ops, ALERT started {started} times, {bad} differences')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
