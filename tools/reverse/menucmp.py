#!/usr/bin/env python3
"""game/stagemenu.cpp's list window against the ROM (offline).

python menucmp.py [scripts] [frames]

Plays random frames on the ROM's own StageMenuList (eerun.py, 0x208d40.. and the fades 0x22fea0..) and
on the host's in a small exe compiled from src/game/stagemenu.cpp and fontdata.cpp (work/menucmp/):
open, addLine with six texts, orders and enabled flags, next, prev, pick, close, draw. After every op
each side prints what it returned, the window's state and all 16 lines (float bits); each draw prints
every plate's prim, colour, UV and corners and every text's position, alpha and scale. The scripts are
frames of a menu as the game drives it: addLine several times a frame, then one key, then the draw.
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
WORK = HERE / 'work' / 'menucmp'
SRC = yamipan.REPO / 'src'
TEXTS = 6

HOST = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define private public
#include "game/stagemenu.h"
#undef private
#include "game/fontdata.h"
#include "game/clock.h"
#include "game/gamesystem.h"
#include "game/stagescene.h"
#include "game/thread.h"
#include "game/wstring.h"
#include "rena/rena_2d_texture_tim2.h"
#include "yamipan/rendering/yamipan_drawui.h"

static u_short gTexts[TEXTS][2];
static u_int bits(float f) { u_int u; memcpy(&u, &f, 4); return u; }
static int textId(const u_short *t) { return t == nullptr ? -1 : (int)((const u_short (*)[2])t - gTexts); }

// The draws: what the ROM's packet holds.
static unsigned gRgba; static unsigned long long gPrim; static int gCorner; static YamiPanUiVertex gFirst;
void YamiPan_DrawUiReg(const void *, int, uint64_t) {}
void YamiPan_DrawUiPrim(const void *, uint64_t prim, const uint8_t *c) { gPrim = prim; memcpy(&gRgba, c, 4); gCorner = 0; }
void YamiPan_DrawUiTexture(const void *, const void *) {}
void YamiPan_DrawUiSqueeze(const void *, int) {}
void YamiPan_DrawUiBegin(const void *, int) {}
void YamiPan_DrawUiVertex(const void *, const YamiPanUiVertex *v)
{
    if (gCorner++ == 0) { gFirst = *v; return; }
    printf("P %llx %08x %x %x %x %x %x %x %x %x\n", gPrim, gRgba, gFirst.u, gFirst.v, gFirst.x, gFirst.y, v->u, v->v, v->x, v->y);
}
class TestPrinter : public WideFontPrinter
{
public:
    TestPrinter() : WideFontPrinter(0, 0) {}
    int printScaledText(float scale, int x, int y, const u_short *text) override
    {
        printf("T %08x %d %d %d %08x %x %x\n", m_rgba, x, y, textId(text), bits(scale), m_flags, m_spacing);
        return 0;
    }
};

// What stagemenu.cpp's menu half reaches, not run here.
GameSystem *gGameSystem;
StageScene *gStageScene;
bool actionAllowsMenu(int) { return false; }
int StageScene::playerAction() { return 0; }
void *StageScene::getInputFocus() { return nullptr; }
int bitMask(u_int bit) { return 1 << bit; }
int wstrcmp(const u_short *a, const u_short *b) { while (*a && *a == *b) { a++; b++; } return *a - *b; }
Clock *Clock::createChild() { return nullptr; }
Clock::~Clock() {}
ClockNode::~ClockNode() {}
Tim2Texture::Tim2Texture(const void *) {}
Tim2Texture::~Tim2Texture() {}
int gfxGetVramBlock(int) { return 0; }

static void dump(StageMenuList &w)
{
    printf("W %d %d %d %d %d\n", w.m_state, w.m_closeStep, w.m_page, w.m_laidOut, w.m_lineCount);
    for (int i = 0; i < kStageMenuLines; i++) {
        const StageMenuLine &l = w.m_pages[w.m_page][i];
        const StageMenuFade &f = l.fade;
        const StageMenuTrack *t = f.color.track;
        printf("L %d %d %d %d %d %d %d %d", l.state, textId(l.text), l.order, l.enabled, l.shown, l.selected, l.changed, l.slidesIn);
        printf(" %d %d %d %d %d", l.rowChanged, l.row, l.rows, l.asked, l.unused50);
        printf(" %08x %08x %08x %08x %08x %08x", bits(f.fromX), bits(f.fromY), bits(f.toX), bits(f.toY), bits(f.x), bits(f.y));
        printf(" %d %d %d %d %d", f.moveFrame, f.moveFrames, f.color.frame, f.color.from, f.color.to);
        if (t != nullptr)
            printf(" %d %08x %08x %d\n", t->frames, t->colors[0], t->colors[t->frames - 1], t->showsText);
        else
            printf(" -\n");
    }
}

int main(int, char **argv)
{
    for (int i = 0; i < TEXTS; i++)
        gTexts[i][0] = 'a' + i;
    TestPrinter printer;
    StageMenuList *w = new StageMenuList(&printer, 0);
    FILE *f = fopen(argv[1], "r");
    char op;
    int a, b, c;
    while (fscanf(f, " %c %d %d %d", &op, &a, &b, &c) == 4) {
        printf("%c\n", op);
        switch (op) {
        case 'O': w->m_lineCount = 0; if (w->m_state == 0) { w->reset(); w->m_state = 1; } break;
        case 'C': if (w->m_state != 0 && w->m_state != 3) { w->m_state = 3; w->close(false); } break;
        case 'A': printf("R %d\n", (int)w->addLine(gTexts[a], b, c != 0)); break;
        case 'N': printf("R %d\n", (int)w->selectNext()); break;
        case 'B': printf("R %d\n", (int)w->selectPrev()); break;
        case 'K': printf("R %d\n", textId(w->pick())); break;
        case 'D': w->draw((sceVif1Packet *)1, nullptr); break;
        }
        dump(*w);
    }
    return 0;
}
'''.replace('TEXTS', str(TEXTS))


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        self.out = []
        self.gs = []
        for name in ('sceVif1PkCnt', 'sceVif1PkAddCode', 'sceVif1PkOpenDirectCode', 'sceVif1PkOpenGifTag',
                     'sceVif1PkCloseGifTag', 'sceVif1PkCloseDirectCode', 'sceVif1PkCloseDmaTag',
                     'sceVif1PkAlign'):
            ee.hook(name, lambda e: 0)
        ee.hook('sceVif1PkAddGsData', self.gsdata)
        self.packet = ee.alloc(0x40)
        self.buffer = ee.alloc(0x1000)
        self.texts = []
        for i in range(TEXTS):
            t = ee.alloc(4)
            ee.write(t, struct.pack('<HH', ord('a') + i, 0))
            self.texts.append(t)
        # The printer: its vptr at +0x10, slots 3 (+0x14), 2 (+0x10), 11 (+0x34).
        self.printer = ee.alloc(0x60)
        vtable = ee.alloc(0x60)
        ee.wr(self.printer + 0x10, 4, vtable)
        for slot, at, fn in ((0x14, 0x2000, lambda e: 0), (0x10, 0x2010, lambda e: 0), (0x34, 0x2020, self.text)):
            ee.wr(vtable + slot, 4, at)
            ee.hooks[at] = fn
        self.w = ee.alloc(0xaa8)
        ee.call(0x20ac30, self.w, self.printer, 0)

    def gsdata(self, ee):
        self.gs.append(ee.g('a1') & 0xffffffffffffffff)
        if len(self.gs) == 6:
            prim, rgba, uv0, xy0, uv1, xy1 = self.gs
            self.gs = []
            self.out.append('P %x %08x %x %x %x %x %x %x %x %x' % (
                prim, rgba & 0xffffffff, uv0 & 0xffff, uv0 >> 16 & 0xffff, xy0 & 0xffff, xy0 >> 16 & 0xffff,
                uv1 & 0xffff, uv1 >> 16 & 0xffff, xy1 & 0xffff, xy1 >> 16 & 0xffff))
        return 0

    def text(self, ee):
        p = eerun.s32(ee.g('a0'))
        x, y, t = eerun.s32(ee.g('a1')), eerun.s32(ee.g('a2')), ee.g('a3') & 0xffffffff
        self.out.append('T %08x %d %d %d %08x %x %x' % (ee.u32(p), x, y, self.textid(t), ee.f[12], ee.u32(p + 0xc), ee.u32(p + 0x38)))
        return 0

    def textid(self, t):
        return self.texts.index(t) if t in self.texts else -1

    def dump(self):
        ee, w = self.ee, self.w
        page = ee.u32(w + 0x14)
        self.out.append('W %d %d %d %d %d' % (ee.u32(w), ee.u32(w + 4), page, ee.rd(w + 0xa98, 1), ee.u32(w + 0xaa4)))
        for i in range(16):
            at = w + 0x18 + page * 0x540 + i * 0x54
            u = lambda o: eerun.s32(ee.u32(at + o))
            b = lambda o: ee.rd(at + o, 1)
            track = ee.u32(at + 0x34)
            s = 'L %d %d %d %d %d %d %d %d' % (u(0), self.textid(ee.u32(at + 4)), u(8), b(0xc), b(0xd), b(0xe), b(0xf), b(0x40))
            s += ' %d %d %d %d %d' % (b(0x41), u(0x44), u(0x48), u(0x4c), u(0x50))
            s += ' ' + ' '.join('%08x' % ee.u32(at + o) for o in (0x10, 0x14, 0x18, 0x1c, 0x20, 0x24))
            s += ' %d %d %d %d %d' % (u(0x28), u(0x2c), u(0x30), u(0x38), u(0x3c))
            if track:
                n, colors = ee.u32(track), ee.u32(track + 4)
                s += ' %d %08x %08x %d' % (n, ee.u32(colors), ee.u32(colors + (n - 1) * 4), 1 if ee.rd(track + 8, 1) else 0)
            else:
                s += ' -'
            self.out.append(s)

    def op(self, o, a, b, c):
        ee, w = self.ee, self.w
        self.out.append(o)
        if o == 'O':
            ee.wr(w + 0xaa4, 4, 0)
            if ee.u32(w) == 0:
                ee.call(0x20aaf0, w)
                ee.wr(w, 4, 1)
        elif o == 'C':
            if ee.u32(w) not in (0, 3):
                ee.wr(w, 4, 3)
                ee.call(0x208f40, w, 0)
        elif o == 'A':
            self.out.append('R %d' % ee.call(0x209d90, w, self.texts[a], b, c))
        elif o == 'N':
            self.out.append('R %d' % ee.call(0x209870, w))
        elif o == 'B':
            self.out.append('R %d' % ee.call(0x209b00, w))
        elif o == 'K':
            self.out.append('R %d' % self.textid(ee.call(0x209710, w) & 0xffffffff))
        elif o == 'D':
            ee.wr(self.packet, 4, self.buffer)
            ee.call(0x209080, w, self.packet, 0)
        self.dump()


def script(rng, frames):
    ops = []
    chosen = {}
    is_open = False
    for frame in range(frames):
        if not is_open:
            if rng.random() < 0.2:
                ops.append(('O', 0, 0, 0))
                is_open = True
                chosen = {t: (rng.randrange(1, 9), rng.random() < 0.8)
                          for t in rng.sample(range(TEXTS), rng.randrange(1, TEXTS + 1))}
        else:
            if rng.random() < 0.08:
                t = rng.randrange(TEXTS)
                if t in chosen and len(chosen) > 1:
                    del chosen[t]
                else:
                    chosen[t] = (rng.randrange(1, 9), rng.random() < 0.8)
            for t, (order, enabled) in chosen.items():
                ops.append(('A', t, order, int(enabled)))
            r = rng.random()
            if r < 0.1:
                ops.append(('N', 0, 0, 0))
            elif r < 0.2:
                ops.append(('B', 0, 0, 0))
            elif r < 0.23:
                ops.append(('K', 0, 0, 0))
                is_open = False
            elif r < 0.26:
                ops.append(('C', 0, 0, 0))
                is_open = False
        ops.append(('D', 0, 0, 0))
    return ops


def main():
    scripts = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    frames = int(sys.argv[2]) if len(sys.argv) > 2 else 150
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++20', '-O1', '-w', '-I', str(SRC),
                    str(WORK / 'host.cpp'), str(SRC / 'game' / 'stagemenu.cpp'),
                    str(SRC / 'game' / 'fontdata.cpp'), '-o', str(exe)],
                   check=True)
    rom = Rom()
    total = differ = plates = texts = picks = 0
    for n in range(scripts):
        ops = script(random.Random(n), frames)
        (WORK / 'ops.txt').write_text(''.join('%s %d %d %d\n' % o for o in ops))
        host = subprocess.run([str(exe), str(WORK / 'ops.txt')], capture_output=True, text=True,
                              timeout=120).stdout.split('\n')
        rom.out = []
        rom.w = rom.ee.alloc(0xaa8)     # the host's exe starts anew too
        rom.ee.call(0x20ac30, rom.w, rom.printer, 0)
        for o in ops:
            rom.op(*o)
        ours = host[:-1] if host and host[-1] == '' else host
        plates += sum(line.startswith('P ') for line in rom.out)
        texts += sum(line.startswith('T ') for line in rom.out)
        picks += sum(rom.out[i - 1] == 'K' and rom.out[i] != 'R -1' for i in range(1, len(rom.out)))
        if len(ours) != len(rom.out):
            print(f'script {n}: {len(rom.out)} ROM lines, {len(ours)} host lines')
        for i, (a, b) in enumerate(zip(rom.out, ours)):
            total += 1
            if a != b:
                differ += 1
                if differ <= 6:
                    print(f'script {n} line {i}:\n  rom  {a}\n  host {b}')
        print(f'script {n}: {len(ops)} ops, {len(rom.out)} lines', flush=True)
    print(f'{scripts} scripts, {total} lines ({plates} plates, {texts} texts, {picks} picks), {differ} differ')
    return 1 if differ else 0


if __name__ == '__main__':
    sys.exit(main())
