/* ==========================================================================
 *  skymist.cpp
 *
 *  The sky mist (skymist.h): FS2 0x2ad030..0x2afc20.
 *
 *  PC: the ROM writes each puff as a GS fan of five vertices into a REGLIST
 *  packet, projected on the scratchpad with the camera's view-screen matrix,
 *  and cuts a puff whose corners come nearer than w 500 at that w (0x2ae200
 *  and its three cases). The host keeps every decision -- which puffs, the
 *  colour and fade, the corners, the tests that drop a puff, the cut -- and
 *  hands the result to the model module as view-space quads of the
 *  effect pass (yamipan_drawmodel.h), so the projection is the renderer's
 *  own at any aspect.
 * ======================================================================== */
#include "game/skymist.h"

#include <math.h>

#include "game/demofleck.h"
#include "game/gamepacket.h"
#include "game/sysdata.h"
#include "game/vu0math.h"
#include "rena/rena_2d_texture_tim2.h"
#include "rena/rena_draw.h"
#include "yamipan/rendering/yamipan_drawmodel.h"

namespace {

// The stages (StageSelect::stage) the mist treats apart.
constexpr int kStageNoMist = 0x1b;
constexpr int kStageSwirlA = 0x5d;      // the puffs pulled in toward the point
constexpr int kStageSwirlB = 0x19;
constexpr int kStageNearA = 0x17;       // near and with depth, as the swirl's
constexpr int kStageNearB = 0x16;
constexpr int kStageReversed = 0x1c;    // the drift away from the point

bool isSwirlStage(int stage) { return stage == kStageSwirlA || stage == kStageSwirlB; }
bool isNearStage(int stage) { return isSwirlStage(stage) || stage == kStageNearA || stage == kStageNearB; }
bool isFewPuffStage(int stage) { return stage == kStageNearA || stage == kStageNearB; }

constexpr int kFewPuffs = 0x46;

// The two looks: radius, height above the eye, drift speed.
constexpr float kNearRadius = 1000.0f;
constexpr float kNearHeight = 700.0f;
constexpr float kNearDrift = 3.0f;
constexpr float kFarRadius = 2000.0f;
constexpr float kFarHeight = 1000.0f;
constexpr float kFarDrift = 1.0f;
constexpr float kReversedDrift = -1.8f;

// The GS state the ROM sends before the puffs. The host's effect pass tests
// depth and never writes it: kMistTestFar's "no depth test" has no host
// counterpart (open-items.md).
constexpr u_long kMistTestNear = 0x50000;       // TEST_1: z GEQUAL (list 5)
constexpr u_long kMistTestFar = 0x31001;        // TEST_1: z ALWAYS, alpha NEVER keeping the frame (list 3)
constexpr u_long kMistAlpha = 0x8000000044;     // ALPHA_1: (Cs - Cd) * As + Cd
constexpr u_long kMistPrim = 0x55;              // PRIM: fan, textured, blended
constexpr int kMistVramArea = 4;                // gfxGetVramBlock's area for the sheet
constexpr int kFogNone = 0xff;

// A puff's tests, in the projection's w and the GS window's units.
constexpr int   kMistAlphaMin = 2;      // fainter: not drawn
constexpr float kNearW = 500.0f;        // nearer: cut
constexpr float kFarW = 10000.0f;       // a corner farther: not drawn
constexpr float kWindowMax = 4095.0f;

constexpr float kMistDead = -1.0f;      // a record's age when it dies

// The swirl's spawn and its pull.
constexpr int   kSwirlSpawnMax = 3;     // a frame
constexpr float kSwirlLife = 1.5f;
constexpr float kSwirlSpeed = 800.0f;
constexpr float kSwirlSize = 700.0f;
constexpr float kSwirlSlowInv = 0.0013333333f;  // 1 / 750: nearer the point slows
constexpr float kSwirlSlowing = 0.97f;

// The drift's spawns: at most one a frame, a wisp every 32 frames.
constexpr int kDriftSpawnMax = 1;
constexpr int kWispInterval = 0x1f;

constexpr u_int kRandShift = 0x10;
constexpr u_int kRandMask = 0x7fff;
constexpr float kRandScale = 3.0517578e-05f;    // 1 / 32768
constexpr float kRandRange = 32768.0f;
constexpr float kPi = 3.1415927f;
constexpr float kTurn = 6.2831855f;
constexpr float kFrameRate = 30.0f;     // lives are drawn in frames

// A spawn's life (frames) and size, base and random range. layout: host.
struct MistSpawn
{
    int lifeBase;
    int lifeRange;
    int sizeBase;
    int sizeRange;
};
constexpr MistSpawn kPuffSpawn = { 128, 30, 900, 300 };     // 0x2af9a0
constexpr MistSpawn kWispSpawn = { 50, 150, 500, 600 };     // 0x2af710

// The sheet's two cells, (u, v) for each corner in the order the corners
// turn, 0..1 of the sheet. layout: EE (data 0x30b280).
const float kMistCells[2][4][2] = {
    { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 0.5f }, { 0.0f, 0.5f } },
    { { 0.0f, 0.5f }, { 1.0f, 0.5f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } },
};

// The colour keys by stage group (mistKeySet). The ctor writes the inverses
// into these, as the ROM writes them into its data.
constexpr int kMistKeySets = 10;
MistKeys gMistPuffKeys[kMistKeySets] = {        // data 0x30ab00
    { { { 25, 17, 15, 0 }, { 60, 44, 40, 96 }, { 45, 37, 35, 96 }, { 25, 17, 15, 0 } }, 0.4f, 0.5f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 96 }, { 12, 12, 14, 96 }, { 12, 12, 14, 0 } }, 0.4f, 0.5f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 96 }, { 12, 12, 14, 96 }, { 12, 12, 14, 0 } }, 0.4f, 0.5f, 0, 0, 0, {} },
    { { { 9, 8, 9, 0 }, { 8, 7, 5, 96 }, { 30, 25, 20, 96 }, { 9, 8, 9, 0 } }, 0.4f, 0.5f, 0, 0, 0, {} },
    { { { 13, 10, 10, 0 }, { 10, 10, 10, 96 }, { 25, 12, 12, 96 }, { 13, 10, 10, 0 } }, 0.4f, 0.5f, 0, 0, 0, {} },
    { { { 20, 10, 15, 10 }, { 150, 55, 50, 10 }, { 40, 10, 10, 0 }, { 20, 10, 15, 10 } }, 0.4f, 0.5f, 0, 0, 0, {} },
    { { { 20, 12, 11, 10 }, { 150, 55, 50, 20 }, { 40, 10, 10, 20 }, { 20, 12, 11, 10 } }, 0.4f, 0.5f, 0, 0, 0, {} },
    { { { 30, 15, 14, 10 }, { 150, 55, 50, 20 }, { 40, 10, 10, 20 }, { 30, 15, 14, 10 } }, 0.2f, 0.8f, 0, 0, 0, {} },
    { { { 14, 12, 12, 0 }, { 14, 12, 12, 96 }, { 70, 12, 12, 60 }, { 12, 12, 14, 0 } }, 0.4f, 0.5f, 0, 0, 0, {} },
    { { { 22, 20, 5, 0 }, { 14, 13, 8, 96 }, { 14, 13, 8, 96 }, { 22, 20, 5, 0 } }, 0.4f, 0.5f, 0, 0, 0, {} },
};
MistKeys gMistWispKeys[kMistKeySets] = {        // data 0x30aec0
    { { { 60, 52, 52, 0 }, { 128, 52, 52, 32 }, { 60, 52, 52, 32 }, { 60, 52, 52, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 32 }, { 12, 12, 14, 32 }, { 12, 12, 14, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 14, 12, 12, 0 }, { 50, 12, 12, 32 }, { 14, 12, 12, 32 }, { 14, 12, 12, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 32 }, { 12, 12, 14, 32 }, { 12, 12, 14, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 32 }, { 12, 12, 14, 32 }, { 12, 12, 14, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 32 }, { 12, 12, 14, 32 }, { 12, 12, 14, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 32 }, { 12, 12, 14, 32 }, { 12, 12, 14, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 32 }, { 12, 12, 14, 32 }, { 12, 12, 14, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 32 }, { 12, 12, 14, 32 }, { 12, 12, 14, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
    { { { 12, 12, 14, 0 }, { 12, 12, 14, 32 }, { 12, 12, 14, 32 }, { 12, 12, 14, 0 } }, 0.4f, 0.6f, 0, 0, 0, {} },
};

// The ctor's choice of keys by stage.
int mistKeySet(int stage)
{
    switch (stage) {
    case 0x1d:
        return 9;
    case 0x1c:
        return 8;
    case kStageSwirlA:
    case kStageSwirlB:
        return 7;
    case kStageNearA:
        return 6;
    case kStageNearB:
        return 5;
    case 0x15:
        return 4;
    case 3:
        return 3;
    case 0x50: case 0x1b: case 0x1a: case 0x18: case 0x10: case 0xf: case 0xe: case 0xd:
    case 0xc: case 0xb: case 0xa: case 9: case 8: case 7: case 6:
        return 2;
    case 5: case 4: case 2: case 1:
        return 1;
    default:
        return 0;
    }
}

// The point the drift heads for, by stage (x, y, z, w).
void mistTarget(sceVu0FVECTOR out, int stage)
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    switch (stage) {
    case 0: case 7: case 0xe: case 0x14:
        x = 298.1679992675781f, y = 20592.98046875f;
        break;
    case 1: case 6: case 0x15:
        x = 6824.2568359375f, y = 23467.5703125f;
        break;
    case 2: case 4: case 0x12:
        x = 1642.4990234375f, y = 21865.26171875f;
        break;
    case 3: case 8: case 0x18:
        x = -6828.125f, y = 16078.5615234375f;
        break;
    case 5: case 0xb: case 0xf:
        x = 14316.732421875f, y = -12429.59765625f;
        break;
    case 9: case 0xa: case 0x13:
        x = 22345.3046875f, y = -13110.244140625f;
        break;
    case 0xc: case 0xd: case 0x11:
        x = 22673.947265625f, y = 10431.6796875f;
        break;
    case 0x10: case 0x1a: case 0x1c: case 0x1d:
        x = 4369.81787109375f, y = 20558.6484375f, z = 5436.61181640625f;
        break;
    default:
        break;
    }
    out[0] = x;
    out[1] = y;
    out[2] = z;
    out[3] = 1.0f;
}

// Folds v outside +-half back by whole periods of 2 half -- as written: the
// ROM divides 0.5 by v, not by the period, so the count is always 0 and the
// answer v + 2 half (what reaches the screen is only what lands inside the
// radius: puffs in front of the camera by more than the radius never show).
// FS2 0x002af6b0
float mistWrap(float v, float half)
{
    if (fabsf(v) <= half)
        return v;
    const float period = half + half;
    const float count = v * (0.5f / v);
    const float whole = (float)(int)count;
    const float more = (float)(int)((1.5f + count) - whole);
    const float shift = whole * period;
    return ((v + period) - shift) - more * shift;
}

// A spawn's random turn, -pi..pi.
float randomTurn(u_int bits)
{
    return kPi - ((float)(int)bits * kTurn) / kRandRange;
}

// The GS window's test of a projected point: x and y both in (0, 4095].
// FS2 0x002ad6a0 (mistMinf) and 0x002ad690 (mistMaxf) are its min.s / max.s.
bool onWindow(const sceVu0FVECTOR clip)
{
    sceVu0FVECTOR screen;
    scaleVec4(1.0f / clip[3], screen, clip);
    return fminf(screen[0], screen[1]) > 0.0f && fmaxf(screen[0], screen[1]) <= kWindowMax;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ae160
// The ROM's lerp is on VU0 (to x f + from x (1 - f)), then vftoi0 and
// ppach / ppacb: each lane's low byte.

u_int MistPuff::color(const MistKeys *keys) const
{
    const float t = age * invLife;
    const float *from;
    const float *to;
    float f;
    if (t < keys->first) {
        from = keys->colors[0];
        to = keys->colors[1];
        f = t * keys->invFirst;
    } else if (t < keys->second) {
        from = keys->colors[1];
        to = keys->colors[2];
        f = (t - keys->first) * keys->invMiddle;
    } else {
        from = keys->colors[2];
        to = keys->colors[3];
        f = (t - keys->second) * keys->invLast;
    }
    const float rest = 1.0f - f;
    u_int rgba = 0;
    for (int lane = 0; lane < 4; lane++)
        rgba |= ((u_int)(int)(to[lane] * f + from[lane] * rest) & 0xff) << (lane * 8);
    return rgba;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002aef00
// callcheck: operator_new, Tim2Texture, memset, memset (the records are zeroed by their initialisers)

SkyMist::SkyMist(const void *image, int stage)
    : m_texture(new Tim2Texture(image))
{
    m_drift[0] = 1.0f;          // data 0x2fb7f0
    m_drift[1] = 0.0f;
    m_drift[2] = 0.0f;
    m_drift[3] = 1.0f;

    const int set = mistKeySet(stage);
    m_puffKeys = &gMistPuffKeys[set];
    m_wispKeys = &gMistWispKeys[set];
    for (MistKeys *keys : { m_puffKeys, m_wispKeys }) {
        keys->invFirst = 1.0f / keys->first;
        keys->invMiddle = 1.0f / (keys->second - keys->first);
        keys->invLast = 1.0f / (1.0f - keys->second);
    }
    m_puffCount = isFewPuffStage(stage) ? kFewPuffs : kMistPuffMax;
    mistTarget(m_target, stage);
}

// Inline in SkyMistHolder's dtor (0x2ad070): the sheet's deleting dtor.
SkyMist::~SkyMist()
{
    delete m_texture;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002adc70
// callcheck: getStageSelect, openList, openList, getStageSelect, getStageSelect, stepDrift, stepSwirl, gfxGetVramBlock, upload, putTex0, memcpy, memcpy, memcpy, memcpy, memcpy, getStageSelect, drawDrift, drawSwirl, closeList (the sheet, ALPHA_1, TEST_1 and the GIF tag are the effect pass; the scratchpad copies are MistFrame; the stage is read once)

void SkyMist::draw(float deltaSeconds, GamePacketManager *packets, StageObject *camera)
{
    const int stage = getStageSelect()->stage;
    if (stage == kStageNoMist)
        return;

    float driftScale;
    sceVif1Packet *packet;
    if (isNearStage(stage)) {
        m_radius = kNearRadius;
        m_height = kNearHeight;
        driftScale = kNearDrift;
        packet = packets->openList(PacketList_RenaMain);        // kMistTestNear
    } else {
        m_radius = kFarRadius;
        m_height = kFarHeight;
        driftScale = kFarDrift;
        packet = packets->openList(PacketList_ParticlesFar);    // kMistTestFar
    }
    m_radiusSq = m_radius * m_radius;
    m_invRadiusSq = 1.0f / m_radiusSq;
    if (stage == kStageReversed)
        driftScale = kReversedDrift;

    const bool swirl = isSwirlStage(stage);
    if (swirl) {
        stepSwirl(deltaSeconds, camera);
    } else {
        const float *eye = camera->getMatrix()[3];
        for (int lane = 0; lane < 4; lane++)
            m_drift[lane] = m_target[lane] - eye[lane];
        m_drift[2] = 0.0f;
        const float x = m_drift[0];
        const float scale = (1.0f / sqrtf(x * x + m_drift[1] * m_drift[1])) * driftScale;
        m_drift[0] = x * scale;
        m_drift[1] = m_drift[1] * scale;
        m_drift[3] = 1.0f;
        stepDrift(deltaSeconds);
    }

    if (m_livePuffs + m_liveWisps != 0) {
        const StageCamera *lens = static_cast<const StageCamera *>(camera);
        renaBeginEffects(packet, lens->m_screen);
        YamiPanModelEffect effect = {};
        effect.texture = m_texture;
        effect.alpha1 = kMistAlpha;
        effect.fog = kFogNone;      // kMistPrim: no fog
        YamiPan_ModelSetEffect(packet, &effect);

        MistFrame frame;
        frame.viewScreen = lens->m_viewScreen;
        frame.view = lens->m_view;
        frame.eye = camera->getMatrix()[3];
        frame.width = 16.0f * (float)m_texture->m_width;
        frame.height = 16.0f * (float)m_texture->m_height;
        frame.packet = packet;
        if (swirl)
            drawSwirl(frame);
        else
            drawDrift(frame);
        renaEndPass(packet);
    }
    packets->closeList();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ae960
// callcheck: demoFleckRandNext, sinCosPair, demoFleckRandNext, sinCosPair, demoFleckRandNext (nextRandom)
// The swirl never sets a record's invLife: its colour stays at the first key.

void SkyMist::stepSwirl(float deltaSeconds, StageObject *camera)
{
    camera->getMatrix();            // the ROM asks and keeps nothing
    m_livePuffs = 0;
    int spawned = 0;
    for (int index = 0; index < m_puffCount; index++) {
        MistPuff &puff = m_puffs[index];
        if (0.0f <= puff.age)
            puff.age += deltaSeconds;
        if (puff.age < puff.life) {
            const float dx = puff.position[0] - m_target[0];
            const float dy = puff.position[1] - m_target[1];
            const float inverse = 1.0f / sqrtf(dx * dx + dy * dy);
            if (kSwirlSlowInv < inverse) {
                puff.speed *= kSwirlSlowing;
                puff.halfX *= kSwirlSlowing;
                puff.halfY *= kSwirlSlowing;
            }
            puff.position[0] = puff.position[0] - deltaSeconds * puff.speed * dx * inverse;
            puff.position[1] = puff.position[1] - deltaSeconds * puff.speed * dy * inverse;
        } else {
            puff.age = kMistDead;
        }

        if (puff.age <= 0.0f) {
            if (spawned < kSwirlSpawnMax) {
                puff.age = 0.0f;
                puff.life = kSwirlLife;
                puff.speed = kSwirlSpeed;
                float turn[2];
                sinCosPair(randomTurn(nextRandom()), turn);
                puff.halfX = turn[1] * kSwirlSize;
                puff.halfY = turn[0] * kSwirlSize;
                sinCosPair(randomTurn(nextRandom()), turn);
                puff.position[0] = m_radius * turn[0] + m_target[0];
                puff.position[1] = m_radius * turn[1] + m_target[1];
                puff.position[3] = 1.0f;
                puff.cell = (int)(nextRandom() & 1);
                spawned++;
            }
        } else {
            m_livePuffs++;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002aecc0
// callcheck: scaleVec4, spawnPuff, scaleVec4, spawnWisp

void SkyMist::stepDrift(float deltaSeconds)
{
    m_livePuffs = 0;
    int spawned = 0;
    for (int index = 0; index < m_puffCount; index++) {
        MistPuff &puff = m_puffs[index];
        puff.age += deltaSeconds;
        if (puff.life <= puff.age || puff.age <= 0.0f) {
            puff.age = kMistDead;
        } else {
            sceVu0FVECTOR moved;
            scaleVec4(puff.speed * deltaSeconds, moved, m_drift);
            for (int lane = 0; lane < 4; lane++)
                puff.position[lane] += moved[lane];
        }
        if (0.0f < puff.age) {
            m_livePuffs++;
        } else if (spawned < kDriftSpawnMax) {
            spawnPuff(&puff);
            spawned++;
        }
    }

    m_liveWisps = 0;
    spawned = 0;
    for (MistPuff &wisp : m_wisps) {
        wisp.age += deltaSeconds;
        if (wisp.life <= wisp.age || wisp.age <= 0.0f) {
            wisp.age = kMistDead;
        } else {
            sceVu0FVECTOR moved;
            scaleVec4(wisp.speed * deltaSeconds, moved, m_drift);
            for (int lane = 0; lane < 4; lane++)
                wisp.position[lane] += moved[lane];
        }
        if (0.0f < wisp.age) {
            m_liveWisps++;
        } else if (spawned < kDriftSpawnMax && kWispInterval < m_wispTimer) {
            m_wispTimer = 0;
            spawnWisp(&wisp);
            spawned++;
        }
    }
    m_wispTimer++;
}

// demoFleckRandNext on the mist's seed: the next 15 random bits (16..30).
u_int SkyMist::nextRandom()
{
    m_seed = demoFleckRandNext(m_seed);
    return (m_seed >> kRandShift) & kRandMask;
}

// The spawns' common body. The place is radius..3 radius in the world,
// whatever the eye: putWrappedPuff's offsets take it from there.
void SkyMist::spawnRecord(MistPuff *puff, int lifeBase, int lifeRange, int sizeBase, int sizeRange)
{
    puff->age = 0.0f;
    const float life = (float)lifeBase + kRandScale * (float)(int)(nextRandom() * lifeRange);
    puff->life = life / kFrameRate;
    puff->invLife = 1.0f / puff->life;
    puff->speed = m_speedMin + kRandScale * ((m_speedMax - m_speedMin) * (float)(int)nextRandom());
    puff->position[0] = m_radius + kRandScale * ((m_radius + m_radius) * (float)(int)nextRandom());
    puff->position[1] = m_radius + kRandScale * ((m_radius + m_radius) * (float)(int)nextRandom());
    puff->position[3] = 1.0f;
    float turn[2];
    sinCosPair(kPi - kRandScale * (kTurn * (float)(int)nextRandom()), turn);
    const float size = (float)sizeBase + kRandScale * (float)(int)(nextRandom() * sizeRange);
    puff->halfX = size * turn[1];
    puff->halfY = size * turn[0];
    puff->cell = (int)(nextRandom() & 1);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002af9a0
// callcheck: demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, sinCosPair, demoFleckRandNext, demoFleckRandNext (nextRandom in spawnRecord)

void SkyMist::spawnPuff(MistPuff *puff)
{
    spawnRecord(puff, kPuffSpawn.lifeBase, kPuffSpawn.lifeRange, kPuffSpawn.sizeBase, kPuffSpawn.sizeRange);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002af710
// callcheck: demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, sinCosPair, demoFleckRandNext, demoFleckRandNext (nextRandom in spawnRecord)

void SkyMist::spawnWisp(MistPuff *puff)
{
    spawnRecord(puff, kWispSpawn.lifeBase, kWispSpawn.lifeRange, kWispSpawn.sizeBase, kWispSpawn.sizeRange);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ad160
// callcheck: color, putPuff

void SkyMist::drawSwirl(const MistFrame &frame)
{
    float layer = 0.0f;
    for (int index = 0; index < m_puffCount; index++) {
        const MistPuff &puff = m_puffs[index];
        if (0.0f < puff.age) {
            layer -= 1.0f;
            putPuff(layer, puff, puff.color(m_puffKeys), frame);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ad6b0
// callcheck: color, putWrappedPuff, color, putWrappedPuff

void SkyMist::drawDrift(const MistFrame &frame)
{
    float layer = 0.0f;
    for (int index = 0; index < m_puffCount; index++) {
        const MistPuff &puff = m_puffs[index];
        if (0.0f < puff.age) {
            layer -= 1.0f;
            putWrappedPuff(layer, puff, puff.color(m_puffKeys), frame);
        }
    }
    for (const MistPuff &wisp : m_wisps) {
        if (0.0f < wisp.age) {
            layer -= 1.0f;
            putWrappedPuff(layer, wisp, wisp.color(m_wispKeys), frame);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ad250
// callcheck: applyMatrix, scaleVec4, mistMinf, mistMaxf, mistPutXyz, clipPuff (putCorners; the window test is onWindow)

void SkyMist::putPuff(float layer, const MistPuff &puff, u_int rgba, const MistFrame &frame)
{
    const int dx = (int)(puff.position[0] - frame.eye[0]);
    const int dy = (int)(puff.position[1] - frame.eye[1]);
    putCorners(layer, puff, dx, dy, rgba, frame);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ad810
// callcheck: mistWrap, mistWrap, applyMatrix, scaleVec4, mistMinf, mistMaxf, mistPutXyz, clipPuff (putCorners; the window test is onWindow)

void SkyMist::putWrappedPuff(float layer, const MistPuff &puff, u_int rgba, const MistFrame &frame)
{
    const int dx = (int)mistWrap(puff.position[0] - frame.eye[0], m_radius);
    const int dy = (int)mistWrap(puff.position[1] - frame.eye[1], m_radius);
    putCorners(layer, puff, dx, dy, rgba, frame);
}

// The body putPuff and putWrappedPuff share: the alpha faded by the distance
// to the eye, four corners turning a quarter each at the eye's height plus
// the mist's plus the layer, projected for the tests. A corner farther than
// w 10000, or one off the GS window before any near corner, drops the puff;
// near corners send it to the cut. ROM: the dropped slot's XYZ are zeroed.
void SkyMist::putCorners(float layer, const MistPuff &puff, int dx, int dy, u_int rgba, const MistFrame &frame)
{
    const int alpha = (int)((float)(rgba >> 24) * m_invRadiusSq * (m_radiusSq - (float)(dx * dx + dy * dy)));
    if (alpha < kMistAlphaMin)
        return;
    const u_int color = (u_int)alpha << 24 | (rgba & 0xffffff);
    const float x = frame.eye[0] + (float)dx;
    const float y = frame.eye[1] + (float)dy;
    const float z = frame.eye[2] + m_height + layer;

    sceVu0FVECTOR world[4];
    sceVu0FVECTOR clip[4];
    float uv[4][2];
    float reachX = puff.halfX;
    float reachY = puff.halfY;
    int near = 0;
    for (int corner = 0; corner < 4; corner++) {
        world[corner][0] = x + reachX;
        world[corner][1] = y + reachY;
        world[corner][2] = z;
        world[corner][3] = 1.0f;
        applyMatrix(clip[corner], frame.viewScreen, world[corner]);
        uv[corner][0] = kMistCells[puff.cell][corner][0];
        uv[corner][1] = kMistCells[puff.cell][corner][1];
        if (clip[corner][3] < kNearW) {
            near++;
        } else {
            if (kFarW < clip[corner][3])
                return;
            if (near == 0 && !onWindow(clip[corner]))
                return;
        }
        const float turned = -reachX;
        reachX = reachY;
        reachY = turned;
    }
    if (near == 4)
        return;
    clipPuff(world, clip, uv, color, frame);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ae200
// callcheck: clipPuff3, clipPuff2, clipPuff1 (the three cases, and the uncut fan, are one cut here)
// PC: 0x2ae270 / 0x2ae460 / 0x2ae670 keep the corners at w 500 and over and
// put, for each near one, the points where its edges toward far corners
// cross w 500 (mistClipToNear 0x2ae8c0: the clip coordinates and the uv
// lerped alike); every point must pass the window test or the puff is
// dropped. The same polygon comes out of cutting the corner loop here, in
// the world, and goes to the model module as a fan of quads in view space.

void SkyMist::clipPuff(const sceVu0FVECTOR *world, const sceVu0FVECTOR *clip, const float (*uv)[2],
                       u_int rgba, const MistFrame &frame)
{
    constexpr int kMaxPoints = 5;
    sceVu0FVECTOR points[kMaxPoints + 1];
    float pointUv[kMaxPoints + 1][2];
    int count = 0;
    const auto put = [&](const sceVu0FVECTOR place, const sceVu0FVECTOR projected, const float *texel) {
        if (!onWindow(projected))
            return false;
        for (int lane = 0; lane < 4; lane++)
            points[count][lane] = place[lane];
        pointUv[count][0] = texel[0];
        pointUv[count][1] = texel[1];
        count++;
        return true;
    };
    for (int corner = 0; corner < 4; corner++) {
        const int next = (corner + 1) & 3;
        const bool inside = kNearW <= clip[corner][3];
        if (inside && !put(world[corner], clip[corner], uv[corner]))
            return;
        if (inside != (kNearW <= clip[next][3])) {
            // mistClipToNear: f of the way from this corner to the next.
            const float f = (kNearW - clip[corner][3]) / (clip[next][3] - clip[corner][3]);
            const float rest = 1.0f - f;
            sceVu0FVECTOR place;
            sceVu0FVECTOR projected;
            float texel[2];
            for (int lane = 0; lane < 4; lane++) {
                place[lane] = world[next][lane] * f + world[corner][lane] * rest;
                projected[lane] = clip[next][lane] * f + clip[corner][lane] * rest;
            }
            texel[0] = uv[corner][0] + (uv[next][0] - uv[corner][0]) * f;
            texel[1] = uv[corner][1] + (uv[next][1] - uv[corner][1]) * f;
            if (!put(place, projected, texel))
                return;
        }
    }

    // The fan as quads (a strip each): 0 1 2 3, then 0 3 4 (4 repeated).
    for (int first = 1; first + 1 < count; first += 2) {
        const int fan[4] = { 0, first, first + 2 < count ? first + 2 : first + 1, first + 1 };
        YamiPanModelQuad quad;
        for (int corner = 0; corner < 4; corner++) {
            sceVu0FVECTOR inView;
            applyMatrix(inView, frame.view, points[fan[corner]]);
            for (int axis = 0; axis < 3; axis++)
                quad.position[corner][axis] = inView[axis];
            quad.uv[corner][0] = pointUv[fan[corner]][0] * frame.width;
            quad.uv[corner][1] = pointUv[fan[corner]][1] * frame.height;
        }
        quad.color = rgba;
        quad.owner = nullptr;   // PC: a wrapped puff is drawn more than once: moved with the camera only
        quad.slot = 0;
        YamiPan_ModelDrawQuad(frame.packet, &quad);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ad100
// callcheck: operator_new, SkyMist

SkyMistHolder::SkyMistHolder(const void *image, int stage)
    : mist(new SkyMist(image, stage))
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ad070
// callcheck: operator_delete, operator_delete (the sheet's deleting dtor and the mist; this one's by the delete-expression)

SkyMistHolder::~SkyMistHolder()
{
    delete mist;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ad030
// callcheck: draw

void SkyMistHolder::draw(float deltaSeconds, GamePacketManager *packets, StageObject *camera)
{
    mist->draw(deltaSeconds, packets, camera);
}
