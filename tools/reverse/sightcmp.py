#!/usr/bin/env python3
"""game/menuprogset.cpp against the ROM (offline).

python sightcmp.py [frames] [--plant]

The same random sights on both: the ROM's MenuProgSet::draw (0x2bb260) on eerun.py and the host's in a small
exe compiled from src/game/menuprogset.cpp and vu0math.cpp (work/sightcmp/). Each frame draws state 1 or 2
at a random zoom with random texture sizes. The ROM's GS packet is walked (DMA tag, VIF code, GIF tag, A+D or
REGLIST) into one row per vertex; the host prints the same row from the YamiPan_DrawUi* calls. Compared:
texture, ALPHA_1, TEST_1, PRIM, RGBAQ, UV and XYZ of every vertex, in order. Then quatBetween on random
vectors, unit or not, and on opposite ones, as bits.

Two things differ on purpose and are handled here, not hidden in the source:
- the mask's far end: the host stops at the outer texel (span), the ROM one past it (span - 1). The host's
  copy is compiled with the ROM's value, so every other number is still compared.
- sightBrighten: eight sprites 64 wide in the ROM, one gsFillRect in the host: the sprites must tile the
  frame with the fill's colour and blend.
--plant compiles the host with two bugs in it, to prove the tool sees them.
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
WORK = HERE / 'work' / 'sightcmp'
SRC = yamipan.REPO / 'src'
SOURCE = SRC / 'game' / 'menuprogset.cpp'
SYSTEM_GP = 0x7aec          # gp - this: gGameSystem
PACKETS = 0x46c             # GameSystem::m_packets
TEXTURES = 6
ORIGIN = 0x7000
FRAME = 0x2000              # the frame, in 1/16 pixel
SEED = 20261106
QUATS = 3000

HOST = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define private public
#include "game/menuprogset.h"
#undef private
#include "game/gamehooks.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "rena/rena_2d_texture_tim2.h"
#include "yamipan/rendering/yamipan_drawui.h"

static unsigned long long sAlpha, sTest;
static unsigned sPrim, sRgba;
static int sTexture;
static const void *sTextures[6];
static int sIndex;

GameSystem *gGameSystem;
sceVif1Packet *GamePacketManager::openList(int) { return nullptr; }
void GamePacketManager::closeList() {}

// Only what the sight reads of a texture: id and size come through `image`.
Tim2Texture::Tim2Texture(const void *image)
{
    memset((void *)&m_tex0, 0, 0x40);
    const short *size = (const short *)image;
    m_width = size[0];
    m_height = size[1];
    sTextures[sIndex++] = this;
}
Tim2Texture::~Tim2Texture() {}

void YamiPan_DrawUiReg(const void *, int reg, uint64_t value)
{
    if (reg == SCE_GS_ALPHA_1)
        sAlpha = value & 0xff;
    else if (reg == SCE_GS_TEST_1)
        sTest = value;
}
void YamiPan_DrawUiPrim(const void *, uint64_t prim, const uint8_t *rgbaq)
{
    sPrim = (unsigned)prim;
    sRgba = (unsigned)rgbaq[0] | rgbaq[1] << 8 | rgbaq[2] << 16 | (unsigned)rgbaq[3] << 24;
}
void YamiPan_DrawUiTexture(const void *, const void *owner)
{
    for (int i = 0; i < 6; i++)
        if (sTextures[i] == owner)
            sTexture = i;
}
void YamiPan_DrawUiSqueeze(const void *, int) {}
float YamiPan_DrawUiSqueezeFactor(void) { return 1.0f; }
void YamiPan_DrawUiBegin(const void *, int) {}
void YamiPan_DrawUiVertex(const void *, const YamiPanUiVertex *v)
{
    printf("V %d %llx %llx %x %x %d %d %d %d %u\n", sTexture, sAlpha, sTest, sPrim, sRgba, v->u, v->v, v->x, v->y, v->z);
}
void gsFillRect(sceVif1Packet *, int x, int y, int x2, int y2, u_int rgba)
{
    printf("F %llx %llx %x %d %d %d %d\n", sAlpha, sTest, rgba, x, y, x2, y2);
}

int main(int argc, char **argv)
{
    static char system[0x2000];
    gGameSystem = (GameSystem *)system;
    FILE *in = fopen(argv[1], "r");
    int count;
    fscanf(in, "%d", &count);
    for (int i = 0; i < count; i++) {
        int state;
        float zoom;
        short sizes[6][2];
        fscanf(in, "%d %f", &state, &zoom);
        for (int k = 0; k < 6; k++) {
            int w, h;
            fscanf(in, "%d %d", &w, &h);
            sizes[k][0] = (short)w;
            sizes[k][1] = (short)h;
        }
        MenuProgSet set;
        sIndex = 0;
        set.load(sizes[0], sizes[1], sizes[2], sizes[3], sizes[4], sizes[5]);
        set.state = state;
        printf("D\n");
        set.draw(zoom);
    }
    int quats;
    fscanf(in, "%d", &quats);
    for (int i = 0; i < quats; i++) {
        unsigned bits[8];
        int unit;
        fscanf(in, "%d", &unit);
        for (int k = 0; k < 8; k++)
            fscanf(in, "%x", &bits[k]);
        sceVu0FVECTOR a, b, out;
        memcpy(a, bits, 16);
        memcpy(b, bits + 4, 16);
        quatBetween(out, a, b, unit);
        unsigned *o = (unsigned *)out;
        printf("Q %08x %08x %08x %08x\n", o[0], o[1], o[2], o[3]);
    }
    return 0;
}
'''

PLANTS = (('constexpr float kTickZoomScale = 4.0f;', 'constexpr float kTickZoomScale = 5.0f;'),
          ('kSightOrigin + (y + h) * kFixedPixel - kSpriteEdge', 'kSightOrigin + (y + h) * kFixedPixel - 8'))


def build(plant):
    WORK.mkdir(parents=True, exist_ok=True)
    text = SOURCE.read_text(encoding='utf-8')
    for name in ('U', 'V'):
        old = f'const float back{name} = first{name};'
        if old not in text:
            raise SystemExit(f'source moved: {old}')
        text = text.replace(old, f'const float back{name} = (float)(span{name} - 1);')
    if plant:
        for old, new in PLANTS:
            if old not in text:
                raise SystemExit(f'plant moved: {old}')
            text = text.replace(old, new)
    (WORK / 'menuprogset.cpp').write_text(text, encoding='utf-8')
    (WORK / 'host.cpp').write_text(HOST, encoding='utf-8')
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-fpermissive', '-I', str(SRC),
                    '-I', str(SRC / 'yamipan'), str(WORK / 'host.cpp'), str(WORK / 'menuprogset.cpp'),
                    str(SRC / 'game' / 'vu0math.cpp'), '-o', str(exe)], check=True)
    return exe


def walk(ee, start, end, bound):
    """Rows of the packet, one per XYZ: (texture, alpha, test, prim, rgba, u, v, x, y, z).

    bound: (packet offset, texture) of each putTex0, the texture bound from there on."""
    alpha = test = prim = rgba = u = v = 0
    texture = -1
    bindings = list(bound)
    rows = []

    def regs(reg, value):
        nonlocal alpha, test, prim, rgba, u, v
        if reg == 0x42:
            alpha = value & 0xff
        elif reg == 0x47:
            test = value
        elif reg == 0:
            prim = value & 0x7ff
        elif reg == 1:
            rgba = value & 0xffffffff
        elif reg == 3:
            u, v = value & 0xffff, (value >> 16) & 0xffff
        elif reg in (4, 5, 0xc, 0xd):
            rows.append((texture, alpha, test, prim, rgba, u, v, value & 0xffff, (value >> 16) & 0xffff,
                         (value >> 32) & 0xffffffff, reg))

    at = start
    while at < end:
        while bindings and bindings[0][0] <= at:
            texture = bindings.pop(0)[1]
        tag = ee.rd(at, 8)
        qwc = tag & 0xffff
        vif = ee.rd(at + 8, 8)
        direct = (vif >> 32) & 0xffffffff
        if (tag >> 28) & 7 != 1 or (direct >> 24) not in (0x50, 0x51) or direct & 0xffff != qwc:
            raise SystemExit(f'not a CNT with DIRECT at {at - start:#x}: {tag:#x} {vif:#x}')
        p = at + 16
        stop = p + qwc * 16
        while p < stop:
            low, high = ee.rd(p, 8), ee.rd(p + 8, 8)
            p += 16
            nloop = low & 0x7fff
            pre = (low >> 46) & 1
            flg = (low >> 58) & 3
            nreg = (low >> 60) & 15 or 16
            if pre:
                prim = (low >> 47) & 0x7ff
            order = [(high >> (4 * k)) & 15 for k in range(nreg)]
            if flg == 0:            # PACKED, A+D only here
                if order != [0xe]:
                    raise SystemExit(f'a packed tag with regs {order}')
                for _ in range(nloop):
                    regs(ee.rd(p + 8, 8) & 0xff, ee.rd(p, 8))
                    p += 16
            elif flg == 1:          # REGLIST
                for _ in range(nloop):
                    for reg in order:
                        regs(reg, ee.rd(p, 8))
                        p += 8
                if (nloop * nreg) & 1:
                    p += 8
            else:
                raise SystemExit(f'GIF tag {low:#x}')
        at = stop
    return rows


class Rom:
    def __init__(self):
        ee = self.ee = eerun.Ee()
        env = ee.alloc(0x100)
        ee.wr(env + 0x30, 2, ORIGIN)
        ee.wr(env + 0x34, 2, ORIGIN)
        self.buffer = ee.alloc(0x20000)
        self.packet = ee.alloc(0x20)
        self.images = [ee.alloc(0x48) for _ in range(TEXTURES)]
        self.bound = []
        ee.hook('gfxGetDrawEnv', lambda e: env)
        ee.hook('gfxGetVramBlock', lambda e: 0)
        # libgcc's soft 64-bit code: a positive float truncated.
        ee.hook('__fixunssfdi', lambda e: int(eerun.f32(e.f[12])))
        ee.hook('GamePacketManager::openList', lambda e: self.packet)
        ee.hook('GamePacketManager::closeList', lambda e: 0)
        ee.hook('Tim2Texture::upload', lambda e: 0)
        ee.hook('Tim2Texture::putTex0', self.put)
        system = ee.alloc(0x1000)
        ee.wr(eerun.GP - SYSTEM_GP, 4, system)
        self.set = ee.alloc(0x1c)
        for i, image in enumerate(self.images):
            ee.wr(self.set + 4 + i * 4, 4, image)

    def put(self, ee):
        self.bound.append((ee.u32(self.packet), self.images.index(ee.g('a0') & 0xffffffff)))
        return 0

    def draw(self, state, zoom, sizes):
        ee = self.ee
        ee.wr(self.set, 4, state)
        for image, (w, h) in zip(self.images, sizes):
            ee.wr(image + 0x1c, 2, w)
            ee.wr(image + 0x1e, 2, h)
        ee.wr(self.packet, 4, self.buffer)
        self.bound = []
        ee.f[12] = eerun.b32(zoom)
        ee.call(0x2bb260, self.set)
        return walk(ee, self.buffer, ee.u32(self.packet), self.bound)

    def quat(self, unit, bits):
        ee = self.ee
        a, b, out = self.q
        for i in range(4):
            ee.wr(a + i * 4, 4, bits[i])
            ee.wr(b + i * 4, 4, bits[4 + i])
        ee.call(0x2bb510, out, a, b, unit)
        return tuple(ee.u32(out + i * 4) for i in range(4))


def bright(rows):
    """The ROM's first eight sprites tile the frame: one fill row, the rest of the rows."""
    sprites = rows[:16]
    if not sprites or sprites[0][3] != 0x146:
        raise SystemExit('state 2 does not start with the fill')
    prim = {(r[1], r[2], r[4]) for r in sprites}
    if len(prim) != 1:
        raise SystemExit(f'the fill changes state: {prim}')
    for k in range(8):
        a, b = sprites[k * 2], sprites[k * 2 + 1]
        if a[7] != ORIGIN + k * 0x400 or a[8] != ORIGIN or b[8] != ORIGIN + FRAME or b[7] != ORIGIN + (k + 1) * 0x400 - 9:
            raise SystemExit(f'fill sprite {k}: {a} {b}')
    return ('F', *prim.pop()), rows[16:]


def scenarios(rng, frames):
    masks = ((128, 128), (64, 128), (256, 128), (100, 90), (129, 127))
    out = []
    for _ in range(frames):
        state = rng.choice((1, 2))
        zoom = rng.choice((0.0, 1.0, 0.3, rng.random()))
        sizes = [rng.choice(masks), (rng.randint(8, 64), rng.choice((32, 36, 448, 447, 1, 300))), rng.choice(masks),
                 (rng.randint(1, 16), rng.randint(1, 16)), (512, 8), (8, 448)]
        out.append((state, zoom, sizes))
    return out


def vectors(rng):
    out = []
    for i in range(QUATS):
        a = [rng.gauss(0, 1) for _ in range(3)]
        if i % 5 == 0:
            b = [-x for x in a]             # opposite
        elif i % 5 == 1:
            b = list(a)
        else:
            b = [rng.gauss(0, 1) for _ in range(3)]
        unit = rng.randint(0, 1)
        if unit:
            for v in (a, b):
                n = sum(x * x for x in v) ** 0.5
                v[:] = [x / n for x in v]
        out.append((unit, [eerun.b32(x) for x in a] + [eerun.b32(1.0)] + [eerun.b32(x) for x in b] + [eerun.b32(1.0)]))
    return out


def main(argv):
    plant = '--plant' in argv
    args = [x for x in argv if not x.startswith('--')]
    frames = int(args[0]) if args else 400
    rng = random.Random(SEED)
    cases = scenarios(rng, frames)
    quats = vectors(rng)
    lines = [str(frames)]
    for state, zoom, sizes in cases:
        lines.append(f'{state} {zoom:.9g} ' + ' '.join(f'{w} {h}' for w, h in sizes))
    lines.append(str(len(quats)))
    for unit, bits in quats:
        lines.append(f'{unit} ' + ' '.join(f'{b:08x}' for b in bits))
    script = WORK / 'script.txt'
    exe = build(plant)
    script.write_text('\n'.join(lines) + '\n')
    text = subprocess.run([str(exe), str(script)], capture_output=True, text=True, check=True).stdout.split('\n')

    hosts, hostq = [], []
    for line in text:
        word = line.split()
        if not word:
            continue
        if word[0] == 'D':
            hosts.append([])
        elif word[0] == 'V':
            t, alpha, test, prim, rgba = int(word[1]), int(word[2], 16), int(word[3], 16), int(word[4], 16), int(word[5], 16)
            hosts[-1].append((t, alpha, test, prim, rgba, *(int(x) for x in word[6:11])))
        elif word[0] == 'F':
            hosts[-1].append(('F', int(word[1], 16), int(word[2], 16), int(word[3], 16)))
        elif word[0] == 'Q':
            hostq.append(tuple(int(x, 16) for x in word[1:5]))

    rom = Rom()
    rom.q = (rom.ee.alloc(0x10), rom.ee.alloc(0x10), rom.ee.alloc(0x10))
    differ = vertices = 0
    for f, (state, zoom, sizes) in enumerate(cases):
        rows = rom.draw(state, zoom, sizes)
        want = hosts[f]
        fill = None
        if state == 2:
            fill, rows = bright(rows)
            if want[0] != fill:
                differ += 1
                print(f'frame {f}: fill rom {fill} host {want[0]}')
            want = want[1:]
        if len(rows) != len(want):
            differ += 1
            print(f'frame {f} state {state}: rom {len(rows)} vertices, host {len(want)}')
            continue
        for k, (r, h) in enumerate(zip(rows, want)):
            vertices += 1
            if r[:10] != h:
                differ += 1
                if differ <= 8:
                    print(f'frame {f} state {state} zoom {zoom:.4f} vertex {k}:\n  rom  {r}\n  host {h}')

    qdiff = 0
    worst = 0
    for (unit, bits), h in zip(quats, hostq):
        r = rom.quat(unit, bits)
        if r != h:
            ulps = max(abs(a - b) for a, b in zip(r, h))
            worst = max(worst, ulps)
            qdiff += 1
            if qdiff <= 4:
                print(f'quat unit {unit}: rom {[hex(x) for x in r]} host {[hex(x) for x in h]}')
    print(f'{frames} frames, {vertices} vertices: {differ} differ; {len(quats)} quatBetween: {qdiff} differ'
          f' (largest {worst} ulps)')
    return 1 if differ or qdiff else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
