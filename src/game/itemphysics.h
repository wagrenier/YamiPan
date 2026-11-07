/* ==========================================================================
 *  game/itemphysics.h
 *
 *  How a thrown item or a dropped weapon falls, 0x23c010..0x23de20: two to
 *  four points (the motion's bn_col bones) joined by stiff links, integrated
 *  by Verlet. update moves them and pulls the links true, the collision
 *  world pushes them out of what they meet (syncBody, collisionHook0),
 *  endFrame turns what is left into velocities and the owner's matrix. The
 *  class, the file and every name here are the port's: no source string,
 *  no symbols. architecture.md "The items' physics".
 * ======================================================================== */
#ifndef _GAME_ITEMPHYSICS_H
#define _GAME_ITEMPHYSICS_H

#include "game/gamehooks.h"
#include "game/stagecollision.h"

class StageItem;

constexpr int kItemNodes = 4;
constexpr int kItemLinks = 6;   // every pair of four nodes

// One point of the body, EE 0x40.
// layout: host -- EE offsets in the comments, from the node's start.
struct ItemNode
{
    sceVu0FVECTOR position = {}; // FS2 +0x00
    sceVu0FVECTOR previous = {}; // FS2 +0x10: last frame's
    sceVu0FVECTOR velocity = {}; // FS2 +0x20: the frame's, damped
    float damping = 1.0f;       // FS2 +0x30: x and y only, what touched it last
    StageCollisionFace *face = nullptr;     // FS2 +0x34: met this frame
    StageCollisionFace *lastFace = nullptr; // FS2 +0x38: met the frame before
};

// Two nodes held a length apart, EE 0xc.
// layout: host
struct ItemLink
{
    float *a = nullptr;         // FS2 +0x0: one node's position
    float *b = nullptr;         // FS2 +0x4
    float length = 0.0f;        // FS2 +0x8
};

// ItemPhysics::m_bodies, in the ROM's order.
enum ItemBody
{
    ItemBody_Touch,             // the sphere around the nodes; thrown items only
    ItemBody_Wall,
    ItemBody_Floor,
    ItemBody_Count,
};

// A CollisionOwner at +0x240 in the thrown items and the weapons (vtable
// 0x316510); what the map stops, a person it hits.
// layout: host -- EE: the owner's fields at +0x240, 0x210 bytes; offsets here
// from the owner's start, add 0x240 for the item's.
class ItemPhysics : public CollisionOwner
{
public:
    // isWeapon: its wall tests start from the floor under it (see syncBody).
    // hasTouchBody: it has the sphere that characters meet.
    ItemPhysics(bool isWeapon, bool hasTouchBody);                       // FS2 0x0023db50
    ~ItemPhysics();                                                      // FS2 0x0023db00

    // The bodies and the nodes from the bones of motion, which is owner's. At rest.
    void setup(StageItem *owner, void *motion);                          // FS2 0x0023d600
    // Before the collision world's step: moves, links, gravity, the landing sound.
    void update();                                                       // FS2 0x0023ca50
    // After it: the velocities, the owner's matrix, or the rest.
    void endFrame();                                                     // FS2 0x0023c910
    // The mean of the nodes' velocities.
    void getMeanVelocity(sceVu0FVECTOR out);                             // FS2 0x0023ce00
    // Falls again (moving: the nodes start from the owner's matrix) or comes
    // to rest; settled: at rest, the owner keeps the matrix it fell to.
    void setMoving(bool moving, bool settled);                           // FS2 0x0023cfc0
    // Adds to a node's position, every node's with -1: the next endFrame reads
    // it as velocity.
    void push(const sceVu0FVECTOR offset, int node);                     // FS2 0x0023cf50

    int  collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                        float depth) override;                           // FS2 0x0023c020
    // FS2 0x0023c010
    int  onCollide(CollisionContact *, const float *, StageCollisionFace *, float, float) override { return 0; }
    void syncBody(CollisionBody *body) override;                         // FS2 0x0023c300
    // FS2 0x0023c2f0
    void collisionHook3(CollisionBody *) override {}
    // FS2 0x001840b0
    StageObject *getCollisionParent() override { return m_parent; }

    u_char       m_moving = 0;  // FS2 +0x1fc (the owner's +0x43c): still falling or thrown
    StageObject *m_parent = nullptr; // FS2 +0x200 (the owner's +0x440): who threw it

private:
    // The turn the nodes show: x along the first two, the rest as the count allows.
    void getNodeFrame(sceVu0FMATRIX out) const;                          // FS2 0x0023c7f0
    // Not yet at rest: fast enough, or slow for under a second.
    bool keepsMoving();                                                  // FS2 0x0023ce60
    // The nodes where the owner's matrix has them, pulled back from what lies
    // between them and the owner, still.
    void placeNodes();                                                   // FS2 0x0023d0c0
    // The floor under the owner plus a margin, or its own height (inline in the ROM).
    float heightOverFloor();

    ItemNode m_nodes[kItemNodes]; // FS2 +0x10
    ItemLink m_links[kItemLinks]; // FS2 +0x110
    int   m_nodeCount = 0;      // FS2 +0x158
    int   m_linkCount = 0;      // FS2 +0x15c
    StageItem *m_owner = nullptr; // FS2 +0x160
    sceVu0FVECTOR m_restPoints[kItemNodes] = {}; // FS2 +0x170: the bones, in the owner's space
    sceVu0FMATRIX m_restFrame = {}; // FS2 +0x1b0: getNodeFrame at the bones, inverted
    CollisionBody *m_bodies[ItemBody_Count] = {}; // FS2 +0x1f0: ItemBody
    u_char m_isWeapon;          // FS2 +0x204
    u_char m_hasTouchBody;      // FS2 +0x205
    float  m_stillSeconds = 0.0f; // FS2 +0x208
};

#endif /* _GAME_ITEMPHYSICS_H */
