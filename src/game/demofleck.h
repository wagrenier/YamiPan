/* ==========================================================================
 *  demofleck.h
 *
 *  The flecks: dirt that darkens the frame for a few frames at a time, over
 *  a sight-jack's view and over an event's film (FS2 0x2a11c0..0x2a2070).
 *  28 large and 12 small squares, each at a random place and turn, drawn
 *  from a sheet the init pack gives (InitItem_PartyImage). The sheet's alpha
 *  is all that counts: the blend only takes colour away.
 *
 *  Names of the class and its methods are the port's, read from the code.
 * ======================================================================== */
#ifndef GAME_DEMOFLECK_H
#define GAME_DEMOFLECK_H

#include "game/gamehooks.h"

constexpr int kFleckLarge = 0x1c;
constexpr int kFleckSmall = 0xc;

// A cell of the sheet, in texels times 16. layout: EE (table rows of 4 bytes).
struct FleckCell
{
    u_short u, v;
};
static_assert(sizeof(FleckCell) == 4);

// One square. Stride 0xc. An age of 0 is a free record.
struct Fleck
{
    u_short x, y;   // FS2 +0x00: the centre, in window coordinates times 16
    short   cosine; // FS2 +0x04: the corner's reach, size * cos * 16
    short   sine;   // FS2 +0x06: size * sin * 16
    char    life;   // FS2 +0x08: frames it lasts
    char    age;    // FS2 +0x09: frames so far, from 1
    char    cell;   // FS2 +0x0a: of the sheet's row for its kind
    char    pad;    // FS2 +0x0b
};
static_assert(sizeof(Fleck) == 0xc);

// Every square, alive or free. layout: host (the texture pointer); the EE
// offsets are in the comments, 0x1f0 bytes.
class FleckSet
{
public:
    explicit FleckSet(Tim2Texture *texture) : m_texture(texture) {}

    void draw(sceVif1Packet *packet);                                    // FS2 0x002a18c0
    void step();                                                         // FS2 0x002a1eb0

private:
    u_int nextDraw();                                                    // the spawns' common head
    void  place(Fleck *fleck, float size);                               // the spawns' common tail
    void  spawnLarge(Fleck *fleck);                                      // FS2 0x002a1ca0
    void  spawnSmall(Fleck *fleck);                                      // FS2 0x002a1ac0
    void  drawLarge(float level, const Fleck *fleck, sceVif1Packet *packet); // FS2 0x002a15d0
    void  drawSmall(float level, const Fleck *fleck, sceVif1Packet *packet); // FS2 0x002a1550
    void  putQuad(const Fleck *fleck, u_int rgba, sceVif1Packet *packet,
                  const FleckCell *first, const FleckCell *last);        // FS2 0x002a1760

    Tim2Texture *m_texture;             // FS2 +0x000
    u_int m_seed = 1;                   // FS2 +0x004: demoFleckRandNext's
    Fleck m_large[kFleckLarge] = {};    // FS2 +0x008
    Fleck m_small[kFleckSmall] = {};    // FS2 +0x158
    int   m_largeAlive = 0;             // FS2 +0x1e8
    int   m_smallAlive = 0;             // FS2 +0x1ec
};

// The object the sight-jack and the event hold. FS2 class Vt_00318860, 8 bytes:
// the vtable and the set. layout: host (the vptr and the pointer).
class DemoFleck
{
public:
    explicit DemoFleck(Tim2Texture *texture);                            // FS2 0x002a1240
    virtual ~DemoFleck();                                                // FS2 0x002a11e0: deleting (obj, flag)

    void draw(sceVif1Packet *packet);                                    // FS2 0x002a11c0
    void step();                                                         // FS2 0x002a11d0

private:
    FleckSet *m_set;                    // FS2 +0x4
};

// The game's linear congruential step, used far beyond the flecks.
u_int demoFleckRandNext(u_int seed);                                     // FS2 0x002a2050

#endif /* GAME_DEMOFLECK_H */
