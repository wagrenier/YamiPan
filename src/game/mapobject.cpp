/* ==========================================================================
 *  game/mapobject.cpp
 *
 *  What a map model becomes: applyBgTarget makes an instance of each node and
 *  hands it here. The file name is the port's. Written so far, for the first
 *  3D picture: the plain object every node gets by default, and the part of
 *  the set-up that puts the model on the stage.
 *
 *  Not written yet: createMapObject's special classes (the doors _dp / _ds
 *  0x174940, 0x28e5a0, 0x295360, 0x2a7340, 0x2b2a60: they come out as plain
 *  objects, so they stand still and open nothing) and setupMapObject's
 *  collision half (0x23adb0, 0x234440, 0x176bc0, CollisionBody), so nothing
 *  on a map collides. Its string "Root" names the map's one big model.
 * ======================================================================== */
#include <string.h>

#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/stagescene.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"

namespace {

constexpr char kRootModelName[] = "Root";   // data 0x3030c0
constexpr float kFrameSeconds = 0.033333335f;   // 1/30 s: a motion frame

// Bits setupMapObject sets on every map model.
constexpr char kGateModelA[] = "oGK99_01";   // data 0x3030d8
constexpr char kGateModelB[] = "oDC99_01";   // data 0x3030e8
constexpr u_short kMapModelFlags = 0x91;    // ModelFlag names none of these bits yet (unread)
constexpr u_short kMapModelLitBit = 0x80;   // off on a model with its own lights

// The plain map object (vtable 0x3115c0, side table 0x3115e4): the ROM's ctor
// also makes a table of collision slots, which waits for setupMapObject's
// collision half. The door class 0x174940 shares its setters and sideHook0.
// layout: host -- EE 0x240.
class StageMapObject : public StageObject
{
public:
    // Its placement is the node's matrix, which setModel writes straight into
    // the world matrix. Left dirty, the first calcmatrix would rebuild that
    // from position and rotation, both zero, and put the object at the origin.
    // FS2 0x001765e0
    // callcheck: StageObject, operator_new[], setDirtyTree (the base, the collision table, slot +0x1c)
    StageMapObject() { setDirtyTree(false); }

    int getKind() override { return StageObject_MapObject; }

    void setPosition(const float *pos) override;                         // FS2 0x00174aa0
    void setRotation(const float *rot) override;                         // FS2 0x00174a40
    void setYaw(float yaw) override;                                     // FS2 0x00174a10

    void sideHook0() override;                                           // FS2 0x001756b0
    void sideHook1() override {}                                         // FS2 0x00177260
    void sideHook2() override;                                           // FS2 0x001771b0
    void sideHook3() override {}                                         // FS2 0x001771a0

private:
    void markMoved();

    // Who touched it this frame: only setupMapObject's collision half (not
    // written, file header) sets it.
    StageObject *m_touchedBy = nullptr; // FS2 +0x210
    // Its transform is dirty for this frame only (markMoved).
    u_char m_movedThisFrame = 0;        // FS2 +0x21c
};

// A map object rests with a clean transform, so a move has to ask for its one
// rebuild; sideHook0 takes it back on the next frame. Inline in the ROM.
void StageMapObject::markMoved()
{
    if (!m_xform.dirty) {
        setDirty(true);
        m_movedThisFrame = 1;
    }
}

// FS2 0x00174aa0
// callcheck: setPosition

void StageMapObject::setPosition(const float *pos)
{
    m_xform.setPosition(pos);
    markMoved();
}

// FS2 0x00174a40
// callcheck: setRotationVec

void StageMapObject::setRotation(const float *rot)
{
    m_xform.setRotationVec(rot);
    markMoved();
}

// FS2 0x00174a10

void StageMapObject::setYaw(float yaw)
{
    sceVu0FVECTOR rotation = { m_xform.rotation[0], m_xform.rotation[1], yaw, m_xform.rotation[3] };
    setRotation(rotation);
}

// FS2 0x001756b0

void StageMapObject::sideHook0()
{
    if (m_movedThisFrame) {
        setDirty(false);
        m_movedThisFrame = 0;
    }
}

// FS2 0x001771b0
// callcheck: getDeltaSeconds, advance

void StageMapObject::sideHook2()
{
    m_model->advance(gStageScene->clock->getDeltaSeconds() / kFrameSeconds,
                     m_xform.position, m_xform.rotation, 0);
    for (BoneMap::Node *node = m_model->boneMap.first; node != nullptr; node = node->next)
        node->value->updateMatrix();
    m_touchedBy = nullptr;
}

} // namespace

// The object for a map model's node, by its name. Every name gets the plain
// object for now (file header).
// FS2 0x001766c0
// callcheck: strstr, strstr, strcmp, strcmp, heapAllocAligned, FUN_0028e5a0, strcmp, heapAllocAligned, FUN_00295360, strcmp, strcmp, heapAllocAligned, FUN_002a7340, operator_new, FUN_002a89b0, strncmp, heapAllocAligned, FUN_002b2a60, operator_new, FUN_002b2c80, heapAllocAligned, FUN_001765e0, sscanf, heapAllocAligned, FUN_00174940, sscanf, heapAllocAligned, FUN_00174940, heapAllocAligned, FUN_001765e0 (the special classes: file header)

StageObject *createMapObject(const char *name, void *collision)
{
    (void)name;
    (void)collision;
    return new StageMapObject;
}

// A new map object takes its node: the scene's lists, instance, textures.
// FS2 0x001760c0
// callcheck: setName, add1, add, findEntry, setEntry, setModel, strcmp, collisionFindRecord, operator_new, FUN_00234440, operator_new, MapObjectParts, operator_new, CollisionBody, add, refresh (the collision half: file header)

void StageObject::setupMapObject(const char *name, ModelInstance *instance,
                                 StringMap<Tim2Texture *> *textures, void *collision)
{
    (void)collision;
    setName(name);
    gStageScene->objects->add1(this);
    gStageScene->pairs->add(this, ScenePairList_Objects);
    scriptRunner()->setEntry(gStageScene->scripts[0]->findEntry(m_name));
    setModel(instance, textures);
    m_model->_180 = strcmp(name, kRootModelName) != 0;
    applyMapModelFlags();
}

// FS2 0x001769e0
// callcheck: strcmp, strcmp

bool isGateModelName(const char *name)
{
    return strcmp(name, kGateModelA) == 0 || strcmp(name, kGateModelB) == 0;
}

// PC: the data test: a sub-scene's node may have no model data. The ROM has none.
// FS2 0x001763e0
// callcheck: getKind, getKind

void StageObject::applyMapModelFlags()
{
    if (m_model->data == nullptr)
        return;
    m_model->data->flags |= kMapModelFlags;
    int kind = getKind();
    if (kind == StageObject_Kind3 || kind == StageObject_SubSceneGate) {
        m_model->data->flags &= ~kMapModelLitBit;
        m_model->m_ownAmbient = 1;
        m_model->m_lit = 1;
    }
}
