/* ==========================================================================
 *  game/mapobject.cpp
 *
 *  What a map model becomes: applyBgTarget makes an instance of each node and
 *  hands it here. The file name is the port's. Written so far, for the first
 *  3D picture: the plain object every node gets by default, and the part of
 *  the set-up that puts the model on the stage.
 *
 *  The doors _dp / _ds are game/stagedoor.cpp. Not written yet: the rest of
 *  createMapObject's special classes (0x28e5a0, 0x295360, 0x2a7340, 0x2b2a60:
 *  they come out as plain objects). The string "Root" names the map's one big
 *  model.
 * ======================================================================== */
#include <stdio.h>
#include <string.h>

#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/mapobject.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/stagedoor.h"
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

constexpr char kMapModelPrefix = 'o';        // a placed model; the rest are never doors
constexpr char kDoorPartR[] = "_dp";         // data 0x3030c8
constexpr char kDoorPartT[] = "_ds";         // data 0x3030d0
constexpr char kDoorScanR[] = "_dp_%02d";    // data 0x303130
constexpr char kDoorScanT[] = "_ds_%02d";    // data 0x303140
constexpr char kTopBone[]   = "bn_top";      // data 0x3030b8

// Bits setupMapObject sets on every map model.
constexpr char kGateModelA[] = "oGK99_01";   // data 0x3030d8
constexpr char kGateModelB[] = "oDC99_01";   // data 0x3030e8
constexpr u_short kMapModelFlags = ModelFlag_TakesSpot | ModelFlag_TakesPoint | ModelFlag_TakesShadow;
constexpr u_short kMapModelLitBit = ModelFlag_TakesPoint;   // off on a model with its own lights

} // namespace

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

// The object for a map model's node, by its name: _dp_nn and _ds_nn are doors,
// and every other name gets the plain object for now (file header).
// FS2 0x001766c0
// callcheck: strstr, strstr, strcmp, strcmp, heapAllocAligned, FUN_0028e5a0, strcmp, heapAllocAligned, FUN_00295360, strcmp, strcmp, heapAllocAligned, FUN_002a7340, operator_new, FUN_002a89b0, strncmp, heapAllocAligned, FUN_002b2a60, operator_new, FUN_002b2c80, heapAllocAligned, FUN_001765e0, sscanf, heapAllocAligned, FUN_00174940, sscanf, heapAllocAligned, FUN_00174940, heapAllocAligned, FUN_001765e0 (the special classes: file header)

StageObject *createMapObject(const char *name, void *collision)
{
    (void)collision;
    int number = 0;
    if (name[0] == kMapModelPrefix) {
        const char *door = strstr(name, kDoorPartR);
        if (door != nullptr)
            return sscanf(door, kDoorScanR, &number) > 0 ? new StageDoor(StageDoorType_R, number) : nullptr;
        door = strstr(name, kDoorPartT);
        if (door != nullptr)
            return sscanf(door, kDoorScanT, &number) > 0 ? new StageDoor(StageDoorType_T, number) : nullptr;
    }
    return new StageMapObject;
}

// The base has no motions of its own.
// FS2 vtable +0x270

void StageObject::setMotionList(void *, int, int)
{
}

// The motion kinds of setBaseMotion by object kind: 6 -> 4, 5 -> 3, 4 -> 2,
// 3 -> 1, the rest 0. The objects of those kinds keep their own placement.
static int baseMotionKind(int kind)
{
    return kind >= 3 && kind <= 6 ? kind - 2 : 0;
}

// FS2 0x00175d40
// callcheck: matrixToRotYXZ, setBaseMotion, setBaseMotion, setBaseMotion, setBaseMotion, setBaseMotion, addMotion, operator_new, MapObjectParts, find, matrixToRotYXZ, translate, updateMatrix, find, setInverseTree, getWorldMatrix

void StageMapObject::setMotionList(void *list, int arg, int arg2)
{
    if (arg != 0) {
        const sceVu0FVECTOR *world = getWorldMatrix();
        matrixToRotYXZ(m_xform.rotation, (const sceVu0FVECTOR *)world);
        memcpy(m_xform.position, world[3], sizeof(sceVu0FVECTOR));
    }

    StageList<void *>::Node *node = static_cast<StageList<void *> *>(list)->head;
    if (node == nullptr)
        return;     // PC: the ROM reads through the null node
    const int kind = getKind();
    const int baseKind = baseMotionKind(kind);
    m_model->setBaseMotion(node->value, baseKind);
    for (node = node->next; node != nullptr; node = node->next)
        m_model->addMotion(node->value, 0, nullptr);

    m_mapParts = new MapObjectParts(this, m_record, arg2 != 0);
    if (m_mapParts->switches != nullptr && baseKind != 1)
        m_lightOn = 1;

    if (arg != 0) {
        BoneMap::Node *topNode = m_model->boneMap.find(kTopBone);
        if (baseKind == 0 && topNode != nullptr) {     // PC: a model with no bn_top keeps its placement
            StageNodeBase *top = topNode->value;
            const sceVu0FVECTOR *bone = top->getWorldMatrix();
            sceVu0FVECTOR rotation;
            matrixToRotYXZ(rotation, bone);
            for (int i = 0; i < 4; i++) {
                m_xform.rotation[i] -= rotation[i];
                m_xform.position[i] -= bone[3][i];
            }
        }
        m_model->pose->translate(m_xform.position);
        for (BoneMap::Node *bone = m_model->boneMap.first; bone != nullptr; bone = bone->next)
            bone->value->updateMatrix();
    }

    StageCollision *mesh = collisionAt(0);
    BoneMap::Node *topNode = m_model->boneMap.find(kTopBone);
    if (mesh != nullptr && topNode != nullptr) {
        // The faces follow the top bone, which needs its inverse for that.
        StageNodeBase *top = topNode->value;
        top->setInverseTree(true);
        mesh->m_matrix = top->getWorldMatrix();
        mesh->m_inverse = top->m_inverse;
    }
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
