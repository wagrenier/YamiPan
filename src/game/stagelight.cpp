/* ==========================================================================
 *  game/stagelight.cpp
 *
 *  StageLight, SpotLight and PointLight, 0x184860..0x186ee0 (game/stagelight.h).
 *  The file name is the port's (no source string). The class's accessors sit
 *  first in the ROM, the ctors and dtors after them.
 * ======================================================================== */
#include "game/stagelight.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/clock.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"
#include "game/vu0math.h"

// Degrees to radians (cosf's argument).
static const float kDegToRad = 0.017453292f;
// The segment a spot tests against the map: StageSound's values.
constexpr u_int kLightRayMask  = 0x0e42600f;
constexpr u_int kLightRayFlags = 0x00040800;
// A stage light's body: the sphere around it.
static const float kLightBodyRadius = 20.0f;
// The effects of the named lights: at the light, turned to its axis
// (rodata 0x2f7cb0..0x2f7cf0, two equal pairs).
static const sceVu0FVECTOR kEffectPos = {0.0f, 0.0f, 0.0f, 1.0f};
static const sceVu0FVECTOR kEffectRot = {0.0f, 1.5707964f, 1.5707964f, 1.0f};

// PC: the ROM's libc strcasecmp (0x127ea0); MSVC has no such name.
static int lightStrCaseCmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = (unsigned char)*a;
        int cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
        if (ca != cb)
            return ca - cb;
        if (ca == 0)
            return 0;
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00184860
// callcheck: ~StageLight, operator_delete (the base dtor and delete of the deleting form)

SpotLight::~SpotLight()
{
}

// ──────────────────────────────────────────────────────────────────────────
// A key on the colour track.
// FS2 0x00184b40
// callcheck: operator_new, VectorKey, addKey

void StageLight::cmdAddColorKey(int frames, const float *color)
{
    m_track->addKey(new VectorKey(color, 0), frames, true);
}

// ──────────────────────────────────────────────────────────────────────────
// With m_limitByMap the light ends where its axis meets the map; the effect
// is told how far.
// FS2 0x00184bd0
// callcheck: applyMatrix, raycast, vecDistanceSq, setRangeSquared, getWorldMatrix (virtual)

void SpotLight::sideHook3()
{
    if (m_limitByMap && isLit()) {
        const sceVu0FVECTOR tip = {0.0f, m_radius, 0.0f, 1.0f};
        sceVu0FVECTOR end;
        sceVu0FVECTOR hit;
        applyMatrix(end, getWorldMatrix(), tip);
        if (gStageScene->raycast(hit, getWorldMatrix()[3], end, 0, kLightRayMask, kLightRayFlags,
                                 nullptr) == nullptr) {
            m_reachSq = m_radiusSq;
            m_reach = m_radius;
        } else {
            float distanceSq = vecDistanceSq(getWorldMatrix()[3], hit);
            m_reachSq = distanceSq;
            m_reach = sqrtf(distanceSq);
        }
        if (m_effect != nullptr)
            m_effect->setRangeSquared(m_reachSq);
        return;
    }
    m_reachSq = m_radiusSq;
    m_reach = m_radius;
}

// ──────────────────────────────────────────────────────────────────────────
float StageLight::offsetTo(sceVu0FVECTOR out, const sceVu0FVECTOR point)
{
    const float *position = getMatrix()[3];
    out[0] = point[0] - position[0];
    out[1] = point[1] - position[1];
    out[2] = point[2] - position[2];
    out[3] = point[3] - position[3];
    return vecDot(out, out);
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00184d50
// callcheck: vecDot, vecNormalize, vecDot (offsetTo has the first; getMatrix is virtual)

float SpotLight::shadowAt(const sceVu0FVECTOR point)
{
    if (!m_castsShadow || !isLit())
        return 0.0f;
    sceVu0FVECTOR toPoint;
    float distanceSq = offsetTo(toPoint, point);
    float limit = m_reachSq;
    if (m_mode == StageLight_Hard && limit > kHardLightRangeSq)
        limit = kHardLightRangeSq;
    if (distanceSq < limit) {
        vecNormalize(toPoint, toPoint);
        float cosine = vecDot(toPoint, getMatrix()[1]);
        float edge = m_coneCos;
        if (edge <= cosine) {
            if (m_mode == StageLight_Hard)
                return 1.0f;
            return ((cosine - edge) / (1.0f - edge)) * ((m_radiusSq - distanceSq) / m_radiusSq);
        }
    }
    return 0.0f;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00184f00
// callcheck: vecDot, vecNormalize, vecDot (offsetTo has the first; getMatrix is virtual)

bool SpotLight::inReach(const sceVu0FVECTOR point)
{
    if (!isLit())
        return false;
    sceVu0FVECTOR toPoint;
    float distanceSq = offsetTo(toPoint, point);
    if (distanceSq < m_reachSq) {
        vecNormalize(toPoint, toPoint);
        if (m_coneCos <= vecDot(toPoint, getMatrix()[1]))
            return true;
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185010
// callcheck: vecDot, vecNormalize, vecDot (offsetTo has the first; getWeight is inline in the ROM)

float SpotLight::lightAt(const sceVu0FVECTOR point)
{
    if (!isLit())
        return 0.0f;
    sceVu0FVECTOR toPoint;
    float distanceSq = offsetTo(toPoint, point);
    if (distanceSq < m_reachSq) {
        vecNormalize(toPoint, toPoint);
        float cosine = vecDot(toPoint, getMatrix()[1]);
        float edge = m_coneCos;
        if (edge <= cosine) {
            return getWeight() * ((cosine - edge) / (1.0f - edge))
                 * ((m_radiusSq - distanceSq) / m_radiusSq);
        }
    }
    return 0.0f;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185210
// callcheck: cosf

void SpotLight::setConeAngle(int degrees)
{
    m_coneDegrees = degrees;
    m_coneCos = cosf((float)degrees * kDegToRad);
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185260
// callcheck: StageLight (the base's ctor)

SpotLight::SpotLight()
{
    m_coneDegrees = 0;
    m_coneCos = 0.0f;
    m_limitByMap = 0;
    m_reach = 0.0f;
    m_reachSq = 0.0f;
    m_texture = nullptr;
    m_intensityPercent = 100;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x001852e0
// callcheck: vecDot (offsetTo has it; getMatrix is virtual)

float PointLight::shadowAt(const sceVu0FVECTOR point)
{
    if (!m_castsShadow || !isLit())
        return 0.0f;
    sceVu0FVECTOR toPoint;
    float distanceSq = offsetTo(toPoint, point);
    float limit = m_radiusSq;
    if (m_mode == StageLight_Hard && limit > kHardLightRangeSq)
        limit = kHardLightRangeSq;
    if (limit <= distanceSq)
        return 0.0f;
    if (m_mode == StageLight_Hard)
        return 1.0f;
    return 1.0f - sqrtf(distanceSq) / m_radius;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185430
// callcheck: vecDot (offsetTo has it; getMatrix is virtual)

bool PointLight::inReach(const sceVu0FVECTOR point)
{
    if (!isLit())
        return false;
    sceVu0FVECTOR toPoint;
    return offsetTo(toPoint, point) < m_radiusSq;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185510
// callcheck: vecDot (offsetTo has it; getWeight is inline in the ROM)

float PointLight::lightAt(const sceVu0FVECTOR point)
{
    if (!isLit())
        return 0.0f;
    sceVu0FVECTOR toPoint;
    float distanceSq = offsetTo(toPoint, point);
    if (distanceSq < m_radiusSq)
        return getWeight() * ((m_radiusSq - distanceSq) / m_radiusSq);
    return 0.0f;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x001856b0
// callcheck: StageLight (the base's ctor)

PointLight::PointLight()
{
}

// ──────────────────────────────────────────────────────────────────────────
// A bullet that ended on the body breaks the light.
// FS2 0x00185710
// callcheck: spawn, setScriptFlag2, getCollisionObject, getKind (virtual)

int StageLight::onCollide(CollisionContact *contact, const float *, StageCollisionFace *, float, float)
{
    StageObject *other = contact->body->owner->getCollisionObject();
    if (other->getKind() == StageObject_Bullet && other->m_dead) {
        gStageScene->nodeTree->spawn("lightbreak", nullptr, m_body->sphere.center, nullptr, 0, 0);
        m_tree->setScriptFlag2(true);
        m_enabled = 0;
        m_lightOn = 0;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x001857e0
// callcheck: getWorldMatrix (virtual)

void StageLight::syncBody(CollisionBody *body)
{
    if (body->category == kCollisionLight)
        memcpy(body->sphere.center, getWorldMatrix()[3], sizeof(body->sphere.center));
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185830
// callcheck: step, setEmitting, setEmitting, show, show, getDeltaSeconds

void StageLight::sideHook2()
{
    if (m_track != nullptr && m_track->count != 0) {
        m_track->step();
        m_color[0] = m_track->value[0];
        m_color[1] = m_track->value[1];
        m_color[2] = m_track->value[2];
        m_color[3] = 1.0f;
    }

    if (!m_switchedOn)
        m_fade = (m_fade != -1 && m_fade > 0) ? m_fade - 1 : 0;
    else
        m_fade = (m_fade != -1 && m_fade < m_fadeFrames) ? m_fade + 1 : m_fadeFrames;

    if (m_effect != nullptr)
        m_effect->setEmitting(isLit());
    if (m_parts != nullptr)
        m_parts->show(m_broken ? -1 : m_switchedOn);
    if (m_offParts != nullptr)
        m_offParts->show(m_broken ? 0 : -(int)m_switchedOn);

    if (m_keepFlags) {
        m_lifetime -= gStageScene->clock->getDeltaSeconds();
        if (m_lifetime < 0.0f)
            m_dead = 1;
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185a50

void StageLight::setLifetime(float seconds)
{
    m_keepFlags = 1;
    m_lifetime = seconds;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185a60
// callcheck: contains

bool StageLight::inBoxes(const sceVu0FVECTOR point)
{
    if (m_boxes.count == 0)
        return true;
    for (StageList<AreaBox *>::Node *link = m_boxes.head; link != nullptr; link = link->next) {
        if (link->value->contains(point))
            return true;
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185ae0

const float *StageLight::getBodyPosition()
{
    return m_body->sphere.center;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185af0

float StageLight::getWeight() const
{
    if (!isLit())
        return 0.0f;
    float weight;
    if (m_track == nullptr || m_track->count == 0)
        weight = (float)m_fade / (float)m_fadeFrames;
    else
        weight = m_track->value[3];
    return weight * m_brightness;
}

// ──────────────────────────────────────────────────────────────────────────
void StageLight::updateBounds()
{
    BoundingSphere sphere;
    memcpy(sphere.center, getWorldMatrix()[3], sizeof(sphere.center));
    sphere.radius = m_radius;
    sphere.radiusSq = m_radius * m_radius;
    sphere.toBox(m_bounds);
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185bb0, 0x00184aa0
// callcheck: BoundingSphere, toBox, getWorldMatrix (updateBounds, inline in the ROM)

void StageLight::setRadiusSquared(float radiusSquared)
{
    m_radiusSq = radiusSquared;
    m_radius = sqrtf(radiusSquared);
    if (m_envLight || m_yamLight)
        updateBounds();
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185c40

void StageLight::setVector(float x, float y, float z)
{
    m_color[0] = x;
    m_color[1] = y;
    m_color[2] = z;
    m_color[3] = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185c60
// callcheck: strstr, strstr, spawn, spawnNamed, spawn, strcasecmp, erase, erase, pushBack, getKind (virtual; setParent is the erases and the push, inline in the ROM)

void StageLight::attachEffect()
{
    StageNodeTree *nodeTree = gStageScene->nodeTree;
    if (getKind() == StageObject_SpotLight) {
        if (strstr(m_name, "TORCH") != nullptr)
            m_effect = nodeTree->spawn("dento1", this, kEffectPos, kEffectRot, 0, 0);
        else if (strstr(m_name, "SigLamp") != nullptr)
            m_effect = nodeTree->spawn("siglight", this, kEffectPos, kEffectRot, 0, 0);
    }
    if (m_effect == nullptr)
        m_effect = nodeTree->spawnNamed(m_name, 0, 0);
    if (m_effect == nullptr && gStageScene->lpt != nullptr) {
        const LptFile *lpt = gStageScene->lpt;
        const LptRecord *records = (const LptRecord *)lpt->data.get();
        for (int i = 0; i < lpt->_04; i++) {
            if (lightStrCaseCmp(records[i].name, m_name) == 0) {
                m_effect = nodeTree->spawn(records[i].effect, nullptr, records[i].pos,
                                           records[i].rot, 0, 0);
                break;
            }
        }
    }
    if (m_effect != nullptr)
        m_effect->m_tree->setParent(m_tree);
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185fb0

void StageLight::setEnabled(bool on)
{
    m_enabled = on;
    m_lightOn = on;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00185fd0

bool StageLight::isLit() const
{
    if (m_broken || m_brightness == 0.0f)
        return false;
    return m_switchedOn || (u_int)(m_fade + 1) >= 2;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00186030
// callcheck: sprintf, strlen, modelFindPart, operator_new, PartSwitch, init, sprintf, strlen, modelFindPart, sprintf, operator_new, PartSwitch, init

void StageLight::bindParts(ModelInstance *model)
{
    // PC: the ROM's two buffers are 0x10 bytes for an m_name of up to 0xf.
    char name[0x20];
    snprintf(name, sizeof(name), "bn_%s_0", m_name);
    if (modelFindPart(model, name, strlen(name), 0) != nullptr) {
        m_parts = new PartSwitch;
        m_parts->init(model, m_name, 2, -1);
    }
    snprintf(name, sizeof(name), "bn_%s_f_0", m_name);
    if (modelFindPart(model, name, strlen(name), 0) != nullptr) {
        snprintf(name, sizeof(name), "%s_f", m_name);
        m_offParts = new PartSwitch;
        m_offParts->init(model, name, 1, -1);
    }
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00186160
// callcheck: setScriptFlag, setScriptFlag2

void StageLight::copyShownFrom(StageLight *other)
{
    m_tree->setScriptFlag(other->m_switchedOn);
    m_tree->setScriptFlag2(other->m_broken);
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x001861b0
// callcheck: setName, add5, add, findEntry, setEntry, attachEffect, operator_new, VectorTrack, BoundingSphere, toBox, refresh, updateMatrix, setDirtyTree, getWorldMatrix (virtual slots; updateBounds, inline in the ROM)

void StageLight::addToScene(const char *name, bool withEffect)
{
    setName(name);
    gStageScene->objects->add5(this);
    gStageScene->pairs->add(this, ScenePairList_Lights);
    setEntry(gStageScene->scripts[0]->findEntry(m_name));
    if (withEffect)
        attachEffect();

    if (m_envLight || m_mode == StageLight_Keyed) {
        m_track = new VectorTrack;
        m_track->linear = 1;
    }

    if (m_envLight || m_yamLight) {
        updateMatrix();
        setDirtyTree(false);
        updateBounds();
        if (m_body != nullptr) {
            const float *position = m_effect != nullptr ? m_effect->getWorldMatrix()[3]
                                                        : getWorldMatrix()[3];
            memcpy(m_body->sphere.center, position, sizeof(m_body->sphere.center));
            m_body->refresh();
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
// The ROM frees a body and takes the light out of the scene only for a
// temporary light at its end; the map's go with the scene's teardown.
// FS2 0x00186380
// callcheck: ~PartSwitch, ~PartSwitch, remove, FUN_00184260, FUN_00184430, operator_delete, remove, remove5, remove, ~VectorTrack, FUN_00186b20, ~StageObject, operator_delete (the body's members are its own stand-in's; 0x186b20 is m_boxes' dtor)

StageLight::~StageLight()
{
    delete m_parts;
    delete m_offParts;
    for (StageList<AreaBox *>::Node *link = m_boxes.head; link != nullptr; link = link->next)
        delete link->value;
    if (m_keepFlags && m_dead) {
        if (m_body != nullptr) {
            gStageScene->records->remove(m_body);
            delete m_body;
        }
        gStageScene->pairs->remove(this);
        gStageScene->objects->remove5(this);
        gStageScene->nodeTree->remove(m_effect);
    }
    delete m_track;
}

// ──────────────────────────────────────────────────────────────────────────
// The base CollisionOwner's flag starts at 1 and the ctor clears it (the
// ROM's setEnabled inline); so does a YAM_ test on a m_yamLight just cleared.
// FS2 0x001866f0
// callcheck: StageObject, FUN_00186b10, operator_new, CollisionBody, add (0x186b10 is m_boxes' ctor)

StageLight::StageLight()
{
    m_envLight = 0;
    m_yamLight = 0;
    m_mode = StageLight_Soft;
    m_effect = nullptr;
    m_parts = nullptr;
    m_offParts = nullptr;
    m_switchedOn = 1;
    memset(m_color, 0, sizeof(m_color));
    m_radiusSq = 0.0f;
    m_radius = 0.0f;
    m_fadeFrames = kLightFadeFrames;
    m_fade = -1;
    m_modelFade = 0;
    m_modelFadeFrames = 30;
    m_lightsModels = 1;
    m_modelColor[0] = 42.0f / 255.0f;
    m_modelColor[1] = 46.0f / 255.0f;
    m_modelColor[2] = 54.0f / 255.0f;
    m_modelColor[3] = 0.0f;
    m_modelAmbient[0] = 40.0f / 255.0f;
    m_modelAmbient[1] = 44.0f / 255.0f;
    m_modelAmbient[2] = 46.0f / 255.0f;
    m_modelAmbient[3] = 0.0f;
    m_usable = 1;
    m_broken = 0;
    _273 = 0;
    setEnabled(false);
    m_castsShadow = 1;
    m_keepFlags = 0;
    m_lifetime = 0.0f;
    m_brightness = 1.0f;
    m_body = nullptr;
    if (!gStageScene->isSubScene) {
        m_body = new CollisionBody(this, 2, kCollisionLight, 0);
        gStageScene->records->add(m_body);
        m_body->shape = CollisionShape_Sphere;
        m_body->sphere.radius = kLightBodyRadius;
        m_body->sphere.radiusSq = kLightBodyRadius * kLightBodyRadius;
    }
    m_track = nullptr;
    m_priority = -1;
    m_shadowOwner = nullptr;
}

// ──────────────────────────────────────────────────────────────────────────
// FS2 0x00186a40
// callcheck: ~StageLight, operator_delete (the base dtor and delete of the deleting form)

PointLight::~PointLight()
{
}

// The rest of the file, nothing to write on the host:
//   0x186550, the deleting dtor of vtable 0x314180 over StageNode's (no ctor
//     stores it in a read run);
//   0x1868d0, 0x186920: the deleting dtors of StageNodeBase's two roots
//     (vtables 0x314120, 0x314150), folded into StageNodeBase (stagenode.h);
//   0x186b10..0x186c50: StageList's instances (stagelist.h).
// The adjustor thunks, the host's multiple inheritance:
//   FS2 0x00186cc0  -0x40 to StageLight::~StageLight
//   FS2 0x00186cd0  -0x18 to PointLight::sideHook3
//   FS2 0x00186ce0  -0x18 to PointLight::sideHook1
//   FS2 0x00186cf0  -0x18 to PointLight::sideHook0
//   FS2 0x00186d00  -0x40 to PointLight::~PointLight
//   FS2 0x00186d10  -0x18 to SpotLight::sideHook3
//   FS2 0x00186d20  -0x18 to StageLight::sideHook2
//   FS2 0x00186d30  -0x18 to SpotLight::sideHook1
//   FS2 0x00186d40  -0x18 to SpotLight::sideHook0
//   FS2 0x00186d50  -0x40 to StageLight::cmdIsLit
//   FS2 0x00186d60  -0x40 to StageLight::cmdSetEnabled
//   FS2 0x00186d70  -0x40 to StageLight::cmdSetCastsShadow
//   FS2 0x00186d80  -0x40 to StageLight::cmdSetColor260
//   FS2 0x00186d90  -0x40 to StageLight::cmdSetColor250
//   FS2 0x00186da0  -0x40 to StageLight::cmdSetFlag248
//   FS2 0x00186db0  -0x40 to StageLight::cmdSetValue244
//   FS2 0x00186dc0  -0x40 to StageLight::cmdSetFlag240
//   FS2 0x00186dd0  -0x40 to StageLight::cmdSetFadeFrames
//   FS2 0x00186de0  -0x40 to StageLight::cmdSetTreeBroken
//   FS2 0x00186df0  -0x40 to StageLight::setScriptFlag2
//   FS2 0x00186e00  -0x40 to StageLight::cmdSetTreeSwitchedOn
//   FS2 0x00186e10  -0x40 to StageLight::setScriptFlag
//   FS2 0x00186e20  -0x40 to SpotLight::cmdSetValue2d8
//   FS2 0x00186e30  -0x40 to SpotLight::cmdSetConeAngle
//   FS2 0x00186e40  -0x40 to StageLight::cmdSetRadiusSquared
//   FS2 0x00186e50  -0x40 to StageLight::cmdSetColorLoop
//   FS2 0x00186e60  -0x40 to StageLight::cmdAddColorKey
//   FS2 0x00186e70  -0x40 to StageLight::cmdSetColor
//   FS2 0x00186e80  -0x40 to SpotLight::~SpotLight
//   FS2 0x00186e90  -0x200 to StageLight::getCollisionObject
//   FS2 0x00186ea0  -0x200 to StageLight::collisionHook3
//   FS2 0x00186eb0  -0x200 to StageLight::syncBody
//   FS2 0x00186ec0  -0x200 to StageLight::onCollide
//   FS2 0x00186ed0  -0x200 to StageLight::collisionHook0
