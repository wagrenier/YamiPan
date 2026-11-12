/* ==========================================================================
 *  game/itemphysics.cpp
 *
 *  ItemPhysics, 0x23c010..0x23de20. The frame of a moving item: update (the
 *  object's side hook 2) moves the nodes by their velocities, holds the
 *  links and adds gravity; the collision world's step then calls syncBody
 *  and collisionHook0, which push nodes out of the map and characters;
 *  endFrame (side hook 3) reads the velocities back from how far the nodes
 *  really went and turns the owner's matrix to match.
 * ======================================================================== */
#include "game/itemphysics.h"
#include "game/gamerate.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/clock.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/stageitem.h"
#include "game/stagescene.h"
#include "game/vu0math.h"

namespace {

// What stops a node (StageCollisionFace::flags), and what never does.
constexpr u_int kFaceFloor      = 0x42400c;
constexpr u_int kFaceWall       = 0xe002003;
constexpr u_int kFaceWallWeapon = 0xea021f3;
constexpr u_int kFaceSkip       = 0x40000;

constexpr float kFloorProbe  = 500.0f;  // how far under the owner it looks
constexpr float kFloorMargin = 50.0f;   // a weapon's tests run this far over the floor

// A node that meets something comes off faster the faster it went.
constexpr float kBounceMax        = 15.0f;
constexpr float kBounceFloorScale = 50.0f; // speed squared over this, upward
constexpr float kBounceWallScale  = 3.0f;  // speed over this, outward

constexpr float kDampingAir   = 0.995f;
constexpr float kDampingHit   = 0.8f;
constexpr float kDampingTouch = 0.9f;

constexpr int   kLinkPasses    = 30;
constexpr float kLinkStiffness = 0.005f;
constexpr float kGravity       = 1.0f;   // units a frame, each frame

constexpr float kLandSpeed      = 2.0f;  // a landing this slow is silent
constexpr float kStillSpeed     = 0.001f;
constexpr float kStillSeconds   = 1.0f;

constexpr int kObjectVectorBytes = sizeof(sceVu0FVECTOR);

// a += b, all four lanes (vadd.xyzw).
inline void addTo(sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        a[lane] += b[lane];
}

inline void copyVector(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    memcpy(out, in, kObjectVectorBytes);
}

inline float nodeSpeed(const ItemNode &node)
{
    const float *v = node.velocity;
    return sqrtf(v[1] * v[1] + v[0] * v[0] + v[2] * v[2]);
}

// The cap as c.le.s tests it.
inline float capBounce(float bounce)
{
    return bounce <= kBounceMax ? bounce : kBounceMax;
}

inline float floorBounce(const ItemNode &node)
{
    float speed = nodeSpeed(node);
    return capBounce(speed * speed / kBounceFloorScale);
}

inline float wallBounce(const ItemNode &node)
{
    return capBounce(nodeSpeed(node) / kBounceWallScale);
}

// Which nodes a body's links join, by its node count (0x23d600).
// layout: host
struct LinkPair
{
    int a, b;
};
constexpr LinkPair kLinks2[] = { { 0, 1 } };
constexpr LinkPair kLinks3[] = { { 0, 1 }, { 1, 2 }, { 2, 0 } };
constexpr LinkPair kLinks4[] = { { 0, 1 }, { 2, 3 }, { 0, 2 }, { 1, 3 }, { 0, 3 }, { 2, 1 } };

} // namespace

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023db50

ItemPhysics::ItemPhysics(bool isWeapon, bool hasTouchBody)
    : m_isWeapon(isWeapon), m_hasTouchBody(hasTouchBody)
{
}

// ──────────────────────────────────────────────────────────────────────
// The scene's teardown takes the bodies (the ROM frees nothing here).
// FS2 0x0023db00
// callcheck: operator_delete (the deleting dtor's)

ItemPhysics::~ItemPhysics()
{
}

// ──────────────────────────────────────────────────────────────────────
// Three pushes, by the body. The floor's also lifts the node, the wall's
// adds to the depth; both move the one sphere with it. The sphere around all
// the nodes moves them all and leaves the faces alone.
// FS2 0x0023c020
// callcheck: scaleVec4, scaleVec4, FUN_001f4c30, scaleVec4, FUN_001f4c30

int ItemPhysics::collisionHook0(CollisionContact *contact, const float *normal,
                                StageCollisionFace *face, float depth)
{
    const u_int category = contact->self->category;
    if (category == kCollisionItemFloor || category == kCollisionItemWall) {
        ItemNode &node = m_nodes[contact->selfIndex];
        sceVu0FVECTOR offset;
        if (category == kCollisionItemFloor) {
            float bounce = floorBounce(node);
            scaleVec4(depth, offset, normal);
            offset[2] += bounce;
        }
        else {
            scaleVec4(depth + wallBounce(node), offset, normal);
        }
        addTo(node.position, offset);
        addTo(contact->self->spheres.at(contact->selfIndex)->value.center, offset);
        node.damping = kDampingHit;
        node.face = face;
    }
    else if (category == kCollisionItemTouch) {
        sceVu0FVECTOR offset;
        scaleVec4(depth, offset, normal);
        for (int i = 0; i < m_nodeCount; i++)
            addTo(m_nodes[i].position, offset);
        for (int i = 0; i < m_nodeCount; i++)
            m_nodes[i].damping = kDampingTouch;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2: the opening of 0x0023c300's wall branch and of 0x0023d0c0.
// callcheck: getWorldMatrix, getWorldMatrix, getWorldMatrix, raycast

float ItemPhysics::heightOverFloor()
{
    float height = m_owner->getWorldMatrix()[3][2];
    sceVu0FVECTOR from, to, hit;
    copyVector(from, m_owner->getWorldMatrix()[3]);
    copyVector(to, m_owner->getWorldMatrix()[3]);
    to[2] -= kFloorProbe;
    if (gStageScene->raycast(hit, from, to, 0, kFaceFloor, kFaceSkip, nullptr) != nullptr)
        height = hit[2] + kFloorMargin;
    return height;
}

// ──────────────────────────────────────────────────────────────────────
// The world's step asks first: each node's way this frame is a ray, and what
// it crossed sends the node back out by its radius, plus the bounce. A weapon
// is long and thin: its nodes are also tried level with height, and its
// spheres stay at that height, so it cannot lie through a low wall.
// FS2 0x0023c300
// callcheck: operator_new[], fromPoints, operator_delete[], raycast, raycast, raycast, vecDistance, vecDirection, scaleVec4, raycast, vecDistance, vecDirection, scaleVec4 (heightOverFloor's getWorldMatrix and raycast inline)

void ItemPhysics::syncBody(CollisionBody *body)
{
    if (body->category == kCollisionItemFloor) {
        StageList<BoundingSphere>::Node *sphere = body->spheres.head;
        for (int i = 0; i < m_nodeCount; i++, sphere = sphere->next) {
            ItemNode &node = m_nodes[i];
            sceVu0FVECTOR from, to, hit;
            copyVector(from, node.previous);
            copyVector(to, node.position);
            StageCollisionFace *face = gStageScene->raycast(hit, from, to, 0, kFaceFloor, kFaceSkip, nullptr);
            if (face != nullptr) {
                float back = vecDistance(hit, to) + sphere->value.radius;
                float bounce = floorBounce(node);
                sceVu0FVECTOR direction, offset;
                vecDirection(direction, from, to);
                scaleVec4(back, offset, direction);
                offset[2] += bounce;
                addTo(node.position, offset);
                node.damping = kDampingHit;
                node.face = face;
            }
            copyVector(sphere->value.center, node.position);
        }
    }
    else if (body->category == kCollisionItemWall) {
        float height = 0.0f;
        if (m_isWeapon)
            height = heightOverFloor();
        const u_int mask = m_isWeapon ? kFaceWallWeapon : kFaceWall;
        StageList<BoundingSphere>::Node *sphere = body->spheres.head;
        for (int i = 0; i < m_nodeCount; i++, sphere = sphere->next) {
            ItemNode &node = m_nodes[i];
            sceVu0FVECTOR from, to, hit;
            copyVector(from, node.previous);
            copyVector(to, node.position);
            StageCollisionFace *face = gStageScene->raycast(hit, from, to, 0, mask, kFaceSkip, nullptr);
            if (face == nullptr && m_isWeapon) {
                from[2] = height;
                to[2] = height;
                face = gStageScene->raycast(hit, from, to, 0, mask, kFaceSkip, nullptr);
            }
            if (face != nullptr) {
                float back = vecDistance(hit, to) + sphere->value.radius + wallBounce(node);
                sceVu0FVECTOR direction, offset;
                vecDirection(direction, from, to);
                scaleVec4(back, offset, direction);
                addTo(node.position, offset);
                node.damping = kDampingHit;
                node.face = face;
            }
            copyVector(sphere->value.center, node.position);
            if (m_isWeapon)
                sphere->value.center[2] = height;
        }
    }
    else if (body->category == kCollisionItemTouch) {
        sceVu0FVECTOR *points = new sceVu0FVECTOR[m_nodeCount];
        for (int i = 0; i < m_nodeCount; i++)
            copyVector(points[i], m_nodes[i].position);
        body->sphere.fromPoints(m_nodeCount, points);
        delete[] points;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Row 0 along nodes 0 - 1. Two nodes roll about it as they like, so up is
// the third axis; with more, nodes 1 - 2 give the second.
// FS2 0x0023c7f0
// callcheck: vecNormalize, vecCross, vecNormalize, vecCross, vecNormalize, vecCross, vecNormalize, vecCross, identityMatrix

void ItemPhysics::getNodeFrame(sceVu0FMATRIX out) const
{
    sceVu0FVECTOR axisX, axisY = {}, axisZ = {};
    for (int lane = 0; lane < 4; lane++)
        axisX[lane] = m_nodes[0].position[lane] - m_nodes[1].position[lane];
    vecNormalize(axisX, axisX);
    if (m_nodeCount == 2) {
        axisZ[2] = 1.0f;
        vecCross(axisY, axisZ, axisX);
        vecNormalize(axisY, axisY);
        vecCross(axisZ, axisX, axisY);
    }
    if (m_nodeCount == 3 || m_nodeCount == 4) {
        for (int lane = 0; lane < 4; lane++)
            axisY[lane] = m_nodes[1].position[lane] - m_nodes[2].position[lane];
        vecNormalize(axisY, axisY);
        vecCross(axisZ, axisX, axisY);
        vecNormalize(axisZ, axisZ);
        vecCross(axisY, axisZ, axisX);
    }
    identityMatrix(out);
    copyVector(out[0], axisX);
    copyVector(out[1], axisY);
    copyVector(out[2], axisZ);
}

// ──────────────────────────────────────────────────────────────────────
// How far the nodes really went is their velocity. The damping is x and y
// only: nothing here slows a fall (the ROM stores z back as it was).
// FS2 0x0023c910
// callcheck: keepsMoving, setMoving, getNodeFrame, multiplyMatrix, applyMatrix, getWorldMatrix, copyMatrix

void ItemPhysics::endFrame()
{
    if (m_owner == nullptr || !m_moving)
        return;
    // PC: game_rate (game/gamerate.h): a body's simulation (Verlet steps of a 25 Hz frame, its gravity, damping,
    // link stiffness and landing speed all in those steps) runs whole 25 Hz frames, as the ROM's.
    if (palTicks() == 0)
        return;
    for (int i = 0; i < m_nodeCount; i++) {
        ItemNode &node = m_nodes[i];
        for (int lane = 0; lane < 4; lane++)
            node.velocity[lane] = node.position[lane] - node.previous[lane];
        node.velocity[0] *= node.damping;
        node.velocity[1] *= node.damping;
        copyVector(node.previous, node.position);
    }
    if (!keepsMoving()) {
        setMoving(false, true);
        return;
    }
    // PC: the ROM reads through the null link of a body with under two nodes.
    if (m_links[0].a == nullptr)
        return;
    sceVu0FMATRIX frame;
    getNodeFrame(frame);
    multiplyMatrix(frame, frame, m_restFrame);
    // Row 3 is the point that puts bone 0 on node 0; its w stays the node's.
    sceVu0FVECTOR turned;
    applyMatrix(turned, frame, m_restPoints[0]);
    frame[3][0] = m_links[0].a[0] - turned[0];
    frame[3][1] = m_links[0].a[1] - turned[1];
    frame[3][2] = m_links[0].a[2] - turned[2];
    frame[3][3] = m_links[0].a[3];
    copyMatrix(const_cast<sceVu0FVECTOR *>(m_owner->getWorldMatrix()), frame);
}

// ──────────────────────────────────────────────────────────────────────
// A frame of a moving item. The links are soft: thirty small passes each
// frame. A node on a floor is pressed into it along the face's normal, which
// the ray of syncBody then catches, so it sits rather than sinks; in the air
// it falls.
// FS2 0x0023ca50
// callcheck: scaleVec4, scaleVec4, getNormal, scaleVec4, getNormal, vecDot, getKind, getLandSound, playNamedHeard2

void ItemPhysics::update()
{
    if (!m_moving)
        return;
    if (palTicks() == 0)   // PC: game_rate, as endFrame
        return;
    for (int i = 0; i < m_nodeCount; i++)
        addTo(m_nodes[i].position, m_nodes[i].velocity);

    for (int pass = 0; pass < kLinkPasses; pass++) {
        for (int i = 0; i < m_linkCount; i++) {
            ItemLink &link = m_links[i];
            sceVu0FVECTOR apart, offset;
            for (int lane = 0; lane < 4; lane++)
                apart[lane] = link.a[lane] - link.b[lane];
            float length = sqrtf(apart[1] * apart[1] + apart[0] * apart[0] + apart[2] * apart[2]);
            scaleVec4(kLinkStiffness * (link.length - length), offset, apart);
            addTo(link.a, offset);
            scaleVec4(kLinkStiffness * (length - link.length), offset, apart);
            addTo(link.b, offset);
        }
    }

    for (int i = 0; i < m_nodeCount; i++) {
        ItemNode &node = m_nodes[i];
        if (node.face == nullptr || (node.face->flags & kFaceFloor) == 0) {
            node.position[2] -= kGravity;
        }
        else {
            sceVu0FVECTOR normal, press;
            node.face->getNormal(normal);
            scaleVec4(-1.0f, press, normal);
            addTo(node.position, press);
        }
    }
    for (int i = 0; i < m_nodeCount; i++)
        m_nodes[i].damping = kDampingAir;

    // Only the first new contact counts, loud enough or not.
    for (int i = 0; i < m_nodeCount; i++) {
        ItemNode &node = m_nodes[i];
        if (node.lastFace != nullptr || node.face == nullptr)
            continue;
        sceVu0FVECTOR normal;
        node.face->getNormal(normal);
        if (vecDot(node.velocity, normal) > 0.0f && nodeSpeed(node) > kLandSpeed) {
            if (m_owner->getKind() == StageObject_ThrownItem) {
                StageThrownItem *item = static_cast<StageThrownItem *>(m_owner);
                item->playNamedHeard2(item->getLandSound(), node.position, nullptr);
            }
            break;
        }
    }
    for (int i = 0; i < m_nodeCount; i++) {
        m_nodes[i].lastFace = m_nodes[i].face;
        m_nodes[i].face = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// A character asks: what hit it, and which way.
// FS2 0x0023ce00
// callcheck: divideVec4

void ItemPhysics::getMeanVelocity(sceVu0FVECTOR out)
{
    memset(out, 0, kObjectVectorBytes);
    for (int i = 0; i < m_nodeCount; i++)
        addTo(out, m_nodes[i].velocity);
    divideVec4((float)m_nodeCount, out, out);
}

// ──────────────────────────────────────────────────────────────────────
// The ROM keeps the running total in an int: each step truncates, so nodes
// slower than a unit a frame add up to nothing and the item counts as still.
// FS2 0x0023ce60
// callcheck: getDeltaSeconds

bool ItemPhysics::keepsMoving()
{
    if (!m_moving)
        return false;
    int total = 0;
    for (int i = 0; i < m_nodeCount; i++)
        total = (int)((float)total + nodeSpeed(m_nodes[i]));
    if ((float)total > kStillSpeed) {
        m_stillSeconds = 0.0f;
        return true;
    }
    // PC: game_rate: called once a 25 Hz frame (endFrame), so it adds a 25 Hz frame's seconds; the ROM's dt at 25.
    m_stillSeconds += gStageScene->clock->getDeltaSeconds() / palScale();
    return m_stillSeconds < kStillSeconds;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023cf50

void ItemPhysics::push(const sceVu0FVECTOR offset, int node)
{
    if (node < 0) {
        for (int i = 0; i < m_nodeCount; i++)
            addTo(m_nodes[i].position, offset);
    }
    else {
        addTo(m_nodes[node].position, offset);
    }
}

// ──────────────────────────────────────────────────────────────────────
// While it moves the matrix is this class's, so the owner's transform stops
// rebuilding it (setDirty false); at rest the transform takes over from what
// the matrix came to.
// FS2 0x0023cfc0
// callcheck: getWorldMatrix, matrixToRotYXZ, getWorldMatrix, setDirty, placeNodes, setDirty

void ItemPhysics::setMoving(bool moving, bool settled)
{
    if (m_owner == nullptr)
        return;
    m_moving = moving;
    for (int i = 0; i < ItemBody_Count; i++) {
        if (m_bodies[i] != nullptr)
            m_bodies[i]->on = moving;
    }
    if (m_moving) {
        placeNodes();
        m_owner->setDirty(false);
    }
    else if (settled) {
        matrixToRotYXZ(m_owner->m_xform.rotation, m_owner->getWorldMatrix());
        copyVector(m_owner->m_xform.position, m_owner->getWorldMatrix()[3]);
        m_owner->setDirty(true);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The same rays as syncBody with the owner's point for the start: the wall
// body's list, then the floor's. Nothing goes off with a bounce.
// FS2 0x0023d0c0
// callcheck: applyMatrix, getWorldMatrix (the count's nine, as a loop), raycast, raycast, raycast, vecDistance, vecDirection, scaleVec4, raycast, vecDistance, vecDirection, scaleVec4 (heightOverFloor's getWorldMatrix and raycast inline)

void ItemPhysics::placeNodes()
{
    if (m_owner == nullptr)
        return;
    if (m_nodeCount >= 2 && m_nodeCount <= kItemNodes) {
        for (int i = 0; i < m_nodeCount; i++)
            applyMatrix(m_nodes[i].position, m_owner->getWorldMatrix(), m_restPoints[i]);
    }
    float height = 0.0f;
    if (m_isWeapon)
        height = heightOverFloor();

    const u_int mask = m_isWeapon ? kFaceWallWeapon : kFaceWall;
    StageList<BoundingSphere>::Node *sphere = m_bodies[ItemBody_Wall]->spheres.head;
    for (int i = 0; i < m_nodeCount; i++, sphere = sphere->next) {
        ItemNode &node = m_nodes[i];
        sceVu0FVECTOR from, to, hit;
        copyVector(from, m_owner->getWorldMatrix()[3]);
        copyVector(to, node.position);
        StageCollisionFace *face = gStageScene->raycast(hit, from, to, 0, mask, kFaceSkip, nullptr);
        if (face == nullptr && m_isWeapon) {
            from[2] = height;
            to[2] = height;
            face = gStageScene->raycast(hit, from, to, 0, mask, kFaceSkip, nullptr);
        }
        if (face != nullptr) {
            float back = vecDistance(hit, to) + sphere->value.radius;
            sceVu0FVECTOR direction, offset;
            vecDirection(direction, from, to);
            scaleVec4(back, offset, direction);
            addTo(node.position, offset);
        }
    }

    sphere = m_bodies[ItemBody_Floor]->spheres.head;
    for (int i = 0; i < m_nodeCount; i++, sphere = sphere->next) {
        ItemNode &node = m_nodes[i];
        sceVu0FVECTOR from, to, hit;
        copyVector(from, m_owner->getWorldMatrix()[3]);
        copyVector(to, node.position);
        if (gStageScene->raycast(hit, from, to, 0, kFaceFloor, kFaceSkip, nullptr) != nullptr) {
            float back = vecDistance(hit, to) + sphere->value.radius;
            sceVu0FVECTOR direction, offset;
            vecDirection(direction, from, to);
            scaleVec4(back, offset, direction);
            addTo(node.position, offset);
        }
    }

    for (int i = 0; i < m_nodeCount; i++) {
        ItemNode &node = m_nodes[i];
        copyVector(node.previous, node.position);
        memset(node.velocity, 0, sizeof(node.velocity));
    }
    for (int i = 0; i < m_nodeCount; i++) {
        m_nodes[i].face = nullptr;
        m_nodes[i].lastFace = nullptr;
    }
    m_stillSeconds = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// bn_col0_.. name the nodes; what follows the name is the radius, in tenths.
// The bones' y and z swap into the game's axes, as the pick point's do.
// FS2 0x0023d600
// callcheck: sprintf, motionBoneSuffix, sprintf, motionBoneMatrix, vecDistance, getNodeFrame, invertRigidMatrix, operator_new, CollisionBody, add, operator_new, CollisionBody, operator_new, CollisionBody, add, sprintf, BoundingSphere, motionBoneSuffix, FUN_001f4d30, FUN_001f4d30, add, setMoving

void ItemPhysics::setup(StageItem *owner, void *motion)
{
    static const char kNodeBone[] = "bn_col%d_";

    m_owner = owner;
    m_nodeCount = 0;
    for (;;) {
        char bone[128];
        sprintf(bone, kNodeBone, m_nodeCount);
        if (motionBoneSuffix(motion, bone) < 0)
            break;
        m_nodeCount++;
    }

    const LinkPair *pairs = nullptr;
    if (m_nodeCount == 2) {
        m_linkCount = 1;
        pairs = kLinks2;
    }
    if (m_nodeCount == 3) {
        m_linkCount = 3;
        pairs = kLinks3;
    }
    if (m_nodeCount == 4) {
        m_linkCount = kItemLinks;
        pairs = kLinks4;
    }

    for (int i = 0; i < m_nodeCount; i++) {
        char bone[128];
        sceVu0FMATRIX matrix;
        sprintf(bone, kNodeBone, i);
        motionBoneMatrix(matrix, motion, bone, 1);
        m_restPoints[i][0] = matrix[3][0];
        m_restPoints[i][1] = -matrix[3][2];
        m_restPoints[i][2] = matrix[3][1];
        m_restPoints[i][3] = matrix[3][3];
        copyVector(m_nodes[i].position, m_restPoints[i]);
    }

    if (pairs != nullptr) {
        for (int i = 0; i < m_linkCount; i++) {
            ItemLink &link = m_links[i];
            link.a = m_nodes[pairs[i].a].position;
            link.b = m_nodes[pairs[i].b].position;
            link.length = vecDistance(link.a, link.b);
        }
    }

    getNodeFrame(m_restFrame);
    invertRigidMatrix(m_restFrame, m_restFrame);

    if (m_hasTouchBody) {
        CollisionBody *touch = new CollisionBody(this, 2, kCollisionItemTouch, 2);
        m_bodies[ItemBody_Touch] = touch;
        gStageScene->records->add(touch);
        touch->shape = CollisionShape_Sphere;
    }
    CollisionBody *wall = new CollisionBody(this, 2, kCollisionItemWall, kCollisionMesh);
    m_bodies[ItemBody_Wall] = wall;
    gStageScene->records->add(wall);
    wall->shape = CollisionShape_Spheres;
    wall->rayMask = m_isWeapon ? kFaceWallWeapon : kFaceWall;
    wall->rayFlags = kFaceSkip;

    CollisionBody *floor = new CollisionBody(this, 2, kCollisionItemFloor, kCollisionMesh);
    m_bodies[ItemBody_Floor] = floor;
    gStageScene->records->add(floor);
    floor->shape = CollisionShape_Spheres;
    floor->rayMask = kFaceFloor;
    floor->rayFlags = kFaceSkip;

    for (int i = 0; i < m_nodeCount; i++) {
        char bone[128];
        sprintf(bone, kNodeBone, i);
        BoundingSphere sphere;
        sphere.radius = (float)motionBoneSuffix(motion, bone) / 10.0f;
        sphere.radiusSq = sphere.radius * sphere.radius;
        wall->spheres.pushBack(sphere);
        floor->spheres.pushBack(sphere);
    }
    setMoving(false, false);
}
