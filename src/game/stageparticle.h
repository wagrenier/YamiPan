/* ==========================================================================
 *  game/stageparticle.h
 *
 *  The stage's particle effects: StageParticle (vtable 0x3149e0, kind 0x1d),
 *  what a definition of the particle files says (ParticleDef), and
 *  StageNodeTree, which owns the files, the effects' 16 textures and the
 *  effects (the name is an old guess; it is the particle system).
 *
 *  The file holds flat records, read in place. Names of the fields come from
 *  what the code does with them; architecture.md "Particles" has the frame.
 * ======================================================================== */
#ifndef _GAME_STAGEPARTICLE_H
#define _GAME_STAGEPARTICLE_H

#include <vector>

#include "game/gamehooks.h"
#include "game/stagecollision.h"

class StageCamera;
class Tim2Texture;
class BoxGrid;

// The version the particle files must have, else "this particle data is old!".
constexpr int kParticleFileVersion = 0xf;
// The particle file's header: +4 version, +8 count, the records from +0x10.
constexpr int kParticleFileHeader = 0x10;
constexpr int kParticleNameSize = 0x10;
constexpr int kParticleColorKeys = 4;
constexpr int kColorLanes = 4;
constexpr int kAxes = 3;

// ParticleDef::flags. Who reads them is in the code; what they mean is read
// from there, and the names of the rarely seen ones are guesses.
enum ParticleFlag : u_int
{
    ParticleFlag_Billboard    = 0x1,      // the quad faces the view, turned only by rot y
    ParticleFlag_ViewLocal    = 0x2,      // hangs under the view; particles are kept around it, tilted with it
    ParticleFlag_Stretch      = 0x4,      // the quad's top leans with the speed in the view
    ParticleFlag_ViewAround   = 0x10,     // hangs under the view; particles start in a box around it (weather)
    ParticleFlag_OnFloor      = 0x20,     // a new particle drops to the floor; on water the splash takes it
    ParticleFlag_Refract      = 0x40,     // draws the frame itself, shifted: heat and glass
    ParticleFlag_Collides     = 0x80,     // the map ends a particle
    ParticleFlag_RandomFrame  = 0x100,    // a random tile, kept; otherwise the tiles play
    ParticleFlag_NoFog        = 0x200,    // FGE off
    ParticleFlag_Local        = 0x400,    // particles live in the effect's space and move with it
    ParticleFlag_ScaleByLens  = 0x800,    // ViewAround's spread y follows the camera's m_scrZ
    ParticleFlag_ChainAttached = 0x1000,  // as the chain of another, it hangs under that effect
    ParticleFlag_LitByFacing  = 0x2000,   // darker as the effect's up turns from the view
    ParticleFlag_NoRoomInside = 0x4000,   // a particle ends in the boxes (StageNodeTree::contains)
    ParticleFlag_NoEmitInside = 0x8000,   // the effect does nothing while it is in them
    ParticleFlag_NoFarLimit   = 0x10000,  // no draw distance, and it sorts with key 0
    ParticleFlag_AlphaByFacing = 0x20000, // alpha by how the effect faces the view
    ParticleFlag_TakesTurn    = 0x40000,  // the quad takes the effect's own turn
    ParticleFlag_FarWorld     = 0x80000,  // drawn with the far models, at 1 / 20 scale
    ParticleFlag_PositionOnly = 0x100000, // NodeTransform::positionOnly

    ParticleFlags_FollowsView = ParticleFlag_ViewLocal | ParticleFlag_ViewAround,
    ParticleFlags_Unculled    = ParticleFlag_NoFarLimit | ParticleFlag_FarWorld,
};

// EE layout: the file's own records, 0x158 bytes.
struct ParticleDef
{
    char   name[kParticleNameSize];      // +0x000
    char   chainName[kParticleNameSize]; // +0x010: an effect spawned with this one, StageParticle::m_chain
    char   deathName[kParticleNameSize]; // +0x020: an effect where each particle ends
    char   splashName[kParticleNameSize]; // +0x030: StageParticle::m_splash, OnFloor's on water
    int    life;                         // +0x040: frames
    int    lifeRandom;                   // +0x044
    float  size;                         // +0x048
    float  sizeRandom;                   // +0x04c
    float  sizeGrowth;                   // +0x050: times this a frame
    u_char colors[kParticleColorKeys][kColorLanes]; // +0x054: RGBA, 0x80 is 1.0; life's four keys
    int    fadeInPercent;                // +0x064: of the life, key 0 to 1
    int    fadeOutPercent;               // +0x068: key 1 to 2 ends here, then 2 to 3
    int    _6c;
    unsigned long long alpha1;           // +0x070: ALPHA_1
    char   mode;                         // +0x078: StageParticle::m_mode
    char   substeps;                     // +0x079: steps between last frame's place and this one
    char   subdivide;                    // +0x07a: above 0 the ROM cuts the quad into cells
    char   _7b;
    int    stopFrames;                   // +0x07c: 0 goes on
    float  speed;                        // +0x080
    float  speedRandom;                  // +0x084: percent
    float  rot[kAxes];                   // +0x088: a particle's start
    float  rotRandom[kAxes];             // +0x094
    float  spin[kAxes];                  // +0x0a0
    float  spinRandom[kAxes];            // +0x0ac
    float  pitch;                        // +0x0b8: of the velocity
    float  cone;                         // +0x0bc
    int    spreadStart[kAxes];           // +0x0c0: the box a particle starts in
    int    spreadEnd[kAxes];             // +0x0cc
    int    spreadFrames;                 // +0x0d8
    int    emitDelay;                    // +0x0dc: frames between bursts; 0 and particles do not age
    int    emitDelayRandom;              // +0x0e0
    int    emitCount;                    // +0x0e4: a burst
    int    delayPeak;                    // +0x0e8: pulses: how much the delay swells at a pulse's ends
    int    pulseOn;                      // +0x0ec: frames; 0 no pulses
    int    pulseRamp;                    // +0x0f0
    int    pulseOff;                     // +0x0f4
    float  offset[kAxes];                // +0x0f8: the effect's place (StageNodeTree::effectOffset)
    float  turn[kAxes];                  // +0x104: the effect's rotation
    float  turnRate[kAxes];              // +0x110
    float  stretch;                      // +0x11c
    float  depthOffset;                  // +0x120: added to the depth only
    float  drag;                         // +0x124: the velocity times this a frame
    float  gravity;                      // +0x128
    float  attract;                      // +0x12c: towards the effect
    int    texture;                      // +0x130: index among the system's textures
    int    u0;                           // +0x134: 1 / 16 texel, the first tile
    int    v0;                           // +0x138
    int    u1;                           // +0x13c
    int    v1;                           // +0x140
    int    columnsLess1;                 // +0x144
    int    rowsLess1;                    // +0x148
    int    frameTicks;                   // +0x14c: 0 every frame
    u_int  flags;                        // +0x150: ParticleFlag
    int    _154;
};
static_assert(sizeof(ParticleDef) == 0x158, "ParticleDef: EE 0x158");
static_assert(offsetof(ParticleDef, alpha1) == 0x70, "ParticleDef::alpha1");
static_assert(offsetof(ParticleDef, speed) == 0x80, "ParticleDef::speed");
static_assert(offsetof(ParticleDef, emitDelay) == 0xdc, "ParticleDef::emitDelay");
static_assert(offsetof(ParticleDef, texture) == 0x130, "ParticleDef::texture");
static_assert(offsetof(ParticleDef, flags) == 0x150, "ParticleDef::flags");

// One particle (EE 0x60).
// layout: host -- the EE's 32-bit face pointer is 8 bytes here; the rest keeps
// its offsets.
struct Particle
{
    sceVu0FVECTOR position;     // FS2 +0x00: the world's, or the effect's with ParticleFlag_Local
    sceVu0FVECTOR rotation;     // FS2 +0x10
    sceVu0FVECTOR velocity;     // FS2 +0x20
    sceVu0FVECTOR spin;         // FS2 +0x30
    u_char color[kColorLanes];  // FS2 +0x40
    float  size;                // FS2 +0x44
    int    life;                // FS2 +0x48: 0 free
    int    totalLife;           // FS2 +0x4c
    int    age;                 // FS2 +0x50
    int    frame;               // FS2 +0x54: the tile
    StageCollisionFace *floor;  // FS2 +0x58: OnFloor's
};

// A particle effect of the stage (EE 0x280, built by StageNodeTree::spawn).
// Effects hang from each other through m_chain and every setter walks it.
// layout: host
class StageParticle : public StageObject
{
public:
    // count 0: as many as the life and a burst keep alive, else that many.
    StageParticle(const ParticleDef *def, int count);                    // FS2 0x00190450
    explicit StageParticle(const ParticleDef *def);                      // FS2 0x00190500
    ~StageParticle() override;                                           // FS2 0x001903c0

    // FS2 0x0018c050
    int getKind() override { return StageObject_Particle; }
    // The side base's four hooks are the empty 0x18c060..0x18c090, the base's.
    // FS2 0x0018c0b0
    bool isShown() override { return m_hidden == 0; }
    // FS2 0x0018c0a0
    // callcheck: setHidden
    void setShown(bool on) override { setHidden(!on); }

    // On or off, down the chain.
    void setHidden(bool hidden);                                         // FS2 0x0018c0c0
    void setVisible(bool on);                                            // FS2 0x0018c170
    void setEmitting(bool on);                                           // FS2 0x0018c220
    // m_emits and m_running: off, the effect stays when it runs out.
    void setRunning(bool on);                                            // FS2 0x0018c2d0
    // Particles die this far from the effect; 0 no limit.
    void setRangeSquared(float rangeSquared);                            // FS2 0x00190040
    // m_stopFrames, down the chain: the sooner one wins.
    void setStopFrames(int frames);                                      // FS2 0x001900f0
    // restart, down the chain.
    void restartChain();                                                 // FS2 0x0018fec0
    // From the first frame, with no particle alive.
    void restart();                                                      // FS2 0x0018ffa0

    // A frame: tree for its boxes and new effects, view for what it follows.
    void update(StageNodeTree *tree, StageObject *view);                 // FS2 0x0018e280
    // A particle at place, for one who does not emit by itself (mode 1); null
    // all busy.
    Particle *emitAt(const sceVu0FVECTOR place);                         // FS2 0x0018f9c0
    // The quads of the effect, into the open pass. view is the camera's (the
    // far world's is scaled), screen its screen matrix, camera its own matrix.
    void draw(const sceVu0FVECTOR *view, const sceVu0FVECTOR *screen, const sceVu0FVECTOR *camera,
              Tim2Texture *texture, sceVif1Packet *packet);              // FS2 0x0018d400

    const ParticleDef *m_def;       // FS2 +0x208
    StageParticle *m_chain;         // FS2 +0x20c
    StageParticle *m_splash;        // FS2 +0x210
    StageParticle *m_owner;         // FS2 +0x214: whose splash it is
    u_char m_drawn;                 // FS2 +0x21c: something is alive
    u_char m_emits;                 // FS2 +0x21d
    u_char m_dying;                 // FS2 +0x21f: StageNodeTree::update deletes it next
    u_char m_emitting;              // FS2 +0x271: what setEmitting last said (MapObjectParts::copyShown)
    u_char m_running;               // FS2 +0x273

private:
    void init(const ParticleDef *def, int count);                        // FS2 0x00190220
    // The frame's step: delay and pulses say whether this one emits at all
    // and how many; burst comes in as the definition's.
    bool stepHeld(int &burst);                                           // FS2 0x0018e280
    void stepParticle(Particle &particle, StageNodeTree *tree, const sceVu0FVECTOR center); // FS2 0x0018e280
    void respawn(Particle &particle, StageNodeTree *tree, StageObject *view, const sceVu0FVECTOR emitPoint,
                 const sceVu0FVECTOR *aroundView);                       // FS2 0x0018e280
    void stepColor(Particle &particle) const;                            // FS2 0x0018e280
    void drawQuad(const Particle &particle, const sceVu0FVECTOR *view, const sceVu0FVECTOR *toView,
                  const sceVu0FVECTOR *screen, u_int color, sceVif1Packet *packet) const; // FS2 0x0018d400

    Particle *m_particles;          // FS2 +0x200
    int    m_count;                 // FS2 +0x204
    int    m_mode;                  // FS2 +0x218: 1 emits only through emitAt
    int    m_stopFrames;            // FS2 +0x220
    int    m_spread[kAxes];         // FS2 +0x224
    float  m_baseRotation[kAxes];   // FS2 +0x230
    sceVu0FVECTOR m_lastCenter;     // FS2 +0x240
    float  m_rangeSquared;          // FS2 +0x250
    int    m_frames;                // FS2 +0x254: the tiles
    int    m_arg;                   // FS2 +0x25c: spawn's, passed to what it spawns
    int    m_emitted;               // FS2 +0x260
    int    m_age;                   // FS2 +0x264
    int    m_delay;                 // FS2 +0x268
    int    m_cursor;                // FS2 +0x26c
    u_char m_full;                  // FS2 +0x272: emitAt found nothing free this frame
    u_char m_visible;               // FS2 +0x274
    u_char m_hidden;                // FS2 +0x275

    friend struct StageNodeTree;
};

// What a critter is doing (+0x80).
enum CritterState
{
    CritterState_Idle = 0,
    CritterState_Turn = 1,
    CritterState_Walk = 2,
    CritterState_Hidden = 3,        // at the nest, not drawn
    CritterState_Scared = 4,        // a frame: the player came close
    CritterState_Fleeing = 5,       // back to the nest, fast
};

// The nest's picture (+0x200): which cell of the sheet, and who lives there.
enum CritterKind
{
    CritterKind_Small = 0,
    CritterKind_Rat = 1,            // the other class, its cells a quarter turn round
    CritterKind_Side = 2,
    CritterKind_Long = 3,
};

constexpr int kCritterTexture = 12; // FS2: the nest's +0x214, among the system's textures

// One flat sprite that walks on the map. The class names are readings of the
// code: nothing in the ROM names them.
// layout: host -- EE 0xb0, vptr at +0xa0; +0x7c is a pointer. The ROM's base
// 0x3194f0 has an empty update slot and a class of its own per size; the host
// passes the sizes to the ctor.
class Critter
{
public:
    Critter(float half74, float half78);
    virtual ~Critter() {}

    // A frame. range and nest are the nest's, scare is the player's place or
    // null. The ROM's answers 1 and its caller deletes a critter on 0: never.
    void update(float range, const sceVu0FVECTOR nest, const float *scare);
    // The quad, into the open pass, unless it is off the frame.
    void draw(const sceVu0FVECTOR *view, const sceVu0FVECTOR *screen, const int *cell, bool turned,
              sceVif1Packet *packet) const;                              // FS2 0x002b52c0

    sceVu0FVECTOR m_position;                   // FS2 +0x00
    int    m_state = CritterState_Idle;         // FS2 +0x80

protected:
    // The state's work; false nothing moved, so the matrix stays.
    virtual bool step(float range, const sceVu0FVECTOR nest, const float *scare) = 0;
    // The floor ended under it: what it holds on to now, null nothing.
    virtual const StageCollisionFace *leftSurface() = 0;
    virtual float reach() const = 0;            // how far down it feels for the surface
    virtual float fallSpeed() const = 0;

    void land(const StageCollisionFace *face, const sceVu0FVECTOR hit);
    void stepTurn(int walkFrames);
    void addToPlace(const sceVu0FVECTOR step);
    void endFrame(float range, const sceVu0FVECTOR nest, bool moved);

    sceVu0FVECTOR m_normal;                     // FS2 +0x10: of the surface, w 1
    sceVu0FVECTOR m_ahead;                      // FS2 +0x20
    sceVu0FMATRIX m_matrix;                     // FS2 +0x30
    float  m_heading = 0.0f;                    // FS2 +0x70: about the normal
    float  m_half[2];                           // FS2 +0x74, +0x78: half sizes, along and across its heading on a plain cell
    const StageCollisionFace *m_surface = nullptr; // FS2 +0x7c: null it falls
    int    m_timer = 0;                         // FS2 +0x84
    float  m_turnLeft = 0.0f;                   // FS2 +0x88
    float  m_speed = 0.0f;                      // FS2 +0x8c
    int    m_lastFrame = 1;                     // FS2 +0x90: the two cells of the walk
    int    m_frame = 0;                         // FS2 +0x94
    int    m_frameTicks = 0;                    // FS2 +0x98
    u_char m_forward = 1;                       // FS2 +0x9c: walks backwards on 0
};

// Walks on what is near flat, runs from the player and hides at the nest.
// layout: host
class CritterRat : public Critter
{
public:
    CritterRat() : Critter(4.0f, 8.0f) {}

protected:
    bool step(float range, const sceVu0FVECTOR nest, const float *scare) override; // FS2 0x002b4070
    const StageCollisionFace *leftSurface() override;
    float reach() const override { return 4.0f; }
    float fallSpeed() const override { return 3.0f; }

private:
    void walk();
    void faceAwayFrom(const sceVu0FVECTOR nest);
};

// Walks on any face, walls too, and round an edge.
// layout: host
class CritterCrawler : public Critter
{
public:
    CritterCrawler(float half74, float half78) : Critter(half74, half78) {}

protected:
    bool step(float range, const sceVu0FVECTOR nest, const float *scare) override; // FS2 0x002b4950
    const StageCollisionFace *leftSurface() override;
    float reach() const override { return 3.0f; }
    float fallSpeed() const override { return 2.0f; }

private:
    void walk();
    float angleTo(const sceVu0FVECTOR nest) const;
    bool waitOrTurn(float range, const sceVu0FVECTOR nest);
};

// Where critters come from (EE 0x240, vtable 0x319280), kept by the tree.
// layout: host -- the ROM's vector at +0x22c.
class CritterNest : public StageObject
{
public:
    CritterNest(float range, int kind, int most);                        // FS2 0x002b3ef0
    ~CritterNest() override;                                             // FS2 0x002b3dc0

    // FS2 0x002b3860
    int getKind() override { return StageObject_CritterNest; }
    // The side base's four hooks are the empty 0x2b3870..0x2b38a0, the base's.

    void update(const float *scare);                                     // FS2 0x002b39e0
    void draw(const sceVu0FVECTOR *view, const sceVu0FVECTOR *screen, sceVif1Packet *packet) const; // FS2 0x002b38b0

    int    textureIndex() const { return m_texture; }

private:
    int    m_kind;                  // FS2 +0x200
    int    m_most;                  // FS2 +0x204
    float  m_range;                 // FS2 +0x208: how far they wander
    int    m_spawnDelay = 0;        // FS2 +0x20c: frames between new ones
    int    m_spawnTimer = 0;        // FS2 +0x210
    int    m_texture = kCritterTexture; // FS2 +0x214
    int    m_cell[4];               // FS2 +0x218: u0, v0, u1, v1 in 1 / 16 texel
    bool   m_turned = false;        // FS2 +0x228: the cell lies a quarter turn round
    std::vector<Critter *> m_critters; // FS2 +0x22c: capacity, +0x230 count, +0x234 data
};

// The particle system (+0x1c of the scene; EE: this holds only a pointer to a
// 0x40-byte object, here flattened).
// layout: host
struct StageNodeTree
{
    StageNodeTree();                                                     // FS2 0x00190c20
    ~StageNodeTree();                                                    // FS2 0x00190bc0: deleting
    // A new effect by its definition's name, under parent, at pos / rot
    // (either null: the definition's); null no such name. arg goes to +0x25c.
    StageParticle *spawn(const char *name, StageNodeBase *parent, const float *pos,
                         const float *rot, int arg, int arg2);             // FS2 0x00190890
    // The same with no parent, place or rotation.
    StageParticle *spawnNamed(const char *name, int arg, int arg2);      // FS2 0x001908a0
    // effect stops, and what hangs from it; null is fine.
    void remove(StageParticle *effect);                                  // FS2 0x001908c0
    // effect is still in the tree's list.
    bool isLive(StageParticle *effect);                                  // FS2 0x00190810
    // The definition's offset, null no such name.
    const float *effectOffset(const char *name);                         // FS2 0x00190650
    // The first live effect of that name.
    StageParticle *findLive(const char *name);                           // FS2 0x00190750
    // A nest of critters at pos: kind picks them (CritterKind), count the most
    // at once, range how far they wander. Native 0x20f.
    StageObject *spawnCritters(const float *pos, float range, int kind, int count); // FS2 0x00190990
    // Replaces the inner object's BoxGrid with a new one over a bbx file.
    void setBoxes(void *bbx);                                            // FS2 0x00190900
    // point is in one of those boxes; false without any.
    bool contains(const sceVu0FVECTOR point);                            // FS2 0x001908d0
    // The system's textures: the 16 from common01.
    void addParticleTexture(Tim2Texture *texture);                       // FS2 0x00190aa0
    // The stage's particle files: B is searched first.
    void setParticleFileA(void *file);                                   // FS2 0x00190b50
    void setParticleFileB(void *file);                                   // FS2 0x00190ae0
    // The effects' frame; arg asks them all to end. The nests within range of
    // view take theirs first.
    void update(float range, int arg, StageObject *view, StagePlayer *player); // FS2 0x00190a90
    void draw(GamePacketManager *packets, StageObject *camera);          // FS2 0x00190a80

    class BoxGrid *m_boxes = nullptr; // FS2: +0x3c of the inner object at +0, not written

private:
    const ParticleDef *findDef(const char *name) const;                  // FS2 0x00190650: inline in each caller
    StageParticle *create(const char *name, StageNodeBase *parent, const float *pos, const float *rot,
                          int arg, int arg2);                            // FS2 0x001918d0
    void stopEffect(StageParticle *effect);                              // FS2 0x001916f0
    void forgetEffect(StageParticle *effect);

    std::vector<Tim2Texture *> m_textures;  // FS2 +0x04: capacity, +0x08 count, +0x0c data
    const ParticleDef *m_defsA = nullptr;   // FS2 +0x10
    int    m_defCountA = 0;                 // FS2 +0x14
    const ParticleDef *m_defsB = nullptr;   // FS2 +0x18
    int    m_defCountB = 0;                 // FS2 +0x1c
    std::vector<StageParticle *> m_effects; // FS2 +0x20: capacity, +0x24 count, +0x28 data
    std::vector<CritterNest *> m_nests;     // FS2 +0x2c: capacity, +0x30 count, +0x34 data
};

#endif /* _GAME_STAGEPARTICLE_H */
