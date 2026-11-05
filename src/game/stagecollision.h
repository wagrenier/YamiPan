/* ==========================================================================
 *  game/stagecollision.h
 *
 *  The collision of a stage, two halves the ROM keeps apart (0x1f4bb0..
 *  0x1f6ac0 and 0x231d40..0x236b90, with the file's readers at 0x23adb0):
 *  - the map: StageCollision, one mesh of a collision file's faces, in a 2D
 *    grid of cells (x, y), each cell a tree of height slices (z up);
 *  - the world: SceneRecordBuckets, the shapes (CollisionBody) that lights,
 *    bullets, traps, cameras and characters put in it, paired by category
 *    and mask when they join, tested each frame between the action phases.
 *  The file name and every class name here are the port's: no string or
 *  RTTI names them.
 * ======================================================================== */
#ifndef _GAME_STAGECOLLISION_H
#define _GAME_STAGECOLLISION_H

#include <stddef.h>

#include <vector>

#include "game/s2ddata.h"       /* S2dRef: a pointer the file keeps in 4 bytes */
#include "game/stagelist.h"
#include "game/vu0math.h"
#include "sdk/libvu0.h"
#include "sdk/scetypes.h"

class StageCollision;
class StageObject;

// ──────────────────────────────────────────────────────────────────────
// The file

enum { kCollisionMagic = 0x1a535353 };  // "SSS\x1a"
constexpr int kCollisionNameSize = 0x10;
constexpr int kCollisionSurfaceNames = 0x20;

// What a face's flags say.
enum StageCollisionFlag
{
    StageCollision_Thin = 0x400,    // sound passes once (StageSound: twice the distance)
    StageCollision_Slope = 0x4008,  // either bit: the main camera tilts with the floor
    StageCollision_Door = 0x4000000, // doorId is a door's (StageScene::isDoorFace)
    StageCollision_CameraPass = 0x8000000, // the main camera may stand behind it
};
// Which way a segment may cross a face (segmentHit's side).
enum FaceSide
{
    FaceSide_Any   = 0,
    FaceSide_Front = 1,         // from the normal's side
    FaceSide_Back  = 2,
};

// One triangle. The plane comes from the vertices the first time a mesh takes
// the record (StageCollision's ctor), as do the bytes from +0x4c on.
// layout: file (EE 0x50) -- faces stay in the file, loaded as they are.
struct StageCollisionFace
{
    // The plane's normal in the world (a moved mesh's turned by its matrix).
    void getNormal(sceVu0FVECTOR out) const;                             // FS2 0x00236030
    // point is on the normal's side of the plane, or on it.
    bool inFront(const sceVu0FVECTOR point) const;                       // FS2 0x00235b00
    // box[0] the low corner of the vertices, box[1] the high one.
    void getBox(sceVu0FVECTOR *box) const;                               // FS2 0x00235f90
    // The segment crosses the face on the given side (FaceSide): where in
    // hit. matrix, when not null, says where the face stands.
    bool segmentHit(sceVu0FVECTOR hit, const sceVu0FVECTOR from, const sceVu0FVECTOR to,
                    const sceVu0FVECTOR *matrix, int side) const;        // FS2 0x002358e0
    // The plane's height over x, y, when that point is in the triangle; kept
    // under the highest vertex.
    bool heightAt(float x, float y, float *height) const;                // FS2 0x00235b50
    // The triangle touches the rectangle on the x, y plane.
    bool overlapsRect(float x0, float y0, float x1, float y1) const;     // FS2 0x00235d20
    // The sphere touches the face.
    bool sphereTouches(float radius, const sceVu0FVECTOR center,
                       const sceVu0FVECTOR *matrix) const;               // FS2 0x00235740
    // The same, saying how: hit the point of the face, push the way from
    // there out to the sphere's skin.
    bool spherePush(float radius, const sceVu0FVECTOR center, const sceVu0FVECTOR *matrix,
                    sceVu0FVECTOR hit, sceVu0FVECTOR push) const;        // FS2 0x002353b0
    // Notes which vertex holds each extreme.
    void findExtremes();                                                 // FS2 0x00236070

    // The extremes' vertices, 2 bits each (findExtremes).
    int minXVertex() const { return extremes[0] & 3; }
    int minYVertex() const { return (extremes[0] >> 2) & 3; }
    int minZVertex() const { return (extremes[0] >> 4) & 3; }
    int maxXVertex() const { return extremes[0] >> 6; }
    int maxYVertex() const { return extremes[1] & 3; }
    int maxZVertex() const { return (extremes[1] >> 2) & 3; }
    float minX() const { return vertex[minXVertex()][0]; }
    float maxX() const { return vertex[maxXVertex()][0]; }
    float minY() const { return vertex[minYVertex()][1]; }
    float maxY() const { return vertex[maxYVertex()][1]; }
    float minZ() const { return vertex[minZVertex()][2]; }
    float maxZ() const { return vertex[maxZVertex()][2]; }
    // The mesh that took it last.
    StageCollision *mesh() const;

    sceVu0FVECTOR plane;        // FS2 +0x00: normal, then the plane's d
    sceVu0FVECTOR vertex[3];    // FS2 +0x10
    u_int  flags;               // FS2 +0x40: StageCollisionFlag
    u_short attr;               // FS2 +0x44: StageScene::getFloorAttr
    short  doorId;              // FS2 +0x46: its group in the mesh (setGroup); kDoorFaceBase + what StageScene::findDoorOfFace decodes
    u_int  meshId;              // FS2 +0x48: the EE's mesh pointer. PC: the mesh's slot in a table (a pointer does not fit)
    u_char surface;             // FS2 +0x4c: a bullet's effect and sound (getSurfaceSound)
    u_char material;            // FS2 +0x4d: the name in CollisionFile::surfaceNames
    u_char extremes[2];         // FS2 +0x4e: see the accessors
};
static_assert(sizeof(StageCollisionFace) == 0x50, "StageCollisionFace: EE 0x50");
static_assert(offsetof(StageCollisionFace, vertex) == 0x10, "StageCollisionFace::vertex");
static_assert(offsetof(StageCollisionFace, flags) == 0x40, "StageCollisionFace::flags");
static_assert(offsetof(StageCollisionFace, attr) == 0x44, "StageCollisionFace::attr");
static_assert(offsetof(StageCollisionFace, doorId) == 0x46, "StageCollisionFace::doorId");
static_assert(offsetof(StageCollisionFace, meshId) == 0x48, "StageCollisionFace::meshId");
static_assert(offsetof(StageCollisionFace, surface) == 0x4c, "StageCollisionFace::surface");
static_assert(offsetof(StageCollisionFace, extremes) == 0x4e, "StageCollisionFace::extremes");

// One mesh of the file: a node's faces, by the node's name.
// layout: file (EE 0x20)
struct CollisionRecord
{
    char   name[kCollisionNameSize]; // FS2 +0x00: "COL_" stripped by relocate
    int    faceCount;           // FS2 +0x10
    S2dRef<StageCollisionFace> faces; // FS2 +0x14: an EE pointer once relocated
    int    prepared;            // FS2 +0x18: the faces' planes are done
    int    _1c;
};
static_assert(sizeof(CollisionRecord) == 0x20, "CollisionRecord: EE 0x20");

// A collision file as loaded: this header, the records, every record's faces.
// layout: file (EE 0x210 before the records)
struct CollisionFile
{
    void relocate();                                                     // FS2 0x0023ae40

    int    magic;               // FS2 +0x0: kCollisionMagic
    S2dRef<CollisionRecord> records; // FS2 +0x4: an EE pointer once relocated
    int    recordCount;         // FS2 +0x8
    int    _0c;
    char   surfaceNames[kCollisionSurfaceNames][kCollisionNameSize]; // FS2 +0x10: per face's material
};
static_assert(sizeof(CollisionFile) == 0x210, "CollisionFile: EE 0x210 before the records");

// A collision file's record for the node called name; null none.
CollisionRecord *collisionFindRecord(void *file, const char *name);      // FS2 0x0023adb0

// The surface a material's name stands for (getSurfaceSound's), noted as used
// on this stage; -1 no such name.
int surfaceOfName(const char *name);                                     // FS2 0x002cf590
int useSurfaceOfName(const char *name);                                  // FS2 0x002cedb0

// ──────────────────────────────────────────────────────────────────────
// The map

// A piece of the grid: a leaf with its faces, highest first, or two children
// that split its height in two. The grid's cells are the roots.
// layout: host -- EE 0x30.
struct CollisionNode
{
    CollisionNode() : faces(nullptr), upper(nullptr), lower(nullptr) {}  // FS2 0x00235390
    ~CollisionNode();                                                    // FS2 0x00235090
    // Splits while the node is at least minHeight tall, holds at least
    // minFaces and a split leaves both halves with fewer.
    bool split(float minHeight, int minFaces);                           // FS2 0x00234d10

    sceVu0FVECTOR box[2];       // FS2 +0x00: low, high
    std::vector<StageCollisionFace *> *faces; // FS2 +0x20: a leaf's; null an inner node's (MSL's vector in the ROM)
    CollisionNode *upper;       // FS2 +0x24
    CollisionNode *lower;       // FS2 +0x28
};

// What a sphere's walk calls for a face it touches (hit, push as in
// spherePush); not 0 ends that row of cells.
typedef int (*CollisionFaceVisitor)(StageCollisionFace *face, const sceVu0FVECTOR hit,
                                    const sceVu0FVECTOR push, void *context);

// One collision mesh of a Root object (StageObject::m_collisions). mask /
// flags select faces everywhere: a face with a flag in flags is out, and one
// with flags of its own needs one in mask. A face's group must be on.
// layout: host -- EE 0x220.
class StageCollision
{
public:
    // Takes record's faces. surfaceNames: the file's. cellSize: the grid's;
    // a mesh of few faces gets one big cell.
    StageCollision(CollisionRecord *record, const char (*surfaceNames)[kCollisionNameSize],
                   int cellSize);                                        // FS2 0x00234440
    ~StageCollision();                                                   // FS2 0x002343e0

    // The face the segment from..to hits first (hit: where), null none.
    StageCollisionFace *raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                const sceVu0FVECTOR to, u_int mask,
                                u_int flags, int side);                  // FS2 0x00233810
    // The faces the sphere at pos touches, up to maxHits in out; answers how
    // many.
    int sphereHits(float radius, int maxHits, StageCollisionFace **out, u_int mask, u_int flags,
                   const float *pos);                                    // FS2 0x00232870
    // The segment from..to touches the mesh.
    bool segmentHits(const float *from, const float *to, u_int mask, u_int flags,
                     int side);                                          // FS2 0x00233160
    // A mesh that does not move: the highest face under *height at x, y, no
    // further down than range (not positive: the mesh's bottom). *height
    // becomes its height, *second, when not null, the next face down.
    StageCollisionFace *heightAt(float x, float y, float range, float *height, u_int mask,
                                 u_int flags, StageCollisionFace **second); // FS2 0x002342c0
    // Calls visitor for every face the sphere touches.
    void visitSphere(CollisionFaceVisitor visitor, void *context, u_int mask, u_int flags,
                     const BoundingSphere *sphere);                      // FS2 0x00232190
    // Turns a group of faces (a face's doorId) on or off.
    void setGroup(int group, bool on);                                   // FS2 0x002343c0

    const sceVu0FVECTOR *m_matrix;  // FS2 +0x000: where the mesh stands; null not moved (the map's)
    const sceVu0FVECTOR *m_inverse; // FS2 +0x004

private:
    static constexpr int kGroups = 0x1cc;

    int  visitSphereNode(const CollisionNode *node, CollisionFaceVisitor visitor, void *context,
                         u_int mask, u_int flags, const sceVu0FVECTOR center,
                         const BoundingSphere *sphere);                  // FS2 0x00231d80
    int  sphereHitsNode(float radius, const CollisionNode *node, int *count, int maxHits,
                        StageCollisionFace **out, u_int mask, u_int flags,
                        const sceVu0FVECTOR center);                     // FS2 0x00232460
    bool segmentHitsNode(const sceVu0FVECTOR from, const sceVu0FVECTOR to, u_int mask,
                         u_int flags, const CollisionNode *node, int side); // FS2 0x00232b20
    StageCollisionFace *raycastNode(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                    const sceVu0FVECTOR to, u_int mask, u_int flags,
                                    const CollisionNode *node, int side); // FS2 0x00232da0
    StageCollisionFace *heightAtNode(float x, float y, float low, const CollisionNode *node,
                                     float *height, u_int mask, u_int flags,
                                     StageCollisionFace **second);       // FS2 0x00233f90

    // The cells the segment passes, from its start, until visit answers
    // true; the walk raycast and segmentHits inline.
    template <class Visit>
    bool walkCells(const sceVu0FVECTOR from, const sceVu0FVECTOR to, Visit visit);
    // The face's flags pass mask and flags.
    static bool selects(const StageCollisionFace *face, u_int mask, u_int flags)
    {
        return (face->flags & flags) == 0 && (face->flags == 0 || (face->flags & mask) != 0);
    }
    CollisionNode *cellAt(int x, int y) const { return &m_cells[x + y * m_gridWidth]; }

    sceVu0FVECTOR m_box[2];     // FS2 +0x010: low, high
    const char (*m_surfaceNames)[kCollisionNameSize]; // FS2 +0x030
    int    m_gridWidth;         // FS2 +0x034
    int    m_gridHeight;        // FS2 +0x038
    int    m_cellSizeInt;       // FS2 +0x03c
    float  m_cellSize;          // FS2 +0x040
    int    m_faceCount;         // FS2 +0x044
    u_char m_groupOn[kGroups];  // FS2 +0x048
    StageCollisionFace *m_faces; // FS2 +0x214
    CollisionNode *m_cells;     // FS2 +0x218
    u_int  m_slot;              // PC: its slot in the table of StageCollisionFace::meshId
};

// The segment's line passes through the box. Its first test is the segment's
// own box against this one: both ends off one side is no.
bool boxHitsSegment(const sceVu0FVECTOR *box, const sceVu0FVECTOR from,
                    const sceVu0FVECTOR to);                             // FS2 0x00236930
// The boxes (low, high) overlap.
bool boxesOverlap(const sceVu0FVECTOR *a, const sceVu0FVECTOR *b);       // FS2 0x00236ab0

// ──────────────────────────────────────────────────────────────────────
// The world

class CollisionOwner;
struct CollisionBody;

// A growable array of body pointers, the same shape as StageObjArray: data[0]
// unused, the items at data[1..count], a 0 after them.
// layout: host -- EE 8: +0 data, +4 count, +6 capacity.
class CollisionBodyArray
{
public:
    CollisionBodyArray();                                                // FS2 0x001f6a40
    ~CollisionBodyArray();                                               // FS2 0x00184430

    void reserve(u_short capacity);                                      // FS2 0x001f6760
    int  push(CollisionBody *const &body);                               // FS2 0x001f6910
    CollisionBody **find(CollisionBody *const &body);                    // FS2 0x001f6820
    CollisionBody **erase(CollisionBody **slot);                         // FS2 0x001f6880

    CollisionBody **begin() { return m_count == 0 ? nullptr : &m_data[1]; }
    static CollisionBody **next(CollisionBody **slot) { return slot[1] == nullptr ? nullptr : slot + 1; }

    CollisionBody **m_data;     // FS2 +0x0
    u_short m_count;            // FS2 +0x4
    u_short m_capacity;         // FS2 +0x6

    static const u_short kInitialCapacity = 14;

private:
    CollisionBodyArray(const CollisionBodyArray &);
    CollisionBodyArray &operator=(const CollisionBodyArray &);
};

// CollisionBody::shape, 0 until the owner says.
enum CollisionShape
{
    CollisionShape_Camera  = 1, // the main camera's: a point; updateBox gives it no box
    CollisionShape_Segment = 2,
    CollisionShape_Sphere  = 3,
    CollisionShape_Spheres = 4, // the list at spheres
    CollisionShape_Capsule = 5,
    CollisionShape_Mesh    = 6, // a map object's
};
// A body's category, the ones read.
constexpr u_int kCollisionMesh      = 1;        // a map object's mesh
constexpr u_int kCollisionMelee     = 0x40;     // a hand or a blade: a map object breaks like under a bullet (a guess)
constexpr u_int kCollisionBulletHit = 0x80;     // a bullet's capsule
constexpr u_int kCollisionBulletRay = 0x100;    // a bullet's segment
constexpr u_int kCollisionTrap      = 0x200;
constexpr u_int kCollisionLight     = 0x2000;

// One shape of the collision world (SceneRecordBuckets), EE 0xe0: built by
// lights, bullets, traps, map objects, cameras. The world pairs up bodies
// whose category meets the other's mask, so the names are read from add
// 0x1f6180, not from the collide step.
// layout: host -- EE offsets in the comments.
struct CollisionBody
{
    // Shape 0 until the owner says; on = 1, the three state bytes clear.
    CollisionBody(CollisionOwner *bodyOwner, int bodyBucket, u_int bodyCategory,
                  u_int bodyMask);                                       // FS2 0x001f6690
    // Marks it settled (it needs no sync or box from the collide step) and
    // works out the box.
    void refresh();                                                      // FS2 0x001f6670
    // The box from the one sphere or the list; other shapes keep theirs.
    void updateBox();                                                    // FS2 0x001f6590

    u_char         on;          // FS2 +0x00: 1 from the ctor; a closed trap clears it
    u_char         settled;     // FS2 +0x01: synced and boxed stay set between frames
    u_char         synced;      // FS2 +0x02: syncBody ran this frame
    u_char         boxed;       // FS2 +0x03: the box is good; an owner that moved it answers not 0
    CollisionOwner *owner;      // FS2 +0x04
    int            bucket;      // FS2 +0x08: SceneRecordBuckets' list
    u_int          category;    // FS2 +0x0c
    u_int          mask;        // FS2 +0x10
    CollisionBodyArray partners; // FS2 +0x14: the bodies it is tested against
    sceVu0FVECTOR  box[2];      // FS2 +0x20: low, high
    int            shape;       // FS2 +0x40: CollisionShape
    u_int          rayMask;     // FS2 +0x44: a segment's or a sphere's, against a mesh
    u_int          rayFlags;    // FS2 +0x48: the same
    sceVu0FVECTOR  point;       // FS2 +0x50: a camera's
    sceVu0FVECTOR  segment[2];  // FS2 +0x60: a segment's ends
    BoundingSphere sphere;      // FS2 +0x80
    sceVu0FVECTOR  capsule[2];  // FS2 +0xa0: a capsule's ends
    float          capsuleRadius; // FS2 +0xc0
    const sceVu0FVECTOR *capsuleMatrix; // FS2 +0xc4: turns the sphere it is tested with; null none
    StageCollision *mesh;       // FS2 +0xd0: a mesh body's
    StageList<BoundingSphere> spheres; // FS2 +0xd4
};

// What the owners are told: the two bodies, each side's own first, and which
// sphere of a list met which (0 for other shapes).
// layout: host -- EE order, 0x10 with 4-byte pointers.
struct CollisionContact
{
    CollisionBody *self;        // FS2 +0x00: the owner's own
    CollisionBody *body;        // FS2 +0x04: the other side's
    int selfIndex;              // FS2 +0x08
    int bodyIndex;              // FS2 +0x0c
};
// Both sides of a meeting.
// layout: host -- EE 0x20.
struct CollisionPair
{
    CollisionContact side[2];   // FS2 +0x00
};

// The base that owns a body (EE +0 a flag, +4 the vptr; +0x200 in the lights
// and the Root objects, +0x240 in the items). Vtable 0x3115a0: six slots,
// five of them empty there. Like the object's bases its slots repeat, after
// the runner's, in the class's own table.
// layout: host
class CollisionOwner
{
public:
    // Slot 0: touching, each frame it lasts. depth how far in; a sphere's
    // meeting has the normal and, on the map, the face. Not 0: the body
    // moved, its box is redone. A light answers 0.
    virtual int  collisionHook0(CollisionContact *contact, const float *normal,
                                StageCollisionFace *face, float depth) = 0;
    // Slot 1: the frame's nearest meeting of a body, after the others; answers
    // as slot 0. 0 on a light and a bullet. On a segment: the sweep's vector,
    // the map's face, how far along it (t, distance from the segment's start).
    virtual int  onCollide(CollisionContact *contact, const float *sweep,
                           StageCollisionFace *face, float t, float distance) = 0;
    // Slot 2: the body follows the owner.
    virtual void syncBody(CollisionBody *body) = 0;
    // Slot 3: after the frame's contacts. Empty on a light.
    virtual void collisionHook3(CollisionBody *body) = 0;
    // Slot 4 (+0x18): the object behind the body; the world compares these.
    virtual StageObject *getCollisionObject() = 0;
    // Slot 5 (+0x1c): an object the body must not meet (add 0x1f6180
    // compares it with the other's); null here.
    // FS2 0x00171ad0
    virtual StageObject *getCollisionParent() { return nullptr; }

    u_char m_enabled = 1;       // FS2 +0x00: 1 from the base's ctor
};

constexpr int kCollisionBuckets = 4;

// +0x18 of the StageScene: four arrays of bodies, reserved for 0x1fe each, and
// the frame's nearest meeting of the body in hand. layout: host (EE 0x70).
struct SceneRecordBuckets
{
    SceneRecordBuckets();                                                // FS2 0x001f6510
    ~SceneRecordBuckets();                                               // FS2 0x001f63f0: deleting
    // body joins its bucket, and is paired up with what its category meets.
    void add(CollisionBody *body);                                       // FS2 0x001f6180
    void remove(CollisionBody *body);                                    // FS2 0x001f60a0
    void collide();                                                      // FS2 0x001f5250

private:
    // One pair's test, by the shapes.
    void collidePair(CollisionBody *self, CollisionBody *other);         // FS2 0x001f5680

    CollisionBodyArray m_lists[kCollisionBuckets]; // FS2 +0x00
    float m_nearest;            // FS2 +0x20
    CollisionPair m_nearestPair; // FS2 +0x24
    u_char m_nearestIsMap;      // FS2 +0x44: a segment on the map: the next three
    sceVu0FVECTOR m_nearestSweep; // FS2 +0x50
    float m_nearestT;           // FS2 +0x60
    StageCollisionFace *m_nearestFace; // FS2 +0x64
};

#endif /* _GAME_STAGECOLLISION_H */
