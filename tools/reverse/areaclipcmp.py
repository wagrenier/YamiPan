#!/usr/bin/env python3
"""game/areaclip.cpp against the ROM, on the disc's own area clip files (offline).

python areaclipcmp.py --scan             unpack the SLDI packs, write every "ac1" file to work/areaclip/
python areaclipcmp.py [frames] [--files N] [--full N] [--samples N] [--plant]

For each file of work/areaclip/: the host's AreaClip in a small exe compiled from src/game/areaclip.cpp
with stand-in headers (work/areaclipcmp/include: models, the scene, the object list) against the ROM on
eerun.py. The ROM's ctor 0x2dccf0 is slow (0x2db080 runs 40,000 instructions for a cell it misses, a
smallest file takes 3 minutes), so it runs whole, compared cell by cell, on the --full smallest files
(default 1); on the others the ROM's relocate 0x2dceb0 runs, --samples random (box, cell) pairs of 0x2db080
are compared with the host's grid (default 60), and the ROM's vectors are built from that grid. Then
random frames on both, update 0x2dbd10 with a camera walking from box to box, jumping off the map, or
null, and three characters standing in boxes, one now and then inactive. Stand-in targets log every
setClipped; after each frame the log, the boxes the camera is in and each character's clipped flag are
compared. A tenth of the targets are null. findTarget is not compared: it needs real models, the game
shows it. --plant compiles the host with three bugs, to prove the tool sees them.
"""
import mmap
import os
import random
import re
import struct
import subprocess
import sys
from pathlib import Path

import eerun
import sssall
import yamipan

HERE = Path(__file__).parent
FILES = HERE / 'work' / 'areaclip'
WORK = HERE / 'work' / 'areaclipcmp'
SRC = yamipan.REPO / 'src'

CTOR, UPDATE, RELOCATE, TOUCHES = 0x2dccf0, 0x2dbd10, 0x2dceb0, 0x2db080
SCENE_POINTER = eerun.GP - 0x7a7c
SET_CLIPPED = 0x1f00000     # no code here: a hook
MAGIC = b'ac1\0'
CHARACTERS = 3
PLANTS = (('if (!boxesHide(*list, inside))', 'if (true)'),
          ('line.hi = std::max(a[0], b[0]);', 'line.hi = std::min(a[0], b[0]);'),
          ('if (!character->m_active)', 'if (character->m_active == 2)'))

STANDINS = {
    'game/s2ddata.h': r'''
#pragma once
#include <stdint.h>
template <class T> struct S2dRef {
    int32_t rel;
    T *get() const { return rel != 0 ? (T *)((const char *)this + rel) : nullptr; }
    operator T *() const { return get(); }
    void set(const void *p) { rel = p != nullptr ? (int32_t)((const char *)p - (const char *)this) : 0; }
};
''',
    'game/gamehooks.h': r'''
#pragma once
#include <stddef.h>
#include <string.h>
#include "game/s2ddata.h"
typedef unsigned char u_char; typedef unsigned short u_short; typedef unsigned int u_int;
typedef float sceVu0FVECTOR[4];
class ClipTarget { public:
    virtual ~ClipTarget() {}
    virtual void setClipped(u_char clipped) = 0;
    virtual u_char getClipped() const = 0; };
struct ModelMesh { const char *name; };
struct ModelMeshes { ModelMesh *get() const { return nullptr; } };
struct ModelNode { const char *name; ModelMeshes meshes; };
struct ModelPartRec : ClipTarget { void setClipped(u_char) override {} u_char getClipped() const override { return 0; } };
struct ModelInstance : ClipTarget { u_char clipped = 0; ModelNode *node = nullptr; ModelPartRec *partRecs = nullptr;
    void setClipped(u_char c) override { clipped = c; } u_char getClipped() const override { return clipped; } };
inline ModelPartRec *modelFindPart(ModelInstance *, const char *, size_t, int) { return nullptr; }
struct StageModel { ModelInstance *data; };
struct NodeTransform { float position[4]; };
class StageObject { public: StageModel *m_model; u_char m_active; NodeTransform m_xform; };
''',
    'game/drawsys.h': r'''
#pragma once
#include <vector>
struct DrawSysRecord { std::vector<ModelInstance *> models; };
inline DrawSysRecord *drawSysCurrent() { static DrawSysRecord record; return &record; }
''',
    'game/stageobjlist.h': r'''
#pragma once
enum { StageObjGroup_Characters = 2 };
class StageObjArray { public: StageObject **m_data = nullptr; u_short m_count = 0;
    StageObject **begin() { return m_count == 0 ? nullptr : &m_data[1]; }
    static StageObject **next(StageObject **slot) { return slot[1] == nullptr ? nullptr : slot + 1; } };
class StageObjectList { public: StageObjArray m_lists[3];
    StageObject *findCharacter(const char *, int) { return nullptr; } };
''',
    'game/stageparticle.h': r'''
#pragma once
struct StageParticle { void setEmitting(bool) {} };
struct StageNodeTree { const float *effectOffset(const char *) { return nullptr; }
    StageParticle *findLive(const char *) { return nullptr; } };
''',
    'game/stagescene.h': r'''
#pragma once
struct StageScene { StageObject *rootAny; StageObjectList *objects; StageNodeTree *nodeTree; };
''',
    'game/stageloop.h': r'''
#pragma once
struct StageScene;
extern StageScene *gStageScene;
''',
}

HOST = r'''
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#define private public
#include "game/areaclip.h"
#undef private
#include "game/drawsys.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"
StageScene *gStageScene;
static std::vector<int> sLog;
struct Fake : ClipTarget { int id; explicit Fake(int i) : id(i) {}
    void setClipped(u_char c) override { sLog.push_back(id * 2 + c); } u_char getClipped() const override { return 0; } };
static float fl(unsigned bits) { float f; memcpy(&f, &bits, 4); return f; }
int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    void *bytes = malloc(n + 16); fread(bytes, 1, n, f); fclose(f);
    StageObjectList objects; StageScene scene = {nullptr, &objects, nullptr}; gStageScene = &scene;
    ModelInstance models[3]; StageModel holders[3]; StageObject people[3];
    StageObject *list[5] = {nullptr, &people[0], &people[1], &people[2], nullptr};
    objects.m_lists[StageObjGroup_Characters].m_data = list; objects.m_lists[StageObjGroup_Characters].m_count = 3;
    for (int i = 0; i < 3; i++) { holders[i].data = &models[i]; people[i].m_model = &holders[i]; }
    AreaClip clip(bytes);
    for (auto &column : clip.m_cells) for (auto &cell : column) {
        printf("%d", (int)cell.size()); for (u_short b : cell) printf(" %d", b); printf("\n"); }
    FILE *q = fopen(argv[2], "r"); unsigned count;
    fscanf(q, "%u", &count); clip.m_targets.resize(count);
    for (unsigned i = 0; i < count; i++) { int on; fscanf(q, "%d", &on); clip.m_targets[i] = on ? new Fake(i) : nullptr; }
    int cam;
    while (fscanf(q, "%d", &cam) == 1) {
        StageObject camera; unsigned b[4]; memset(&camera, 0, sizeof camera);
        fscanf(q, "%x %x %x", &b[0], &b[1], &b[2]);
        for (int i = 0; i < 3; i++) camera.m_xform.position[i] = fl(b[i]);
        for (int i = 0; i < 3; i++) { unsigned p[4]; int a; fscanf(q, "%d %x %x %x %x", &a, &p[0], &p[1], &p[2], &p[3]);
            people[i].m_active = a; for (int k = 0; k < 4; k++) people[i].m_xform.position[k] = fl(p[k]); models[i].clipped = 9; }
        sLog.clear();
        clip.update(cam ? &camera : nullptr);
        for (int v : sLog) printf("%d ", v);
        printf("| "); for (u_short b : clip.m_boxes) printf("%d ", b);
        printf("| "); for (int i = 0; i < 3; i++) printf("%d ", models[i].clipped);
        printf("\n");
    }
    return 0;
}
'''


def extent(d, at):
    """The file at d[at:] ends where its last list does; None when the header cannot be one."""
    boxes, targets = struct.unpack_from('<II', d, at + 4)
    refs = struct.unpack_from('<4I', d, at + 0x30)
    if not 0 < boxes < 0x4000 or not 0 < targets < 0x4000:
        return None
    size = len(d) - at
    if any(r >= size or r % 4 for r in refs) or refs[0] < 0x40 or not refs[1] or not refs[2]:
        return None
    end = refs[0] + boxes * 0x40
    try:
        for i in range(targets):
            name = struct.unpack_from('<I', d, at + refs[1] + i * 4)[0]
            stop = d.find(b'\0', at + name, at + name + 0x100)
            if stop < 0:
                return None
            end = max(end, stop + 1 - at)
        for table in refs[2:]:
            if table == 0:
                continue
            end = max(end, table + boxes * 4)
            for i in range(boxes):
                at_list = struct.unpack_from('<I', d, at + table + i * 4)[0]
                if at_list == 0:
                    continue
                while struct.unpack_from('<h', d, at + at_list)[0] != -1:
                    at_list += 2
                end = max(end, at_list + 2)
    except struct.error:
        return None
    return end


def scan_bytes(d, seen, where):
    at = d.find(MAGIC)
    while at >= 0:
        if at % 16 == 0:
            n = extent(d, at)
            if n is not None:
                seen.setdefault(bytes(d[at:at + n]), where)
        at = d.find(MAGIC, at + 4)


def scan():
    FILES.mkdir(parents=True, exist_ok=True)
    seen = {}
    packs = 0
    files = sorted(p for p in Path(yamipan.GAME_DIR).iterdir() if re.match(r'ROMD?\.\d+$', p.name))
    for path in files:
        with open(path, 'rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as d:
            size = len(d)
            at = d.find(b'SLDI\x01\0\0\0')
            while at >= 0:
                data = sssall.sldi(d, at, size)
                if data is not None:
                    packs += 1
                    scan_bytes(bytes(data), seen, f'{path.name}@{at:#x}')
                at = d.find(b'SLDI\x01\0\0\0', at + 4)
        print(f'{path.name}: {packs} packs, {len(seen)} files so far', flush=True)
    for i, (data, where) in enumerate(seen.items()):
        boxes, targets = struct.unpack_from('<II', data, 4)
        (FILES / f'{i:03d}.ac1').write_bytes(data)
        print(f'{i:03d} {where} {len(data):#x} bytes, {boxes} boxes, {targets} targets, '
              f'character lists {"yes" if struct.unpack_from("<I", data, 0x3c)[0] else "no"}')


def build(plant):
    for name, text in STANDINS.items():
        path = WORK / 'include' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    source = (SRC / 'game' / 'areaclip.cpp').read_text(encoding='utf-8')
    if plant:
        for old, new in PLANTS:
            if old not in source:
                raise SystemExit('plant text not found: ' + old)
            source = source.replace(old, new)
    (WORK / 'areaclip.cpp').write_text(source, encoding='utf-8')
    header = (SRC / 'game' / 'areaclip.h').read_text(encoding='utf-8')
    (WORK / 'include' / 'game' / 'areaclip.h').write_text(header, encoding='utf-8')
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(WORK / 'include'),
                    str(WORK / 'host.cpp'), str(WORK / 'areaclip.cpp'), '-o', str(exe)], check=True)
    return exe


class Rom:
    def __init__(self, ee, data, targets_on, grid):
        self.ee = ee
        ee.heap = 0x1000000
        self.base = ee.alloc(len(data) + 16)
        ee.write(self.base, data)
        self.this = ee.alloc(0x40)
        if grid is None:
            ee.call(CTOR, self.this, self.base, maxsteps=400000000)
        else:
            ee.call(RELOCATE, self.base)
            self.put_grid(grid)
        self.log = []
        table = ee.alloc(0x20)
        ee.wr(table + 0xc, 4, SET_CLIPPED)
        ee.hooks[SET_CLIPPED] = self.set_clipped
        self.objects = {}
        array = ee.alloc(len(targets_on) * 4 + 4)
        for i, on in enumerate(targets_on):
            if on:
                target = ee.alloc(8)
                ee.wr(target, 4, table)
                self.objects[target] = i
                ee.wr(array + i * 4, 4, target)
        ee.wr(self.this + 0x14, 4, len(targets_on))
        ee.wr(self.this + 0x18, 4, array)
        self.camera = ee.alloc(0x200)
        scene = ee.alloc(0x130)
        ee.wr(SCENE_POINTER, 4, scene)
        objects = ee.alloc(0x100)
        ee.wr(scene + 0x10, 4, objects)
        slots = ee.alloc(4 * (CHARACTERS + 2))
        ee.wr(objects + 0xa0, 4, slots)
        ee.wr(objects + 0xa4, 2, CHARACTERS)
        self.people = []
        for i in range(CHARACTERS):
            person, holder, model = ee.alloc(0x200), ee.alloc(0x10), ee.alloc(0x10)
            ee.wr(person + 0x30, 4, holder)
            ee.wr(holder, 4, model)
            ee.wr(slots + 4 + i * 4, 4, person)
            self.people.append((person, model))
        self.mark = ee.heap

    def put_grid(self, grid):
        """The ROM's three levels of MSL vector ({capacity, size, data}) from the host's cells."""
        ee = self.ee
        ee.wr(self.this, 4, self.base)
        columns = ee.alloc(8 * 0xc)
        for at, count in ((self.this + 4, columns),):
            ee.wr(at, 4, 8)
            ee.wr(at + 4, 4, 8)
            ee.wr(at + 8, 4, count)
        for x in range(8):
            cells = ee.alloc(8 * 0xc)
            ee.wr(columns + x * 0xc, 4, 8)
            ee.wr(columns + x * 0xc + 4, 4, 8)
            ee.wr(columns + x * 0xc + 8, 4, cells)
            for y in range(8):
                boxes = grid[x * 8 + y]
                items = ee.alloc(len(boxes) * 2 + 2)
                ee.wr(cells + y * 0xc, 4, len(boxes))
                ee.wr(cells + y * 0xc + 4, 4, len(boxes))
                ee.wr(cells + y * 0xc + 8, 4, items)
                for i, box in enumerate(boxes):
                    ee.wr(items + i * 2, 2, box)

    def touches(self, box, x, y):
        boxes = self.ee.u32(self.base + 0x30)
        return self.ee.call(TOUCHES, self.this, boxes + box * 0x40, x, y) != 0

    def set_clipped(self, ee):
        self.log.append(self.objects[ee.g('a0') & 0xffffffff] * 2 + (ee.g('a1') & 0xff))
        return 0

    def vector(self, at):
        return self.ee.u32(at + 4), self.ee.u32(at + 8)

    def cells(self):
        ee = self.ee
        out = []
        columns, data = self.vector(self.this + 4)
        for x in range(columns):
            cells, column = self.vector(data + x * 0xc)
            for y in range(cells):
                n, items = self.vector(column + y * 0xc)
                out.append(' '.join([str(n)] + [str(ee.rd(items + i * 2, 2)) for i in range(n)]))
        return out

    def frame(self, cam, bits, people):
        ee = self.ee
        for i in range(3):
            ee.wr(self.camera + 0x1f0 + i * 4, 4, bits[i])
        for (person, model), (active, place) in zip(self.people, people):
            ee.wr(person + 0x34, 1, active)
            ee.wr(model + 4, 1, 9)
            for i in range(4):
                ee.wr(person + 0x1f0 + i * 4, 4, place[i])
        self.log = []
        ee.call(UPDATE, self.this, self.camera if cam else 0)
        n, data = self.vector(self.this + 0x1c)
        boxes = ''.join(f'{ee.rd(data + i * 2, 2)} ' for i in range(n))
        log = ''.join(f'{v} ' for v in self.log)
        flags = ''.join(f'{ee.rd(model + 4, 1)} ' for _, model in self.people)
        return f'{log}| {boxes}| {flags}'.strip()


def invert(box):
    """Where the box's centre is: its matrix takes that place to 0."""
    r0, r1, r2, t = box
    a = (r0[0], r1[0], r2[0])
    b = (r0[1], r1[1], r2[1])
    c = (r0[2], r1[2], r2[2])
    cross = lambda u, v: (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    bc, ca, ab = cross(b, c), cross(c, a), cross(a, b)
    det = sum(x * y for x, y in zip(bc, a))
    if det == 0.0:
        return None
    return [(bc[i] * -t[0] + ca[i] * -t[1] + ab[i] * -t[2]) / det for i in range(3)]


def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def walker(rng, data):
    boxes, _ = struct.unpack_from('<II', data, 4)
    low = struct.unpack_from('<2f', data, 0x10)
    high = struct.unpack_from('<2f', data, 0x20)
    at = struct.unpack_from('<I', data, 0x30)[0]
    centres = []
    for i in range(boxes):
        v = struct.unpack_from('<16f', data, at + i * 0x40)
        centres.append(invert([v[0:4], v[4:8], v[8:12], v[12:16]]))
    # The boxes each box's character list names.
    lists = struct.unpack_from('<I', data, 0x3c)[0]
    hides = {}
    for i in range(boxes if lists else 0):
        where = struct.unpack_from('<I', data, lists + i * 4)[0]
        names = []
        while where and struct.unpack_from('<h', data, where)[0] != -1:
            names.append(struct.unpack_from('<h', data, where)[0])
            where += 2
        if names:
            hides[i] = names
    spread = (high[0] - low[0]) / 200

    def near(box):
        return [c + rng.uniform(-spread, spread) for c in centres[box]]

    def place():
        kind = rng.random()
        known = [i for i, c in enumerate(centres) if c is not None]
        if kind < 0.8 and known:
            return near(rng.choice(known))
        if kind < 0.95:
            return [rng.uniform(low[0], high[0]), rng.uniform(low[1], high[1]), rng.uniform(-500, 500)]
        return [rng.uniform(low[0] * 2, high[0] * 2), rng.uniform(low[1] * 2, high[1] * 2), 0.0]

    def scene():
        """A camera and three people's places: most frames someone stands where the camera's box hides."""
        boxes_with = [i for i in hides if centres[i] is not None]
        if boxes_with and rng.random() < 0.7:
            camera = rng.choice(boxes_with)
            names = [n for n in hides[camera] if centres[n] is not None]
            if names:
                return near(camera), [near(rng.choice(names)) if rng.random() < 0.7 else place()
                                      for _ in range(CHARACTERS)]
        return place(), [place() for _ in range(CHARACTERS)]
    return scene


def main(argv):
    if '--scan' in argv:
        scan()
        return 0
    plant = '--plant' in argv
    args = [a for a in argv[1:] if not a.startswith('--')]
    def option(name, default):
        if name not in argv:
            return default
        value = argv[argv.index(name) + 1]
        args.remove(value)
        return int(value)
    limit = option('--files', None)
    full = option('--full', 1)
    samples = option('--samples', 60)
    frames = int(args[0]) if args else 200
    exe = build(plant)
    rng = random.Random(CTOR)
    ee = eerun.Ee()
    paths = sorted(FILES.glob('*.ac1'), key=lambda p: p.stat().st_size)
    if limit is not None:
        paths = paths[:limit]
    totals = {'cells': 0, 'pairs': 0, 'touching': 0, 'frames': 0, 'calls': 0, 'hidden': 0, 'in box': 0}
    bad = 0
    for index, path in enumerate(paths):
        data = path.read_bytes()
        target_count = struct.unpack_from('<I', data, 8)[0]
        on = [rng.random() >= 0.1 for _ in range(target_count)]
        scene = walker(rng, data)
        script = [f'{target_count} ' + ' '.join('1' if x else '0' for x in on)]
        walks = []
        for _ in range(frames):
            cam = 0 if rng.random() < 0.03 else 1
            eye, spots = scene()
            eye = [bits(x) for x in eye]
            people = [(0 if rng.random() < 0.1 else 1, [bits(x) for x in spot] + [bits(1.0)]) for spot in spots]
            walks.append((cam, eye, people))
            script.append(f'{cam} {eye[0]:x} {eye[1]:x} {eye[2]:x} '
                          + ' '.join(f'{a} {p[0]:x} {p[1]:x} {p[2]:x} {p[3]:x}' for a, p in people))
        (WORK / 'ops.txt').write_text('\n'.join(script) + '\n')
        out = subprocess.run([str(exe), str(path), str(WORK / 'ops.txt')], capture_output=True, text=True,
                             check=True).stdout.splitlines()
        host_cells, host_frames = out[:64], out[64:]
        file_bad = 0
        if index < full:
            rom = Rom(ee, data, on, None)
            for i, (a, b) in enumerate(zip(rom.cells(), host_cells)):
                totals['cells'] += 1
                if a != b.strip():
                    file_bad += 1
                    if file_bad <= 3:
                        print(f'{path.name} cell {i // 8},{i % 8}: ROM {a} | host {b}')
        else:
            grid = [[int(x) for x in line.split()[1:]] for line in host_cells]
            rom = Rom(ee, data, on, grid)
            boxes = struct.unpack_from('<I', data, 4)[0]
            for _ in range(samples):
                box, x, y = rng.randrange(boxes), rng.randrange(8), rng.randrange(8)
                answer = rom.touches(box, x, y)
                totals['pairs'] += 1
                totals['touching'] += 1 if answer else 0
                if answer != (box in grid[x * 8 + y]):
                    file_bad += 1
                    if file_bad <= 3:
                        print(f'{path.name} box {box} cell {x},{y}: ROM {answer}')
        for n, (walk, host) in enumerate(zip(walks, host_frames)):
            answer = rom.frame(*walk)
            totals['frames'] += 1
            log, boxes, flags = answer.split('|')
            totals['calls'] += len(log.split())
            totals['in box'] += 1 if boxes.strip() else 0
            totals['hidden'] += flags.split().count('1')
            if answer != host.strip():
                file_bad += 1
                if file_bad <= 3:
                    print(f'{path.name} frame {n}: {walk}\n  ROM  {answer}\n  host {host.strip()}')
        print(f'{path.name}: {len(data):#x} bytes, {file_bad} differences', flush=True)
        bad += file_bad
    print(f'{len(paths)} files, {totals}, {bad} differences, {ee.count} instructions')
    return 1 if bad and not plant else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
