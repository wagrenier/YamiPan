#!/usr/bin/env python3
"""game/vehiclepose.cpp against the ROM (offline).

python vehiclecmp.py [scripts] [ops]

Nothing in a smoke run reaches CarPose or ElevatorPose yet (the car and elevator objects are not
written), so random scripts run on the ROM's own code (eerun.py) and on the host's in a small exe
compiled from src/game/vehiclepose.cpp and vu0math.cpp (work/vehiclecmp/):

- car: setDrive, drive (with four contacts, or null), getWheelPoints and followGround on a pose and
  twelve bones filled with random words; ModelPose and ModelPoseBone are stand-ins on both sides
  that log their calls (solve, settle, settleFixed, blendSolve, aimAlongAxis, quatFromAxisAngle,
  turnVector), so the log is compared with its float arguments as well.
- elevator: the buttons, reopenDoors, setScene and stepQueue in random order, from every state, with
  the log of playFile and playFileLayered (which motion, which flags).

After each op both print the fields as bits. Differences are counted by word, with the worst
distance in units in the last place. powf is Python's on the ROM's side. CarPose::update and ElevatorPose::update are not covered:
they are ModelPose::update's shape around these, and need real bones.
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
WORK = HERE / 'work' / 'vehiclecmp'
SRC = yamipan.REPO / 'src'

FOLLOW, DRIVE, POINTS, SET_DRIVE = 0x27de20, 0x27e650, 0x27ec80, 0x27ed70
STEP, REOPEN, RELEASE, IN_B3, IN_B1, FLOOR_B3, FLOOR_B1, SCENE, SET_FILE = (
    0x27f2b0, 0x27f5a0, 0x27f7d0, 0x27f880, 0x27f9a0, 0x27fac0, 0x27fc40, 0x27fdc0, 0x27ff10)

# (EE offset, words): the car's fields, in the order the host's harness reads and prints them.
CAR = ((0x30, 4), (0x40, 4), (0x90, 16), (0xd0, 4), (0x3c0, 16), (0x400, 4), (0x410, 4), (0x420, 4),
       (0x430, 4), (0x440, 4), (0x450, 6), (0x470, 16))
BONE = ((0x80, 16), (0x200, 4), (0x210, 4), (0x220, 4), (0x230, 4))
BONES = 12
CAR_WORDS = sum(n for _, n in CAR)
BONE_WORDS = sum(n for _, n in BONE)
# ROOT, BASE, BODY, ACCEL 3..6, WHEEL 7..10, STEERING 11
PARENT = (-1, 0, 1, 2, 2, 2, 2, 3, 4, 5, 6, 2)

HOST = r'''
#include <stdio.h>
#include <string.h>
#include <string>
#include <stdint.h>
#define private public
#include "game/vehiclepose.h"
#undef private

static std::string gLog;
static unsigned gTurn[4];
static int gEnding;
static CarPose *gCar;
static ModelPoseBone *gBones;

static unsigned bitsOf(float v) { unsigned b; memcpy(&b, &v, 4); return b; }
static float word(FILE *f) { unsigned b; fscanf(f, "%x", &b); float v; memcpy(&v, &b, 4); return v; }
static void readWords(FILE *f, float *to, int n) { for (int i = 0; i < n; i++) to[i] = word(f); }
static void putWords(const float *from, int n) { for (int i = 0; i < n; i++) printf(" %x", bitsOf(from[i])); }
static void log(const char *text) { gLog += text; }
static void logf(const char *fmt, unsigned a, unsigned b = 0, unsigned c = 0, unsigned d = 0, unsigned e = 0,
                 unsigned g = 0, unsigned h = 0)
{
    char line[160];
    snprintf(line, sizeof(line), fmt, a, b, c, d, e, g, h);
    gLog += line;
}
static int index(const ModelPoseBone *bone) { return bone == nullptr ? -1 : (int)(bone - gBones); }

ModelPoseBone::ModelPoseBone() {}
ModelPose::ModelPose() {}
ModelPose::~ModelPose() {}
int ModelPose::load(void *, int, const char *, u_int) { return 0; }
void ModelPose::update(float, bool) {}
void ModelPose::solve(int slot, int count, int flagged) { logf(" V %d %d %d", slot, count, flagged); }
int ModelPose::playFile(void *file, u_int flags, int groups)
{
    logf(" P %x %x %x", (unsigned)(uintptr_t)file, flags, groups);
    return 0;
}
int ModelPose::playFileLayered(void *file, u_int flags, int groups)
{
    logf(" L %x %x %x", (unsigned)(uintptr_t)file, flags, groups);
    return 0;
}
bool ModelPose::isBoneEnding(const char *) const { return gEnding != 0; }
bool ModelPoseBone::advance(float) { return false; }
bool ModelPoseBone::interpolate(float, int, int, int) { return false; }
bool ModelPoseBone::buildMatrix(float, int, int) { return false; }
void ModelPoseBone::settle(float rate, int count) { logf(" S %d %x %d", index(this), bitsOf(rate), count); }
void ModelPoseBone::settleFixed(float rate, int count) { logf(" F %d %x %d", index(this), bitsOf(rate), count); }
void ModelPoseBone::blendSolve(float t, int slot) { logf(" B %d %x %d", index(this), bitsOf(t), slot); }
bool ModelPoseBone::aimAlongAxis(float limit, const sceVu0FVECTOR target, int axis)
{
    logf(" A %d %x %x %x %x %x %d", index(this), bitsOf(limit), bitsOf(target[0]), bitsOf(target[1]),
         bitsOf(target[2]), bitsOf(target[3]), axis);
    return false;
}
void ModelPoseBone::turnVector(sceVu0FVECTOR out, const sceVu0FVECTOR in) const
{
    logf(" T %d %x %x %x %x", index(this), bitsOf(in[0]), bitsOf(in[1]), bitsOf(in[2]), bitsOf(in[3]));
    memcpy(out, gTurn, 16);
}
void quatFromAxisAngle(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    int at = -1;
    for (int i = 0; i < 12; i++) {
        if (out == gBones[i].rotation[PoseSlot_Solve])
            at = i;
    }
    logf(" Q %d %x %x %x %x", at, bitsOf(in[0]), bitsOf(in[1]), bitsOf(in[2]), bitsOf(in[3]));
}

static void readCar(FILE *f)
{
    CarPose *c = gCar;
    readWords(f, c->rotation, 4);
    readWords(f, c->position, 4);
    readWords(f, &c->placement[0][0], 16);
    readWords(f, c->rootOffset, 4);
    readWords(f, &c->contact[0][0], 16);
    readWords(f, c->lastVelocity, 4);
    readWords(f, c->drift, 4);
    readWords(f, c->tilt, 4);
    readWords(f, c->wheelAngle, 4);
    readWords(f, c->_440, 4);
    readWords(f, &c->speed, 6);
    readWords(f, &c->bodyMatrix[0][0], 16);
}
static void putCar()
{
    CarPose *c = gCar;
    putWords(c->rotation, 4);
    putWords(c->position, 4);
    putWords(&c->placement[0][0], 16);
    putWords(c->rootOffset, 4);
    putWords(&c->contact[0][0], 16);
    putWords(c->lastVelocity, 4);
    putWords(c->drift, 4);
    putWords(c->tilt, 4);
    putWords(c->wheelAngle, 4);
    putWords(c->_440, 4);
    putWords(&c->speed, 6);
    putWords(&c->bodyMatrix[0][0], 16);
    putWords(gBones[0].rawPosition, 4);
    putWords(gBones[0].lastRawPosition, 4);
}

int main(int, char **argv)
{
    static const int parent[12] = { -1, 0, 1, 2, 2, 2, 2, 3, 4, 5, 6, 2 };
    FILE *f = fopen(argv[1], "r");
    char op[16];
    gCar = new CarPose;
    ElevatorPose *e = new ElevatorPose;
    gBones = new ModelPoseBone[12];
    for (int i = 0; i < 12; i++)
        gBones[i].parent = parent[i] < 0 ? nullptr : &gBones[parent[i]];
    gCar->rootBone = &gBones[0];
    gCar->bones = gBones;
    gCar->baseBone = &gBones[1];
    gCar->bodyBone = &gBones[2];
    for (int i = 0; i < 4; i++) {
        gCar->accelBones[i] = &gBones[3 + i];
        gCar->wheelBones[i] = &gBones[7 + i];
    }
    gCar->steeringBone = &gBones[11];
    for (int i = 0; i < kElevatorMotions; i++)
        e->motions[i] = (void *)(uintptr_t)(0x1000 + i);

    bool car = true;
    while (fscanf(f, "%15s", op) == 1) {
        gLog.clear();
        car = true;
        int n;
        if (!strcmp(op, "setup")) {
            readCar(f);
            for (int i = 0; i < 12; i++) {
                ModelPoseBone *b = &gBones[i];
                readWords(f, &b->matrix[0][0], 16);
                readWords(f, b->worldPosition, 4);
                readWords(f, b->lastWorldPosition, 4);
                readWords(f, b->rawPosition, 4);
                readWords(f, b->lastRawPosition, 4);
            }
        } else if (!strcmp(op, "set")) {
            float a = word(f), b = word(f), c = word(f);
            gCar->setDrive(a, b, c);
        } else if (!strcmp(op, "drive")) {
            float rate = word(f);
            for (int i = 0; i < 4; i++) fscanf(f, "%x", &gTurn[i]);
            sceVu0FVECTOR c[4];
            readWords(f, &c[0][0], 16);
            gCar->drive(rate, c[0], c[1], c[2], c[3]);
        } else if (!strcmp(op, "drive0")) {
            gCar->drive(word(f), nullptr, nullptr, nullptr, nullptr);
        } else if (!strcmp(op, "ground")) {
            for (int i = 0; i < 4; i++) fscanf(f, "%x", &gTurn[i]);
            gCar->followGround();
        } else if (!strcmp(op, "points")) {
            sceVu0FVECTOR c[4];
            gCar->getWheelPoints(c[0], c[1], c[2], c[3]);
            for (int i = 0; i < 16; i++)
                logf(" %x", bitsOf(c[0][i]));
        } else {
            car = false;
            int answer = 0;
            if (!strcmp(op, "reopen")) e->reopenDoors();
            else if (!strcmp(op, "release")) answer = e->release();
            else if (!strcmp(op, "in3")) answer = e->callInsideB3();
            else if (!strcmp(op, "in1")) answer = e->callInsideB1();
            else if (!strcmp(op, "floor3")) answer = e->callFloorB3();
            else if (!strcmp(op, "floor1")) answer = e->callFloorB1();
            else if (!strcmp(op, "scene")) { fscanf(f, "%d", &n); answer = e->setScene(n); }
            else if (!strcmp(op, "file")) { fscanf(f, "%d", &n); e->setMotionFile(e->motions[n], n); }
            else if (!strcmp(op, "step")) {
                fscanf(f, "%d", &gEnding);
                e->timer = word(f);
                e->stepQueue();
            }
            printf("%d %x %x", answer, e->state, bitsOf(e->timer));
            for (int i = 0; i < 16; i++) printf(" %x", (unsigned)e->queue[i]);
            printf(" %x", (unsigned)e->queueStep);
        }
        if (car)
            putCar();
        printf(" |%s\n", gLog.c_str());
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
                    str(WORK / 'host.cpp'), str(SRC / 'game' / 'vehiclepose.cpp'),
                    str(SRC / 'game' / 'vu0math.cpp'), '-o', str(exe)], check=True)
    return exe


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        self.log = []
        self.turn = [0, 0, 0, 0]
        self.ending = 0
        self.car = ee.alloc(0x4e0)
        self.bones = ee.alloc(0x300 * BONES)
        self.elevator = ee.alloc(0x460)
        self.scratch = ee.alloc(0x80)
        for i, parent in enumerate(PARENT):
            ee.wr(self.bones + i * 0x300 + 0xc, 4, 0 if parent < 0 else self.bones + parent * 0x300)
        ee.wr(self.car + 0x15c, 4, self.bones)
        ee.wr(self.car + 0x168, 4, self.bones)
        for i in range(BONES - 1):
            ee.wr(self.car + 0x4b0 + i * 4, 4, self.bones + (i + 1) * 0x300)
        for i in range(18):
            ee.wr(self.elevator + 0x3c0 + i * 4, 4, 0x1000 + i)
        ee.wr(self.elevator + 0x40c, 4, eerun.b32(600.0))
        for name, fn in (('ModelPose::solve', self.solve), ('ModelPose::playFile', self.play('P')),
                         ('ModelPose::playFileLayered', self.play('L')),
                         ('ModelPose::isBoneEnding', lambda e: self.ending),
                         ('ModelPoseBone::settle', self.bone_rate('S')),
                         ('ModelPoseBone::settleFixed', self.bone_rate('F')),
                         ('ModelPoseBone::blendSolve', self.bone_rate('B')),
                         ('ModelPoseBone::aimAlongAxis', self.aim), ('ModelPoseBone::turnVector', self.turn_vector),
                         ('quatFromAxisAngle', self.quat), ('powf', self.powf)):
            ee.hook(name, fn)

    def fresh_elevator(self):
        e = self.elevator
        self.ee.write(e + 0x408, bytes(0x4c))
        self.ee.wr(e + 0x40c, 4, eerun.b32(600.0))

    def powf(self, ee):
        # The libm's own uses ops eerun does not have; both sides differ from it by an ulp or so.
        ee.f[0] = eerun.b32(eerun.f32(ee.f[12]) ** eerun.f32(ee.f[13]))
        return 0

    def index(self, at):
        return (at - self.bones) // 0x300

    def hex4(self, at):
        return ' '.join(f'{w:x}' for w in struct.unpack('<4I', bytes(self.ee.m[at:at + 16])))

    def solve(self, ee):
        self.log.append(f'V {eerun.s32(ee.g("a1"))} {eerun.s32(ee.g("a2"))} {eerun.s32(ee.g("a3"))}')
        return 0

    def play(self, kind):
        def fn(ee):
            self.log.append(f'{kind} {ee.g("a1") & 0xffffffff:x} {ee.g("a2") & 0xffffffff:x} {ee.g("a3") & 0xffffffff:x}')
            return 0
        return fn

    def bone_rate(self, kind):
        def fn(ee):
            self.log.append(f'{kind} {self.index(ee.g("a0"))} {ee.f[12]:x} {eerun.s32(ee.g("a1"))}')
            return 0
        return fn

    def aim(self, ee):
        self.log.append(f'A {self.index(ee.g("a0"))} {ee.f[12]:x} {self.hex4(ee.g("a1"))} {eerun.s32(ee.g("a2"))}')
        return 0

    def turn_vector(self, ee):
        self.log.append(f'T {self.index(ee.g("a0"))} {self.hex4(ee.g("a2"))}')
        ee.write(ee.g('a1'), struct.pack('<4I', *self.turn))
        return 0

    def quat(self, ee):
        at = ee.g('a0') - self.bones - 0x1d0
        self.log.append(f'Q {at // 0x300 if 0 <= at and at % 0x300 == 0 else -1} {self.hex4(ee.g("a1"))}')
        return 0

    def call(self, addr, *args, floats=()):
        ee = self.ee
        ee.vf[1:] = [[0.0, 0.0, 0.0, 0.0] for _ in range(31)]
        for i, bits in enumerate(floats):
            ee.f[12 + i] = bits
        return ee.call(addr, *args)

    def fill(self, at, layout, words):
        for offset, n in layout:
            self.ee.write(at + offset, struct.pack(f'<{n}I', *words[:n]))
            words = words[n:]
        return words

    def car_state(self):
        ee, out = self.ee, []
        for offset, n in CAR:
            out += [f'{w:x}' for w in struct.unpack(f'<{n}I', bytes(ee.m[self.car + offset:self.car + offset + n * 4]))]
        return ' '.join(out) + ' ' + self.hex4(self.bones + 0x220) + ' ' + self.hex4(self.bones + 0x230)

    def elevator_state(self, answer):
        ee, e = self.ee, self.elevator
        words = [ee.u32(e + 0x410 + i * 4) for i in range(16)]
        return (f'{answer} {ee.u32(e + 0x408):x} {ee.u32(e + 0x40c):x} ' + ' '.join(f'{w:x}' for w in words)
                + f' {ee.u32(e + 0x450):x}')

    def run(self, op):
        w = op.split()
        self.log = []
        name, ee = w[0], self.ee
        car = True
        if name == 'setup':
            words = [int(x, 16) for x in w[1:]]
            words = self.fill(self.car, CAR, words)
            for i in range(BONES):
                words = self.fill(self.bones + i * 0x300, BONE, words)
        elif name == 'set':
            self.call(SET_DRIVE, self.car, floats=[int(x, 16) for x in w[1:4]])
        elif name == 'drive':
            self.turn = [int(x, 16) for x in w[2:6]]
            words = [int(x, 16) for x in w[6:22]]
            ee.write(self.scratch, struct.pack('<16I', *words))
            s = self.scratch
            self.call(DRIVE, self.car, s, s + 0x10, s + 0x20, s + 0x30, floats=[int(w[1], 16)])
        elif name == 'drive0':
            self.call(DRIVE, self.car, 0, 0, 0, 0, floats=[int(w[1], 16)])
        elif name == 'ground':
            self.turn = [int(x, 16) for x in w[1:5]]
            self.call(FOLLOW, self.car)
        elif name == 'points':
            s = self.scratch
            self.call(POINTS, self.car, s, s + 0x10, s + 0x20, s + 0x30)
            self.log.append(' '.join(f'{x:x}' for x in struct.unpack('<16I', bytes(ee.m[s:s + 0x40]))))
        else:
            car = False
            answer = 0
            e = self.elevator
            calls = {'release': RELEASE, 'in3': IN_B3, 'in1': IN_B1, 'floor3': FLOOR_B3, 'floor1': FLOOR_B1}
            if name == 'reopen':
                self.call(REOPEN, e)
            elif name in calls:
                answer = self.call(calls[name], e)
            elif name == 'scene':
                answer = self.call(SCENE, e, int(w[1]))
            elif name == 'file':
                self.call(SET_FILE, e, ee.u32(e + 0x3c0 + int(w[1]) * 4), int(w[1]))
            elif name == 'step':
                self.ending = int(w[1])
                ee.wr(e + 0x40c, 4, int(w[2], 16))
                self.call(STEP, e)
            return self.elevator_state(answer) + ' |' + ''.join(' ' + x for x in self.log)
        return self.car_state() + ' |' + ''.join(' ' + x for x in self.log)


def hexes(values):
    return ' '.join(f'{eerun.b32(v):x}' for v in values)


def unit_ish(rng):
    return [rng.uniform(-1.0, 1.0) for _ in range(4)]


def car_setup(rng):
    words = []
    for _, n in CAR:
        words += [rng.uniform(-3.0, 3.0) for _ in range(n)]
    # The contacts: on the ground or off it.
    on = rng.random() < 0.85
    at = sum(n for _, n in CAR[:4])
    for i in range(4):
        words[at + i * 4 + 3] = 1.0 if on else 0.0
    for _ in range(BONES):
        words += [rng.uniform(-1.0, 1.0) for _ in range(12)]
        words += [rng.uniform(-300.0, 300.0) for _ in range(3)] + [1.0]
        words += [rng.uniform(-300.0, 300.0) for _ in range(BONE_WORDS - 16)]
    return 'setup ' + hexes(words)


def car_script(rng, n):
    ops = [car_setup(rng)]
    for _ in range(n):
        r = rng.random()
        if r < 0.05:
            ops.append(car_setup(rng))
        elif r < 0.25:
            ops.append('set ' + hexes((rng.uniform(-15, 15), rng.uniform(-2, 15), rng.uniform(-3, 3))))
        elif r < 0.6:
            rate = rng.choice((1.0, 1.0, 1.0, eerun.f32(0x3f999999), rng.uniform(0.2, 3.0)))
            ops.append('drive ' + hexes([rate] + unit_ish(rng) + [rng.uniform(-500, 500) for _ in range(16)]))
        elif r < 0.65:
            ops.append('drive0 ' + hexes([1.0]))
        elif r < 0.7:
            ops.append('points')
        else:
            ops.append('ground ' + hexes(unit_ish(rng)))
    return ops


def elevator_script(rng, n):
    ops = [f'scene {rng.choice((2, 4, 8, 10))}']
    for _ in range(n):
        r = rng.random()
        if r < 0.45:
            ops.append(f'step {rng.choice((0, 1, 1, 1))} ' + hexes([rng.choice((0.0, 50.0, 120.0, 121.0, 600.0))]))
        elif r < 0.85:
            ops.append(rng.choice(('release', 'in3', 'in1', 'floor3', 'floor1')))
        elif r < 0.92:
            ops.append('reopen')
        elif r < 0.97:
            ops.append(f'scene {rng.choice((2, 4, 5, 6, 8, 10, 3))}')
        else:
            ops.append(f'file {rng.randrange(0, 18)}')
    return ops


def ulps(a, b):
    def line(x):
        return x & 0x7fffffff if not x >> 31 else -(x & 0x7fffffff)
    return abs(line(a) - line(b))


def main(argv):
    scripts = int(argv[1]) if len(argv) > 1 else 40
    n = int(argv[2]) if len(argv) > 2 else 150
    exe = build()
    rng = random.Random(0x27dc40)
    rom = Rom()
    where = {}
    words = bad = worst = shown = lines = inexact = 0
    for s in range(scripts):
        ops = car_script(rng, n) if s % 2 == 0 else elevator_script(rng, n)
        rom.fresh_elevator()    # the host's exe starts over for each script
        (WORK / 'ops.txt').write_text('\n'.join(ops) + '\n')
        out = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                             check=True).stdout.splitlines()
        if len(out) != len(ops):
            print('host stopped early', len(out), len(ops))
            bad += 1
        for op, host in zip(ops, out):
            got = rom.run(op)
            lines += 1
            a, b = got.split(), host.split()
            if len(a) != len(b):
                bad += 1
                if shown < 6:
                    shown += 1
                    print(f'{op.split()[0]}: shape differs\n rom  {got[-300:]}\n host {host[-300:]}')
                continue
            for i, (x, y) in enumerate(zip(a, b)):
                words += 1
                if x == y:
                    continue
                try:
                    d = ulps(int(x, 16), int(y, 16))
                except ValueError:
                    d = 1 << 31
                worst = max(worst, d)
                inexact += 1
                where[('car' if s % 2 == 0 else 'elevator', i)] = where.get(('car' if s % 2 == 0 else 'elevator', i), 0) + 1
                if d > 16:
                    bad += 1
                    if shown < 6:
                        shown += 1
                        print(f'{op.split()[0]}: word {i}: rom {x} host {y} ({d} ulp)')
    print(f'{scripts} scripts, {lines} ops, {words} words: {inexact} differ, {bad} past 16 ulp, worst {worst} ulp')
    print('words that differ:', sorted(where.items(), key=lambda kv: -kv[1])[:12])
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
