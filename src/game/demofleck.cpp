#include "game/demofleck.h"

#include "game/vu0math.h"
#include "sdk/libgraph.h"
#include "yamipan/rendering/yamipan_drawui.h"

namespace {

constexpr u_int kRandShift = 0x10;
constexpr u_int kRandMask = 0x7fff;
constexpr u_int kRandBits = 15;

constexpr int kSpawnLargeMax = 9;       // a frame
constexpr int kSpawnSmallMax = 4;

constexpr int kLargeLifeBase = 4;       // and one more at random
constexpr int kLargeLifeRange = 2;
constexpr int kLargeCells = 3;
constexpr int kLargeSizeBase = 0x70;    // half a diagonal, in pixels
constexpr int kLargeSizeRange = 0x70;
constexpr int kSmallLife = 3;
constexpr int kSmallCells = 4;
constexpr int kSmallSizeBase = 0x14;
constexpr int kSmallSizeRange = 0xc;

// The frame's corner and width, and the 16ths the coordinates count in.
constexpr int   kPlaceOrigin = 0x7000;
constexpr int   kPlaceSpan = 0x2000;
constexpr float kFixed = 16.0f;

constexpr float kPi = 3.1415927f;
constexpr float kTurn = 6.2831855f;
constexpr float kRandRange = 32768.0f;

constexpr float kLevelBase = 2.5f;
constexpr float kLevelRange = 4194304.0f;   // 2.5 .. 2.5078
constexpr float kSmallLevel = 2.0f;
constexpr float kSmallAlpha = 10.0f;

constexpr u_int kGrey = 0x808080;

constexpr u_long kFleckAlpha = 0x8000000046;    // ALPHA_1: (0 - Cd) * As + Cd
constexpr u_long kFleckTest = 0x30000;          // TEST_1: past any depth
// PRIM: strip, textured, blended, UV (bit 11 is past the register).
constexpr u_long kFleckPrim = 0x954;

// A large fleck's alpha by age, a row for each life: 4 and 5.
constexpr int kLifeRows = 2;
constexpr int kLifeAges = 5;
const float kLargeFade[kLifeRows][kLifeAges] = {    // data 0x309d8c
    { 0.0f, 2.34375f, 3.0f, 2.08f, 0.0f },
    { 0.0f, 1.875f, 3.0f, 3.0f, 1.6666666f },
};

// The sheet: three large cells, then four small ones. One table has each
// cell's first corner, the other its last.
const FleckCell kLargeFirst[4] = { { 0, 0 }, { 0x400, 0 }, { 0x800, 0 }, { 0, 0 } };    // data 0x2fb678
const FleckCell kLargeLast[4] = { { 0x400, 0x400 }, { 0x800, 0x400 }, { 0xc00, 0x400 }, { 0, 0 } }; // data 0x2fb688
const FleckCell kSmallFirst[4] = { { 0xc00, 0 }, { 0xe50, 0 }, { 0xc00, 0x200 }, { 0xe50, 0x200 } }; // data 0x2fb6a0
const FleckCell kSmallLast[4] = { { 0xe50, 0x200 }, { 0x10a0, 0x200 }, { 0xe50, 0x400 }, { 0x10a0, 0x400 } }; // data 0x2fb6b0

// FS2 0x002a1670
// PC: the ROM's REGLIST tag goes too: each fleck is its own vertex run.
void fleckStripsBegin(sceVif1Packet *packet)
{
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kFleckTest);
    YamiPan_DrawUiPrim(packet, kFleckPrim, nullptr);
}

} // namespace

// FS2 0x002a1240
// callcheck: operator_new

DemoFleck::DemoFleck(Tim2Texture *texture)
{
    m_set = new FleckSet(texture);
}

// FS2 0x002a11e0
// callcheck: operator_delete, operator_delete

DemoFleck::~DemoFleck()
{
    delete m_set;
}

// FS2 0x002a11c0
void DemoFleck::draw(sceVif1Packet *packet) { m_set->draw(packet); }

// FS2 0x002a11d0
void DemoFleck::step() { m_set->step(); }

// FS2 0x002a1760
// The turned square, corners in strip order, the cell's corners on them. At a
// wide display the frame is stretched, so its width is taken back from the
// corners' reach: the square keeps its shape, the place stays where it fell.

void FleckSet::putQuad(const Fleck *fleck, u_int rgba, sceVif1Packet *packet,
                       const FleckCell *first, const FleckCell *last)
{
    const float wide = YamiPan_DrawUiSqueezeFactor();
    const int cosine = fleck->cosine, sine = fleck->sine;
    const int x = fleck->x, y = fleck->y;
    const FleckCell &a = first[(int)fleck->cell];
    const FleckCell &b = last[(int)fleck->cell];
    const int cosineX = (int)(cosine * wide), sineX = (int)(sine * wide);
    const YamiPanUiVertex corners[] = {
        { {}, a.u, a.v, (uint16_t)(x - sineX), (uint16_t)(y + cosine), 0 },
        { {}, b.u, a.v, (uint16_t)(x + cosineX), (uint16_t)(y + sine), 0 },
        { {}, a.u, b.v, (uint16_t)(x - cosineX), (uint16_t)(y - sine), 0 },
        { {}, b.u, b.v, (uint16_t)(x + sineX), (uint16_t)(y - cosine), 0 },
    };
    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_RGBAQ | YAMIPAN_UIV_UV);
    for (YamiPanUiVertex corner : corners) {
        corner.rgba[0] = (uint8_t)rgba;
        corner.rgba[1] = (uint8_t)(rgba >> 8);
        corner.rgba[2] = (uint8_t)(rgba >> 16);
        corner.rgba[3] = (uint8_t)(rgba >> 24);
        YamiPan_DrawUiVertex(packet, &corner);
    }
}

// FS2 0x002a1550
// callcheck: __fixunssfdi, putQuad

void FleckSet::drawSmall(float level, const Fleck *fleck, sceVif1Packet *packet)
{
    putQuad(fleck, (u_int)(level * kSmallAlpha) << 24 | kGrey, packet, kSmallFirst, kSmallLast);
}

// FS2 0x002a15d0
// callcheck: __fixunssfdi, putQuad

void FleckSet::drawLarge(float level, const Fleck *fleck, sceVif1Packet *packet)
{
    const float fade = kLargeFade[fleck->life - kLargeLifeBase][(int)fleck->age];
    putQuad(fleck, (u_int)(level * fade) << 24 | kGrey, packet, kLargeFirst, kLargeLast);
}

// FS2 0x002a18c0
// The ROM steps a copy of the seed for each large fleck and then reads the
// seed itself, so all of them have one level in a frame, and that level moves
// only when a spawn draws. Kept: the copy is left out, it changes nothing.
// callcheck: gfxGetVramBlock, upload, putTex0, fleckStripsBegin, demoFleckRandNext, drawLarge, drawSmall (the first three are one texture bind here; the local step reaches nothing)

void FleckSet::draw(sceVif1Packet *packet)
{
    if (m_largeAlive + m_smallAlive == 0)
        return;

    YamiPan_DrawUiTexture(packet, m_texture);
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kFleckAlpha);
    fleckStripsBegin(packet);

    const float level = (float)(int)(m_seed >> kRandShift & kRandMask) / kLevelRange + kLevelBase;
    for (const Fleck &fleck : m_large) {
        if (fleck.age != 0)
            drawLarge(level, &fleck, packet);
    }
    for (const Fleck &fleck : m_small) {
        if (fleck.age != 0)
            drawSmall(kSmallLevel, &fleck, packet);
    }
}

u_int FleckSet::nextDraw()
{
    m_seed = demoFleckRandNext(m_seed);
    return m_seed >> kRandShift & kRandMask;
}

// Inline in both spawns: `size` is kept whole in the place's span, which it
// widens by a few pixels (a size in pixels added to 16ths: the ROM's).
void FleckSet::place(Fleck *fleck, float size)
{
    const int span = (int)size * 2 + kPlaceSpan;
    fleck->x = (u_short)((int)(span * nextDraw()) >> kRandBits) + kPlaceOrigin;
    fleck->y = (u_short)((int)(span * nextDraw()) >> kRandBits) + kPlaceOrigin;
    const float turn = kPi - ((float)(int)nextDraw() * kTurn) / kRandRange;
    float pair[2];
    sinCosPair(turn, pair);
    fleck->cosine = (short)(int)(size * pair[1] * kFixed);
    fleck->sine = (short)(int)(size * pair[0] * kFixed);
}

// FS2 0x002a1ac0
// callcheck: demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, sinCosPair (nextDraw, place)

void FleckSet::spawnSmall(Fleck *fleck)
{
    fleck->age = 1;
    fleck->life = kSmallLife;
    fleck->cell = (char)((int)(nextDraw() * kSmallCells) >> kRandBits);
    const int size = (int)(nextDraw() * kSmallSizeRange) >> kRandBits;
    place(fleck, (float)(size + kSmallSizeBase));
}

// FS2 0x002a1ca0
// callcheck: demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, sinCosPair (nextDraw, place)

void FleckSet::spawnLarge(Fleck *fleck)
{
    fleck->age = 1;
    fleck->life = (char)((int)(nextDraw() * kLargeLifeRange) >> kRandBits) + kLargeLifeBase;
    fleck->cell = (char)((int)(nextDraw() * kLargeCells) >> kRandBits);
    const int size = (int)(nextDraw() * kLargeSizeRange) >> kRandBits;
    place(fleck, (float)(size + kLargeSizeBase));
}

// FS2 0x002a1eb0
// Every record ages, and a free one comes through it free again: its life is
// 0. Then the free ones fill up, at most so many a frame.
// callcheck: spawnLarge, spawnSmall

void FleckSet::step()
{
    const auto age = [](Fleck &fleck) {
        if (fleck.life <= ++fleck.age)
            fleck.life = fleck.age = 0;
    };

    for (Fleck &fleck : m_large)
        age(fleck);
    m_largeAlive = 0;
    int spawned = 0;
    for (Fleck &fleck : m_large) {
        if (fleck.age == 0) {
            if (spawned < kSpawnLargeMax) {
                spawnLarge(&fleck);
                spawned++;
                m_largeAlive++;
            }
        } else {
            m_largeAlive++;
        }
    }

    for (Fleck &fleck : m_small)
        age(fleck);
    m_smallAlive = 0;
    spawned = 0;
    for (Fleck &fleck : m_small) {
        if (fleck.age == 0) {
            if (spawned < kSpawnSmallMax) {
                spawnSmall(&fleck);
                spawned++;
                m_smallAlive++;
            }
        } else {
            m_smallAlive++;
        }
    }
}

// FS2 0x002a2050
u_int demoFleckRandNext(u_int seed) { return seed * 0x15a4e35 + 1; }
