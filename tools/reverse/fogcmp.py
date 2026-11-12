#!/usr/bin/env python3
"""game/stagefog.cpp against the ROM (offline).

python fogcmp.py [scripts] [ops] [--plant]

Random frames of both fogs on the ROM's own code (eerun.py, 0x2ce0c0..0x2cedb0) and on the host's in a
small exe compiled from src/game/stagefog.cpp (work/fogcmp/). The ROM's list code runs for real; effects,
characters, items and bones are blocks with only the fields the file reads, on the host calloc'd real
classes. StageNodeTree::spawn, the particles' setters, vecDistance, sceneRenderGroup, the items' two
tests and the brain's pace are stand-ins on both sides and log their calls, floats as bits. After each
op both print the log, every fog (owner, shown, the effect's bone and tree parents, the glow's distance
as bits), the sources, the render's two fields and the child lists of each bone and each character's
tree node, in order. The first differences and a count go out. ~SmokeFog runs but only its lack of a
crash is compared, and a bad m_itemDef is not tried (the ROM reads address 0x10, the host the first
record). --plant compiles the host with three bugs, to prove the tool sees them.
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
WORK = HERE / 'work' / 'fogcmp'
SRC = yamipan.REPO / 'src'

SET_ACTIVE_FOR, STORE_ACTIVE, UPDATE_RANGE, ADD_OBJECT, ATTACH_CHARACTERS, SMOKE_DTOR, SMOKE_CREATE = (
    0x2ce0c0, 0x2ce180, 0x2ce1c0, 0x2ce360, 0x2ce3e0, 0x2ce5a0, 0x2ce620)
UPDATE_VISIBLE, UPDATE_FOR, GLOW_CREATE, SET_ACTIVE, ATTACH_TO = (0x2ce790, 0x2ce820, 0x2ce9d0, 0x2ceb70, 0x2cebd0)
RECORDS = 0x539b10
CHARACTERS, ITEMS = 12, 7
RENDER_SIZE = 0xe30
KINDS = (0, 2, 3, 5, 5, 7, 9, 0xb, 0xc, 0xc, 0xe, 0xf)
RANGES = (100.0, 400.0, 400.0, 900.0, 0.0)

PLANTS = (('        if (inSmoke && owner == linked)\n            show = true;\n',
           '        show = inSmoke && owner == linked;\n'),
          ('if (m_fogs[0]->m_owner != nullptr)', 'if (m_fogs[1]->m_owner != nullptr)'),
          ('pace != BrainState_Slot4c && pace != BrainState_Slot4b', 'pace != BrainState_Slot4b'))

HOST = r'''
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#define private public
#include "game/stagefog.h"
#include "game/characterbrain.h"
#include "game/gamehooks.h"
#include "game/scenerender.h"
#include "game/stageitem.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/sysdata.h"
#include "game/vu0math.h"
#undef private

constexpr int kCharacters = 12, kItems = 7;
static std::string gLog;
static std::map<const void *, int> gIds;
static int gSelect, gNull, gNextEffect = 100;
static int gPace[kCharacters + 1], gSmoking[kItems + 1], gInArea[kItems + 1];
static StageCharacter *gChars[kCharacters + 1];
static StageThrownItem *gItems[kItems + 1];
static StageNodeBase *gBones[kCharacters + 1];
static StageObjectTree *gTrees[kCharacters + 1];
static StageModel *gModels[kCharacters + 1];
StageObjRecord gStageObjRecords[kStageObjRecordSets][kStageObjRecordCount];

static void logf(const char *format, unsigned a = 0, unsigned b = 0)
{
    char line[64];
    sprintf(line, format, a, b);
    gLog += line;
}
static int idOf(const void *p) { return p == nullptr ? 0 : gIds.count(p) ? gIds[p] : -1; }
static unsigned bitsOf(float value) { unsigned u; memcpy(&u, &value, 4); return u; }

const StageObjRecord *stageObjRecord(int index)
{
    const int set = gSelect == 2 ? 2 : gSelect == 0 ? 0 : 1;
    return &gStageObjRecords[set][index];
}
void StageParticle::setVisible(bool on) { logf(" V%d %d", idOf(this), on); }
void StageParticle::setRunning(bool on) { logf(" R%d %d", idOf(this), on); }
int  sceneRenderGroup(int mode) { logf(" G%d", mode); return 0x1000 + mode; }
int  StageCharacter::getBrainPace() { logf(" P%d", idOf(this)); return gPace[idOf(this)]; }
bool StageThrownItem::isSmoking() { logf(" S%d", idOf(this)); return gSmoking[idOf(this) - 50] != 0; }
bool StageThrownItem::isInSmokeArea() { logf(" A%d", idOf(this)); return gInArea[idOf(this) - 50] != 0; }
float vecDistance(const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    double sum = 0;
    for (int i = 0; i < 3; i++)
        sum += ((double)a[i] - b[i]) * ((double)a[i] - b[i]);
    const float distance = (float)sqrt(sum);
    logf(" D%x", bitsOf(distance));
    return distance;
}
StageParticle *StageNodeTree::spawn(const char *name, StageNodeBase *parent, const float *pos, const float *rot,
                                    int arg, int arg2)
{
    gLog += " new "; gLog += name;
    logf(" %x %x", (unsigned)(parent != nullptr), bitsOf(pos[3]));
    logf(" %x %x", bitsOf(pos[0]), bitsOf(pos[1]));
    logf(" %x %x", bitsOf(pos[2]), bitsOf(rot[0]));
    logf(" %x %x", bitsOf(rot[1]), bitsOf(rot[2]));
    logf(" %x %x", bitsOf(rot[3]), (unsigned)(arg + arg2));
    if (gNull)
        return nullptr;
    StageParticle *effect = (StageParticle *)calloc(1, sizeof(StageParticle));
    effect->m_tree = (StageObjectTree *)calloc(1, sizeof(StageObjectTree));
    gIds[effect] = gNextEffect;
    gIds[effect->m_tree] = gNextEffect + 500;
    gNextEffect++;
    return effect;
}

static void dumpFog(const CharacterFog *fog)
{
    StageParticle *effect = fog->m_effect;
    printf(" (%d %d %d", idOf(effect), fog->m_shown != 0, idOf(fog->m_owner));
    if (effect != nullptr)
        printf(" %d %d", idOf(effect->m_parent), idOf(effect->m_tree->m_parent));
    printf(")");
}

int main(int, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    SceneRender *render = (SceneRender *)calloc(1, sizeof(SceneRender));
    StageObjectList *objects = (StageObjectList *)calloc(1, sizeof(StageObjectList));
    StageObjArray &list = objects->m_lists[StageObjGroup_Characters];
    list.m_data = (StageObject **)calloc(kCharacters + 2, sizeof(StageObject *));
    list.m_count = kCharacters;
    for (int i = 1; i <= kCharacters; i++) {
        gChars[i] = (StageCharacter *)calloc(1, sizeof(StageCharacter));
        gIds[gChars[i]] = i;
        list.m_data[i] = gChars[i];
        gChars[i]->m_itemDef = i;
        gBones[i] = (StageNodeBase *)calloc(1, sizeof(StageObject));
        gIds[gBones[i]] = 200 + i;
        gTrees[i] = (StageObjectTree *)calloc(1, sizeof(StageObjectTree));
        gIds[gTrees[i]] = 300 + i;
        gChars[i]->m_tree = gTrees[i];
        gModels[i] = (StageModel *)calloc(1, sizeof(StageModel));
        if ((i & 1) == 0) {
            gChars[i]->m_glow = (StageParticle *)calloc(1, sizeof(StageParticle));
            gIds[gChars[i]->m_glow] = 20 + i;
        }
    }
    for (int i = 1; i <= kItems; i++) {
        gItems[i] = (StageThrownItem *)calloc(1, sizeof(StageThrownItem));
        gIds[gItems[i]] = 50 + i;
        strcpy(gItems[i]->m_name, i < 3 ? "I_NUT" : "I_HATUEN");
    }
    gNull = 0;
    SmokeFog *smoke = nullptr;
    GlowFog *glow = nullptr;
    StageNodeTree *tree = (StageNodeTree *)(uintptr_t)16;
    char op[20];
    while (fscanf(f, "%19s", op) == 1) {
        gLog.clear();
        unsigned a = 0, b = 0, c = 0, d = 0, e = 0, g = 0;
        float x, y, z;
        #define CHR(i) ((StageCharacter *)((i) ? gChars[i] : nullptr))
        if (!strcmp(op, "select")) { fscanf(f, "%u", &a); gSelect = (int)a; }
        else if (!strcmp(op, "nulls")) { fscanf(f, "%u", &a); gNull = (int)a; }
        else if (!strcmp(op, "char")) {
            fscanf(f, "%u %u %u %u %d %u %u %f %f %f", &a, &b, &c, &d, &gPace[0], &e, &g, &x, &y, &z);
            StageCharacter *who = gChars[a];
            for (int set = 0; set < 3; set++)
                gStageObjRecords[set][a].kind = set == (gSelect == 2 ? 2 : gSelect == 0 ? 0 : 1) ? (int)b : (int)0xe;
            who->m_active = (u_char)c;
            who->m_endurance = (int)d - 2;
            gPace[a] = gPace[0];
            who->m_model = e ? gModels[a] : nullptr;
            StageNodeBase *bone = gBones[a];
            if (g) { if (gModels[a]->boneMap.first == nullptr) gModels[a]->boneMap.insert("bn_head", bone); }
            else if (gModels[a]->boneMap.root != nullptr) gModels[a]->boneMap.root->value = nullptr;
            if (g && gModels[a]->boneMap.root != nullptr) gModels[a]->boneMap.root->value = bone;
            who->m_xform.position[0] = x; who->m_xform.position[1] = y; who->m_xform.position[2] = z;
        }
        else if (!strcmp(op, "item")) {
            fscanf(f, "%u %u %u %f %f %f", &a, &b, &c, &x, &y, &z);
            gSmoking[a] = (int)b; gInArea[a] = (int)c;
            gItems[a]->m_xform.position[0] = x; gItems[a]->m_xform.position[1] = y; gItems[a]->m_xform.position[2] = z;
        }
        else if (!strcmp(op, "smoke")) {
            fscanf(f, "%u", &a);
            if (smoke == nullptr) smoke = SmokeFog::createFor(gItems[a], tree);
        }
        else if (!strcmp(op, "glow")) {
            fscanf(f, "%u", &a);
            if (glow == nullptr) glow = GlowFog::createFor(gChars[a], tree);
        }
        else if (!strcmp(op, "frame")) {
            if (smoke != nullptr) smoke->m_sourceCount = 0;
            if (glow != nullptr) glow->m_unmeasured = 1;
        }
        else if (!strcmp(op, "add")) {
            fscanf(f, "%u", &a);
            if (smoke != nullptr) smoke->addObject(gItems[a]);
        }
        else if (!strcmp(op, "measure")) {
            fscanf(f, "%u %u", &a, &b);
            if (glow != nullptr) glow->updateFor(gChars[a], CHR(b));
        }
        else if (!strcmp(op, "attach")) {
            if (smoke != nullptr) smoke->attachCharacters(objects);
        }
        else if (!strcmp(op, "activefor")) {
            fscanf(f, "%u", &a);
            if (smoke != nullptr) smoke->setActiveFor(CHR(a), render);
        }
        else if (!strcmp(op, "range")) {
            fscanf(f, "%f %u", &x, &a);
            if (smoke != nullptr) smoke->updateRange(x, CHR(a), render);
        }
        else if (!strcmp(op, "store")) {
            fscanf(f, "%u", &a);
            if (smoke != nullptr) smoke->storeActive(CHR(a));
        }
        else if (!strcmp(op, "setactive")) {
            fscanf(f, "%u", &a);
            if (glow != nullptr) glow->setActive(a != 0, render);
        }
        else if (!strcmp(op, "visible")) {
            if (glow != nullptr) glow->updateVisible(render);
        }
        else if (!strcmp(op, "attachto")) {
            fscanf(f, "%u", &a);
            if (glow != nullptr) glow->attachTo(CHR(a));
        }
        else if (!strcmp(op, "shown")) {
            fscanf(f, "%u", &a);
            if (glow != nullptr) glow->m_shown = (u_char)a;
        }
        else if (!strcmp(op, "drop")) {
            delete smoke;
            smoke = nullptr;
        }

        printf("%s |", gLog.c_str());
        if (smoke != nullptr) {
            printf(" smoke %d", smoke->m_sourceCount);
            for (int i = 0; i < smoke->m_sourceCount; i++)
                printf(" %d", idOf(smoke->m_sources[i]));
            for (int i = 0; i < kSmokeFogs; i++)
                dumpFog(smoke->m_fogs[i]);
        }
        if (glow != nullptr) {
            printf(" glow %d", glow->m_unmeasured != 0);
            if (glow->m_unmeasured == 0)
                printf(" %x", bitsOf(glow->m_nearest));
            dumpFog(glow);
        }
        printf(" | %d %x |", render->m_snap, (unsigned)render->m_forcedSet);
        render->m_snap = 0;
        render->m_forcedSet = 0;
        for (int i = 1; i <= kCharacters; i++) {
            for (StageNodeList::Node *n = gBones[i]->m_children.head; n != nullptr; n = n->next)
                printf(" b%d:%d", i, idOf(n->value));
            for (StageList<StageObjectTree *>::Node *n = gTrees[i]->m_children.head; n != nullptr; n = n->next)
                printf(" t%d:%d", i, idOf(n->value));
        }
        printf("\n");
    }
    return 0;
}
'''


def build(plant):
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    source = (SRC / 'game' / 'stagefog.cpp').read_text(encoding='utf-8').replace('\r\n', '\n')
    if plant:
        for bug, wrong in PLANTS:
            assert bug in source, bug
            source = source.replace(bug, wrong)
    (WORK / 'stagefog.cpp').write_text(source, encoding='utf-8')
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-fpermissive', '-I', str(SRC),
                    '-I', str(SRC / 'sdk'), str(WORK / 'host.cpp'), str(WORK / 'stagefog.cpp'), '-o', str(exe)],
                   check=True)
    return exe


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        self.log = []
        self.ids = {0: 0}
        self.select = 0
        self.nulls = 0
        self.next_effect = 100
        self.smoke = self.glow = 0
        self.pace = {}
        self.smoking = {}
        self.in_area = {}
        self.render = ee.alloc(RENDER_SIZE)
        self.select_block = ee.alloc(0x10)
        self.table = ee.alloc(0x280)
        ee.wr(self.table + 0x278, 4, 0xfff000)
        ee.hooks[0xfff000] = self.get_pace
        self.objects = ee.alloc(0xdc)
        data = ee.alloc(4 * (CHARACTERS + 2))
        ee.wr(self.objects + 0xa0, 4, data)
        ee.wr(self.objects + 0xa4, 2, CHARACTERS)
        self.chars, self.bones, self.trees, self.nodes, self.items = {}, {}, {}, {}, {}
        for i in range(1, CHARACTERS + 1):
            who = self.chars[i] = ee.alloc(0x800)
            ee.wr(who, 4, self.table)
            ee.wr(data + 4 * i, 4, who)
            ee.wr(who + 0x200, 4, i)
            self.ids[who] = i
            if i & 1 == 0:
                lamp = ee.alloc(0x280)
                ee.wr(who + 0x410, 4, lamp)
                self.ids[lamp] = 20 + i
            bone = self.bones[i] = ee.alloc(0x40)
            self.ids[bone] = 200 + i
            tree = self.trees[i] = ee.alloc(0x20)
            self.ids[tree] = 300 + i
            ee.wr(who + 0x38, 4, tree)
            model = ee.alloc(0x20)
            self.nodes[i] = (model, ee.alloc(0x14))
        for i in range(1, ITEMS + 1):
            item = self.items[i] = ee.alloc(0x200)
            self.ids[item] = 50 + i
            ee.write(item + 0x20, (b'I_NUT' if i < 3 else b'I_HATUEN') + b'\0')
        for name, fn in (('StageParticle::setVisible', self.visible), ('StageParticle::setRunning', self.running),
                         ('StageNodeTree::spawn', self.spawn), ('sceneRenderGroup', self.group),
                         ('vecDistance', self.distance), ('StageThrownItem::isSmoking', self.is_smoking),
                         ('StageThrownItem::isInSmokeArea', self.is_in_area), ('getStageSelect', self.get_select),
                         ('StringMap<StageNodeBase>::find', self.find)):
            ee.hook(name, fn)

    def a(self, reg):
        return self.ee.g(reg) & 0xffffffff

    def visible(self, ee):
        self.log.append(f'V{self.ids[self.a("a0")]} {int(self.a("a1") != 0)}')
        return 0

    def running(self, ee):
        self.log.append(f'R{self.ids[self.a("a0")]} {int(self.a("a1") != 0)}')
        return 0

    def get_pace(self, ee):
        who = self.ids[self.a('a0')]
        self.log.append(f'P{who}')
        return self.pace[who]

    def group(self, ee):
        self.log.append(f'G{self.a("a0")}')
        return 0x1000 + self.a('a0')

    def get_select(self, ee):
        ee.wr(self.select_block + 4, 4, self.select)
        return self.select_block

    def is_smoking(self, ee):
        who = self.ids[self.a('a0')]
        self.log.append(f'S{who}')
        return self.smoking[who - 50]

    def is_in_area(self, ee):
        who = self.ids[self.a('a0')]
        self.log.append(f'A{who}')
        return self.in_area[who - 50]

    def distance(self, ee):
        a, b = self.a('a0'), self.a('a1')
        p = [eerun.f32(ee.u32(a + 4 * i)) for i in range(3)]
        q = [eerun.f32(ee.u32(b + 4 * i)) for i in range(3)]
        bits = eerun.b32(math.sqrt(sum((x - y) ** 2 for x, y in zip(p, q))))
        self.log.append(f'D{bits:x}')
        ee.f[0] = bits
        return 0

    def find(self, ee):
        return self.a('a0')

    def spawn(self, ee):
        name = bytes(ee.m[self.a('a1'):ee.m.index(0, self.a('a1'))]).decode('ascii')
        pos, rot = self.a('a3'), self.a('t0')
        p = [ee.u32(pos + 4 * i) for i in range(4)]
        r = [ee.u32(rot + 4 * i) for i in range(4)]
        self.log.append(f'new {name} {int(self.a("a2") != 0):x} {p[3]:x} {p[0]:x} {p[1]:x} {p[2]:x} {r[0]:x} '
                        f'{r[1]:x} {r[2]:x} {r[3]:x} {(self.a("t1") + self.a("t2")) & 0xffffffff:x}')
        if self.nulls:
            return 0
        effect = ee.alloc(0x280)
        tree = ee.alloc(0x20)
        ee.wr(effect + 0x38, 4, tree)
        self.ids[effect] = self.next_effect
        self.ids[tree] = self.next_effect + 500
        self.next_effect += 1
        return effect

    def fog(self, at):
        ee = self.ee
        effect = ee.u32(at)
        text = f' ({self.ids[effect]} {int(ee.m[at + 4] != 0)} {self.ids[ee.u32(at + 8)]}'
        if effect:
            text += f' {self.ids[ee.u32(effect + 4)]} {self.ids[ee.u32(ee.u32(effect + 0x38))]}'
        return text + ')'

    def children(self, head, tag, i):
        ee = self.ee
        out = ''
        node = ee.u32(head)
        while node:
            out += f' {tag}{i}:{self.ids[ee.u32(ee.u32(node))]}'
            node = ee.u32(node + 4)
        return out

    def run(self, op):
        w = op.split()
        name, ee = w[0], self.ee
        self.log = []
        smoke, glow, render = self.smoke, self.glow, self.render
        chr_ = lambda i: self.chars[i] if i else 0
        if name == 'select':
            self.select = int(w[1])
        elif name == 'nulls':
            self.nulls = int(w[1])
        elif name == 'char':
            i, kind, active, endurance, pace, model, head = (int(x) for x in w[1:8])
            chosen = 2 if self.select == 2 else 0 if self.select == 0 else 1
            for s in range(3):
                ee.wr(RECORDS + s * 0x13b00 + i * 0xfc + 0x10, 4, kind if s == chosen else 0xe)
            who = self.chars[i]
            ee.m[who + 0x34] = active
            ee.wr(who + 0x598, 4, (endurance - 2) & 0xffffffff)
            self.pace[i] = pace
            block, node = self.nodes[i]
            ee.wr(who + 0x30, 4, block if model else 0)
            ee.wr(block + 0x14, 4, node if head else 0)
            ee.wr(node, 4, self.bones[i])
            for k in range(3):
                ee.wr(who + 0x150 + 4 * k, 4, eerun.b32(float(w[8 + k])))
        elif name == 'item':
            i = int(w[1])
            self.smoking[i], self.in_area[i] = int(w[2]), int(w[3])
            for k in range(3):
                ee.wr(self.items[i] + 0x150 + 4 * k, 4, eerun.b32(float(w[4 + k])))
        elif name == 'smoke':
            if not smoke:
                self.smoke = smoke = ee.call(SMOKE_CREATE, self.items[int(w[1])], 16) & 0xffffffff
        elif name == 'glow':
            if not glow:
                self.glow = glow = ee.call(GLOW_CREATE, self.chars[int(w[1])], 16) & 0xffffffff
        elif name == 'frame':
            if smoke:
                ee.wr(smoke + 0x14, 4, 0)
            if glow:
                ee.m[glow + 0xc] = 1
        elif name == 'add':
            if smoke:
                ee.call(ADD_OBJECT, smoke, self.items[int(w[1])])
        elif name == 'measure':
            if glow:
                ee.call(UPDATE_FOR, glow, self.chars[int(w[1])], chr_(int(w[2])))
        elif name == 'attach':
            if smoke:
                ee.call(ATTACH_CHARACTERS, smoke, self.objects)
        elif name == 'activefor':
            if smoke:
                ee.call(SET_ACTIVE_FOR, smoke, chr_(int(w[1])), render)
        elif name == 'range':
            if smoke:
                ee.f[12] = eerun.b32(float(w[1]))
                ee.call(UPDATE_RANGE, smoke, chr_(int(w[2])), render)
        elif name == 'store':
            if smoke:
                ee.call(STORE_ACTIVE, smoke, chr_(int(w[1])))
        elif name == 'setactive':
            if glow:
                ee.call(SET_ACTIVE, glow, int(w[1]), render)
        elif name == 'visible':
            if glow:
                ee.call(UPDATE_VISIBLE, glow, render)
        elif name == 'attachto':
            if glow:
                ee.call(ATTACH_TO, glow, chr_(int(w[1])))
        elif name == 'shown':
            if glow:
                ee.m[glow + 4] = int(w[1])
        elif name == 'drop':
            if smoke:
                ee.call(SMOKE_DTOR, smoke, 1)
            self.smoke = smoke = 0

        text = ''.join(' ' + x for x in self.log) + ' |'
        if smoke:
            count = ee.u32(smoke + 0x14)
            text += f' smoke {count}'
            for i in range(count):
                text += f' {self.ids[ee.u32(smoke + 4 * i)]}'
            for i in range(8):
                text += self.fog(ee.u32(smoke + 0x18 + 4 * i))
        if glow:
            text += f' glow {int(ee.m[glow + 0xc] != 0)}'
            if not ee.m[glow + 0xc]:
                text += f' {ee.u32(glow + 0x10):x}'
            text += self.fog(glow)
        text += f' | {ee.m[render + 0xe24]} {ee.u32(render + 0xe28):x} |'
        ee.m[render + 0xe24] = 0
        ee.wr(render + 0xe28, 4, 0)
        for i in range(1, CHARACTERS + 1):
            text += self.children(self.bones[i] + 8, 'b', i) + self.children(self.trees[i] + 4, 't', i)
        return text


def char_op(rng, i):
    near = rng.random() < 0.9
    spot = [rng.uniform(-600, 600) if near else rng.uniform(-5000, 5000) for _ in range(3)]
    return (f'char {i} {rng.choice(KINDS)} {int(rng.random() < 0.85)} {rng.randrange(5)} {rng.randrange(8)} '
            f'{int(rng.random() < 0.9)} {int(rng.random() < 0.9)} {spot[0]:.3f} {spot[1]:.3f} {spot[2]:.3f}')


def item_op(rng, i):
    spot = [rng.uniform(-600, 600) for _ in range(3)]
    return f'item {i} {int(rng.random() < 0.7)} {int(rng.random() < 0.8)} {spot[0]:.3f} {spot[1]:.3f} {spot[2]:.3f}'


def script(rng, n):
    ops = [f'select {rng.randrange(4)}', f'nulls {int(rng.random() < 0.1)}']
    ops += [char_op(rng, i) for i in range(1, CHARACTERS + 1)]
    ops += [item_op(rng, i) for i in range(1, ITEMS + 1)]
    ops += [f'smoke {i}' for i in (1, rng.randrange(1, ITEMS + 1), 3)]
    ops += [f'glow {i}' for i in rng.sample(range(1, CHARACTERS + 1), CHARACTERS)]
    ops.append('attach')
    while len(ops) < n:
        for _ in range(rng.randrange(3)):
            ops.append(char_op(rng, rng.randrange(1, CHARACTERS + 1)))
        if rng.random() < 0.5:
            ops.append(item_op(rng, rng.randrange(1, ITEMS + 1)))
        linked = rng.choice((0, 1, 1, 1, 1, 1, 2, 3, rng.randrange(1, CHARACTERS + 1)))
        player = rng.choice((0, 1, 1, 1, 1, 1, 1, rng.randrange(1, CHARACTERS + 1)))
        on = int(linked != 0 and linked == player)
        ops.append('frame')
        for i in range(1, ITEMS + 1):
            ops.append(f'add {rng.randrange(1, ITEMS + 1) if rng.random() < 0.2 else i}')
        for i in range(1, CHARACTERS + 1):
            ops.append(f'measure {i} {player}')
        ops += ['attach', f'activefor {linked}', f'range {rng.choice(RANGES)} {linked}', f'store {linked}',
                f'setactive {on}']
        if on:
            ops.append('visible')
        ops += [f'attachto {player}', f'shown {on}']
    ops.append('drop')
    return ops


def main(argv):
    plant = '--plant' in argv
    numbers = [int(x) for x in argv[1:] if not x.startswith('--')]
    scripts = numbers[0] if numbers else 40
    n = numbers[1] if len(numbers) > 1 else 800
    exe = build(plant)
    rng = random.Random(0x2ce0c0)
    bad = total = shown = glowing = 0
    for s in range(scripts):
        ops = script(rng, n)
        (WORK / 'ops.txt').write_text('\n'.join(ops) + '\n')
        run = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True)
        out = run.stdout.splitlines()
        rom = Rom()
        for op, host in zip(ops, out):
            got = rom.run(op)
            total += 1
            shown += ' G2' in got
            glowing += ' G1' in got
            if ' '.join(got.split()) != ' '.join(host.split()):
                bad += 1
                if bad <= 5:
                    print(f'{op}\n  rom  {got[:400]}\n  host {host[:400]}')
        if len(out) != len(ops):
            print('host stopped early', len(out), len(ops), run.returncode)
            bad += 1
    print(f'{scripts} scripts, {total} ops, smoke shown {shown} times, glow fog {glowing} times, {bad} differences')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
