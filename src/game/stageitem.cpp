/* ==========================================================================
 *  game/stageitem.cpp
 *
 *  The bullet and the items, 0x17fce0..0x184860 (game/stageitem.h). The list
 *  instances at the end of the run are game/stagelist.h's; the adjustor
 *  thunks that close the file are listed at its end. The file name is the
 *  port's.
 * ======================================================================== */
#include "game/playermotion.h"
#include "game/stageitem.h"

#include <stdio.h>
#include <string.h>

#include "game/areagrid.h"
#include "game/clock.h"
#include "game/heap.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"
#include "game/stagesound.h"
#include "game/sysdata.h"
#include "game/vu0math.h"

// One frame of the game's 30, as the motion calls count them.
static constexpr float kFrameSeconds = 0.033333335f;
static constexpr float kPi = 3.1415927f;
// atan2Octant's half turn around a character's facing: within this it is in
// front (canPick, canSpring).
static constexpr float kPickFacing   = 1.1219975f;
static constexpr float kSpringFacing = 0.7853982f;

/* sdata 0x2f7c20 */
static const char *const kLandSounds[kItemLandSounds] = {
    "SMOKE_LAND", "CAN_LAND", "DRINK_LAND", "NUT_LAND", "KNIFE_LAND",
};
/* sdata 0x2f7c40 */
static const char *const kPickSounds[kItemPickSounds] = {
    "GETITEM1", "GETITEM2", "GETITEM3", "GETITEM4", "GETITEM5", "GETITEM6", "GETITEM7",
    "GETITEM11", "WEAPON1", "WEAPON2", "WEAPON3", "WEAPON4", "WEAPON5", "WEAPON6",
    "WEAPON7", "WEAPON8", "WEAPON9", "WEAPON10", "GETITEM12",
};
// "????" in the 16-bit big-endian text the mission's items use.
static const char kUnknownItemName[] = { 0, '?', 0, '?', 0, '?', 0, '?', 0, 0 }; /* rdata 0x303860 */

static const char kFlareName[] = "I_HATUEN";

// ──────────────────────────────────────────────────────────────────────
// index = name's place in table; an empty name is -1 and fine, an unknown one
// -1 and false. The ROM has the loop twice.
static bool setSoundIndex(signed char &index, const char *name, const char *const *table, int count)
{
    index = -1;
    if (name[0] == '\0')
        return true;
    for (int i = 0; i < count; i++) {
        if (strcmp(table[i], name) == 0) {
            index = (signed char)i;
            return true;
        }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0017fd40
// callcheck: strcmp (in setSoundIndex)

bool ItemDef::setLandSound(const char *soundName)
{
    return setSoundIndex(landSound, soundName, kLandSounds, kItemLandSounds);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0017fdf0
// callcheck: strcmp (in setSoundIndex)

bool ItemDef::setPickSound(const char *soundName)
{
    return setSoundIndex(pickSound, soundName, kPickSounds, kItemPickSounds);
}

// ──────────────────────────────────────────────────────────────────────
// A weapon's definition. PC: the first for a bad index, where the ROM reads
// from address 0.
static const WeaponDef &weaponDefAt(int index)
{
    return gWeaponDefs[index < 0 || index >= kWeaponDefCount ? 0 : index];
}

const ItemDef &StageItem::itemDef() const
{
    return gItemDefs[m_itemDef < 0 || m_itemDef >= kItemDefCount ? 0 : m_itemDef];
}

// ──────────────────────────────────────────────────────────────────────
// The bullet.

// The ROM's is inline in StageBullet's: the base's, the name, the list.
StageBulletBase::StageBulletBase(StageCharacter *owner, StageItem *weapon)
    : m_owner(owner), m_weapon(weapon)
{
    setName("bullet");
    gStageScene->objects->addBullet(this);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180670
// callcheck: ~StageObject, operator_delete (the base, the deleting dtor)

StageBulletBase::~StageBulletBase()
{
    if (m_dead)
        gStageScene->objects->removeBullet(this);
}

// ──────────────────────────────────────────────────────────────────────
// Two bodies follow the bullet: a capsule for what it can meet, a segment
// for the map. A weapon gives the range in frames; speed is per frame at the
// clock's rate.
// FS2 0x001803e0
// callcheck: StageObject, setName, addBullet, add, getDeltaSeconds, alloc, operator_new, CollisionBody, add, operator_new, CollisionBody, add (the base is inline in the ROM)

StageBullet::StageBullet(StageCharacter *owner, StageItem *weapon)
    : StageBulletBase(owner, weapon)
{
    gStageScene->pairs->add(this, ScenePairList_Items);
    float dt = gStageScene->clock->getDeltaSeconds();
    m_speed = kBulletUnitsPerSecond / (float)(int)(1.0f / dt);
    if (weapon != nullptr)
        m_framesLeft = (int)((float)weaponDefAt(weapon->m_weaponDef).range / m_speed + 1.0f);
    m_lastPosition = (float *)gGlobalHeap.alloc(0x10, 0x10);

    m_bodies[0] = new CollisionBody(this, kBulletBodyHitBucket, kCollisionBulletHit, kBulletHitMask);
    gStageScene->records->add(m_bodies[0]);
    m_bodies[0]->shape = CollisionShape_Capsule;
    m_bodies[0]->capsuleRadius = kBulletRadius;

    m_bodies[1] = new CollisionBody(this, kBulletBodyRayBucket, kCollisionBulletRay, kBulletRayMask);
    gStageScene->records->add(m_bodies[1]);
    m_bodies[1]->shape = CollisionShape_Segment;
    m_bodies[1]->rayMask = kBulletRayFaceMask;
    m_bodies[1]->rayFlags = kBulletRayFaceFlags;
}

// ──────────────────────────────────────────────────────────────────────
// Only a bullet that died in flight leaves the scene; the scene's teardown
// takes the rest. The base's body is inline in the ROM, the bodies' dtors too.
// FS2 0x00180260
// callcheck: remove, remove, FUN_00184260, FUN_00184430, operator_delete, removeBullet, ~StageObject, operator_delete (the bodies EE dtors are inline, the rest the base)

StageBullet::~StageBullet()
{
    if (m_dead) {
        gStageScene->pairs->remove(this);
        for (int i = 0; i < kBulletBodies; i++) {
            gStageScene->records->remove(m_bodies[i]);
            delete m_bodies[i];
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0017fd20

StageObject *StageBullet::getCollisionParent()
{
    return m_owner;
}

// ──────────────────────────────────────────────────────────────────────
// The segment's contact is the map: the bullet stops at it unless the face
// lets it through, and then only a face further on stops it. The capsule's
// contact kills the bullet unless the map is nearer.
// FS2 0x0017fea0
// callcheck: scaleVec4, randNext, raycast, vecDistance

int StageBullet::onCollide(CollisionContact *contact, const float *sweep, StageCollisionFace *face,
                           float t, float distance)
{
    CollisionBody *self = contact->self;
    if (self->category == kCollisionBulletRay) {
        sceVu0FVECTOR step;
        scaleVec4(t, step, sweep);
        for (int i = 0; i < 4; i++)
            m_hitPoint[i] = self->segment[1][i] + step[i];
        if ((face->flags & kFaceBulletPasses) == 0) {
            m_hit = 1;
        }
        else if (face->doorId == 0 && (int)randNext() % 100 < 50) {
            m_hit = 1;
        }
        else {
            sceVu0FVECTOR from;
            memcpy(from, m_hitPoint, sizeof(from));
            face = gStageScene->raycast(m_hitPoint, from, self->segment[1], 0, self->rayMask,
                                        self->rayFlags | kFaceBulletPasses, nullptr);
            if (face != nullptr) {
                m_hit = 1;
                distance = vecDistance(self->segment[0], m_hitPoint);
            }
        }
        m_hitFace = face;
        m_hitDistance = distance;
    }
    else if (self->category == kCollisionBulletHit) {
        if (!m_hit)
            m_dead = 1;
        else if (distance < m_hitDistance)
            m_dead = 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// After the frame's contacts: a bullet still flying that hit the map shows
// it there, with the surface's effect and sound, and is gone.
// FS2 0x00180030
// callcheck: getStageSelect, spawn, getSurfaceSound, getWorldMatrix, playEntryHeard

void StageBullet::collisionHook3(CollisionBody *body)
{
    if (body->category != kCollisionBulletHit || !m_hit || m_dead)
        return;

    const char *effect = "GUNHIT";
    if (m_hitFace != nullptr) {
        u_char surface = m_hitFace->surface;
        if (surface == kSurfaceKyusyu)
            effect = getStageSelect()->stage == kStageKyusyu ? "kyusyu" : nullptr;
        else if (surface == kSurfaceSplash)
            effect = "GUN_SPLASH";
    }
    if (effect != nullptr)
        gStageScene->nodeTree->spawn(effect, nullptr, m_hitPoint, nullptr, 0, 0);

    // PC: the ROM reads the surface whatever the face is; m_hit means it is set.
    const SeEntry *entry = getSurfaceSound((signed char)m_hitFace->surface);
    if (entry != nullptr)
        playEntryHeard(entry, getWorldMatrix()[3], nullptr);
    m_dead = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The two ends of the frame's sweep for the body: the position propagate
// kept, and where the bullet is now.
// FS2 0x00180170

void StageBullet::syncBody(CollisionBody *body)
{
    sceVu0FVECTOR *ends;
    if (body->category == kCollisionBulletRay)
        ends = body->segment;
    else if (body->category == kCollisionBulletHit)
        ends = body->capsule;
    else
        return;
    memcpy(ends[0], m_lastPosition, sizeof(ends[0]));
    memcpy(ends[1], m_xform.position, sizeof(ends[1]));
}

// ──────────────────────────────────────────────────────────────────────
// The frame. A shooter with no scatter (m_shotScatter 0) keeps steering it at
// its aim point.
// FS2 0x001801c0
// callcheck: vecNormalize, scaleVec4

void StageBullet::sideHook2()
{
    // PC: the bullet setupCameras makes has no owner (and never runs).
    if (m_owner != nullptr && m_owner->m_shotScatter == 0.0f) {
        for (int i = 0; i < 4; i++)
            m_velocity[i] = m_owner->m_aimPoint[i] - m_xform.position[i];
        vecNormalize(m_velocity, m_velocity);
        scaleVec4(m_speed, m_velocity, m_velocity);
    }
    for (int i = 0; i < 4; i++)
        m_xform.position[i] += m_velocity[i];
    if (m_framesLeft == 0)
        m_dead = 1;
    else if (m_owner != nullptr)
        m_owner->m_bulletFlying = 1;
    m_framesLeft--;
    m_hit = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The base item. The ROM's ctors of the four classes inline it.
// FS2 0x00183f40
// callcheck: StageObject, add

StageItem::StageItem()
{
    m_sprung = 0;
    gStageScene->pairs->add(this, ScenePairList_Items);
    m_itemDef = -1;
    m_holder = nullptr;
    m_inUse = 0;
    m_count = 0;
    m_itemMotion = nullptr;
    // PC: the ROM leaves these to setupItem.
    m_maxCount = 0;
    memset(m_pickPoint, 0, sizeof(m_pickPoint));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00183ec0
// callcheck: ~StageObject, operator_delete (the base's, the deleting dtor's)

StageItem::~StageItem()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00183ea0

void StageObject::markModelAsItem()
{
    m_model->m_ownAmbient = 1;
    m_model->m_lit = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The object joins the scene: group 3, its script by name, its model
// (flagged as an item) and motions: with more than one the rest go on the
// first, and the last starts. Count from the definition; pick point the
// motion's bn_pic, turned from the file's axes.
// FS2 0x00183bf0
// callcheck: setName, add3, findEntry, setEntry, setModel, setBaseMotion, addMotion, playMotion, applyMotion, bindBones, placeBones, motionBoneMatrix

void StageItem::setupItem(const char *name, ModelInstance *instance,
                          StringMap<Tim2Texture *> *textures, StageList<void *> *motions)
{
    setName(name);
    gStageScene->objects->add3(this);
    setEntry(gStageScene->scripts[0]->findEntry(m_name));
    instance->flags |= ModelFlag_Item;
    setModel(instance, textures);
    markModelAsItem();

    StageList<void *>::Node *first = motions->head;
    if (first != nullptr)
        m_itemMotion = first->value;
    if (motions->count > 1) {
        m_model->setBaseMotion(first->value, 0);
        ModelMotion *last = nullptr;
        for (StageList<void *>::Node *node = first->next; node != nullptr; node = node->next)
            last = m_model->addMotion(node->value, 0, nullptr);
        if (last != nullptr) {
            m_model->playMotion(last, false, false, 0x40, 0);
            m_model->applyMotion(m_xform.position, m_xform.rotation);
        }
    }
    m_model->bindBones();
    m_model->placeBones(this);

    const ItemDef &def = itemDef();
    m_maxCount = def.stackKind;
    if (m_count > m_maxCount)
        m_count = m_maxCount;
    m_count = def.startCount <= m_maxCount ? def.startCount : m_maxCount;

    memset(m_pickPoint, 0, sizeof(m_pickPoint));
    sceVu0FMATRIX bone;
    if (m_itemMotion != nullptr && motionBoneMatrix(bone, m_itemMotion, "bn_pic", 0) > 0) {
        m_pickPoint[0] = bone[3][0];
        m_pickPoint[1] = -bone[3][2];
        m_pickPoint[2] = bone[3][1];
    }
    m_pickPoint[3] = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00183b80
// callcheck: getDeltaSeconds, advance

void StageItem::sideHook2()
{
    m_model->advance(gStageScene->clock->getDeltaSeconds() / kFrameSeconds,
                     m_xform.position, m_xform.rotation, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Frozen unless the item lies free with its matrix dirty.
// FS2 0x00183af0

void StageItem::sideHook3()
{
    bool freeAndDirty = m_parent == nullptr && m_holder == nullptr && m_xform.dirty != 0
                        && !m_inUse && !m_sprung;
    if (freeAndDirty)
        m_model->data->flags &= ~ModelFlag_Frozen;
    else
        m_model->data->flags |= ModelFlag_Frozen;
}

// ──────────────────────────────────────────────────────────────────────
// who faces the pick point closely enough (past 30 units away).
static bool isFacing(StageObject *who, const sceVu0FVECTOR point)
{
    float angle = atan2Octant(who->m_xform.position[0] - point[0],
                              -(who->m_xform.position[1] - point[1]));
    angle = addAngle(angle, -who->m_xform.rotation[2]);
    angle = addAngle(angle, -kPi);
    return fabsf(angle) < kPickFacing;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00183790
// callcheck: getMatrix, applyMatrix, atan2Octant, addAngle, addAngle (isFacing, inline)

bool StageItem::canPickFar(StageObject *who)
{
    sceVu0FVECTOR point;
    applyMatrix(point, getMatrix(), m_pickPoint);
    float dx = who->m_xform.position[0] - point[0];
    float dy = who->m_xform.position[1] - point[1];
    float dz = who->m_xform.position[2] - point[2];
    float flat = dx * dx + dy * dy;
    if (sqrtf(dz * dz + flat + 0.0f) > 250.0f)
        return false;
    return sqrtf(flat) <= 30.0f || isFacing(who, point);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001838d0, 0x00183780 (a jump to it)
// callcheck: getMatrix, applyMatrix, atan2Octant, addAngle, addAngle (isFacing, inline)

bool StageItem::canPick(StageObject *who)
{
    sceVu0FVECTOR point;
    applyMatrix(point, getMatrix(), m_pickPoint);
    float dx = who->m_xform.position[0] - point[0];
    float dy = who->m_xform.position[1] - point[1];
    float dz = who->m_xform.position[2] - point[2];
    float flat = sqrtf(dx * dx + dy * dy + 0.0f);
    if (flat > 100.0f)
        return false;
    if (dz <= -150.0f || dz >= 50.0f)
        return false;
    return flat <= 30.0f || isFacing(who, point);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00183a40
// callcheck: currentMission, getItemText

const char *StageItem::getDisplayName()
{
    const char *text = gStageScene->currentMission()->getItemText(m_name);
    return text != nullptr ? text : kUnknownItemName;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00183a90

const char *StageItem::getPickSound()
{
    signed char sound = itemDef().pickSound;
    return sound < 0 ? nullptr : kPickSounds[sound];
}

// ──────────────────────────────────────────────────────────────────────
// A firearm's magazine full, nothing else's.
// FS2 0x001823f0

void StageItem::initWeapon()
{
    m_ammoB = itemDef().isFirearm() ? weaponDefAt(m_weaponDef).magazine : 0;
    m_ammoA = m_ammoB;
}

// ──────────────────────────────────────────────────────────────────────
// The thrown items. The ROM's StageItem part is inline here and in the three
// other ctors.
// FS2 0x00180d30
// callcheck: StageObject, add, ItemPhysics (the bases)

StageThrownItem::StageThrownItem()
    : ItemPhysics(false, true)
{
    m_smokeArea = kSmokeAreaUnasked;
    m_restTime = 0.0f;
    m_smoke = nullptr;
    m_roomSmoke = nullptr;
    m_burnTime = 0.0f;
    m_sound = nullptr;
    // PC: the ROM leaves it to setThrowVector.
    memset(m_throwVector, 0, sizeof(m_throwVector));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00183fe0
// callcheck: ~ItemPhysics, ~StageObject, operator_delete (the bases, the deleting dtor's)

StageThrownItem::~StageThrownItem()
{
}

// ──────────────────────────────────────────────────────────────────────
bool StageThrownItem::isFlare() const
{
    return strcmp(m_name, kFlareName) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// The flare's two fires start out, the room's only once it lies in a box.
// FS2 0x00180c40
// callcheck: setupItem, setup, strcmp, spawn, setRunning, spawn, setRunning, createBuffers

void StageThrownItem::setupItem(const char *name, ModelInstance *instance,
                                StringMap<Tim2Texture *> *textures, StageList<void *> *motions)
{
    StageItem::setupItem(name, instance, textures, motions);
    if (m_itemMotion != nullptr)
        ItemPhysics::setup(this, m_itemMotion);
    if (isFlare()) {
        m_smoke = gStageScene->nodeTree->spawn("hatsuen", this, nullptr, nullptr, 0, 0);
        if (m_smoke != nullptr)
            m_smoke->setRunning(false);
        m_roomSmoke = gStageScene->nodeTree->spawn("heyasmoke", this, nullptr, nullptr, 0, 0);
        if (m_roomSmoke != nullptr)
            m_roomSmoke->setRunning(false);
    }
    m_smokeArea = kSmokeAreaUnasked;
    gStageScene->createBuffers(this);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180710
// callcheck: strcmp, getMatrix, contains

bool StageThrownItem::isInSmokeArea()
{
    if (m_smokeArea == kSmokeAreaUnasked) {
        if (!isFlare()) {
            m_smokeArea = 0;
        }
        else {
            if (!m_inUse || m_moving)
                return false;
            m_smokeArea = gStageScene->areas[StageArea_Smoke]->contains(getMatrix()[3]) != nullptr;
        }
    }
    return m_smokeArea != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001807c0

const char *StageThrownItem::getLandSound()
{
    signed char sound = itemDef().landSound;
    return sound < 0 ? nullptr : kLandSounds[sound];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180820

void StageThrownItem::setThrowVector(const sceVu0FVECTOR vector)
{
    memcpy(m_throwVector, vector, sizeof(m_throwVector));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180830
// callcheck: strcmp

bool StageThrownItem::isSmoking()
{
    return isFlare() && m_burnTime < kFlareBurnSeconds;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001808b0
// callcheck: endFrame, addBufferObject

void StageThrownItem::sideHook3()
{
    if (m_itemMotion != nullptr)
        ItemPhysics::endFrame();
    gStageScene->addBufferObject(this);
}

// ──────────────────────────────────────────────────────────────────────
// The motion and the flight; then, used and at rest, the item goes after a
// while unless it is a flare still burning. A flare in a smoke box fills
// the room; otherwise it smokes where it lies, with its sound, until it has
// burnt out. The ROM has isInSmokeArea and isSmoking inline.
// FS2 0x001808f0
// callcheck: getDeltaSeconds, advance, update, strcmp, setActive, setShown, getDeltaSeconds, strcmp, getMatrix, contains, setRunning, setRunning, setRunning, strcmp, playNamedHeard2, getDeltaSeconds, stopSound, setRunning, setRunning

void StageThrownItem::sideHook2()
{
    float dt = gStageScene->clock->getDeltaSeconds();
    m_model->advance(dt / kFrameSeconds, m_xform.position, m_xform.rotation, 0);
    if (m_itemMotion != nullptr)
        ItemPhysics::update();
    if (!m_inUse)
        return;

    if (!m_moving) {
        if (!isSmoking()) {
            if (m_restTime >= kItemLingerSeconds) {
                m_tree->setActive(false);
                m_tree->setShown(false);
            }
            else {
                m_restTime += gStageScene->clock->getDeltaSeconds();
            }
        }
        if (m_roomSmoke != nullptr && !m_roomSmoke->m_running && isInSmokeArea()) {
            if (m_smoke != nullptr)
                m_smoke->setRunning(false);
            m_roomSmoke->setRunning(true);
        }
    }

    if (m_burnTime < kFlareBurnSeconds) {
        if (m_smoke != nullptr && !m_smoke->m_running
            && (m_roomSmoke == nullptr || !m_roomSmoke->m_running))
            m_smoke->setRunning(true);
        if (isFlare() && m_sound == nullptr) {
            // The ROM also has 800 in f12 here; nothing reads it.
            static const sceVu0FVECTOR kOrigin = { 0.0f, 0.0f, 0.0f, 1.0f }; /* sdata 0x2f7ca0 */
            m_sound = playNamedHeard2("SMOKE_ON", kOrigin, this);
        }
        m_burnTime += gStageScene->clock->getDeltaSeconds();
    }
    else {
        if (m_sound != nullptr)
            stopSound(m_sound);
        m_sound = nullptr;
        if (m_smoke != nullptr)
            m_smoke->setRunning(false);
        if (m_roomSmoke != nullptr)
            m_roomSmoke->setRunning(false);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The trap.
// FS2 0x00181200
// callcheck: StageObject, add, operator_new, CollisionBody, add (the bases, the body)

StageTrapItem::StageTrapItem()
{
    m_lingering = 0;
    m_lingered = 0.0f;
    m_body = new CollisionBody(this, 2, kCollisionTrap, 8);
    gStageScene->records->add(m_body);
    m_body->shape = CollisionShape_Sphere;
    m_body->sphere.radius = kTrapRadius;
    m_body->sphere.radiusSq = kTrapRadius * kTrapRadius;
    m_body->on = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The scene's teardown takes the body (the ROM frees nothing here).
// FS2 0x00181150
// callcheck: ~StageObject, operator_delete (the base's, the deleting dtor's)

StageTrapItem::~StageTrapItem()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180e40

void StageTrapItem::syncBody(CollisionBody *body)
{
    if (body->category == kCollisionTrap)
        memcpy(body->sphere.center, m_xform.position, sizeof(body->sphere.center));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180e70
// callcheck: getDeltaSeconds, advance, setActive, setShown, getDeltaSeconds

void StageTrapItem::sideHook2()
{
    m_model->advance(gStageScene->clock->getDeltaSeconds() / kFrameSeconds,
                     m_xform.position, m_xform.rotation, 0);
    if (!m_lingering)
        return;
    if (m_lingered >= kItemLingerSeconds) {
        m_tree->setActive(false);
        m_tree->setShown(false);
    }
    else {
        m_lingered += gStageScene->clock->getDeltaSeconds();
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180f40

void StageTrapItem::startLinger()
{
    m_lingering = 1;
    m_lingered = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180f50
// callcheck: playNamed

void StageTrapItem::spring()
{
    m_inUse = 0;
    m_sprung = 1;
    m_body->on = 0;
    m_model->playNamed(0, "s_OBJ_098_CLOSE", false, false, 0x40, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180f90

void StageTrapItem::disarm()
{
    m_body->on = 0;
    m_inUse = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180fa0
// callcheck: playNamed

void StageTrapItem::arm()
{
    m_body->on = 1;
    m_inUse = 1;
    m_model->playNamed(0, "s_OBJ_098_OPEN", false, false, 0x40, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00180fe0
// callcheck: vecDistance, atan2Octant, addAngle, addAngle

bool StageTrapItem::canSpring(StageObject *who)
{
    if (!m_inUse)
        return false;
    if (vecDistance(who->m_xform.position, m_xform.position) > kTrapSpringRange)
        return false;
    float angle = atan2Octant(who->m_xform.position[0] - m_xform.position[0],
                              -(who->m_xform.position[1] - m_xform.position[1]));
    angle = addAngle(angle, -who->m_xform.rotation[2]);
    angle = addAngle(angle, -kPi);
    return fabsf(angle) < kSpringFacing;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001810b0
// callcheck: atan2Octant, addAngle

void StageTrapItem::getWorkPlace(sceVu0FVECTOR pos, sceVu0FVECTOR rot, sceVu0FVECTOR lift,
                                 StageObject *who)
{
    memcpy(pos, m_xform.position, sizeof(sceVu0FVECTOR));
    memset(rot, 0, sizeof(sceVu0FVECTOR));
    rot[2] = atan2Octant(who->m_xform.position[0] - m_xform.position[0],
                         -(who->m_xform.position[1] - m_xform.position[1]));
    rot[2] = addAngle(rot[2], kPi);
    memset(lift, 0, sizeof(sceVu0FVECTOR));
    lift[1] = 60.0f;
}

// ──────────────────────────────────────────────────────────────────────
// The weapons.
// FS2 0x00182670
// callcheck: StageObject, add, ItemPhysics (the bases)

StageWeaponItem::StageWeaponItem()
    : ItemPhysics(true, false)
{
    m_ammoB = 0;
    m_ammoA = 0;
    m_weaponDef = -1;
    m_45c = 0;
    memset(m_aimPoint, 0, sizeof(m_aimPoint));
    memset(m_firePoint, 0, sizeof(m_firePoint));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00184110
// callcheck: ~ItemPhysics, ~StageObject, operator_delete (the bases, the deleting dtor's)

StageWeaponItem::~StageWeaponItem()
{
}

// ──────────────────────────────────────────────────────────────────────
// The motion's bones, turned from the file's axes.
// FS2 0x001825a0
// callcheck: setupItem, setup, motionBoneMatrix, motionBoneMatrix

void StageWeaponItem::setupItem(const char *name, ModelInstance *instance,
                                StringMap<Tim2Texture *> *textures, StageList<void *> *motions)
{
    StageItem::setupItem(name, instance, textures, motions);
    if (m_itemMotion != nullptr) {
        ItemPhysics::setup(this, m_itemMotion);
        sceVu0FMATRIX bone;
        if (motionBoneMatrix(bone, m_itemMotion, "bn_point", 0) > 0) {
            m_aimPoint[0] = bone[3][0];
            m_aimPoint[1] = -bone[3][2];
            m_aimPoint[2] = -bone[3][1];
            m_aimPoint[3] = bone[3][3];
        }
        if (motionBoneMatrix(bone, m_itemMotion, "bn_fire", 0) > 0) {
            m_firePoint[0] = bone[3][0];
            m_firePoint[1] = -bone[3][2];
            m_firePoint[2] = -bone[3][1];
            m_firePoint[3] = bone[3][3];
        }
    }
    m_45c = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00181400
// callcheck: getDeltaSeconds, advance, update

void StageWeaponItem::sideHook2()
{
    StageItem::sideHook2();
    if (m_itemMotion != nullptr)
        ItemPhysics::update();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00181340
// callcheck: endFrame

void StageWeaponItem::sideHook3()
{
    StageItem::sideHook3();
    if (m_itemMotion != nullptr)
        ItemPhysics::endFrame();
}

// ──────────────────────────────────────────────────────────────────────
bool StageWeaponItem::loadsByMagazine() const
{
    return !(strcmp(m_name, "G_NN38") == 0 || strcmp(m_name, "R_MURATA") == 0
             || strcmp(m_name, "G_26NEN") == 0);
}

// ──────────────────────────────────────────────────────────────────────
bool StageWeaponItem::isSamuraiBlade() const
{
    const ItemDef &def = itemDef();
    if (def.isBlade != 1)
        return false;
    if (def.kind == ItemKind_MeleeLight)
        return strcmp(m_name, "LW_SAMURAI_B") == 0;
    if (def.kind == ItemKind_MeleeHeavy)
        return strcmp(m_name, "LW_SAMURAI_B2") == 0;
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00181490

const char *StageWeaponItem::getPickSound()
{
    signed char sound = itemDef().pickSound;
    return sound < 0 ? nullptr : kPickSounds[sound];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001814f0
// callcheck: strcmp

const char *StageWeaponItem::getReloadSetSound()
{
    return strcmp(m_name, "R_MURATA") == 0 ? "RELOAD_SET2" : "RELOAD_SET1";
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00181540
// callcheck: strcmp, strcmp, strcmp, strcmp

const char *StageWeaponItem::getMagazineSetSound()
{
    if (loadsByMagazine())
        return itemDef().isPistol() ? "MAGAZ_SET1" : "MAGAZ_SET2";
    return strcmp(m_name, "R_MURATA") == 0 ? "RELOAD_OFF2" : "RELOAD_OFF1";
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001816d0
// callcheck: strcmp, strcmp, strcmp, strcmp

const char *StageWeaponItem::getMagazineOutSound()
{
    if (loadsByMagazine())
        return itemDef().isPistol() ? "MAGAZ_OUT1" : "MAGAZ_OUT2";
    return strcmp(m_name, "R_MURATA") == 0 ? "RELOAD_ON2" : "RELOAD_ON1";
}

// ──────────────────────────────────────────────────────────────────────
// By the kind, and a letter of the name where two guns share one.
// FS2 0x00181860

const char *StageWeaponItem::getShotSound()
{
    switch (itemDef().kind) {
    case 6:
        return m_name[2] == '6' ? "BT_SHOT_64" : "BT_SHOT_RF";
    case 5:
        return "BT_SHOT_89";
    case 4:
        return "BT_SHOT_26";
    case 3:
        return m_name[5] == 'M' ? "BT_SHOT_9MM" : "BT_SHOT_9M";
    case 2:
        return "BT_SHOT_NN";
    default:
        return "";
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00181960
// callcheck: strcmp, strcmp (isSamuraiBlade, inline)

const char *StageWeaponItem::getHitSound()
{
    const ItemDef &def = itemDef();
    if (def.isBlade == 1) {
        if (isSamuraiBlade())
            return "BT_AT_BLD";
        return def.kind == ItemKind_MeleeHeavy ? "BT_HT_H_BLD" : "BT_HT_L_BLD";
    }
    return def.kind == ItemKind_MeleeHeavy ? "BT_HT_H_AT" : "BT_HT_L_AT";
}

// ──────────────────────────────────────────────────────────────────────
// light picks the lighter of a melee weapon's two; null past those.
// FS2 0x00181b50
// callcheck: strcmp, strcmp (isSamuraiBlade, inline)

const char *StageWeaponItem::getSwingSound(bool light)
{
    const ItemDef &def = itemDef();
    if (isSamuraiBlade())
        return def.kind == ItemKind_MeleeHeavy ? "BT_ATL_H" : "BT_ATL_L";
    if (def.isFirearm())
        return def.isRifle() ? "BT_AT_RIFLE" : "BT_AT_GUN";
    if (def.kind == ItemKind_MeleeHeavy)
        return light ? "BT_ATH_L" : "BT_ATH_H";
    if (def.kind == ItemKind_MeleeLight)
        return light ? "BT_ATL_L" : "BT_ATL_H";
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Blank shots (the ROM's last argument) spend a round with no bullet; a
// character with +0xbd4 set on its motion shows no flash. The flash light
// lives a tenth of a second.
// FS2 0x00181f40
// callcheck: operator_new, StageBullet, setPosition, vecNormalize, scaleVec4, updateMatrix, propagate, spawn, strcmp, strcmp, strcmp, spawn, operator_new, PointLight, addToScene, setPosition, erase, erase, pushBack, setVector, setRadiusSquared, setLifetime, getShotSound, getWorldMatrix, playNamedHeard2, playerEvent (setParent is inline in the ROM)

void StageWeaponItem::fire(StageCharacter *shooter, const sceVu0FVECTOR from, const sceVu0FVECTOR to,
                           bool blank)
{
    if (!itemDef().isFirearm() || m_ammoA <= 0)
        return;
    int left = m_ammoA - 1;
    if (left < 0)
        left = 0;
    m_ammoA = left <= m_ammoB ? left : m_ammoB;

    if (!blank) {
        StageBullet *bullet = new StageBullet(shooter, this);
        bullet->m_xform.setPosition(from);
        for (int i = 0; i < 4; i++)
            bullet->m_velocity[i] = to[i] - bullet->m_xform.position[i];
        vecNormalize(bullet->m_velocity, bullet->m_velocity);
        scaleVec4(bullet->m_speed, bullet->m_velocity, bullet->m_velocity);
        bullet->updateMatrix();
        bullet->propagate();
    }

    if (!shooter->m_motion->scopeOn) {
        static const sceVu0FVECTOR kFlashTurn = { 0.0f, 1.5707964f, 0.0f, 0.0f }; /* sdata 0x2f7c90 */
        gStageScene->nodeTree->spawn("M_Flash", this, m_firePoint, kFlashTurn, 0, 0);
        if (loadsByMagazine())
            gStageScene->nodeTree->spawn("yakkyo", this, nullptr, nullptr, 0, 0);
    }

    PointLight *light = new PointLight;
    light->addToScene("flash", true);
    light->m_xform.setPosition(m_firePoint);
    light->setParent(this);
    light->setVector(0.5f, 0.5f, 0.5f);
    light->setRadiusSquared(90000.0f);
    light->m_castsShadow = 0;
    light->setLifetime(0.1f);

    const char *sound = getShotSound();
    // The ROM also has 800 in f12 here; nothing reads it.
    shooter->playNamedHeard2(sound, shooter->m_weapons[0]->getWorldMatrix()[3], nullptr);
    shooter->playerEvent("GUNSHOOT");
}

// ──────────────────────────────────────────────────────────────────────
// The lamp's four lights.

// One light under the owner, in both trees, pointing down the model's axis.
// Inline four times in the ROM.
static void hangLight(StageLight *light, StageObject *owner)
{
    light->m_xform.setRotation(0.0f, 0.0f, -1.5707964f);
    light->setParent(owner);
    light->m_tree->setParent(owner->m_tree);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182f40
// callcheck: sprintf, operator_new, SpotLight, addToScene, setRotation, erase, pushBack, erase, pushBack, setVector, setRadiusSquared, setConeAngle, PointLight, PartSwitch, init (the parent links inline; the node's and the tree's list instances)

ItemLamp::ItemLamp(StageObject *owner)
    : m_owner(owner)
{
    char name[0x10];

    sprintf(name, "%s_sl", owner->m_name);
    m_spot = new SpotLight;
    m_spot->addToScene(name, true);
    hangLight(m_spot, owner);
    m_spot->m_limitByMap = 1;
    m_spot->m_priority = 0;
    m_spot->m_mode = StageLight_Hard;

    sprintf(name, "%s_sl2", owner->m_name);
    m_wideSpot = new SpotLight;
    m_wideSpot->addToScene(name, false);
    m_wideSpot->setVector(1.0f, 1.0f, 1.0f);
    m_wideSpot->setRadiusSquared(40000.0f);
    m_wideSpot->setConeAngle(45);
    hangLight(m_wideSpot, owner);
    m_wideSpot->m_mode = StageLight_Hard;

    sprintf(name, "%s_pl", owner->m_name);
    m_point = new PointLight;
    m_point->addToScene(name, true);
    hangLight(m_point, owner);
    m_point->m_castsShadow = 0;
    m_point->m_mode = StageLight_Hard;

    sprintf(name, "%s_pl2", owner->m_name);
    m_point2 = new PointLight;
    m_point2->addToScene(name, true);
    hangLight(m_point2, owner);
    m_point2->m_castsShadow = 0;
    m_point2->m_usable = 0;
    m_point2->m_priority = 0;
    m_point2->m_mode = StageLight_Hard;

    m_parts = new PartSwitch;
    m_parts->init(owner->m_model->data, "nl", 2, 1);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182ee0
// callcheck: ~PartSwitch, operator_delete

ItemLamp::~ItemLamp()
{
    delete m_parts;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182b80

void ItemLamp::setCastsShadow(bool on)
{
    m_spot->m_castsShadow = on;
    m_wideSpot->m_castsShadow = on;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182ba0

void ItemLamp::setValue2b0(int value)
{
    m_point2->m_priority = value;
    m_spot->m_priority = value;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182bc0
// callcheck: erase, pushBack (setParent is inline in the ROM)

void ItemLamp::attachTo(StageNodeBase *parent)
{
    m_spot->setParent(parent);
    m_wideSpot->setParent(parent);
    m_point->setParent(parent);
    m_point2->setParent(parent);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182eb0
// callcheck: setScriptFlag

void ItemLamp::setSwitchedOn(bool on)
{
    m_owner->m_tree->setScriptFlag(on);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182ec0
// callcheck: show

void ItemLamp::setDark(bool dark)
{
    m_parts->show(dark ? 0 : 1);
}

// ──────────────────────────────────────────────────────────────────────
// StageItemE.
// FS2 0x00182ad0
// callcheck: StageObject, add (the base)

StageItemE::StageItemE()
{
    m_isTorch2 = 0;
    m_lamp = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182a00
// callcheck: ~PartSwitch, operator_delete, ~StageObject, operator_delete (the lamp's dtor inline, the base's)

StageItemE::~StageItemE()
{
    delete m_lamp;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182970
// callcheck: setupItem, operator_new, ItemLamp, setScriptFlag, strcmp

void StageItemE::setupItem(const char *name, ModelInstance *instance,
                           StringMap<Tim2Texture *> *textures, StageList<void *> *motions)
{
    StageItem::setupItem(name, instance, textures, motions);
    m_lamp = new ItemLamp(this);
    m_lamp->m_owner->m_tree->setScriptFlag(false);
    if (strcmp(name, "T_TORCH2") == 0)
        m_isTorch2 = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00184230
// callcheck: show

void StageItemE::setDark(bool dark)
{
    m_lamp->m_parts->show(dark ? 0 : 1);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00184210
// callcheck: setScriptFlag

void StageItemE::setSwitchedOn(bool on)
{
    m_lamp->m_owner->m_tree->setScriptFlag(on);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00184200

bool StageItemE::isSwitchedOn()
{
    return m_lamp->m_spot->m_switchedOn != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001828b0

bool StageItemE::ownsLight(StageObject *light) const
{
    return light == m_lamp->m_spot || light == m_lamp->m_wideSpot
           || light == m_lamp->m_point || light == m_lamp->m_point2;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00182930

void StageItemE::setShadowOwner(StageObject *owner)
{
    m_lamp->m_spot->m_shadowOwner = owner;
    m_lamp->m_wideSpot->m_shadowOwner = owner;
    m_lamp->m_point->m_shadowOwner = owner;
    m_lamp->m_point2->m_shadowOwner = owner;
}

// ──────────────────────────────────────────────────────────────────────
// With a holder: from its head (at the item's height) out to the light and
// 40 past; a wall on that line moves the matrix 30 back from it.
// FS2 0x00182770
// callcheck: getWorldMatrix, FUN_00136430, find, getMatrix, getMatrix, vecDirection, scaleVec4, raycast, scaleVec4 (FUN_00136430 is copyMatrix)

void StageItemE::getLightMatrix(sceVu0FMATRIX out)
{
    PointLight *light = m_lamp->m_point2;
    copyMatrix(out, light->getWorldMatrix());
    StageObject *holder = light->m_shadowOwner;
    if (holder == nullptr)
        return;

    BoneMap::Node *bone = holder->m_model->boneMap.find("bn_head");
    sceVu0FVECTOR head;
    memcpy(head, bone->value->getMatrix()[3], sizeof(head));
    head[2] = getMatrix()[3][2];

    sceVu0FVECTOR dir, step, tip, hit;
    vecDirection(dir, out[3], head);
    scaleVec4(40.0f, step, dir);
    for (int i = 0; i < 4; i++)
        tip[i] = out[3][i] + step[i];
    if (gStageScene->raycast(hit, head, tip, 0, 0xe42600f, 0x40000, nullptr) != nullptr) {
        scaleVec4(-30.0f, step, dir);
        for (int i = 0; i < 4; i++)
            out[3][i] = hit[i] + step[i];
    }
}

// ──────────────────────────────────────────────────────────────────────
// The adjustor thunks that end the file (the host's compiler makes its own).
//   FS2 0x00184610  -0x40 to StageItem::~StageItem
//   FS2 0x00184620  -0x18 to StageItem::sideHook2
//   FS2 0x00184630  -0x40 to StageItemE::isSwitchedOn
//   FS2 0x00184640  -0x40 to StageItemE::setSwitchedOn
//   FS2 0x00184650  -0x40 to StageItemE::setDark
//   FS2 0x00184660  -0x40 to StageItemE::~StageItemE
//   FS2 0x00184670  -0x18 to StageWeaponItem::sideHook3
//   FS2 0x00184680  -0x18 to StageWeaponItem::sideHook2
//   FS2 0x00184690  -0x40 to StageWeaponItem::~StageWeaponItem
//   FS2 0x001846a0  -0x240 to StageWeaponItem::getCollisionObject
//   FS2 0x001846b0  -0x18 to StageItem::sideHook3
//   FS2 0x001846c0  -0x18 to StageTrapItem::sideHook2
//   FS2 0x001846d0  -0x40 to StageTrapItem::~StageTrapItem
//   FS2 0x001846e0  -0x240 to StageTrapItem::getCollisionObject
//   FS2 0x001846f0  -0x240 to StageTrapItem::collisionHook3
//   FS2 0x00184700  -0x240 to StageTrapItem::syncBody
//   FS2 0x00184710  -0x240 to StageTrapItem::onCollide
//   FS2 0x00184720  -0x240 to StageTrapItem::collisionHook0
//   FS2 0x00184730  -0x18 to StageThrownItem::sideHook3
//   FS2 0x00184740  -0x18 to StageThrownItem::sideHook2
//   FS2 0x00184750  -0x18 to StageItem::sideHook1
//   FS2 0x00184760  -0x18 to StageItem::sideHook0
//   FS2 0x00184770  -0x40 to StageItem::canPick
//   FS2 0x00184780  -0x40 to StageThrownItem::~StageThrownItem
//   FS2 0x00184790  -0x240 to StageThrownItem::getCollisionObject
//   FS2 0x001847a0  -0x40 to StageBulletBase::~StageBulletBase
//   FS2 0x001847b0  -0x18 to StageBullet::sideHook3
//   FS2 0x001847c0  -0x18 to StageBullet::sideHook2
//   FS2 0x001847d0  -0x18 to StageBullet::sideHook1
//   FS2 0x001847e0  -0x18 to StageBullet::sideHook0
//   FS2 0x001847f0  -0x40 to StageBullet::~StageBullet
//   FS2 0x00184800  -0x240 to StageBullet::getCollisionParent
//   FS2 0x00184810  -0x240 to StageBullet::getCollisionObject
//   FS2 0x00184820  -0x240 to StageBullet::collisionHook3
//   FS2 0x00184830  -0x240 to StageBullet::syncBody
//   FS2 0x00184840  -0x240 to StageBullet::onCollide
//   FS2 0x00184850  -0x240 to StageBullet::collisionHook0

// FS2 0x00180018 is no function: the two nops after StageBullet::onCollide.

// Inline in the ROM (uigame.cpp reads it through the player's +0x234).
bool StageItemE::isSpotOn() const
{
    return m_lamp->m_spot->m_switchedOn != 0;
}
