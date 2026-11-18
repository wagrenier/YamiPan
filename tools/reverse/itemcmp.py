#!/usr/bin/env python3
"""game/itemphysics.cpp against the ROM (offline).

python itemcmp.py [scripts] [ops] [--plant]

Random lives of a thrown item or weapon on the ROM's own code (eerun.py, 0x23c010..0x23de20) and on the
host's in a small exe compiled from src/game/itemphysics.cpp with vu0math.cpp and stand-in headers
(work/itemcmp/include: the owner, the scene, the motion). Each frame is what the game does: update,
syncBody of each body, a random collisionHook0, endFrame; throws, pushes, a rest forced now and then.

The scene's raycast, a face's normal, the bones, matrixToRotYXZ, fromPoints, the bucket's add and the
owner's slots are stand-ins on both sides and log their arguments as bits. The raycast's world: a floor
at z 0, a wall at x 300, and now and then a third face, by the ray's number. After each op both print
every node, link, sphere and flag as bits and the log; the first differences and a count go out. The
rows' w of the owner's matrix and of m_restFrame are not compared: vecCross's is whatever vf6 held.
--plant compiles the host with two bugs in it, to prove the tool sees them.
"""
import math
import os
import random
import subprocess
import sys
from pathlib import Path

import eerun
import yamipan

HERE = Path(__file__).parent
WORK = HERE / 'work' / 'itemcmp'
SRC = yamipan.REPO / 'src'

HOOK0, SYNC, ENDFRAME, UPDATE, MEAN, PUSH, SETMOVING, SETUP, DTOR, CTOR = (
    0x23c020, 0x23c300, 0x23c910, 0x23ca50, 0x23ce00, 0x23cf50, 0x23cfc0, 0x23d600, 0x23db00, 0x23db50)
SCENE_POINTER = eerun.GP - 0x7a7c
CATEGORY = {0: 0x400, 1: 0x800, 2: 0x1000}
FACE_FLAGS = (0x4, 0x2000, 0x8)
NORMALS = ((0.0, 0.0, 1.0, 0.0), (-1.0, 0.0, 0.0, 0.0), (0.6, 0.0, 0.8, 0.5))
WALL_X = 300.0
PLANTS = (('node.velocity[1] *= node.damping;', 'node.velocity[2] *= node.damping;'),
          ('total = (int)((float)total + nodeSpeed(m_nodes[i]));', 'total = (int)ceilf((float)total + nodeSpeed(m_nodes[i]));'))

HEADERS = {
    'gamehooks.h': '''#pragma once
#include "sdk/libvu0.h"
#include "sdk/scetypes.h"
class StageNodeBase;
enum { StageObject_ThrownItem = 0x17 };
''',
    'modelpose.h': '''#pragma once
#include "sdk/libvu0.h"
int motionBoneMatrix(sceVu0FMATRIX out, void *motion, const char *bone, int prefix);
int motionBoneSuffix(void *motion, const char *prefix);
''',
    'stageitem.h': '''#pragma once
#include "game/itemphysics.h"
struct FakeXform { sceVu0FVECTOR position, rotation; sceVu0FMATRIX world; };
class StageObject
{
public:
    FakeXform m_xform;
    int getKind();
    const sceVu0FVECTOR *getWorldMatrix() { return m_xform.world; }
    void setDirty(bool dirty);
    void playNamedHeard2(const char *name, const float *pos, StageNodeBase *parent);
};
class StageItem : public StageObject {};
class StageThrownItem : public StageItem { public: const char *getLandSound(); };
''',
    'stagescene.h': '''#pragma once
#include "game/clock.h"
#include "game/stagecollision.h"
class StageScene
{
public:
    Clock *clock;
    SceneRecordBuckets *records;
    StageCollisionFace *raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from, const sceVu0FVECTOR to, int arg,
                                u_int mask, u_int flags, StageObject *skip);
};
extern StageScene *gStageScene;
''',
}

HOST = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#define private public
#include "game/itemphysics.h"
#include "game/clock.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/stageitem.h"
#include "game/stagescene.h"
#undef private

static std::string gLog;
static unsigned gDelta, gRays;
static int gKind = 0x17, gBoneCount;
static int gSuffix[4];
static unsigned gBone[4][4];
static StageCollisionFace gFaces[3];
static const float kNormals[3][4] = { { 0, 0, 1, 0 }, { -1, 0, 0, 0 }, { 0.6f, 0, 0.8f, 0.5f } };
StageScene *gStageScene;

static void logf(const char *format, unsigned a = 0, unsigned b = 0, unsigned c = 0, unsigned d = 0)
{
    char line[128];
    sprintf(line, format, a, b, c, d);
    gLog += line;
}
static unsigned bitsOf(float value) { unsigned u; memcpy(&u, &value, 4); return u; }
static float floatOf(unsigned u) { float f; memcpy(&f, &u, 4); return f; }
static void logVector(const float *v) { logf(" %08x %08x %08x %08x", bitsOf(v[0]), bitsOf(v[1]), bitsOf(v[2]), bitsOf(v[3])); }
static int faceIndex(const StageCollisionFace *face) { return face == nullptr ? -1 : (int)(face - gFaces); }

// game/gamerate.h at the ROM's 25: every frame is a 25 Hz frame, the owner drawn at the body's newest matrix.
int palTicks() { return 1; }
float palScale() { return 1.0f; }
float palDrawShare() { return 1.0f; }
void palBlendMatrix(float out[4][4], const float (*)[4], const float to[4][4], float) { memcpy(out, to, 64); }

float Clock::getDeltaSeconds() { logf(" T"); return floatOf(gDelta); }
int StageObject::getKind() { logf(" K"); return gKind; }
void StageObject::setDirty(bool dirty) { logf(" dirty %x", dirty); }
const char *StageThrownItem::getLandSound() { logf(" L"); return "LAND"; }
void StageObject::playNamedHeard2(const char *name, const float *pos, StageNodeBase *parent)
{
    gLog += " snd "; gLog += name; logVector(pos); logf(" %x", (unsigned)(uintptr_t)parent);
}
void StageCollisionFace::getNormal(sceVu0FVECTOR out) const
{
    logf(" N%d", faceIndex(this));
    memcpy(out, kNormals[faceIndex(this)], 16);
}
void SceneRecordBuckets::add(CollisionBody *body)
{
    logf(" add %x %x %x", body->bucket, body->category, body->mask);
}
CollisionBodyArray::CollisionBodyArray() {}
CollisionBodyArray::~CollisionBodyArray() {}
CollisionBody::CollisionBody(CollisionOwner *bodyOwner, int bodyBucket, u_int bodyCategory, u_int bodyMask)
    : on(1), settled(0), synced(0), boxed(0), owner(bodyOwner), bucket(bodyBucket), category(bodyCategory),
      mask(bodyMask), shape(0), rayMask(0), rayFlags(0)
{
}
bool BoundingSphere::fromPoints(int count, const sceVu0FVECTOR *points)
{
    logf(" pts %x", count);
    for (int i = 0; i < count; i++)
        logVector(points[i]);
    memcpy(center, points[0], 16);
    return true;
}
void matrixToRotYXZ(sceVu0FVECTOR out, const sceVu0FMATRIX m)
{
    logf(" rot");
    for (int row = 0; row < 3; row++)
        logf(" %08x %08x %08x", bitsOf(m[row][0]), bitsOf(m[row][1]), bitsOf(m[row][2]));
    out[0] = m[0][0]; out[1] = m[1][1]; out[2] = m[2][2]; out[3] = 1.0f;
}
int motionBoneSuffix(void *motion, const char *prefix)
{
    int i = prefix[6] - '0';
    logf(" suffix %x", i);
    return i < gBoneCount ? gSuffix[i] : -1;
}
int motionBoneMatrix(sceVu0FMATRIX out, void *motion, const char *bone, int prefix)
{
    int i = bone[6] - '0';
    logf(" bone %x %x", i, prefix);
    identityMatrix(out);
    memcpy(out[3], gBone[i], 16);
    return 1;
}
StageCollisionFace *StageScene::raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from, const sceVu0FVECTOR to, int arg,
                                        u_int mask, u_int flags, StageObject *skip)
{
    gRays++;
    unsigned roll = (gRays * 2654435761u) >> 28;
    int face = -1;
    if ((mask & 0x18) && roll < 2) {
        face = 2;
        hit[0] = to[0] - 1.0f; hit[1] = to[1] - 2.0f; hit[2] = to[2] - 3.0f; hit[3] = 1.0f;
    }
    else if ((mask & 4) && to[2] <= 0.0f && from[2] >= to[2]) {
        face = 0;
        hit[0] = to[0]; hit[1] = to[1]; hit[2] = 0.0f; hit[3] = 1.0f;
    }
    else if ((mask & 0x2000) && to[0] >= 300.0f) {
        face = 1;
        hit[0] = 300.0f; hit[1] = to[1]; hit[2] = to[2]; hit[3] = 1.0f;
    }
    logf(" ray"); logVector(from); logVector(to);
    logf(" %x %x %x %x", (unsigned)arg, mask, flags, (unsigned)(uintptr_t)skip);
    logf(" =%d", face);
    return face < 0 ? nullptr : &gFaces[face];
}

static void state(ItemPhysics *item, StageItem *owner)
{
    printf("%x %x %x %x %08x", (unsigned)item->m_moving, item->m_nodeCount, item->m_linkCount,
           (unsigned)item->m_isWeapon | (unsigned)item->m_hasTouchBody << 1, bitsOf(item->m_stillSeconds));
    for (int i = 0; i < item->m_nodeCount; i++) {
        ItemNode &node = item->m_nodes[i];
        for (int lane = 0; lane < 4; lane++)
            printf(" %08x", bitsOf(node.position[lane]));
        for (int lane = 0; lane < 4; lane++)
            printf(" %08x", bitsOf(node.previous[lane]));
        for (int lane = 0; lane < 4; lane++)
            printf(" %08x", bitsOf(node.velocity[lane]));
        printf(" %08x %d %d", bitsOf(node.damping), faceIndex(node.face), faceIndex(node.lastFace));
    }
    for (int i = 0; i < item->m_linkCount; i++)
        printf(" %x %x %08x", (int)((sceVu0FVECTOR *)item->m_links[i].a - &item->m_nodes[0].position) / (int)(sizeof(ItemNode) / 16),
               (int)((sceVu0FVECTOR *)item->m_links[i].b - &item->m_nodes[0].position) / (int)(sizeof(ItemNode) / 16),
               bitsOf(item->m_links[i].length));
    for (int row = 0; row < 3; row++)
        printf(" %08x %08x %08x", bitsOf(item->m_restFrame[row][0]), bitsOf(item->m_restFrame[row][1]), bitsOf(item->m_restFrame[row][2]));
    for (int b = 0; b < ItemBody_Count; b++) {
        CollisionBody *body = item->m_bodies[b];
        if (body == nullptr) {
            printf(" -");
            continue;
        }
        printf(" %x %x %x %x", (unsigned)body->on, (unsigned)body->shape, body->rayMask, body->rayFlags);
        for (int lane = 0; lane < 4; lane++)
            printf(" %08x", bitsOf(body->sphere.center[lane]));
        for (StageList<BoundingSphere>::Node *node = body->spheres.head; node != nullptr; node = node->next) {
            printf(" %08x %08x", bitsOf(node->value.radius), bitsOf(node->value.radiusSq));
            for (int lane = 0; lane < 4; lane++)
                printf(" %08x", bitsOf(node->value.center[lane]));
        }
    }
    for (int lane = 0; lane < 4; lane++)
        printf(" %08x", bitsOf(owner->m_xform.position[lane]));
    for (int lane = 0; lane < 4; lane++)
        printf(" %08x", bitsOf(owner->m_xform.rotation[lane]));
    for (int row = 0; row < 4; row++)
        for (int lane = 0; lane < (row == 3 ? 4 : 3); lane++)
            printf(" %08x", bitsOf(owner->m_xform.world[row][lane]));
}

int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    char op[16];
    gStageScene = (StageScene *)calloc(1, sizeof(StageScene));
    gStageScene->clock = (Clock *)(uintptr_t)0x60;
    gStageScene->records = (SceneRecordBuckets *)(uintptr_t)0x70;
    gFaces[0].flags = 0x4; gFaces[1].flags = 0x2000; gFaces[2].flags = 0x8;
    static StageItem owner;
    ItemPhysics *item = nullptr;
    struct Item : ItemPhysics {
        Item(bool a, bool b) : ItemPhysics(a, b) {}
        StageObject *getCollisionObject() override { return nullptr; }
    };
    while (fscanf(f, "%15s", op) == 1) {
        gLog.clear();
        unsigned a = 0, b = 0, c = 0, v[16];
        unsigned answer[4] = { 0, 0, 0, 0 };
        if (!strcmp(op, "new")) {
            fscanf(f, "%x %x %x", &a, &b, &c);
            gBoneCount = (int)c;
            for (int i = 0; i < gBoneCount; i++) {
                fscanf(f, "%x", &v[0]);
                gSuffix[i] = (int)v[0];
                fscanf(f, "%x %x %x %x", &gBone[i][0], &gBone[i][1], &gBone[i][2], &gBone[i][3]);
            }
            gRays = 0;
            memset(&owner, 0, sizeof(owner));
            for (int row = 0; row < 4; row++)
                owner.m_xform.world[row][row] = 1.0f;
            item = new Item(a != 0, b != 0);
            item->setup(&owner, (void *)(uintptr_t)0x900);
        }
        else if (!strcmp(op, "world")) {
            for (int i = 0; i < 16; i++) {
                fscanf(f, "%x", &v[i]);
                owner.m_xform.world[i / 4][i % 4] = floatOf(v[i]);
            }
        }
        else if (!strcmp(op, "delta")) fscanf(f, "%x", &gDelta);
        else if (!strcmp(op, "kind")) fscanf(f, "%x", (unsigned *)&gKind);
        else if (!strcmp(op, "moving")) { fscanf(f, "%x %x", &a, &b); item->setMoving(a != 0, b != 0); }
        else if (!strcmp(op, "push")) {
            float offset[4] __attribute__((aligned(16)));
            fscanf(f, "%x %x %x %x %x", &v[0], &v[1], &v[2], &v[3], &a);
            for (int i = 0; i < 4; i++) offset[i] = floatOf(v[i]);
            item->push(offset, (int)a);
        }
        else if (!strcmp(op, "update")) item->update();
        else if (!strcmp(op, "end")) item->endFrame();
        else if (!strcmp(op, "sync")) {
            fscanf(f, "%x", &a);
            if (item->m_bodies[a] != nullptr)
                item->syncBody(item->m_bodies[a]);
        }
        else if (!strcmp(op, "hook")) {
            float normal[4] __attribute__((aligned(16)));
            fscanf(f, "%x %x %x %x %x %x %x %x", &a, &b, &c, &v[0], &v[1], &v[2], &v[3], &v[4]);
            for (int i = 0; i < 4; i++) normal[i] = floatOf(v[i]);
            if (item->m_bodies[a] != nullptr) {
                CollisionContact contact = { item->m_bodies[a], nullptr, (int)b, 0 };
                int face = (int)v[4];
                answer[0] = (unsigned)item->collisionHook0(&contact, normal, face < 0 ? nullptr : &gFaces[face], floatOf(c));
            }
        }
        else if (!strcmp(op, "mean")) {
            float out[4] __attribute__((aligned(16)));
            item->getMeanVelocity(out);
            for (int i = 0; i < 4; i++) answer[i] = bitsOf(out[i]);
        }
        printf("%x %x %x %x ", answer[0], answer[1], answer[2], answer[3]);
        state(item, &owner);
        printf(" |%s\n", gLog.c_str());
    }
    return 0;
}
'''


def build(plant):
    include = WORK / 'include' / 'game'
    include.mkdir(parents=True, exist_ok=True)
    for name, text in HEADERS.items():
        (include / name).write_text(text)
    (WORK / 'host.cpp').write_text(HOST)
    source = (SRC / 'game' / 'itemphysics.cpp').read_text(encoding='utf-8')
    if plant:
        for bug, wrong in PLANTS:
            assert bug in source, bug
            source = source.replace(bug, wrong)
    (WORK / 'itemphysics.cpp').write_text(source, encoding='utf-8')
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(WORK / 'include'),
                    '-I', str(SRC), '-I', str(SRC / 'sdk'), str(WORK / 'host.cpp'), str(WORK / 'itemphysics.cpp'),
                    str(SRC / 'game' / 'vu0math.cpp'), '-o', str(exe)], check=True)
    return exe


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        self.log = []
        self.delta = 0
        self.kind = 0x17
        self.rays = 0
        self.bones = []
        self.item = None
        ee.hooks[0x103c50] = lambda ee: ee.alloc(ee.g('a0'))    # malloc: the bodies' arrays
        ee.hooks[0x1253e0] = self.memset
        self.scene = ee.alloc(0x130)
        ee.wr(SCENE_POINTER, 4, self.scene)
        ee.wr(self.scene + 0x60, 4, 0x60)
        ee.wr(self.scene + 0x18, 4, 0x70)
        self.faces = [ee.alloc(0x50) for _ in FACE_FLAGS]
        for at, flags in zip(self.faces, FACE_FLAGS):
            ee.wr(at + 0x40, 4, flags)
        self.owner = ee.alloc(0x300)
        table = ee.alloc(0x200)
        ee.wr(self.owner, 4, table)
        self.land = ee.alloc(8)
        ee.write(self.land, b'LAND\0')
        for offset, addr, fn in ((8, 0xfff100, self.set_dirty), (0x18, 0xfff110, self.world_of),
                                 (0x1f4, 0xfff120, self.get_kind)):
            ee.wr(table + offset, 4, addr)
            ee.hooks[addr] = fn
        for name, fn in (('Clock::getDeltaSeconds', self.delta_seconds), ('StageScene::raycast', self.raycast),
                         ('StageCollisionFace::getNormal', self.normal), ('SceneRecordBuckets::add', self.add),
                         ('BoundingSphere::fromPoints', self.from_points), ('matrixToRotYXZ', self.rotation),
                         ('motionBoneSuffix', self.suffix), ('motionBoneMatrix', self.bone), ('sprintf', self.sprintf),
                         ('StageThrownItem::getLandSound', self.land_sound),
                         ('StageObject::playNamedHeard2', self.sound)):
            ee.hook(name, fn)

    def memset(self, ee):
        at, value, n = (ee.g(r) & 0xffffffff for r in ('a0', 'a1', 'a2'))
        ee.write(at, bytes([value & 0xff]) * n)
        return at

    def vector(self, at):
        return ' '.join(f'{self.ee.u32(at + i * 4):08x}' for i in range(4))

    def face_index(self, at):
        return self.faces.index(at) if at else -1

    def set_dirty(self, ee):
        self.log.append(f'dirty {ee.g("a1") & 1:x}')
        return 0

    def world_of(self, ee):
        return self.owner + 0x1c0

    def get_kind(self, ee):
        self.log.append('K')
        return self.kind

    def delta_seconds(self, ee):
        self.log.append('T')
        ee.f[0] = self.delta
        return 0

    def land_sound(self, ee):
        self.log.append('L')
        return self.land

    def sound(self, ee):
        name = bytes(ee.m[ee.g('a1') & 0xffffffff:ee.g('a1') + 4]).decode()
        self.log.append(f'snd {name} {self.vector(ee.g("a2") & 0xffffffff)} {ee.g("a3") & 0xffffffff:x}')
        return 0

    def normal(self, ee):
        i = self.face_index(ee.g('a0') & 0xffffffff)
        self.log.append(f'N{i}')
        out = ee.g('a1') & 0xffffffff
        for lane, value in enumerate(NORMALS[i]):
            ee.wr(out + lane * 4, 4, eerun.b32(value))
        return 0

    def add(self, ee):
        body = ee.g('a1') & 0xffffffff
        self.log.append(f'add {ee.u32(body + 8):x} {ee.u32(body + 0xc):x} {ee.u32(body + 0x10):x}')
        return 0

    def from_points(self, ee):
        sphere, count, points = (ee.g(r) & 0xffffffff for r in ('a0', 'a1', 'a2'))
        self.log.append(f'pts {count:x} ' + ' '.join(self.vector(points + i * 16) for i in range(count)))
        ee.write(sphere + 0x10, bytes(ee.m[points:points + 16]))
        return 1

    def rotation(self, ee):
        out, m = ee.g('a0') & 0xffffffff, ee.g('a1') & 0xffffffff
        rows = ' '.join(f'{ee.u32(m + row * 16 + lane * 4):08x}' for row in range(3) for lane in range(3))
        self.log.append('rot ' + rows)
        for lane in range(3):
            ee.wr(out + lane * 4, 4, ee.u32(m + lane * 20))
        ee.wr(out + 12, 4, eerun.b32(1.0))
        return 0

    def sprintf(self, ee):
        buffer, fmt, value = ee.g('a0') & 0xffffffff, ee.g('a1') & 0xffffffff, ee.g('a2') & 0xffffffff
        assert bytes(ee.m[fmt:fmt + 9]) == b'bn_col%d_'
        ee.write(buffer, b'bn_col%d_\0' % value)
        return 0

    def suffix(self, ee):
        i = ee.m[(ee.g('a1') & 0xffffffff) + 6] - 48
        self.log.append(f'suffix {i:x}')
        return self.bones[i][0] if i < len(self.bones) else -1

    def bone(self, ee):
        out = ee.g('a0') & 0xffffffff
        i = ee.m[(ee.g('a2') & 0xffffffff) + 6] - 48
        self.log.append(f'bone {i:x} {ee.g("a3") & 0xffffffff:x}')
        for at in range(16):
            ee.wr(out + at * 4, 4, eerun.b32(1.0) if at % 5 == 0 else 0)
        for lane, value in enumerate(self.bones[i][1:]):
            ee.wr(out + 0x30 + lane * 4, 4, value)
        return 1

    def raycast(self, ee):
        hit, start, end = (ee.g(r) & 0xffffffff for r in ('a1', 'a2', 'a3'))
        mask, flags = ee.g('t1') & 0xffffffff, ee.g('t2') & 0xffffffff
        self.rays += 1
        roll = ((self.rays * 2654435761) & 0xffffffff) >> 28
        to = [ee.float(end + i * 4) for i in range(3)]
        from_z = ee.float(start + 8)
        point = None
        face = -1
        if mask & 0x18 and roll < 2:
            face = 2
            point = (to[0] - 1.0, to[1] - 2.0, to[2] - 3.0)
        elif mask & 4 and to[2] <= 0.0 and from_z >= to[2]:
            face = 0
            point = (to[0], to[1], 0.0)
        elif mask & 0x2000 and to[0] >= WALL_X:
            face = 1
            point = (WALL_X, to[1], to[2])
        if point is not None:
            for lane, value in enumerate(point + (1.0,)):
                ee.wr(hit + lane * 4, 4, eerun.b32(value))
        self.log.append(f'ray {self.vector(start)} {self.vector(end)} {ee.g("t0") & 0xffffffff:x} {mask:x} {flags:x} '
                        f'{ee.g("t3") & 0xffffffff:x}')
        self.log.append(f'={face}')
        return self.faces[face] if face >= 0 else 0

    def spheres(self, body):
        ee = self.ee
        out = []
        node = ee.u32(body + 0xd4)
        while node:
            sphere = ee.u32(node)
            out.append(f'{ee.u32(sphere):08x} {ee.u32(sphere + 4):08x} {self.vector(sphere + 0x10)}')
            node = ee.u32(node + 4)
        return out

    def state(self):
        ee, at = self.ee, self.item
        count = ee.u32(at + 0x158)
        links = ee.u32(at + 0x15c)
        out = [f'{ee.m[at + 0x1fc]:x} {count:x} {links:x} {ee.m[at + 0x204] | ee.m[at + 0x205] << 1:x} '
               f'{ee.u32(at + 0x208):08x}']
        for i in range(count):
            node = at + 0x10 + i * 0x40
            out.append(f'{self.vector(node)} {self.vector(node + 0x10)} {self.vector(node + 0x20)} '
                       f'{ee.u32(node + 0x30):08x} {self.face_index(ee.u32(node + 0x34))} '
                       f'{self.face_index(ee.u32(node + 0x38))}')
        for i in range(links):
            link = at + 0x110 + i * 12
            a, b = ((ee.u32(link + o) - at - 0x10) // 0x40 for o in (0, 4))
            out.append(f'{a:x} {b:x} {ee.u32(link + 8):08x}')
        for row in range(3):
            out.append(' '.join(f'{ee.u32(at + 0x1b0 + row * 16 + lane * 4):08x}' for lane in range(3)))
        for b in range(3):
            body = ee.u32(at + 0x1f0 + b * 4)
            if not body:
                out.append('-')
                continue
            out.append(f'{ee.m[body]:x} {ee.u32(body + 0x40):x} {ee.u32(body + 0x44):x} {ee.u32(body + 0x48):x} '
                       f'{self.vector(body + 0x90)}')
            out += self.spheres(body)
        owner = self.owner
        out.append(self.vector(owner + 0x150))
        out.append(self.vector(owner + 0x160))
        for row in range(3):
            out.append(' '.join(f'{ee.u32(owner + 0x1c0 + row * 16 + lane * 4):08x}' for lane in range(3)))
        out.append(self.vector(owner + 0x1f0))
        return ' '.join(out)

    def run(self, op):
        w = op.split()
        self.log = []
        name, ee = w[0], self.ee
        at = self.item
        args = [int(x, 16) for x in w[1:]]
        answer = [0, 0, 0, 0]
        if name == 'new':
            self.bones = [args[3 + i * 5:8 + i * 5] for i in range(args[2])]
            self.rays = 0
            ee.m[self.owner + 4:self.owner + 0x300] = bytes(0x2fc)
            for row in range(4):
                ee.wr(self.owner + 0x1c0 + row * 20, 4, eerun.b32(1.0))
            self.item = at = ee.alloc(0x210)
            ee.call(CTOR, at, args[0], args[1])
            ee.call(SETUP, at, self.owner, 0x900)
        elif name == 'world':
            for i, value in enumerate(args):
                ee.wr(self.owner + 0x1c0 + i * 4, 4, value)
        elif name == 'delta':
            self.delta = args[0]
        elif name == 'kind':
            self.kind = args[0]
        elif name == 'moving':
            ee.call(SETMOVING, at, args[0], args[1])
        elif name == 'push':
            offset = ee.alloc(16)
            for lane in range(4):
                ee.wr(offset + lane * 4, 4, args[lane])
            ee.call(PUSH, at, offset, eerun.s32(args[4]))
        elif name == 'update':
            ee.call(UPDATE, at)
        elif name == 'end':
            ee.call(ENDFRAME, at)
        elif name == 'sync':
            body = ee.u32(at + 0x1f0 + args[0] * 4)
            if body:
                ee.call(SYNC, at, body)
        elif name == 'hook':
            body = ee.u32(at + 0x1f0 + args[0] * 4)
            if body:
                normal = ee.alloc(16)
                for lane in range(4):
                    ee.wr(normal + lane * 4, 4, args[3 + lane])
                contact = ee.alloc(16)
                ee.wr(contact, 4, body)
                ee.wr(contact + 8, 4, args[1])
                face = eerun.s32(args[7])
                ee.f[12] = args[2]
                answer[0] = ee.call(HOOK0, at, contact, normal, self.faces[face] if face >= 0 else 0) & 0xffffffff
        elif name == 'mean':
            out = ee.alloc(16)
            ee.call(MEAN, at, out)
            answer = [ee.u32(out + lane * 4) for lane in range(4)]
        head = ' '.join(f'{x:x}' for x in answer)
        return f'{head} {self.state()} |' + ''.join(' ' + x for x in self.log)


def hexf(value):
    return f'{eerun.b32(value):x}'


def life(rng, n):
    weapon, touch = rng.choice(((1, 0), (0, 1), (0, 1), (1, 0), (1, 1), (0, 0)))
    count = rng.choice((2, 2, 3, 4))
    bones = []
    for _ in range(count):
        point = [rng.uniform(-40, 40) for _ in range(3)] + [1.0]
        bones.append(f'{rng.randrange(5, 60):x} ' + ' '.join(hexf(x) for x in point))
    ops = [f'new {weapon:x} {touch:x} {count:x} ' + ' '.join(bones), 'delta ' + hexf(1 / 60)]

    def world():
        turn = rng.uniform(-3, 3)
        c, s = math.cos(turn), math.sin(turn)
        rows = (c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, rng.uniform(-100, 280), rng.uniform(-200, 200),
                rng.uniform(5, 300), 1)
        return 'world ' + ' '.join(hexf(x) for x in rows)

    ops += [world(), 'moving 1 0']
    while len(ops) < n:
        kind = rng.random()
        if kind < 0.02:
            ops += [world(), 'moving 1 0', 'push ' + ' '.join(hexf(rng.uniform(-30, 30)) for _ in range(3)) + ' 0 '
                    + f'{rng.choice((0xffffffff, 0, count - 1)):x}']
        elif kind < 0.03:
            ops.append(f'moving 0 {rng.randrange(2):x}')
        elif kind < 0.05:
            ops.append('delta ' + hexf(rng.choice((1 / 60, 1 / 30, 0.2, 0.6))))
        elif kind < 0.07:
            ops.append(f'kind {rng.choice((0x17, 0x17, 0x1a)):x}')
        elif kind < 0.12:
            ops.append('mean')
        else:
            ops.append('update')
            for body in (0, 1, 2):
                ops.append(f'sync {body:x}')
            if rng.random() < 0.3:
                normal = [rng.uniform(-1, 1) for _ in range(3)]
                length = math.sqrt(sum(x * x for x in normal)) or 1.0
                normal = [x / length for x in normal] + [0.0]
                ops.append(f'hook {rng.randrange(3):x} {rng.randrange(count):x} {hexf(rng.uniform(-8, 8))} '
                           + ' '.join(hexf(x) for x in normal) + f' {rng.choice((0xffffffff, 0, 1, 2)):x}')
            ops.append('end')
    return ops


def main(argv):
    plant = '--plant' in argv
    numbers = [int(x) for x in argv[1:] if not x.startswith('--')]
    scripts = numbers[0] if numbers else 24
    n = numbers[1] if len(numbers) > 1 else 300
    exe = build(plant)
    rng = random.Random(0x23c010)
    bad = total = rays = sounds = rests = 0
    for s in range(scripts):
        ops = life(rng, n)
        (WORK / 'ops.txt').write_text('\n'.join(ops) + '\n')
        out = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                             check=True).stdout.splitlines()
        rom = Rom()
        for op, host in zip(ops, out):
            got = rom.run(op)
            total += 1
            rays += got.count(' ray ')
            sounds += got.count(' snd ')
            rests += got.count(' rot ')
            if got != host:
                bad += 1
                if bad <= 4:
                    fields = [(i, a, b) for i, (a, b) in enumerate(zip(got.split(), host.split())) if a != b]
                    print(f'script {s} op {op[:60]}\n  first different fields {fields[:6]}')
                    print(f'  rom  {got[-160:]}\n  host {host[-160:]}')
        if len(out) != len(ops):
            print('host stopped early', len(out), len(ops))
            bad += 1
    print(f'{scripts} scripts, {total} ops, {rays} rays, {sounds} landings, {rests} rests, {bad} differences')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
