/*
 *  game/mapobject.h -- the object a map model's node becomes (FS2 vtable
 *  0x3115c0); the doors of game/stagedoor.h stand on it.
 */
#ifndef GAME_MAPOBJECT_H
#define GAME_MAPOBJECT_H

#include "game/gamehooks.h"
#include "game/stagescene.h"

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

    // Takes the motions of list: the first is the base, its kind by the
    // object's; arg: the placement moves from the node to bn_top's; arg2: the
    // parts with their effects.
    void setMotionList(void *list, int arg, int arg2) override;          // FS2 0x00175d40

private:
    void markMoved();

protected:
    // Who touched it last: a character, or a bullet's shooter.
    StageObject *m_touchedBy = nullptr; // FS2 +0x210

public:
    CollisionRecord *m_record = nullptr; // FS2 +0x218: its faces (MapObjectParts)
    // Its transform is dirty for this frame only (markMoved).
    u_char m_movedThisFrame = 0;        // FS2 +0x21c
};

#endif // GAME_MAPOBJECT_H
