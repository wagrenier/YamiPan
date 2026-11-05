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
 *  objects, so they stand still and open nothing). The string "Root" names
 *  the map's one big model.
 * ======================================================================== */
#include <stdio.h>
#include <string.h>

#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/stageitem.h"
#include "game/stagescene.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"

namespace {

constexpr char kRootModelName[] = "Root";   // data 0x3030c0
constexpr float kFrameSeconds = 0.033333335f;   // 1/30 s: a motion frame
constexpr int kMapCellSize = 0x96;          // StageCollision's grid on a map
constexpr short kRootPartLimit = 100;       // the Root's groups from here on are the doors'
constexpr int kPartBitWords = 8;
constexpr char kPartName[] = "BKW%02d";     // data 0x303150
constexpr char kGlassEffect[] = "glass%d_0"; // data 0x303168

// Bits setupMapObject sets on every map model.
constexpr char kGateModelA[] = "oGK99_01";   // data 0x3030d8
constexpr char kGateModelB[] = "oDC99_01";   // data 0x3030e8
constexpr u_short kMapModelFlags = 0x91;    // ModelFlag names none of these bits yet (unread)
constexpr u_short kMapModelLitBit = 0x80;   // off on a model with its own lights

// The plain map object (vtable 0x3115c0, side table 0x3115e4, the
// CollisionOwner's 0x31180c at +0x200, whose flag is m_collides). The door
// class 0x174940 shares its setters and sideHook0.
// layout: host -- EE 0x240.
class StageMapObject : public StageObject, public CollisionOwner
{
public:
    // Its placement is the node's matrix, which setModel writes straight into
    // the world matrix. Left dirty, the first calcmatrix would rebuild that
    // from position and rotation, both zero, and put the object at the origin.
    // The ROM takes the number of meshes; every caller read here has one.
    // FS2 0x001765e0
    // callcheck: StageObject, operator_new[], setDirtyTree (the base, the collision table, slot +0x1c)
    StageMapObject()
    {
        m_collides = 1;
        m_collisionCount = 1;
        m_collisions = new StageCollision *[m_collisionCount];
        for (u_int i = 0; i < m_collisionCount; i++)
            m_collisions[i] = nullptr;
        setDirtyTree(false);
    }
    ~StageMapObject() override;                                          // FS2 0x00176480

    int getKind() override { return StageObject_MapObject; }

    void setPosition(const float *pos) override;                         // FS2 0x00174aa0
    void setRotation(const float *rot) override;                         // FS2 0x00174a40
    void setYaw(float yaw) override;                                     // FS2 0x00174a10

    void sideHook0() override;                                           // FS2 0x001756b0
    void sideHook1() override {}                                         // FS2 0x00177260
    void sideHook2() override;                                           // FS2 0x001771b0
    void sideHook3() override {}                                         // FS2 0x001771a0

    int  collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                        float depth) override;                           // FS2 0x001753f0
    // FS2 0x001753e0
    int  onCollide(CollisionContact *, const float *, StageCollisionFace *, float, float) override { return 0; }
    // FS2 0x001755e0
    void syncBody(CollisionBody *) override {}
    // FS2 0x001755d0
    void collisionHook3(CollisionBody *) override {}
    StageObject *getCollisionObject() override { return this; }          // FS2 0x00172360

private:
    void markMoved();

    // Who touched it last: a character, or a bullet's shooter.
    StageObject *m_touchedBy = nullptr; // FS2 +0x210

public:
    CollisionRecord *m_record = nullptr; // FS2 +0x218: its faces (MapObjectParts)
    // Its transform is dirty for this frame only (markMoved).
    u_char m_movedThisFrame = 0;        // FS2 +0x21c
};

// FS2 0x00176480
// callcheck: ~StageCollision, operator_delete[], operator_delete[], __destroy_new_array, operator_delete[], operator_delete, ~StageObject, operator_delete (MapObjectParts' dtor inline)

StageMapObject::~StageMapObject()
{
    for (u_int i = 0; i < m_collisionCount; i++)
        delete m_collisions[i];
    delete[] m_collisions;
    delete m_mapParts;
}

// The player or whoever else touched it stays noted (the player over others),
// and a face in a group breaks that part: a door's own, every second id.
// FS2 0x001753f0
// callcheck: isPlayer, findDoorOfFace, breakPart, breakPart, getKind, isPlayer, findDoorOfFace, breakPart, breakPart, getCollisionObject, getCollisionObject

int StageMapObject::collisionHook0(CollisionContact *contact, const float *, StageCollisionFace *face,
                                   float)
{
    CollisionBody *body = contact->body;
    StageObject *toucher;
    if (body->category == kCollisionMelee) {
        StageObject *object = body->owner->getCollisionObject();
        if (object->m_weapons[0] == nullptr)
            return 0;
        toucher = object;
    } else if (body->category == kCollisionBulletRay) {
        StageObject *object = body->owner->getCollisionObject();
        toucher = object->getKind() == StageObject_Bullet ? static_cast<StageBullet *>(object)->shooter()
                                                          : nullptr;
    } else {
        return 0;
    }
    if (toucher != nullptr || body->category == kCollisionMelee) {
        if (m_touchedBy == nullptr || toucher->isPlayer())
            m_touchedBy = toucher;
    }
    if (face->doorId > 0) {
        StageObject *door = gStageScene->findDoorOfFace(face);
        if (door == nullptr)
            breakPart(face->doorId - 1, true);
        else if (face->doorId % 2 == 1)
            door->breakPart(0, false);
    }
    return 0;
}

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
// callcheck: setName, add1, add, findEntry, setEntry, setModel, strcmp, collisionFindRecord, operator_new, StageCollision, operator_new, MapObjectParts, operator_new, CollisionBody, add, refresh

void StageObject::setupMapObject(const char *name, ModelInstance *instance,
                                 StringMap<Tim2Texture *> *textures, void *collision)
{
    setName(name);
    gStageScene->objects->add1(this);
    gStageScene->pairs->add(this, ScenePairList_Objects);
    scriptRunner()->setEntry(gStageScene->scripts[0]->findEntry(m_name));
    setModel(instance, textures);
    const bool isRoot = strcmp(name, kRootModelName) == 0;
    m_model->_180 = !isRoot;

    // PC: the door classes are not written, so every map object is this class
    // (createMapObject); the ROM reaches the base at +0x200.
    StageMapObject *self = static_cast<StageMapObject *>(this);
    CollisionRecord *record = collisionFindRecord(collision, name);
    self->m_record = record;
    if (record != nullptr) {
        m_collisions[0] = new StageCollision(record, ((CollisionFile *)collision)->surfaceNames,
                                             kMapCellSize);
        if (!isRoot) {
            // The map proper stays in world coordinates; what is placed follows
            // its node.
            StageCollision *mesh = collisionAt(0);
            mesh->m_matrix = getWorldMatrix();
            mesh->m_inverse = m_inverse;
        }
    }
    applyMapModelFlags();
    if (isRoot) {
        m_mapParts = new MapObjectParts(this, record, true);
        if (m_mapParts->switches != nullptr)
            m_lightOn = 1;
    }
    if (collisionAt(0) != nullptr) {
        CollisionBody *body = new CollisionBody(self, 0, kCollisionMesh, 0);
        gStageScene->records->add(body);
        body->shape = CollisionShape_Mesh;
        body->mesh = collisionAt(0);
        body->refresh();
    }

}

// The parts of a mesh are its face groups, 1 up (a group of 0 or less is
// none; the Root's own go up to 99, the rest are the doors'). Each part is a
// switch of model pieces BKW%02d, and with effects a "glass" particle.
// PC: the ROM's bit table takes the shift from 32-bit words, so ids 32 apart
// count once while the list below writes both; here the list stops at the
// count. The doors' effect (kinds 2, 0x21: a bone's name and the door's table)
// waits for those classes and stays null.
// FS2 0x00176bc0
// callcheck: strcmp, memset, operator_new[], memset, operator_new[], FUN_00104d90, sprintf, init, sprintf, init, operator_new[], FUN_00186f80, find, sprintf, sprintf, spawn, sprintf, spawn

MapObjectParts::MapObjectParts(StageObject *owner, CollisionRecord *record, bool withEffects)
{
    const bool isRoot = strcmp(owner->m_name, kRootModelName) == 0;
    auto isPart = [&](short id) { return id > 0 && (!isRoot || id < kRootPartLimit); };

    if (record != nullptr) {
        const StageCollisionFace *faces = record->faces;
        unsigned long long seen[kPartBitWords] = {};
        for (int i = 0; i < record->faceCount; i++) {
            short id = faces[i].doorId;
            if (!isPart(id))
                continue;
            unsigned long long bit = (unsigned long long)(long long)(1 << ((id - 1) & 0x1f));
            unsigned long long &word = seen[(id - 1) >> 6];
            if ((bit & word) == 0) {
                word |= bit;
                count++;
            }
        }
    }
    if (count > 0) {
        const StageCollisionFace *faces = record->faces;
        ids = new int[count];
        memset(ids, -1, count * sizeof(int));
        int filled = 0;
        for (int i = 0; i < record->faceCount && filled < count; i++) {
            short id = faces[i].doorId;
            if (!isPart(id))
                continue;
            int known = 0;
            while (known < filled && ids[known] != id - 1)
                known++;
            if (known == filled)
                ids[filled++] = id - 1;
        }

        switches = new PartSwitch[count];
        int kind = owner->getKind();
        const bool hasStates = kind == StageObject_Kind3 || kind == StageObject_SubSceneGate;
        for (int i = 0; i < count; i++) {
            char name[0x80];
            sprintf(name, kPartName, ids[i] + 1);
            int states = 1;
            if (hasStates)
                states = ids[i] != 0 ? 2 : 3;
            switches[i].init(owner->m_model->data, name, states, -1);
        }
    }
    if (count > 0 && withEffects) {
        effects = new StageParticle *[count];
        int kind = owner->getKind();
        for (int i = 0; i < count; i++) {
            effects[i] = nullptr;
            if (kind == StageObject_Door || kind == StageObject_SubSceneDoor)
                continue;
            char name[0x10];
            sprintf(name, kGlassEffect, ids[i] + 1);
            effects[i] = gStageScene->nodeTree->spawn(name, nullptr, nullptr, nullptr, 0, 0);
        }
    }
}

// FS2 0x00176b30
// callcheck: operator_delete[], __destroy_new_array, operator_delete[], operator_delete

MapObjectParts::~MapObjectParts()
{
    delete[] ids;
    delete[] switches;
    delete[] effects;
}

// PC: the null test; the ROM always has from here.
// FS2 0x00176a40
// callcheck: show, setEmitting

void MapObjectParts::copyShown(const MapObjectParts *from)
{
    if (from == nullptr)
        return;
    for (int i = 0; i < count; i++) {
        if (switches != nullptr && from->switches != nullptr)
            switches[i].show(from->switches[i].shown);
        if (effects != nullptr && from->effects != nullptr && effects[i] != nullptr
            && from->effects[i] != nullptr)
            effects[i]->setEmitting(from->effects[i]->m_emitting != 0);
    }
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
