/* ==========================================================================
 *  skymist.h
 *
 *  The sky mist (FS2 0x2ad030..0x2afc20): faint, slowly fading puffs of a
 *  sheet (init pack item 6, two cells: the sheet's top and bottom halves),
 *  laid flat in a plane above the camera and drifting toward a point of the
 *  map, held by StageScene (+0xa4). Most stages draw them after the far
 *  models without depth (radius 2000, 1000 above the eye); stages 0x16 /
 *  0x17 / 0x19 / 0x5d over the main models with depth (radius 1000, 700
 *  above), and 0x19 / 0x5d pull them in toward the point instead (the
 *  swirl). Stage 0x1b has none.
 *
 *  Two sets: up to 143 puffs (70 on 0x16 / 0x17) spawned one a frame, and 8
 *  wisps spawned one every 32 frames. Each fades through four colour keys
 *  by age, a table per stage group, and out toward the radius.
 *
 *  Names of the classes and their methods are the port's, read from the
 *  code (batch 201; the holder was seeded as MenuProgHolder).
 * ======================================================================== */
#ifndef GAME_SKYMIST_H
#define GAME_SKYMIST_H

#include "game/gamehooks.h"
#include "sdk/libvu0.h"

class GamePacketManager;
class StageObject;
class StageCamera;
class Tim2Texture;

constexpr int kMistPuffMax = 0x8f;          // the first set's room
constexpr int kMistWispCount = 8;           // the second set
constexpr int kMistKeys = 4;

// One colour ramp by age: key 0 -> 1 below `first`, 1 -> 2 below `second`,
// 2 -> 3 after it; the inverses are the ctor's, written into the table.
// layout: EE (ROM data 0x30ab00 / 0x30aec0, ten of each, 0x60 apart).
struct MistKeys
{
    sceVu0FVECTOR colors[kMistKeys];    // FS2 +0x00: r g b a, 0x80 = 1.0
    float first;                        // FS2 +0x40: of the life
    float second;                       // FS2 +0x44
    float invFirst;                     // FS2 +0x48: 1 / first
    float invMiddle;                    // FS2 +0x4c: 1 / (second - first)
    float invLast;                      // FS2 +0x50: 1 / (1 - second)
    float pad[3];                       // FS2 +0x54
};
static_assert(sizeof(MistKeys) == 0x60, "MistKeys: EE 0x60");

// One puff or wisp. Stride 0x30. An age of 0 or less is a free record (-1
// when it died). layout: EE.
struct MistPuff
{
    // The colour by age, RGBA bytes (the alpha before the distance fade).
    u_int color(const MistKeys *keys) const;                             // FS2 0x002ae160

    sceVu0FVECTOR position;             // FS2 +0x00: x y in the world (z unused), w 1 at spawn
    float halfX;                        // FS2 +0x10: the first corner's reach, size x cos
    float halfY;                        // FS2 +0x14: size x sin
    float life;                         // FS2 +0x18: seconds
    float age;                          // FS2 +0x1c: seconds
    float speed;                        // FS2 +0x20: along the drift / toward the point
    int   cell;                         // FS2 +0x24: the sheet's half, 0 or 1
    float invLife;                      // FS2 +0x28: 1 / life (the swirl never sets it)
    float pad;                          // FS2 +0x2c
};
static_assert(sizeof(MistPuff) == 0x30, "MistPuff: EE 0x30");

// What a frame's puffs are drawn with: the ROM's scratchpad (0x70000000
// view-screen matrix, 0x40 the eye, 0x50 / 0xb0 the keys, 0x110 the cells).
// layout: host.
struct MistFrame
{
    const sceVu0FVECTOR *viewScreen;    // the camera's +0x280 (the ROM's tests)
    const sceVu0FVECTOR *view;          // the camera's +0x240 (PC: the host's quads are in view space)
    const float *eye;                   // the camera's matrix row 3
    float width;                        // PC: the sheet in GS 1/16 texels
    float height;
    sceVif1Packet *packet;
};

// EE 0x1cc0. layout: host (the pointers); the EE offsets in the comments.
class SkyMist
{
public:
    SkyMist(const void *image, int stage);                               // FS2 0x002aef00
    ~SkyMist();                                                          // inline in SkyMistHolder's dtor

    void draw(float deltaSeconds, GamePacketManager *packets, StageObject *camera); // FS2 0x002adc70

private:
    void stepSwirl(float deltaSeconds, StageObject *camera);             // FS2 0x002ae960
    void stepDrift(float deltaSeconds);                                  // FS2 0x002aecc0
    void spawnPuff(MistPuff *puff);                                      // FS2 0x002af9a0
    void spawnWisp(MistPuff *puff);                                      // FS2 0x002af710
    u_int nextRandom();                                                  // the spawns' demoFleckRandNext step
    // The two spawns' common body: life and size, base and random range.
    void spawnRecord(MistPuff *puff, int lifeBase, int lifeRange, int sizeBase, int sizeRange);
    void drawSwirl(const MistFrame &frame);                              // FS2 0x002ad160
    void drawDrift(const MistFrame &frame);                              // FS2 0x002ad6b0
    void putPuff(float layer, const MistPuff &puff, u_int rgba, const MistFrame &frame); // FS2 0x002ad250
    void putWrappedPuff(float layer, const MistPuff &puff, u_int rgba, const MistFrame &frame); // FS2 0x002ad810
    // The two puts' common body, from the puff's whole-unit offset to the eye.
    void putCorners(float layer, const MistPuff &puff, int dx, int dy, u_int rgba, const MistFrame &frame);
    // PC: clipPuff 0x2ae200 and its three cases, the polygon cut at the near w.
    void clipPuff(const sceVu0FVECTOR *world, const sceVu0FVECTOR *clip, const float (*uv)[2],
                  u_int rgba, const MistFrame &frame);

    Tim2Texture   *m_texture;                       // FS2 +0x0000
    sceVu0FVECTOR  m_drift;                         // FS2 +0x0010: x y toward the point, z 0, w 1
    sceVu0FVECTOR  m_target;                        // FS2 +0x0020: the point, per stage
    float          m_speedMin = 80.0f;              // FS2 +0x0030
    float          m_speedMax = 125.0f;             // FS2 +0x0034
    float          m_radius = 0.0f;                 // FS2 +0x0038: set each frame
    float          m_radiusSq = 0.0f;               // FS2 +0x003c
    float          m_invRadiusSq = 0.0f;            // FS2 +0x0040
    float          m_height = 0.0f;                 // FS2 +0x0044: above the eye
    int            m_livePuffs = 0;                 // FS2 +0x0048
    int            m_liveWisps = 0;                 // FS2 +0x004c
    int            m_wispTimer = 0;                 // FS2 +0x0050: frames since the last wisp
    u_int          m_seed = 1;                      // FS2 +0x0054: demoFleckRandNext's
    MistKeys      *m_puffKeys;                      // FS2 +0x0058
    MistKeys      *m_wispKeys;                      // FS2 +0x005c
    int            m_puffCount;                     // FS2 +0x0060
    MistPuff       m_puffs[kMistPuffMax] = {};      // FS2 +0x0070
    MistPuff       m_wisps[kMistWispCount] = {};    // FS2 +0x1b40
};

// StageScene +0xa4: a pointer to the mist. layout: host (EE 4).
struct SkyMistHolder
{
    SkyMistHolder(const void *image, int stage);                         // FS2 0x002ad100
    ~SkyMistHolder();                                                    // FS2 0x002ad070: deleting
    // The mist's frame (on the scratchpad stack in the ROM).
    void draw(float deltaSeconds, GamePacketManager *packets, StageObject *camera); // FS2 0x002ad030

    SkyMist *mist;                      // FS2 +0x0
};

#endif /* GAME_SKYMIST_H */
