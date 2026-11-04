/* ==========================================================================
 *  game/subsceneobject.h
 *
 *  The objects of a sub-scene (an event such as d0101): the stage's map, its
 *  characters and its items again, as light copies that only play the
 *  event's motions. StageFiles builds them; the script's native 0x145 gives
 *  each its motion and turns it on (architecture.md "Modules").
 *
 *  Family (vtables at their ctors, operator new aligned 0x40):
 *    SubSceneModelObject  0x316c60, EE 0x240, the base; alone: an actor with
 *                         no model of its own (role 8, native 0x145)
 *    SubSceneMapObject    0x319c90, EE 0x280   SubSceneGateObject 0x319a20, EE 0x2c0
 *    SubSceneDoorObject   0x314ef0, EE 0x2c0
 *    SubSceneCharacter    0x3197b0, EE 0x280   SubSceneItem 0x319540, EE 0x280
 *    SubSceneProp         0x315420, EE 0x240
 *  The ROM's slots +0x250..+0x25c are the side base's four hooks seen from
 *  the first table; only the third does anything.
 * ======================================================================== */
#ifndef _GAME_SUBSCENEOBJECT_H
#define _GAME_SUBSCENEOBJECT_H

#include "game/gamehooks.h"

class ItemLamp;                 // game/stageitem.h
class PointLight;               // game/stagelight.h
class SpotLight;                // game/stagelight.h

// What a sub-scene's object stands for (+0x208, the ctor's argument). Only
// map objects start active and shown.
enum SubSceneRole
{
    SubSceneRole_Map       = 1,
    SubSceneRole_Character = 2,
    SubSceneRole_Item      = 3,
    SubSceneRole_Actor     = 8, // named by the script, with no model of its own
};

// Four lights on a sub-scene object's bn_top (vtable 0x316c30, EE 0x34): the
// shape of ItemLamp's, built on demand (SubSceneModelObject::setLampOn).
// layout: host -- EE offsets in the comments.
class SubSceneLamp : public StageMatrixNode
{
public:
    SubSceneLamp(const sceVu0FVECTOR *matrix, StageObject *owner);       // inline in 0x268830
    ~SubSceneLamp() override = default;                                  // FS2 0x002687c0
    // Inline in setLampOn: the first spot and the first point.
    void setOn(bool on);
    // Inline in the frame.
    void updateLights();

    SpotLight   *m_spot = nullptr;      // FS2 +0x1c
    SpotLight   *m_wideSpot = nullptr;  // FS2 +0x20
    PointLight  *m_point = nullptr;     // FS2 +0x24
    PointLight  *m_point2 = nullptr;    // FS2 +0x28
    StageObject *m_owner;               // FS2 +0x2c
    bool         m_on = false;          // FS2 +0x30

private:
    // Two spots and two points, hung under this node.
    void build();                                                        // FS2 0x00268310
};

// layout: host
class SubSceneModelObject : public StageObject
{
public:
    explicit SubSceneModelObject(int role);                              // FS2 0x00269d00
    ~SubSceneModelObject() override;                                     // FS2 0x00269c30

    // FS2 0x00269150
    int getKind() override { return StageObject_SubSceneModel; }
    // The frame (ScenePairTable). The ROM has it twice: 0x2699f0 in the
    // first table's slot +0x258, 0x1a6830 the copy in the side base's.
    void sideHook2() override;                                           // FS2 0x002699f0
    // The small set of properties the event's objects answer.
    int  getProperty(int id, int sub) override;                          // FS2 0x00269160
    void setProperty(int id, int sub, int value) override;               // FS2 0x00268ae0
    // The transform; a character's motion takes the place too.
    void setPosition(const float *pos) override;                         // FS2 0x00268a90

    // The object joins the scene: its name, the object list's group 8, its
    // model.
    void attach(const char *name, ModelInstance *instance,
                StringMap<Tim2Texture *> *textures);                     // FS2 0x00269bd0
    // Slot +0x24c: the event's motion. A character takes the base motion of
    // its stage self; the rest the pack's file called name.
    virtual void bindMotion(const char *name);                           // FS2 0x00269a90
    // Slot +0x260 (and 0x1a6860, the runner's thunk): plays the pack's
    // motion called name.
    virtual void playNamedMotion(const char *name);                      // FS2 0x00268a20
    // FS2 0x001a6860
    // callcheck: playNamedMotion
    void scriptPlayMotion(const char *name) override { playNamedMotion(name); }
    // Slot +0x264: bn_top's lights.
    virtual void setLampOn(bool on);                                     // FS2 0x00268830

    SubSceneLamp  *m_lamp = nullptr;    // FS2 +0x200
    StageNodeBase *m_extraNode = nullptr; // FS2 +0x204: the dtor deletes it; no writer found
    int            m_role;              // FS2 +0x208: SubSceneRole
};

// The map's objects (the bg target).
// layout: host -- EE 0x280.
class SubSceneMapObject : public SubSceneModelObject
{
public:
    explicit SubSceneMapObject(int role);                                // FS2 0x002bc040
    ~SubSceneMapObject() override;                                       // FS2 0x002bbfb0

    // FS2 0x002bbc00
    int getKind() override { return StageObject_SubSceneMap; }
    void bindMotion(const char *name) override;                          // FS2 0x002bbe30
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures, void *collision);     // FS2 0x002bbed0

    MapObjectParts *m_parts = nullptr;  // FS2 +0x240
    void *m_collisionRecord = nullptr;  // FS2 +0x244
};

// The gates oGK99_01 and oDC99_01.
// layout: host -- EE 0x2c0.
class SubSceneGateObject : public SubSceneMapObject
{
public:
    explicit SubSceneGateObject(int role);                               // FS2 0x002bbdb0
    ~SubSceneGateObject() override;                                      // FS2 0x002bbcf0

    // FS2 0x002bc0a0
    int getKind() override { return StageObject_SubSceneGate; }
    void bindMotion(const char *name) override;                          // FS2 0x002bbb10
    // The gate's effects.
    void setLampOn(bool on) override;                                    // FS2 0x002bbb00
    // A copy of SubSceneMapObject::setup in the ROM.
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures, void *collision);     // FS2 0x002bbc10

    GateParts *m_gate = nullptr;        // FS2 +0x280
};

// A _dp / _ds node (a door's parts). Its constructor is inline in the ROM:
// the base's, then the vtables 0x314ef0 / 0x314f14 / 0x314f2c.
// layout: host -- EE 0x2c0.
class SubSceneDoorObject : public SubSceneMapObject
{
public:
    explicit SubSceneDoorObject(int role) : SubSceneMapObject(role), m_recordValue(0) {}
    // FS2 0x001a45d0
    int getKind() override { return StageObject_SubSceneDoor; }

    int m_recordValue;          // FS2 +0x280: DoorRecord::value
};

// layout: host -- EE 0x280.
class SubSceneCharacter : public SubSceneModelObject
{
public:
    explicit SubSceneCharacter(int role);                                // FS2 0x002bbab0: always 2
    ~SubSceneCharacter() override;                                       // FS2 0x002bba20

    // FS2 0x002bc090
    int getKind() override { return StageObject_SubSceneCharacter; }
    void bindMotion(const char *name) override;                          // FS2 0x002bb840
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures);                      // FS2 0x002bb870

    CharacterHidePart *m_hidePart = nullptr; // FS2 +0x240
};

// ItemKind_E, the lamp.
// layout: host -- EE 0x280.
class SubSceneItem : public SubSceneModelObject
{
public:
    explicit SubSceneItem(int role);                                     // FS2 0x002bb7a0: always 3
    ~SubSceneItem() override;                                            // FS2 0x002bb6e0

    // FS2 0x002bb600
    int getKind() override { return StageObject_SubSceneItem; }
    void bindMotion(const char *name) override;                          // FS2 0x002bb630
    void setLampOn(bool on) override;                                    // FS2 0x002bb620
    // Runner slot +0x60 (0x2bc0e0 the thunk).
    void setScriptFlag(bool on) override;                                // FS2 0x002bb610
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures);                      // FS2 0x002bb680

    ItemLamp *m_itemLamp = nullptr;     // FS2 +0x240
};

// Any other item; vtables 0x315420 / 0x315444 / 0x31545c, the ctor inline in
// the ROM over 0x269d00.
// layout: host -- EE 0x240.
class SubSceneProp : public SubSceneModelObject
{
public:
    explicit SubSceneProp(int role) : SubSceneModelObject(role) {}
    // FS2 0x001a4950
    int getKind() override { return StageObject_SubSceneProp; }
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures);                      // FS2 0x002bb810
};

#endif /* _GAME_SUBSCENEOBJECT_H */
