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

#include "game/gamehooks.h"
#include "game/stagescene.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"

namespace {

constexpr char kRootModelName[] = "Root";   // data 0x3030c0

// Bits setupMapObject sets on every map model.
constexpr u_short kMapModelFlags = 0x91;    // ModelFlag names none of these bits yet (unread)

// The plain map object (vtable 0x3115c0): the ROM's ctor also makes a table
// of collision slots, which waits for setupMapObject's collision half.
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
};

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
// callcheck: setName, add1, add, findEntry, setEntry, setModel, strcmp, FUN_0023adb0, operator_new, FUN_00234440, operator_new, FUN_00176bc0, operator_new, CollisionBody, add, refresh (the collision half: file header)

void StageObject::setupMapObject(const char *name, ModelInstance *instance,
                                 StringMap<Tim2Texture *> *textures, void *collision)
{
    (void)collision;
    setName(name);
    gStageScene->objects->add1(this);
    gStageScene->pairs->add(this, 1);
    scriptRunner()->setEntry(gStageScene->scripts[0]->findEntry(m_name));
    setModel(instance, textures);
    m_model->_180 = strcmp(name, kRootModelName) != 0;
    if (m_model->data != nullptr)
        m_model->data->flags |= kMapModelFlags;
}
