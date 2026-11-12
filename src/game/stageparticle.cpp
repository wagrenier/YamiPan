/* ==========================================================================
 *  game/stageparticle.cpp
 *
 *  The stage's particle effects, 0x18c050..0x1908d0 and the system that owns
 *  them, 0x190a80..0x1920c8 (the file name is the port's; the run has no
 *  source-path string). What the ROM wrote as GS packets goes to the host's
 *  3D draw module as quads in view space.
 * ======================================================================== */
#include "game/stageparticle.h"
#include "game/gamerate.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "common/yamipan_stub.h"
#include "game/drawsys.h"
#include "game/gamepacket.h"
#include "game/gsutil.h"
#include "game/heap.h"
#include "game/mathutil.h"
#include "game/menuprogset.h"
#include "game/modelpose.h"
#include "game/mslsort.h"
#include "game/stagecollision.h"
#include "game/stagescene.h"
#include "game/vu0math.h"
#include "rena/rena_draw.h"
#include "sdk/libvu0.h"
#include "yamipan/rendering/yamipan_drawmodel.h"
#include "yamipan/rendering/yamipan_drawui.h"

namespace
{
constexpr float kPi = 3.1415927f;
constexpr float kTwoPi = 6.2831855f;
constexpr float kOneOverTwoPi = 0.15915494f;
constexpr float kRandomScale = 32768.0f;
constexpr u_int kRandomMask = 0x7fff;
constexpr float kFarMargin = 1.2f;              // the effects outlast the draw distance by this
constexpr float kNearLimit = 10.0f;             // a flat quad's nearest depth
constexpr float kLensReference = 350.0f;        // ParticleFlag_ScaleByLens
constexpr float kFloorProbeHeight = 10000.0f;
constexpr float kFaceBelow = -1.0f;
constexpr u_int kFaceSolid = 0xe42600f;
constexpr u_int kFaceSkip = 0x40000;
constexpr u_int kFaceNoEffect = 0x1000;         // StageCollisionFace::flags: a floor that takes none
constexpr u_char kSurfaceWater = 0x1d;
constexpr u_char kSurfaceDeepWater = 0x1f;
constexpr int kConeSteps = 3600;
constexpr float kFacingDarkening = 0.35f;
constexpr int kSubtractiveAlpha = 2;            // ALPHA_1's low nibble: (0 - Cs) * As + Cd
constexpr int kFogNone = 0xff;
constexpr float kFarWorldScale = 0.05f;         // data 0x2f7d70: what far_1..3 are drawn at
constexpr float kScreenCenter = 2048.0f;        // 0x8000 in the GS's 1 / 16 pixel
constexpr float kScreenLimit = 4096.0f;
constexpr float kFrameHalfWidth = 256.0f;       // 0x1000: the ROM's test for a quad on the frame
constexpr float kFrameHalfHeight = 256.0f;

constexpr int kCritterWarmFrames = 300;
constexpr int kCritterSpawnDelay = 150;
constexpr float kCritterFar = 1000.0f;
constexpr float kCritterScareDistanceSq = 10000.0f; // 100 units
constexpr float kCritterTurnStep = 0.10471976f;     // 6 degrees a frame
constexpr float kCritterFleeSpeed = 3.0f;
constexpr float kCritterRatClimb = 0.7f;            // cosine: what a rat still walks up
constexpr float kCritterHomeRatSq = 25.0f;
constexpr float kCritterHomeCrawlerSq = 100.0f;
constexpr int kCritterFleeFrames = 0x385;
constexpr float kCritterEdgeDepth = 2.0f;
constexpr float kCritterEdgeBack = 5.0f;
constexpr int kCritterFrameTicks = 4;
constexpr u_int kCritterColor = 0x80ffffffu;        // the ROM's RGBAQ: a, b, g, r
constexpr unsigned long long kCritterAlpha1 = 0x44; // (Cs - Cd) * As + Cd; the ROM leaves the list's

const sceVu0FVECTOR kAxisY = {0.0f, 1.0f, 0.0f, 0.0f}; // data 0x2f7d30, 0x2f7d40
const sceVu0FVECTOR kAxisZ = {0.0f, 0.0f, 1.0f, 0.0f}; // data 0x2f7d20

// Four lanes of randNext, each scale * 0..1: the ROM rolls them together even
// where it uses one.
void rollLanes(float scale, float *out)
{
    for (int lane = 0; lane < 4; lane++) {
        const u_int roll = randNext();
        out[lane] = scale * (float)(roll & kRandomMask) / kRandomScale;
    }
}

float wrapTurn(float angle)
{
    if (kPi < fabsf(angle)) {
        float turns = angle * kOneOverTwoPi;
        turns = (turns - (float)(int)turns) + 1.5f;
        angle = (turns - (float)(int)turns) * kTwoPi - kPi;
    }
    return angle;
}

void copyVector(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    memcpy(out, in, sizeof(sceVu0FVECTOR));
}

void addVector3(sceVu0FVECTOR out, const sceVu0FVECTOR add)
{
    for (int lane = 0; lane < kAxes; lane++)
        out[lane] += add[lane];
}
} // namespace

// ──────────────────────────────────────────────────────────────────────
// What both ctors share: the particles (one spare, as the ROM), the state,
// the definition's place and turn.
// FS2 0x00190220
// callcheck: operator_new[], setName, setRotation, setPositionXyz

void StageParticle::init(const ParticleDef *def, int count)
{
    if (def->flags & ParticleFlag_PositionOnly)
        m_xform.positionOnly = 1;
    m_def = def;
    m_count = count;
    // PC: the ROM's new[] leaves them as they are; restart clears the lives.
    m_particles = new Particle[m_count + 1]();

    m_emitting = 1;
    m_visible = 1;
    m_hidden = 0;
    m_emits = 1;
    m_arg = 0;
    m_age = 0;
    m_emitted = 0;
    m_delay = 0;
    m_cursor = 0;
    m_drawn = 1;
    m_dying = 0;
    m_full = 0;
    m_running = 1;
    setName(def->name);
    m_chain = nullptr;
    m_splash = nullptr;
    m_owner = nullptr;
    m_mode = def->mode;
    m_stopFrames = def->stopFrames;
    for (int axis = 0; axis < kAxes; axis++) {
        m_spread[axis] = def->spreadStart[axis];
        m_baseRotation[axis] = def->turn[axis];
    }
    m_xform.setRotation(def->turn[0], def->turn[1], def->turn[2]);
    m_xform.setPositionXyz(def->offset[0], def->offset[1], def->offset[2]);
    m_rangeSquared = 0.0f;
    m_frames = (def->columnsLess1 + 1) * (def->rowsLess1 + 1);
    // PC: the ROM reads m_lastCenter only from the second frame on.
    memset(m_lastCenter, 0, sizeof(m_lastCenter));
}

// FS2 0x00190450
// callcheck: StageObject

StageParticle::StageParticle(const ParticleDef *def, int count)
{
    if (count == 0)
        count = def->emitCount * (def->life + def->lifeRandom / 2);
    init(def, count);
}

// As many as can be alive: bursts in a life, times the steps of a frame.
// FS2 0x00190500
// callcheck: StageObject

StageParticle::StageParticle(const ParticleDef *def)
{
    int count = def->emitCount;
    if (def->emitDelay != 0) {
        const int span = def->emitDelay + def->life + def->lifeRandom / 2 - 1;
        count = def->emitCount * (span / (def->emitDelay + def->emitDelayRandom / 2));
    }
    if (def->substeps > 1)
        count *= def->substeps;
    init(def, count);
}

// FS2 0x001903c0
// callcheck: operator_delete[], ~StageObject, operator_delete

StageParticle::~StageParticle()
{
    delete[] m_particles;
}

// The ROM's setters recurse every ninth link; a loop is the same walk.
// FS2 0x0018c0c0
// callcheck: setHidden (the ROM's recursion: the loop)

void StageParticle::setHidden(bool hidden)
{
    for (StageParticle *effect = this; effect != nullptr; effect = effect->m_chain)
        effect->m_hidden = hidden;
}

// FS2 0x0018c170
// callcheck: setVisible (the ROM's recursion: the loop)

void StageParticle::setVisible(bool on)
{
    for (StageParticle *effect = this; effect != nullptr; effect = effect->m_chain)
        effect->m_visible = on;
}

// FS2 0x0018c220
// callcheck: setEmitting (the ROM's recursion: the loop)

void StageParticle::setEmitting(bool on)
{
    for (StageParticle *effect = this; effect != nullptr; effect = effect->m_chain)
        effect->m_emitting = on;
}

// FS2 0x0018c2d0
// callcheck: setRunning (the ROM's recursion: the loop)

void StageParticle::setRunning(bool on)
{
    for (StageParticle *effect = this; effect != nullptr; effect = effect->m_chain) {
        effect->m_emits = on;
        effect->m_running = on;
    }
}

// FS2 0x00190040
// callcheck: setRangeSquared (the ROM's recursion: the loop)

void StageParticle::setRangeSquared(float rangeSquared)
{
    for (StageParticle *effect = this; effect != nullptr; effect = effect->m_chain)
        effect->m_rangeSquared = rangeSquared;
}

// FS2 0x001900f0
// callcheck: setStopFrames (the ROM's recursion: the loop)

void StageParticle::setStopFrames(int frames)
{
    for (StageParticle *effect = this; effect != nullptr; effect = effect->m_chain) {
        if (effect->m_stopFrames == 0 || frames < effect->m_stopFrames)
            effect->m_stopFrames = frames;
    }
}

// FS2 0x0018fec0
// callcheck: restart, restartChain (the ROM's recursion: the loop)

void StageParticle::restartChain()
{
    for (StageParticle *effect = this; effect != nullptr; effect = effect->m_chain)
        effect->restart();
}

// FS2 0x0018ffa0

void StageParticle::restart()
{
    m_emits = 1;
    m_age = 0;
    m_emitted = 0;
    m_delay = 0;
    m_stopFrames = m_def->stopFrames;
    for (int axis = 0; axis < kAxes; axis++) {
        m_spread[axis] = m_def->spreadStart[axis];
        m_xform.rotation[axis] = m_baseRotation[axis];
    }
    for (int index = 0; index < m_count; index++)
        m_particles[index].life = 0;
}

// Whether this step only moves what is alive: the delay since the last burst
// and, with pulses, where in the pulse it is. A pulse either stretches the
// delay at its ends or, when the swell is no longer than the delay, thins the
// burst there. A part of StageParticle::update.

bool StageParticle::stepHeld(int &burst)
{
    const ParticleDef *def = m_def;
    m_delay++;

    int delay = def->emitDelay;
    if (delay != 0 && def->pulseOn != 0) {
        const int on = def->pulseOn;
        const int ramp = def->pulseRamp;
        const int phase = m_age % (on + def->pulseOff);
        if (on < phase)
            return true;
        if (def->delayPeak <= delay) {
            if (phase < ramp)
                burst = def->emitCount * phase / ramp;
            else if (on - ramp < phase)
                burst = def->emitCount * (ramp - (phase - (on - ramp))) / ramp;
            const u_int roll = randNext();
            m_delay = -((int)(def->emitDelayRandom * (roll & kRandomMask)) >> 16);
            return false;
        }
        if (phase < ramp)
            delay += def->delayPeak * (ramp - phase) / ramp;
        else if (on - ramp < phase)
            delay += def->delayPeak * (phase - (on - ramp)) / ramp;
    }
    if (m_delay < delay)
        return true;
    const u_int roll = randNext();
    m_delay = -((int)(def->emitDelayRandom * (roll & kRandomMask)) >> 16);
    return false;
}

// The life's colour: key 0 to 1 over fadeInPercent of it, 1 to 2 to where
// fadeOutPercent ends, 2 to 3 for what is left. A part of StageParticle::update.

void StageParticle::stepColor(Particle &particle) const
{
    const ParticleDef *def = m_def;
    const int total = particle.totalLife;
    const int fadeIn = total - total * def->fadeInPercent / 100;
    const int fadeOut = total - total * def->fadeOutPercent / 100;
    const int life = particle.life;

    for (int lane = 0; lane < kColorLanes; lane++) {
        const int key0 = def->colors[0][lane];
        const int key1 = def->colors[1][lane];
        const int key2 = def->colors[2][lane];
        const int key3 = def->colors[3][lane];
        int value;
        if (fadeIn < life)
            value = (key0 + (total - life) * (key1 - key0) / (total - fadeIn)) & 0xff;
        else if (fadeOut < life && fadeOut != fadeIn)
            value = (key1 + (fadeIn - life) * (key2 - key1) / (fadeIn - fadeOut)) & 0xff;
        else if (fadeOut == 0)
            value = key3;
        else
            value = (key3 + life * (key2 - key3) / fadeOut) & 0xff;
        particle.color[lane] = (u_char)value;
    }
}

// PC: game_rate (game/gamerate.h): position += velocity x share, the xyz lanes as addVector3 adds them; a share of
// 1 (the ROM's pace) is addVector3 to the bit.

static void moveShare(sceVu0FVECTOR position, const sceVu0FVECTOR velocity, float share)
{
    for (int axis = 0; axis < 3; axis++)
        position[axis] += velocity[axis] * share;
}

// One particle's frame; emitPoint is what attract pulls it to. A part of
// StageParticle::update.

void StageParticle::stepParticle(Particle &particle, StageNodeTree *tree, const sceVu0FVECTOR emitPoint)
{
    const ParticleDef *def = m_def;
    const bool local = (def->flags & ParticleFlag_Local) != 0;
    // PC: game_rate (game/gamerate.h): the velocity, age, life, colour, size and sprite frame change once a 25 Hz
    // frame (on its tick); the position and the spin move this frame's share of a 25 Hz frame's step every frame, so
    // a particle sits where the ROM's does at each 25 Hz frame and goes in a line between. At 25 both are the ROM.
    const bool tick = palTicks() != 0;
    const float share = palScale();

    if (tick) {
    particle.age++;
    particle.velocity[2] -= def->gravity;
    if (def->attract != 0.0f) {
        sceVu0FVECTOR pull;
        if (!local)
            vecDirection(pull, emitPoint, particle.position);
        else
            setVecLength(-1.0f, pull, particle.position);
        scaleVec4(def->attract, pull, pull);
        addVector3(particle.velocity, pull);
    }
    scaleVec4(def->drag, particle.velocity, particle.velocity);
    }

    if (m_rangeSquared != 0.0f) {
        // The centre here is the effect's or its parent's, not the step's.
        const sceVu0FVECTOR *origin = m_parent != nullptr ? m_parent->getWorldMatrix() : getWorldMatrix();
        sceVu0FVECTOR around = {0.0f, 0.0f, 0.0f, 1.0f};
        if (!local)
            copyVector(around, origin[3]);
        float distance = 0.0f;
        for (int axis = 0; axis < kAxes; axis++) {
            const float lane = particle.position[axis] - around[axis];
            distance += lane * lane;
        }
        if (m_rangeSquared <= distance)
            particle.life = 0;
        else
            moveShare(particle.position, particle.velocity, share);
    } else if ((def->flags & ParticleFlag_Collides) == 0) {
        moveShare(particle.position, particle.velocity, share);
    } else {
        sceVu0FVECTOR from;
        copyVector(from, particle.position);
        moveShare(particle.position, particle.velocity, share);
        bool hit;
        if (!local) {
            hit = gStageScene->segmentHitsAny(from, particle.position, 0, kFaceSolid, kFaceSkip, nullptr);
        } else {
            sceVu0FVECTOR to;
            applyMatrix(from, getWorldMatrix(), from);
            applyMatrix(to, getWorldMatrix(), particle.position);
            hit = gStageScene->segmentHitsAny(from, to, 0, kFaceSolid, kFaceSkip, nullptr);
        }
        if (hit)
            particle.life = 0;
    }

    moveShare(particle.rotation, particle.spin, share);
    for (int axis = 0; axis < kAxes; axis++)
        particle.rotation[axis] = wrapTurn(particle.rotation[axis]);

    if (tick) {
    stepColor(particle);
    particle.size *= def->sizeGrowth;
    if (def->emitDelay != 0)
        particle.life--;
    }
    if ((def->flags & ParticleFlag_NoRoomInside) != 0 && tree->contains(particle.position))
        particle.life = 0;

    if (particle.life < 1) {
        if (def->deathName[0] != '\0' && tree != nullptr) {
            StageParticle *death = tree->spawnNamed(def->deathName, m_arg, 0);
            if (death != nullptr) {
                if (!local)
                    death->m_xform.setPosition(particle.position);
                else
                    applyMatrix(death->m_xform.position, getWorldMatrix(), particle.position);
            }
        }
    } else if (tick && m_frames > 1 && (def->flags & ParticleFlag_RandomFrame) == 0 &&
               (def->frameTicks == 0 || particle.age % def->frameTicks == 0)) {   // PC: tick, game_rate
        particle.frame++;
        if (m_frames <= particle.frame)
            particle.frame = 0;
    }
    m_drawn = 1;
}

// A free particle starts: where the flags say, its turn, its velocity down
// the cone, its life. A part of StageParticle::update.

void StageParticle::respawn(Particle &particle, StageNodeTree *tree, StageObject *view,
                            const sceVu0FVECTOR emitPoint, const sceVu0FVECTOR *aroundView)
{
    const ParticleDef *def = m_def;
    float lanes[4];

    m_drawn = 1;
    particle.age = 0;
    if (m_frames < 2 || (def->flags & ParticleFlag_RandomFrame) == 0)
        particle.frame = 0;
    else
        particle.frame = (int)randNext() % m_frames;

    sceVu0FVECTOR offset = {0.0f, 0.0f, 0.0f, 1.0f};
    for (int axis = 0; axis < kAxes; axis++) {
        const int spread = m_spread[axis];
        if (spread != 0)
            offset[axis] = (float)((int)randNext() % spread - spread / 2);
        particle.rotation[axis] = def->rot[axis];
        if (def->rotRandom[axis] != 0.0f) {
            rollLanes(def->rotRandom[axis], lanes);
            particle.rotation[axis] += lanes[0];
        }
        particle.spin[axis] = def->spin[axis];
        if (def->spinRandom[axis] != 0.0f) {
            rollLanes(def->spinRandom[axis], lanes);
            particle.spin[axis] += lanes[0];
        }
    }

    rollLanes(kPi, lanes);
    float yaw = 0.0f;
    float pitch = 0.0f;
    if (def->cone == 0.0f) {
        if (def->pitch != 0.0f) {
            yaw = lanes[1];
            pitch = def->pitch;
        }
    } else {
        const int step = (int)randNext() % kConeSteps;
        yaw = lanes[0];
        pitch = def->pitch + (def->cone * (float)step / (float)kConeSteps - 0.5f * def->cone);
    }
    sceVu0FMATRIX aim;
    identityMatrix(aim);
    rotateMatrixY(pitch, aim, aim);
    rotateMatrixX(0.0f, aim, aim);
    rotateMatrixZ(yaw, aim, aim);
    if ((def->flags & ParticleFlag_Local) == 0)
        multiplyMatrix(aim, getWorldMatrix(), aim);
    applyMatrix(particle.velocity, aim, kAxisZ);

    if (def->speedRandom == 0.0f) {
        scaleVec3(def->speed, particle.velocity, particle.velocity);
    } else {
        const int percent = (int)randNext() % 100;
        scaleVec3(def->speedRandom * (float)percent * 0.01f + def->speed, particle.velocity, particle.velocity);
    }

    rollLanes(0.5f, lanes);
    particle.life = def->life;
    if (def->lifeRandom != 0)
        particle.life += (int)((float)def->lifeRandom * (lanes[0] + 0.5f));

    const u_int flags = def->flags;
    if (flags & ParticleFlag_Local) {
        copyVector(particle.position, offset);
    } else if (flags & ParticleFlag_ViewLocal) {
        applyMatrix(particle.position, aim, offset);
    } else {
        if (flags & ParticleFlag_ViewAround) {
            if (flags & ParticleFlag_ScaleByLens)
                offset[1] *= static_cast<StageCamera *>(view)->m_scrZ / kLensReference;
            applyMatrix(particle.position, aroundView, offset);
        } else {
            offset[3] = 0.0f;
            applyMatrix(particle.position, getWorldMatrix(), offset);
            for (int lane = 0; lane < 4; lane++)
                particle.position[lane] += emitPoint[lane];
        }
        if ((flags & ParticleFlag_NoRoomInside) != 0 && tree->contains(particle.position))
            particle.life = 0;
        if (flags & ParticleFlag_OnFloor) {
            sceVu0FVECTOR probe = {particle.position[0], particle.position[1], kFloorProbeHeight, 1.0f};
            particle.floor = gStageScene->findFloorFaceExcept(probe[0], probe[1], kFaceBelow, &probe[2], kFaceSolid,
                                                              kFaceSkip, nullptr);
            particle.position[2] = probe[2];
            const StageCollisionFace *floor = particle.floor;
            if (floor == nullptr || (floor->flags & kFaceNoEffect) != 0) {
                particle.life = 0;
            } else if (m_splash == nullptr || (floor->surface != kSurfaceWater && floor->surface != kSurfaceDeepWater)) {
                particle.position[2] += m_xform.position[2];
            } else {
                if (!m_splash->m_full) {
                    probe[2] += 1.0f;
                    m_splash->emitAt(probe);
                }
                particle.life = 0;
            }
        }
    }

    particle.totalLife = particle.life;
    memcpy(particle.color, def->colors[0], sizeof(particle.color));
    particle.size = def->size;
    if (def->sizeRandom != 0.0f)
        particle.size += def->sizeRandom * (lanes[1] + 0.5f);
    m_emitted++;
}

// The effect's frame, with the steps of a fast effect spread between last
// frame's place and this one. The ROM has it all in one 0x1740-byte function;
// the host cuts out stepHeld, stepParticle, respawn and stepColor.
// FS2 0x0018e280
// callcheck: sceVu0AddVector, applyMatrix, atan2Octant, contains, emitAt, erase, findFloorFaceExcept, getDrawDistance, identityMatrix, multiplyMatrix, pushBack, randNext, rotateMatrixX, rotateMatrixY, rotateMatrixZ, scaleVec3, scaleVec4, segmentHitsAny, setPosition, setVecLength, spawnNamed, vecDirection, vecDistanceSq (in stepHeld, stepParticle, respawn, and setParent's list calls; 0x10b940 is addVector3)

void StageParticle::update(StageNodeTree *tree, StageObject *view)
{
    const ParticleDef *def = m_def;
    if (!m_emitting || m_hidden || !m_active)
        return;

    if (m_stopFrames == 0 && (def->flags & ParticleFlags_Unculled) == 0 &&
        (def->flags & ParticleFlags_FollowsView) == 0) {
        const float reach = getDrawDistance() * kFarMargin;
        const float *position = m_parent == nullptr ? m_xform.position : (const float *)m_xform.world[3];
        if (reach * reach < vecDistanceSq(position, view->m_xform.world[3]))
            return;
    }
    if (def->flags & ParticleFlags_FollowsView) {
        m_xform.positionOnly = 1;
        if (m_parent != view)
            setParent(view);
    }

    if (m_parent != nullptr)
        m_parent->rebuildMatrix();
    setDirty(true);
    rebuildMatrix();
    sceVu0FVECTOR center;
    copyVector(center, getWorldMatrix()[3]);
    if (m_running)
        m_drawn = 0;
    m_full = 0;
    if ((def->flags & ParticleFlag_NoEmitInside) != 0 && tree->contains(center))
        return;

    // PC: game_rate (game/gamerate.h): between 25 Hz ticks the particles only move (stepParticle); emitting, the
    // effect's age, turn and spread, and its last centre wait for the tick, as the ROM's frame.
    if (palTicks() == 0) {
        const int moves = def->substeps != 0 && m_age != 0 ? def->substeps + 1 : 1;
        for (int move = 0; move < moves; move++) {
            for (int index = 0; index < m_count; index++) {
                if (m_particles[index].life > 0)
                    stepParticle(m_particles[index], tree, center);
            }
        }
        return;
    }

    int steps = 0;
    sceVu0FVECTOR travelled = {0.0f, 0.0f, 0.0f, 1.0f};
    if (def->substeps != 0 && m_age != 0) {
        for (int axis = 0; axis < kAxes; axis++)
            travelled[axis] = center[axis] - m_lastCenter[axis];
        steps = def->substeps;
    }

    for (int step = 0; step <= steps; step++) {
        sceVu0FVECTOR emitPoint;
        if (step == steps) {
            copyVector(emitPoint, center);
        } else {
            scaleVec4((float)(step + 1) / (float)(steps + 1), emitPoint, travelled);
            addVector3(emitPoint, m_lastCenter);
            emitPoint[3] = 1.0f;
        }

        int burst = def->emitCount;
        const bool held = stepHeld(burst);

        // The effect's matrix with the parent's yaw taken out.
        sceVu0FMATRIX aroundView;
        identityMatrix(aroundView);
        if (m_parent != nullptr) {
            sceVu0FVECTOR ahead;
            applyMatrix(ahead, m_parent->getWorldMatrix(), kAxisY);
            rotateMatrixZ(-atan2Octant(ahead[0], ahead[1]), aroundView, aroundView);
            multiplyMatrix(aroundView, aroundView, getWorldMatrix());
            copyVector(aroundView[3], getWorldMatrix()[3]);
        }

        int started = 0;
        for (int index = 0; index < m_count; index++) {
            Particle &particle = m_particles[index];
            if (particle.life > 0)
                stepParticle(particle, tree, emitPoint);
            if (m_mode != 1 && !held && m_emits && particle.life < 1 && started < burst) {
                respawn(particle, tree, view, emitPoint, aroundView);
                started++;
            }
        }

        m_age++;
        if (m_mode != 1 && m_stopFrames != 0 && m_stopFrames <= m_age) {
            m_emits = 0;
            if (m_splash != nullptr)
                m_splash->m_emits = 0;
        }
        for (int axis = 0; axis < kAxes; axis++)
            m_xform.rotation[axis] = wrapTurn(def->turnRate[axis] + m_xform.rotation[axis]);
        if (m_age <= def->spreadFrames) {
            for (int axis = 0; axis < kAxes; axis++) {
                const int start = def->spreadStart[axis];
                m_spread[axis] = start + m_age * (def->spreadEnd[axis] - start) / def->spreadFrames;
                if (m_spread[axis] < 0)
                    m_spread[axis] = 0;
            }
        }
    }
    copyVector(m_lastCenter, center);
}

// FS2 0x0018f9c0
// callcheck: randNext, randNext, randNext, randNext, randNext, randNext, randNext, randNext, randNext, randNext, randNext, randNext, randNext

Particle *StageParticle::emitAt(const sceVu0FVECTOR place)
{
    if (m_full)
        return nullptr;

    const ParticleDef *def = m_def;
    int cursor = m_cursor;
    for (int tried = 0; tried < m_count; tried++) {
        Particle &particle = m_particles[cursor];
        if (particle.life < 1) {
            float lanes[4];
            particle.age = 0;
            if (def->frameTicks < 2 || (def->flags & ParticleFlag_RandomFrame) == 0)
                particle.frame = 0;
            else
                particle.frame = (int)randNext() % def->frameTicks;
            for (int axis = 0; axis < kAxes; axis++) {
                particle.rotation[axis] = def->rot[axis];
                if (def->rotRandom[axis] != 0.0f) {
                    rollLanes(def->rotRandom[axis], lanes);
                    particle.rotation[axis] += lanes[0];
                }
                particle.spin[axis] = def->spin[axis];
                if (def->spinRandom[axis] != 0.0f) {
                    rollLanes(def->spinRandom[axis], lanes);
                    particle.spin[axis] += lanes[0];
                }
            }
            rollLanes(0.5f, lanes);
            particle.life = def->life;
            if (def->lifeRandom != 0)
                particle.life += (int)((float)def->lifeRandom * (lanes[0] + 0.5f));
            particle.totalLife = particle.life;
            memcpy(particle.color, def->colors[0], sizeof(particle.color));
            particle.size = def->size;
            if (def->sizeRandom != 0.0f)
                particle.size += def->sizeRandom * (lanes[1] + 0.5f);
            copyVector(particle.position, place);
            m_cursor = cursor;
            return &particle;
        }
        cursor++;
        if (cursor == m_count)
            cursor = 0;
    }
    m_full = 1;
    return nullptr;
}

// One particle's quad, in view space. PC: the ROM's 0x18c3b0 cuts a quad of
// subdivide > 0 into cells because the GS's FST coordinates are not corrected
// for perspective, and writes the screen's own coordinates; the host's quad is
// corrected, so one quad shows the same. The test for being on the frame takes
// the display's width, not the 4:3 window's. A part of StageParticle::draw.

void StageParticle::drawQuad(const Particle &particle, const sceVu0FVECTOR *view, const sceVu0FVECTOR *toView,
                             const sceVu0FVECTOR *screen, u_int color, sceVif1Packet *packet) const
{
    const ParticleDef *def = m_def;
    const bool flat = def->subdivide < 1;
    const float half = particle.size * 0.5f;

    sceVu0FVECTOR corners[4] = {
        {-half, 0.0f, half, 1.0f},
        {half, 0.0f, half, 1.0f},
        {-half, 0.0f, -half, 1.0f},
        {half, 0.0f, -half, 1.0f},
    };
    if (def->flags & ParticleFlag_Stretch) {
        sceVu0FVECTOR lean;
        applyMatrix(lean, view, particle.velocity);
        const float x = fabsf(lean[0] * def->stretch);
        const float z = fabsf(lean[2] * def->stretch);
        corners[0][0] += x;
        corners[0][2] += z;
        corners[1][0] += x;
        corners[1][2] += z;
    }

    YamiPanModelQuad quad;
    bool onFrame = !flat;
    const float frameHalfWidth = kFrameHalfWidth / YamiPan_DrawUiSqueezeFactor();
    for (int corner = 0; corner < 4; corner++) {
        sceVu0FVECTOR place;
        applyMatrix(place, toView, corners[corner]);
        if (flat) {
            sceVu0FVECTOR onScreen;
            applyMatrix(onScreen, screen, place);
            if (onScreen[3] <= 0.0f)
                return;
            const float x = onScreen[0] / onScreen[3];
            const float y = onScreen[1] / onScreen[3];
            if (x < 0.0f || x >= kScreenLimit || y < 0.0f || y >= kScreenLimit || onScreen[2] < 0.0f)
                return;
            if (fabsf(x - kScreenCenter) <= frameHalfWidth && fabsf(y - kScreenCenter) <= kFrameHalfHeight)
                onFrame = true;
        }
        if (def->depthOffset != 0.0f) {
            // Only the depth moves: the same place on the screen, farther along its ray.
            sceVu0FVECTOR moved;
            corners[corner][1] = def->depthOffset;
            applyMatrix(moved, toView, corners[corner]);
            if (place[1] > 0.0f) {
                const float scale = moved[1] / place[1];
                for (int axis = 0; axis < kAxes; axis++)
                    place[axis] *= scale;
            }
        }
        for (int axis = 0; axis < kAxes; axis++)
            quad.position[corner][axis] = place[axis];
    }
    if (!onFrame)
        return;

    const int du = def->u1 - def->u0;
    const int dv = def->v1 - def->v0;
    const int columns = def->columnsLess1 + 1;
    const int tileU = du * (particle.frame % columns);
    const int tileV = dv * (particle.frame / columns);
    const int us[4] = {def->u0, def->u1, def->u0, def->u1};
    const int vs[4] = {def->v0, def->v0, def->v1, def->v1};
    for (int corner = 0; corner < 4; corner++) {
        quad.uv[corner][0] = (float)(us[corner] + tileU);
        quad.uv[corner][1] = (float)(vs[corner] + tileV);
    }
    quad.color = color;
    quad.owner = this;      // PC: smooth motion pairs the particle across frames
    quad.slot = (int)(&particle - m_particles);
    YamiPan_ModelDrawQuad(packet, &quad);
}

// The effect's quads. The ROM's two functions differ in the cells (drawQuad)
// and in three tests, kept here by flat: the near limit, and which flags spare
// the effect the far test. ParticleFlag_Refract's quads draw the frame through
// the frame's own pixels: no definition on the disc has it (ptcall.py), so the
// host has no such pass.
// FS2 0x0018d400
// callcheck: FUN_0010afd0, FUN_0010b028, sceVu0ApplyMatrix, sceVu0InnerProduct, sceVu0ScaleVector, FUN_0010b9a0, FUN_0010ba18, FUN_0010bae8, FUN_0010bc38, FUN_0010c218, asinAcos, copyMatrix, drawCells, drawSysCurrent, getDrawDistance, multiplyMatrix, sceVif1PkAddGsAD, sceVif1PkCloseDmaTag, sceVif1PkCloseGifTag, sceVif1PkCnt, sceVif1PkOpenGifTag, sceVu0RotMatrixX, sceVu0UnitMatrix, vecDirection, vecDot, applyMatrix (the packets are YamiPan_ModelSetEffect and YamiPan_ModelDrawQuad; drawCells is drawQuad; 0x10b720 sums as applyMatrix, 0x10bae8 and 0x10bc38 turn as rotateMatrixZ and Y, 0x10b9a0 adds to row 3, 0x10b988 scales, 0x10c218 projects, 0x10ba18 is the int cast)

void StageParticle::draw(const sceVu0FVECTOR *view, const sceVu0FVECTOR *screen, const sceVu0FVECTOR *camera,
                         Tim2Texture *texture, sceVif1Packet *packet)
{
    const ParticleDef *def = m_def;
    if (!m_emitting || !m_visible || m_hidden || !m_drawn || !m_active)
        return;
    const u_int flags = def->flags;
    if (flags & ParticleFlag_Refract)
        return;

    const bool flat = def->subdivide < 1;
    const float far = getDrawDistance();
    const u_int sparedByView = flat ? (u_int)ParticleFlags_FollowsView : (u_int)ParticleFlag_ViewLocal;
    const sceVu0FVECTOR *matrix = getMatrix();
    sceVu0FVECTOR viewPlace;
    if ((flags & ParticleFlags_Unculled) == 0 && (flags & sparedByView) == 0) {
        applyMatrix(viewPlace, view, matrix[3]);
        if (far * kFarMargin < viewPlace[1])
            return;
    }

    sceVu0FMATRIX tilt;
    if (flags & ParticleFlag_ViewLocal) {
        sceVu0FVECTOR ahead;
        applyMatrix(ahead, camera, kAxisY);
        const float angle = asinAcos(ahead[2], 0);
        sceVu0UnitMatrix(tilt);
        sceVu0RotMatrixX(tilt, tilt, -angle);
    }

    sceVu0FVECTOR toEye;
    copyVector(toEye, matrix[3]);
    vecDirection(toEye, camera[3], toEye);
    float litScale = 1.0f;
    if (flags & ParticleFlag_LitByFacing) {
        sceVu0FVECTOR up;
        applyMatrix(up, matrix, kAxisZ);
        const float facing = up[0] * toEye[0] + up[1] * toEye[1] + up[2] * toEye[2];
        if (facing < 0.0f)
            litScale = facing * kFacingDarkening + 1.0f;
    }
    float facing = 0.0f;
    if (flags & ParticleFlag_AlphaByFacing)
        facing = std::max(vecDot(toEye, matrix[1]), 0.0f);

    const bool fogFadesAlpha = (flags & ParticleFlag_NoFog) == 0 && (def->alpha1 & 0xf) == kSubtractiveAlpha;
    float depth = m_xform.position[1];
    if ((flags & ParticleFlag_ViewLocal) == 0) {
        applyMatrix(viewPlace, view, matrix[3]);
        depth = viewPlace[1];
    }
    const int fog = drawSysCurrent()->lights->getFog()->getFogValue(depth) & 0xff;

    YamiPanModelEffect effect = {};
    effect.texture = texture;
    effect.alpha1 = def->alpha1;
    effect.fog = (fogFadesAlpha || (flags & ParticleFlag_NoFog) != 0) ? kFogNone : fog;
    YamiPan_ModelSetEffect(packet, &effect);

    for (int index = 0; index < m_count; index++) {
        const Particle &particle = m_particles[index];
        if (particle.life <= 0)
            continue;

        sceVu0FVECTOR inWorld;
        const float *world = inWorld;
        if (flags & ParticleFlag_Local) {
            applyMatrix(inWorld, getMatrix(), particle.position);
            applyMatrix(viewPlace, view, inWorld);
        } else if (flags & ParticleFlag_ViewLocal) {
            applyMatrix(viewPlace, tilt, particle.position);
            addVector3(viewPlace, m_xform.position);
            applyMatrix(inWorld, camera, viewPlace);
        } else {
            applyMatrix(viewPlace, view, particle.position);
            world = particle.position;
        }
        if (flat && viewPlace[1] <= kNearLimit)
            continue;
        if (far <= viewPlace[1] && (flags & ParticleFlag_NoFarLimit) == 0)
            continue;

        sceVu0FMATRIX toView;
        sceVu0UnitMatrix(toView);
        if (flags & ParticleFlag_TakesTurn) {
            copyMatrix(toView, getMatrix());
            copyVector(toView[3], world);
            toView[3][3] = 1.0f;
            multiplyMatrix(toView, view, toView);
        } else if (flags & ParticleFlag_Billboard) {
            rotateMatrixY(particle.rotation[1], toView, toView);
            for (int axis = 0; axis < kAxes; axis++)
                toView[3][axis] += viewPlace[axis];
        } else {
            rotateMatrixY(particle.rotation[1], toView, toView);
            sceVu0RotMatrixX(toView, toView, particle.rotation[0]);
            rotateMatrixZ(particle.rotation[2], toView, toView);
            for (int axis = 0; axis < kAxes; axis++)
                toView[3][axis] += world[axis];
            multiplyMatrix(toView, view, toView);
        }

        u_int color;
        if (flags & ParticleFlag_LitByFacing) {
            color = 0;
            for (int lane = 0; lane < kColorLanes; lane++)
                color |= (u_int)((int)((float)particle.color[lane] * litScale) & 0xff) << (lane * 8);
        } else {
            u_int alpha = particle.color[3];
            if (flags & ParticleFlag_AlphaByFacing)
                alpha = (u_int)(int)((float)alpha * facing);
            color = (u_int)particle.color[0] | (u_int)particle.color[1] << 8 | (u_int)particle.color[2] << 16 |
                    (alpha & 0xff) << 24;
        }
        if (fogFadesAlpha)
            color = (color & 0xffffff) | ((((u_int)fog * (color >> 24)) >> 8) & 0xff) << 24;

        drawQuad(particle, view, toView, screen, color, packet);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The system.

// FS2 0x00190c20
// callcheck: operator_new, Inner (the 0x40-byte object is this one)

StageNodeTree::StageNodeTree()
{
}

// PC: the ROM's three bytes +0, +1, +2 stop spawn, update and draw; nothing in
// the run sets them.
// FS2 0x00190bc0
// callcheck: ~Inner, operator_delete (the 0x40-byte object is this one)

StageNodeTree::~StageNodeTree()
{
    for (StageParticle *effect : m_effects)
        delete effect;
    m_effects.clear();
    for (CritterNest *nest : m_nests)
        delete nest;
    m_nests.clear();
    delete m_boxes;
}

// FS2 0x00190aa0
// callcheck: insert

void StageNodeTree::addParticleTexture(Tim2Texture *texture)
{
    m_textures.push_back(texture);
}

static void readParticleFile(void *file, const ParticleDef *&defs, int &count)
{
    defs = nullptr;
    count = 0;
    if (file == nullptr)
        return;
    if (((const int *)file)[1] != kParticleFileVersion) {
        rxSysPrintf("this particle data is old!\n");
        return;
    }
    count = ((const int *)file)[2];
    defs = (const ParticleDef *)((const char *)file + kParticleFileHeader);
}

// FS2 0x00190b50
// callcheck: rxSysPrintf

void StageNodeTree::setParticleFileA(void *file)
{
    readParticleFile(file, m_defsA, m_defCountA);
}

// FS2 0x00190ae0
// callcheck: rxSysPrintf

void StageNodeTree::setParticleFileB(void *file)
{
    readParticleFile(file, m_defsB, m_defCountB);
}

// File B before file A. Inline in effectOffset and in create, twice.

const ParticleDef *StageNodeTree::findDef(const char *name) const
{
    for (int index = 0; index < m_defCountB; index++) {
        if (strcmp(m_defsB[index].name, name) == 0)
            return &m_defsB[index];
    }
    for (int index = 0; index < m_defCountA; index++) {
        if (strcmp(m_defsA[index].name, name) == 0)
            return &m_defsA[index];
    }
    return nullptr;
}

// FS2 0x00190650
// callcheck: strcmp, strcmp (in findDef)

const float *StageNodeTree::effectOffset(const char *name)
{
    const ParticleDef *def = findDef(name);
    return def != nullptr ? def->offset : nullptr;
}

// FS2 0x00190750
// callcheck: strcmp

StageParticle *StageNodeTree::findLive(const char *name)
{
    for (StageParticle *effect : m_effects) {
        if (strcmp(effect->m_name, name) == 0)
            return effect;
    }
    return nullptr;
}

// FS2 0x00190810

bool StageNodeTree::isLive(StageParticle *effect)
{
    return std::find(m_effects.begin(), m_effects.end(), effect) != m_effects.end();
}

// A new effect, its chain and its splash.
// FS2 0x001918d0
// callcheck: heapAllocAligned, FUN_00190450, heapAllocAligned, StageParticleCount, StageParticle, erase, erase, pushBack, restart, insert, createEffect, createEffect, createEffect, erase, erase, erase, pushBack, strcmp, strcmp, strcmp, strcmp (new on the host; setParent and the vector hold the list calls; strcmp in findDef)

StageParticle *StageNodeTree::create(const char *name, StageNodeBase *parent, const float *pos, const float *rot,
                                     int arg, int arg2)
{
    const ParticleDef *def = findDef(name);
    if (def == nullptr)
        return nullptr;

    StageParticle *effect = arg2 == 0 ? new StageParticle(def) : new StageParticle(def, arg2);
    if (parent != nullptr)
        effect->setParent(parent);
    if ((def->flags & ParticleFlags_FollowsView) == 0) {
        if (rot != nullptr) {
            memcpy(effect->m_xform.rotation, rot, sizeof(sceVu0FVECTOR));
            memcpy(effect->m_baseRotation, rot, sizeof(effect->m_baseRotation));
        }
        if (pos != nullptr)
            memcpy(effect->m_xform.position, pos, sizeof(sceVu0FVECTOR));
    }
    effect->m_arg = arg;
    effect->rebuildMatrix();
    effect->restart();
    m_effects.push_back(effect);

    if (def->chainName[0] != '\0') {
        const ParticleDef *chainDef = findDef(def->chainName);
        if (chainDef != nullptr) {
            StageParticle *chain;
            if ((chainDef->flags & ParticleFlag_ChainAttached) == 0 ||
                (chainDef->flags & ParticleFlags_FollowsView) != 0)
                chain = create(def->chainName, parent, pos, rot, arg, 0);
            else
                chain = create(def->chainName, effect, nullptr, nullptr, arg, 0);
            if (chain != nullptr) {
                effect->m_chain = chain;
                chain->m_tree->setParent(effect->m_tree);
            }
        }
    }
    if (def->splashName[0] != '\0') {
        StageParticle *splash = create(def->splashName, nullptr, nullptr, nullptr, arg, effect->m_count);
        if (splash != nullptr) {
            effect->m_splash = splash;
            splash->m_owner = effect;
            splash->m_mode = 1;
            splash->setParent(effect);
        }
    }
    return effect;
}

// FS2 0x00190890
// callcheck: createEffect

StageParticle *StageNodeTree::spawn(const char *name, StageNodeBase *parent, const float *pos, const float *rot,
                                    int arg, int arg2)
{
    return create(name, parent, pos, rot, arg, arg2);
}

// FS2 0x001908a0
// callcheck: createEffect

StageParticle *StageNodeTree::spawnNamed(const char *name, int arg, int arg2)
{
    return create(name, nullptr, nullptr, nullptr, arg, arg2);
}

// FS2 0x001916f0
// callcheck: stopEffect, stopEffect, stopEffect, stopEffect, FUN_001918c0, FUN_001918b0, FUN_001918c0, FUN_001918b0 (the vector's size and index, in isLive)

void StageNodeTree::stopEffect(StageParticle *effect)
{
    if (effect == nullptr || !isLive(effect))
        return;
    effect->m_emits = 0;
    effect->m_drawn = 0;
    stopEffect(effect->m_chain);
    stopEffect(effect->m_splash);
}

// FS2 0x001908c0
// callcheck: stopEffect

void StageNodeTree::remove(StageParticle *effect)
{
    stopEffect(effect);
}

// PC: the ROM leaves these pointers to freed memory, read and written by the
// chain's setters; on the EE that stays harmless.
void StageNodeTree::forgetEffect(StageParticle *effect)
{
    for (StageParticle *other : m_effects) {
        if (other->m_chain == effect)
            other->m_chain = nullptr;
        if (other->m_splash == effect)
            other->m_splash = nullptr;
        if (other->m_owner == effect)
            other->m_owner = nullptr;
    }
}

// The nests within range of view, then the effects. An effect that stopped
// drawing and emitting gets one more frame, then goes; one that does not run
// stays for whoever holds it.
// FS2 0x00190a90
// callcheck: updateEffects (0x1914c0 is this body)

void StageNodeTree::update(float range, int arg, StageObject *view, StagePlayer *player)
{
    const float *scare = player != nullptr ? reinterpret_cast<StageObject *>(player)->m_xform.position : nullptr;
    const sceVu0FVECTOR *viewer = view->getWorldMatrix();
    // PC: game_rate (game/gamerate.h): the critters (their walks, turns, timers, rolls and collisions) run whole
    // 25 Hz frames, as the ROM's; they move in 25 Hz steps. The nest's warm-up at construction is not this path.
    for (CritterNest *nest : m_nests) {
        if (palTicks() != 0 && vecDistanceSq(nest->m_xform.world[3], viewer[3]) < range * range)
            nest->update(scare);
    }

    for (size_t index = 0; index < m_effects.size(); index++) {
        StageParticle *effect = m_effects[index];
        if (arg != 0) {
            effect->m_emits = 0;
            effect->m_drawn = 0;
            effect->m_dying = 1;
        } else if (effect->m_emits || effect->m_drawn) {
            if (effect->m_active)
                effect->update(this, view);
        } else if (!effect->m_dying) {
            if (effect->m_running) {
                effect->m_emitting = 0;
                effect->m_dying = 1;
            }
        } else {
            m_effects.erase(m_effects.begin() + (std::ptrdiff_t)index);
            forgetEffect(effect);
            delete effect;
            index--;
        }
    }
}

namespace
{
// layout: host
struct SortedEffect
{
    StageParticle *effect;
    int key;
};
} // namespace

// The critters at the end of the main models' list, then the frame's effects:
// the far world's with the far models, the rest over the main models, each
// list nearest first (a key is the depth less the index; ParticleFlag_NoFarLimit's
// is 0, so ties are common and the ROM's own sort decides them, far list first:
// its counter runs from one to the next). The ROM's VRAM placement goes: a
// texture is the host's, by its record.
// FS2 0x00190a80
// callcheck: drawEffects, reserve, resize, insert, mslSort_Effects, ~EffectSortList (0x190c70 is this body; std::vector here, mslSort is the ROM's)

void StageNodeTree::draw(GamePacketManager *packets, StageObject *camera)
{
    const StageCamera *lens = static_cast<StageCamera *>(camera);
    const sceVu0FVECTOR *cameraMatrix = camera->getMatrix();
    const sceVu0FVECTOR *view = lens->m_view;

    if (!m_nests.empty()) {
        // The ROM's list holds no ALPHA_1 of its own here and its TEST writes
        // colour without depth; its RGBAQ is white at alpha 0x80.
        sceVif1Packet *packet = packets->openList(PacketList_RenaMain);
        renaBeginEffects(packet, lens->m_screen);
        for (const CritterNest *nest : m_nests) {
            YamiPanModelEffect effect = {};
            const size_t texture = (size_t)nest->textureIndex();
            effect.texture = texture < m_textures.size() ? m_textures[texture] : nullptr;
            effect.alpha1 = kCritterAlpha1;
            effect.fog = kFogNone;
            YamiPan_ModelSetEffect(packet, &effect);
            nest->draw(view, lens->m_screen, packet);
        }
        renaEndPass(packet);
        packets->closeList();
    }

    sceVu0FMATRIX farView;
    for (int row = 0; row < 4; row++) {
        for (int lane = 0; lane < kAxes; lane++)
            farView[row][lane] = view[row][lane] * kFarWorldScale;
        farView[row][3] = view[row][3];
    }

    std::vector<SortedEffect> nearList;
    std::vector<SortedEffect> farList;
    const float reach = getDrawDistance();
    for (int index = (int)m_effects.size() - 1; index >= 0; index--) {
        StageParticle *effect = m_effects[(size_t)index];
        if (!effect->m_active || !effect->m_emitting || !effect->m_visible || !effect->m_drawn)
            continue;
        const u_int flags = effect->m_def->flags;
        int key = 0;
        if ((flags & ParticleFlag_NoFarLimit) == 0) {
            sceVu0FVECTOR place;
            applyMatrix(place, view, effect->m_xform.world[3]);
            if (place[1] < kNearLimit || (float)(int)(reach * kFarMargin) < place[1])
                continue;
            key = (int)place[1] - index;
        }
        ((flags & ParticleFlag_FarWorld) != 0 ? farList : nearList).push_back({effect, key});
    }
    const auto nearestFirst = [](const SortedEffect &a, const SortedEffect &b) { return a.key < b.key; };
    mslSort(farList.data(), farList.data() + farList.size(), nearestFirst);
    mslSort(nearList.data(), nearList.data() + nearList.size(), nearestFirst);

    for (int pass = 0; pass < 2; pass++) {
        const std::vector<SortedEffect> &list = pass == 0 ? farList : nearList;
        if (list.empty())
            continue;
        sceVif1Packet *packet = packets->openList(pass == 0 ? PacketList_ParticlesFar : PacketList_Particles);
        renaBeginEffects(packet, lens->m_screen);
        for (const SortedEffect &entry : list) {
            const int texture = entry.effect->m_def->texture;
            Tim2Texture *image = texture >= 0 && (size_t)texture < m_textures.size() ? m_textures[(size_t)texture]
                                                                                     : nullptr;
            entry.effect->draw(pass == 0 ? farView : view, lens->m_screen, cameraMatrix, image, packet);
        }
        renaEndPass(packet);
        packets->closeList();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The critters, 0x2b3870..0x2b5740. Far from the rest of the file in the ROM,
// but the tree alone makes, moves and draws them.

// A nest in the tree. The ctor runs with the nest still at the origin, so what
// it makes starts there; the first frames take them home.
// FS2 0x00190990
// callcheck: heapAllocAligned, FUN_002b3ef0, setPositionXyz, insert (new and the vector on the host)

StageObject *StageNodeTree::spawnCritters(const float *pos, float range, int kind, int count)
{
    CritterNest *nest = new CritterNest(range, kind, count);
    nest->m_xform.setPositionXyz(pos[0], pos[1], pos[2]);
    nest->rebuildMatrix();
    m_nests.push_back(nest);
    return nest;
}

// The cells of the sheet are fixed per kind. Three hundred frames with no delay
// fill it before anybody looks.
// FS2 0x002b3ef0
// callcheck: StageObject, FUN_002b39e0

CritterNest::CritterNest(float range, int kind, int most)
    : m_kind(kind), m_most(most), m_range(range)
{
    static const int kCells[4][4] = {
        {0, 0, 0x210, 0x210},           // CritterKind_Small
        {0, 0x610, 0x400, 0x820},       // CritterKind_Rat
        {0x410, 0, 0x610, 0x200},       // CritterKind_Side
        {0, 0x210, 0x210, 0x610},       // CritterKind_Long
    };
    const int *cell = kCells[kind >= 0 && kind < 4 ? kind : (int)CritterKind_Small];
    memcpy(m_cell, cell, sizeof(m_cell));
    m_turned = kind == CritterKind_Rat;

    for (int frame = 0; frame < kCritterWarmFrames; frame++)
        update(nullptr);
    m_spawnDelay = kCritterSpawnDelay;
}

// FS2 0x002b3dc0
// callcheck: operator_delete, ~StageObject, operator_delete

CritterNest::~CritterNest()
{
    for (Critter *critter : m_critters)
        delete critter;
    m_critters.clear();
}

// FS2 0x002b39e0
// callcheck: operator_delete, erase, operator_new, operator_new, operator_new, operator_new, insert (the vector and new on the host; the ROM's delete on a 0 from a critter is never taken)

void CritterNest::update(const float *scare)
{
    if ((u_int)m_critters.size() < (u_int)m_most && m_spawnDelay < ++m_spawnTimer) {
        m_spawnTimer = 0;
        Critter *critter;
        if (m_kind == CritterKind_Long)
            critter = new CritterCrawler(10.0f, 5.0f);
        else if (m_kind == CritterKind_Rat)
            critter = new CritterRat();
        else
            critter = new CritterCrawler(5.0f, 5.0f);
        copyVector(critter->m_position, m_xform.position);
        critter->m_position[2] += 1.0f;
        m_critters.push_back(critter);
    }
    for (Critter *critter : m_critters)
        critter->update(m_range, m_xform.position, scare);
}

// FS2 0x002b38b0
// callcheck: applyMatrix, FUN_002b52c0

void CritterNest::draw(const sceVu0FVECTOR *view, const sceVu0FVECTOR *screen, sceVif1Packet *packet) const
{
    for (const Critter *critter : m_critters) {
        if (critter->m_state == CritterState_Hidden)
            continue;
        sceVu0FVECTOR place;
        applyMatrix(place, view, critter->m_position);
        if (place[1] > 0.0f && place[1] < kCritterFar)
            critter->draw(view, screen, m_cell, m_turned, packet);
    }
}

Critter::Critter(float half74, float half78) : m_half{half74, half78}
{
    static const sceVu0FVECTOR kUp = {0.0f, 0.0f, 1.0f, 1.0f};
    copyVector(m_normal, kUp);
    memset(m_ahead, 0, sizeof(m_ahead)); // PC: the ROM's first frame always sets it
    identityMatrix(m_matrix);
}

// The sprite in the surface's plane, its two cells side by side. The test for
// being on the frame takes the display's width, as StageParticle::drawQuad.
// FS2 0x002b52c0
// callcheck: multiplyMatrix, rotTransPers, sceVif1PkCnt, FUN_0010afd0, sceVif1PkOpenGifTag, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkCloseGifTag, FUN_0010b028, sceVif1PkCloseDmaTag (rotTransPers is the two applyMatrix; the packets are YamiPan_ModelDrawQuad)

void Critter::draw(const sceVu0FVECTOR *view, const sceVu0FVECTOR *screen, const int *cell, bool turned,
                   sceVif1Packet *packet) const
{
    sceVu0FMATRIX toView;
    multiplyMatrix(toView, view, m_matrix);

    const float halfX = m_half[turned ? 0 : 1];
    const float halfY = m_half[turned ? 1 : 0];
    const sceVu0FVECTOR corners[4] = {
        {-halfX, halfY, 0.0f, 1.0f},
        {halfX, halfY, 0.0f, 1.0f},
        {-halfX, -halfY, 0.0f, 1.0f},
        {halfX, -halfY, 0.0f, 1.0f},
    };

    YamiPanModelQuad quad;
    bool onFrame = false;
    const float frameHalfWidth = kFrameHalfWidth / YamiPan_DrawUiSqueezeFactor();
    for (int corner = 0; corner < 4; corner++) {
        sceVu0FVECTOR place;
        sceVu0FVECTOR onScreen;
        applyMatrix(place, toView, corners[corner]);
        applyMatrix(onScreen, screen, place);
        if (onScreen[3] <= 0.0f)
            return;
        const float x = onScreen[0] / onScreen[3];
        const float y = onScreen[1] / onScreen[3];
        if (x < 0.0f || x >= kScreenLimit || y < 0.0f || y >= kScreenLimit || onScreen[2] < 0.0f)
            return;
        if (fabsf(x - kScreenCenter) <= frameHalfWidth && fabsf(y - kScreenCenter) <= kFrameHalfHeight)
            onFrame = true;
        for (int axis = 0; axis < kAxes; axis++)
            quad.position[corner][axis] = place[axis];
    }
    if (!onFrame)
        return;

    const int across = m_frame * (cell[2] - cell[0]);
    const int left = cell[0] + across;
    const int right = cell[2] + across;
    const int top = cell[1];
    const int bottom = cell[3];
    const int us[2][4] = {{left, right, left, right}, {left, left, right, right}};
    const int vs[2][4] = {{top, top, bottom, bottom}, {bottom, top, bottom, top}};
    for (int corner = 0; corner < 4; corner++) {
        quad.uv[corner][0] = (float)us[turned ? 1 : 0][corner];
        quad.uv[corner][1] = (float)vs[turned ? 1 : 0][corner];
    }
    quad.color = kCritterColor;
    quad.owner = this;      // PC: smooth motion pairs the critter across frames
    quad.slot = 0;
    YamiPan_ModelDrawQuad(packet, &quad);
}

void Critter::addToPlace(const sceVu0FVECTOR step)
{
    addVector3(m_position, step);
}

// One point of the surface's normal off the face it hit.
void Critter::land(const StageCollisionFace *face, const sceVu0FVECTOR hit)
{
    copyVector(m_normal, face->plane);
    m_normal[3] = 1.0f;
    copyVector(m_position, hit);
    addVector3(m_position, m_normal);
}

// Shared by both classes' updates: the turn takes 6 degrees a frame, then it
// stands or walks.
void Critter::stepTurn(int walkFrames)
{
    if (kCritterTurnStep < m_turnLeft) {
        m_heading += kCritterTurnStep;
        m_turnLeft -= kCritterTurnStep;
    } else if (m_turnLeft < -kCritterTurnStep) {
        m_heading -= kCritterTurnStep;
        m_turnLeft += kCritterTurnStep;
    } else {
        m_heading += m_turnLeft;
        m_turnLeft = 0.0f;
        if ((int)randNext() % 100 < 50) {
            m_state = CritterState_Idle;
            m_timer = ((int)randNext() % 30) * 9 + 30;
        } else {
            m_state = CritterState_Walk;
            m_speed = 1.0f;
            m_speed += (float)((int)randNext() % 40 - 20) / 100.0f;
            m_timer = ((int)randNext() % 30) * 3 + walkFrames;
        }
    }
    m_heading = wrapAngle(m_heading);
}

// What both classes end a frame with: it feels for the surface under it unless
// it stands, then the matrix if it moved, then the walk's two cells. The two
// update functions of the ROM repeat this.
void Critter::endFrame(float range, const sceVu0FVECTOR nest, bool moved)
{
    sceVu0FVECTOR below = {0.0f, 0.0f, -reach(), 1.0f};
    if (m_surface != nullptr)
        scaleVec4(-reach(), below, m_normal);
    sceVu0FVECTOR to;
    copyVector(to, m_position);
    addVector3(to, below);

    if (m_surface == nullptr || m_state != CritterState_Idle) {
        sceVu0FVECTOR hit;
        const StageCollisionFace *face = gStageScene->raycast(hit, m_position, to, 0, kFaceSolid, kFaceSkip, nullptr);
        if (face != nullptr) {
            land(face, hit);
        } else if (m_surface == nullptr) {
            m_position[2] -= fallSpeed();
            if (range < nest[2] - m_position[2])
                copyVector(m_position, nest);
        } else {
            face = leftSurface();
        }
        m_surface = face;
        moved = true;
    }

    if (moved) {
        static const sceVu0FVECTOR kUp = {0.0f, 0.0f, 1.0f, 1.0f};
        static const sceVu0FVECTOR kForward = {0.0f, 1.0f, 0.0f, 0.0f};
        // The turn from straight up to the normal, both already of length 1.
        sceVu0FVECTOR tilt;
        quatBetween(tilt, kUp, m_normal, 1);
        sceVu0FVECTOR axisAngle;
        copyVector(axisAngle, m_normal);
        axisAngle[3] = m_heading;
        sceVu0FVECTOR turn;
        quatFromAxisAngle(turn, axisAngle);
        quatMultiply(turn, turn, tilt);
        quatToMatrix(m_matrix, turn);
        translateMatrix(m_matrix, m_matrix, m_position);
        applyMatrix(m_ahead, m_matrix, kForward);
    }

    if (m_lastFrame > 0 && m_state != CritterState_Idle && ++m_frameTicks > kCritterFrameTicks) {
        m_frameTicks = 0;
        if (m_lastFrame < ++m_frame)
            m_frame = 0;
    }
}

void Critter::update(float range, const sceVu0FVECTOR nest, const float *scare)
{
    if (scare != nullptr && m_state != CritterState_Fleeing && m_state != CritterState_Hidden &&
        vecDistanceSq(scare, m_position) < kCritterScareDistanceSq) {
        m_state = CritterState_Scared;
    }
    bool moved = true;
    if (m_surface != nullptr)
        moved = step(range, nest, scare);
    endFrame(range, nest, moved);
}

// A step along the heading; a wall a step ahead takes it only if it is nearly
// the same surface, otherwise it stops.
void CritterRat::walk()
{
    sceVu0FVECTOR stride;
    scaleVec4(m_forward ? m_speed : -m_speed, stride, m_ahead);
    sceVu0FVECTOR to;
    copyVector(to, m_position);
    addVector3(to, stride);
    to[3] = 1.0f;

    sceVu0FVECTOR hit;
    const StageCollisionFace *face = gStageScene->raycast(hit, m_position, to, 0, kFaceSolid, kFaceSkip, nullptr);
    if (face == nullptr) {
        addToPlace(stride);
    } else if (kCritterRatClimb < vecDot(m_normal, face->plane)) {
        land(face, hit);
        m_surface = face;
    } else {
        m_state = CritterState_Idle;
        m_timer = ((int)randNext() % 30) * 3 + 1;
    }
    if (m_state == CritterState_Walk && --m_timer < 1) {
        m_state = CritterState_Idle;
        m_timer = ((int)randNext() % 30) * 6 + 60;
    }
}

void CritterRat::faceAwayFrom(const sceVu0FVECTOR nest)
{
    static const sceVu0FVECTOR kNorth = {0.0f, 1.0f, 0.0f, 1.0f};
    sceVu0FVECTOR away;
    for (int axis = 0; axis < kAxes; axis++)
        away[axis] = m_position[axis] - nest[axis];
    away[3] = 0.0f;
    vecNormalize(away, away);
    const float cosine = vecDot(kNorth, away);
    const float side = away[0] >= 0.0f ? -1.0f : 1.0f;
    m_heading = wrapAngle(side * asinAcos(cosine, AsinAcos_Acos) - kPi);
}

// FS2 0x002b4070
// callcheck: vecDistanceSq, vecNormalize, vecDot, asinAcos, wrapAngle, randNext, scaleVec4, raycast, quatBetween, quatFromAxisAngle, quatMultiply, quatToMatrix, translateMatrix, applyMatrix (the rest in endFrame, Critter::update, walk, faceAwayFrom; 0x2bb510 in endFrame)

bool CritterRat::step(float range, const sceVu0FVECTOR nest, const float *scare)
{
    (void)scare;
    switch (m_state) {
    case CritterState_Walk:
        walk();
        break;
    case CritterState_Fleeing:
        if (kCritterHomeRatSq < vecDistanceSq(m_position, nest)) {
            faceAwayFrom(nest);
            walk();
        } else {
            m_state = CritterState_Hidden;
            m_timer = ((int)randNext() % 30) * 10 + 150;
        }
        break;
    case CritterState_Turn:
        stepTurn(30);
        break;
    case CritterState_Scared:
        faceAwayFrom(nest);
        m_forward = 1;
        m_speed = kCritterFleeSpeed;
        m_state = CritterState_Fleeing;
        break;
    default:
        if (--m_timer >= 1)
            return false;
        m_state = CritterState_Turn;
        if (vecDistanceSq(m_position, nest) < range * range) {
            m_turnLeft = (float)((int)randNext() % 360 - 180) * kPi / 180.0f;
        } else {
            sceVu0FVECTOR home;
            for (int axis = 0; axis < kAxes; axis++)
                home[axis] = nest[axis] - m_position[axis];
            home[3] = 0.0f;
            vecNormalize(home, home);
            m_turnLeft = asinAcos(vecDot(home, m_ahead), AsinAcos_Acos);
            m_turnLeft += (float)((int)randNext() % 20 - 10) * kPi / 180.0f;
            m_turnLeft = wrapAngle(m_turnLeft);
        }
        if (kPi / 2.0f < m_turnLeft) {
            m_turnLeft = wrapAngle(m_turnLeft - kPi);
            m_forward = !m_forward;
        } else if (m_turnLeft < -kPi / 2.0f) {
            m_turnLeft = wrapAngle(m_turnLeft + kPi);
            m_forward = !m_forward;
        }
        break;
    }
    return true;
}

const StageCollisionFace *CritterRat::leftSurface()
{
    m_state = CritterState_Idle;
    m_timer = ((int)randNext() % 30) * 3 + 1;
    return nullptr;
}

// A step along the heading; whatever it runs into, it stands on.
void CritterCrawler::walk()
{
    sceVu0FVECTOR stride;
    scaleVec4(m_speed, stride, m_ahead);
    sceVu0FVECTOR to;
    copyVector(to, m_position);
    addVector3(to, stride);
    to[3] = 1.0f;

    sceVu0FVECTOR hit;
    const StageCollisionFace *face = gStageScene->raycast(hit, m_position, to, 0, kFaceSolid, kFaceSkip, nullptr);
    if (face == nullptr) {
        addToPlace(stride);
    } else {
        land(face, hit);
        m_surface = face;
    }
    if (m_state == CritterState_Walk && --m_timer < 1) {
        m_state = CritterState_Idle;
        m_timer = ((int)randNext() % 30) * 5 + 60;
    }
}

// The signed turn that points it at the nest, within the surface.
float CritterCrawler::angleTo(const sceVu0FVECTOR nest) const
{
    sceVu0FVECTOR home;
    for (int axis = 0; axis < kAxes; axis++)
        home[axis] = nest[axis] - m_position[axis];
    home[3] = 0.0f;
    vecNormalize(home, home);
    sceVu0FVECTOR into;
    scaleVec4(-vecDot(home, m_normal), into, m_normal);
    addVector3(home, into);
    vecNormalize(home, home);

    float angle = asinAcos(vecDot(m_ahead, home), AsinAcos_Acos);
    sceVu0FVECTOR across;
    vecCross(across, m_ahead, home);
    if (vecDot(m_normal, across) < 0.0f)
        angle = -angle;
    return angle;
}

// false: the wait goes on and nothing moved.
bool CritterCrawler::waitOrTurn(float range, const sceVu0FVECTOR nest)
{
    if (--m_timer >= 1)
        return false;
    m_state = CritterState_Turn;
    if (vecDistanceSq(m_position, nest) < range * range) {
        m_turnLeft = (float)((int)randNext() % 360 - 180) * kPi / 180.0f;
    } else {
        m_turnLeft = angleTo(nest);
        m_turnLeft += (float)((int)randNext() % 30 - 15) * kPi / 180.0f;
        m_turnLeft = wrapAngle(m_turnLeft);
    }
    return true;
}

// FS2 0x002b4950
// callcheck: vecDistanceSq, vecNormalize, vecDot, scaleVec4, asinAcos, vecCross, wrapAngle, randNext, raycast, quatBetween, quatFromAxisAngle, quatMultiply, quatToMatrix, translateMatrix, applyMatrix (the rest in endFrame, Critter::update, walk, angleTo, waitOrTurn; 0x2bb510 in endFrame)

bool CritterCrawler::step(float range, const sceVu0FVECTOR nest, const float *scare)
{
    switch (m_state) {
    case CritterState_Hidden:
        // PC: the ROM reads through a null scare; nothing near then.
        if (scare != nullptr && vecDistanceSq(scare, m_position) < kCritterScareDistanceSq)
            break;
        return waitOrTurn(range, nest);
    case CritterState_Walk:
        walk();
        break;
    case CritterState_Fleeing:
        if (kCritterHomeCrawlerSq < vecDistanceSq(m_position, nest)) {
            if (m_timer % 30 == 0)
                m_heading = wrapAngle(m_heading + angleTo(nest));
            if (++m_timer < kCritterFleeFrames) {
                walk();
            } else {
                m_state = CritterState_Hidden;
                m_timer = 1;
            }
        } else {
            m_state = CritterState_Hidden;
            m_timer = ((int)randNext() % 30) * 10 + 30;
        }
        break;
    case CritterState_Turn:
        stepTurn(15);
        break;
    case CritterState_Scared:
        m_heading = wrapAngle(m_heading + angleTo(nest));
        m_speed = kCritterFleeSpeed;
        m_state = CritterState_Fleeing;
        m_timer = 0;
        break;
    default:
        return waitOrTurn(range, nest);
    }
    return true;
}

// Round the edge: back under the face, then towards where it came from.
const StageCollisionFace *CritterCrawler::leftSurface()
{
    sceVu0FVECTOR under;
    scaleVec4(-kCritterEdgeDepth, under, m_normal);
    addVector3(under, m_position);
    under[3] = m_position[3];
    sceVu0FVECTOR back;
    scaleVec4(-kCritterEdgeBack, back, m_ahead);
    sceVu0FVECTOR to;
    copyVector(to, under);
    addVector3(to, back);

    sceVu0FVECTOR hit;
    const StageCollisionFace *face = gStageScene->raycast(hit, under, to, 0, kFaceSolid, kFaceSkip, nullptr);
    if (face != nullptr)
        land(face, hit);
    return face;
}

// FS2 0x0018c060, 0x0018c070, 0x0018c080, 0x0018c090: the side base's hooks, empty.
// FS2 0x002b3870, 0x002b3880, 0x002b3890, 0x002b38a0: the nest's, empty.
// FS2 0x002b56f0, 0x002b5700, 0x002b5710, 0x002b5720, 0x002b5730: thunks.
// FS2 0x001905e0, 0x001905f0, 0x00190600, 0x00190610, 0x00190620, 0x00190630, 0x00190640: thunks.
// FS2 0x0018c3b0: StageParticle::drawCells, drawQuad's cells.
// FS2 0x00191e90: the 0x40-byte object's dtor, ~StageNodeTree.
// FS2 0x00192080: its ctor, StageNodeTree.
// FS2 0x001914c0: update's body.
// FS2 0x00190c70: draw's body.
// FS2 0x001918b0, 0x001918c0, 0x001920d0, 0x00192660, 0x001927b0, 0x001928f0: the containers' templates (std::vector here).
// FS2 0x00192db0, 0x00193210, 0x00193410, 0x00193770: the MSL vector of the effects' sort lists (SortedEffect; std::vector here).
// FS2 0x001933a0, 0x001936e0, 0x00193780, 0x001937f0, 0x00193880, 0x001938f0: BoxCells, BoxColumn and BoxList's dtors and clears, for setBoxes and ~StageNodeTree (game/boxgrid.h; std::vector here).
