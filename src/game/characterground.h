/* ==========================================================================
 *  game/characterground.h
 *
 *  What keeps a character on the map and off everybody else (EE 0x1c0, ctor
 *  0x1f3d20, StageObject::m_ground): a CollisionOwner with up to six kinds of
 *  body in the collision world, and the frame's floor. The file name and the
 *  class name are the port's: no string or RTTI names them.
 * ======================================================================== */
#ifndef _GAME_CHARACTERGROUND_H
#define _GAME_CHARACTERGROUND_H

#include "game/stagecollision.h"
#include "game/stagelist.h"
#include "sdk/libvu0.h"
#include "sdk/scetypes.h"

class StageCar;
class StageNodeBase;
class StageObject;
struct StageCollisionFace;

// layout: host -- a bone a body hangs from (game/groundbones.inc). EE 0x20: +0 name, +4 second
// bone, +8 radius, +0x10 an offset, zero in every row of the ROM (left out; the ROM still turns it).
struct GroundBone
{
    const char *name;           // FS2 +0x0
    const char *second;         // FS2 +0x4: blows only: the bone the arm or leg starts from
    float       radius;         // FS2 +0x8
};

constexpr int kGroundFamilies = 10;     // the rows of the bone tables
constexpr int kGroundMaxBlows = 5;      // +0x198..+0x1ab
constexpr int kGroundMaxHitters = 10;   // +0xb4
constexpr int kGroundWallAngles = 8;    // +0xf4

// A body's category, as the ground tells its bodies apart in the hooks
// (the ctor's; kCollisionMelee is the sixth).
constexpr u_int kGroundHitBody  = 2;    // what is hit: +0x184
constexpr u_int kGroundBlowBody = 4;    // what hits, one per hand or foot: +0x198
constexpr u_int kGroundNearBody = 8;    // a sphere around it, only with traps: +0x188
constexpr u_int kGroundPushBody = 0x10; // character against character: +0x18c
constexpr u_int kGroundWallBody = 0x20; // character against the map: +0x190

// How hard a car throws a character: CharacterGround::slideLevel. Below
// GroundSlide_Light it only pushes.
enum GroundSlide
{
    GroundSlide_None  = 0,
    GroundSlide_Light = 2,
    GroundSlide_Heavy = 3,
    GroundSlide_Worst = 4,
};

// layout: host -- EE 0x1c0; the offsets are the ROM's. CollisionOwner's byte
// is +0, its vptr +4. StageCharacter::reset sets the byte.
class CharacterGround : public CollisionOwner
{
public:
    explicit CharacterGround(StageObject *owner);                        // FS2 0x001f3d20
    ~CharacterGround();                                                  // FS2 0x001f3c70: deleting
    // Back where the character stands, nothing remembered.
    void reset();                                                        // FS2 0x001f3b30
    // The floor near a point of the character's own space: from 70 above
    // it, 170 down.
    StageCollisionFace *faceNear(const float *localPoint);               // FS2 0x001eff00

    // Clears (0) or sets the contact bytes of the ground's feet; the
    // player's own byte follows.
    void setContact(int value);                                          // FS2 0x001eff80
    // Which bone of the family's list a hit comes from; -1 none of them.
    void setHitBone(const char *name);                                   // FS2 0x001f00a0
    // The movement hook's end: the frame's flags clear, a car's throw eases.
    void beginFrame();                                                   // FS2 0x001f3930
    // The terminate hook's start: the floor when nothing did it, and where
    // it stands is where the next frame's sweep starts.
    void endFrame();                                                     // FS2 0x001f38d0
    // Not in the air: the feet solve, and nothing pulls it down.
    bool isGrounded() const { return landed != 0 || hasFloor == 0; }

    // CollisionOwner: vtable 0x315ee0.
    int  collisionHook0(CollisionContact *contact, const float *normal,
                        StageCollisionFace *face, float depth) override; // FS2 0x001f0a20
    int  onCollide(CollisionContact *contact, const float *sweep, StageCollisionFace *face,
                   float t, float distance) override;                    // FS2 0x001f06b0
    void syncBody(CollisionBody *body) override;                         // FS2 0x001f2be0
    void collisionHook3(CollisionBody *body) override;                   // FS2 0x001f2b20
    StageObject *getCollisionObject() override;                          // FS2 0x001efef0

    StageObject  *owner = nullptr;  // FS2 +0x008
    int           family = 0;       // FS2 +0x00c: the row of the bone tables, from the sheet's kind and the body size
    u_char        hasFloor = 0;     // FS2 +0x010: it looks for a floor; without one it counts as standing
    u_char        hasPush = 0;      // FS2 +0x011: pushBody
    u_char        hasWall = 0;      // FS2 +0x012: wallBody
    u_char        doorsBlock = 0;   // FS2 +0x013: 0: doors and door faces let it through (doors then in exceptList); kind 6 works it out each frame
    u_char        hasHits = 0;      // FS2 +0x014: volume, blowBodies, meleeBody
    int           groundCheck = 0;  // FS2 +0x018: the motion's 接地判定 (PlayerMotion::stepLower); 1 pulls the feet to the floor
    int           repelLevel = 0;   // FS2 +0x01c: its キャラ反発LV
    u_char        landed = 0;       // FS2 +0x020: a floor under it this frame
    u_char        wasLanded = 0;    // FS2 +0x021: last frame
    StageCollisionFace *floorFace = nullptr; // FS2 +0x024: the face it stands on
    StageObject  *floorObject = nullptr; // FS2 +0x028: whose face that is
    float         floorHeight = 0.0f;    // FS2 +0x02c: its height when it last stood
    sceVu0FVECTOR rayFrom = {};     // FS2 +0x030: the floor ray
    sceVu0FVECTOR rayTo = {};       // FS2 +0x040
    int           floorAttr = 0;    // FS2 +0x050: StageScene::getFloorAttr
    u_char        sweepsWalls = 1;  // FS2 +0x054: 1 from the ctor; nothing else writes it so far
    u_char        nearTarget = 0;   // FS2 +0x055: within 250 of the view target (PlayerMotion::applyMovement)
    StageList<BoundingSphere> lastSpheres; // FS2 +0x058: wallBody's, as last frame
    sceVu0FVECTOR lastPosition = {};  // FS2 +0x060
    u_char        fixPositionName = 0; // FS2 +0x070: it is F_E10_SB1: a script's fix_position turns the walls off
    u_char        wasHit = 0;       // FS2 +0x080: something reached it this frame
    StageNodeBase *hitNode = nullptr; // FS2 +0x084: which bone
    const GroundBone *hitRow = nullptr; // FS2 +0x088: that bone's row of the table
    StageObject  *hitBy = nullptr;  // FS2 +0x08c: what reached it
    sceVu0FVECTOR hitDirection = {}; // FS2 +0x090
    int           hitInBack = 0;    // FS2 +0x0a0: the blow comes along the way it faces
    u_char        blowLanded = 0;   // FS2 +0x0b0: one of its blows met something
    StageObject  *hitters[kGroundMaxHitters] = {}; // FS2 +0x0b4: who this blow reached
    int           hitterCount = 0;  // FS2 +0x0dc
    u_char        nearSomeone = 0;  // FS2 +0x0e0
    StageObject  *nearObject = nullptr; // FS2 +0x0e4: the first of the frame
    u_char        narrow = 0;       // FS2 +0x0e8: Move walks instead of running
    u_char        wonStruggle = 0;  // FS2 +0x0e9: PlayerMotion::winsStruggle
    u_char        climbedOver = 0;  // FS2 +0x0ea: a climber more than 100 over its floor met it
    u_char        pushedThisFrame = 0; // FS2 +0x0eb
    u_char        atDoor = 0;       // FS2 +0x0ec: its wall spheres touch a door in mode 1 or 3
    int           wallCount = 0;    // FS2 +0x0f0
    float         wallAngles[kGroundWallAngles] = {}; // FS2 +0x0f4: of the walls met, from where it faces
    u_char        bumped = 0;       // FS2 +0x114: a blow met a wall this frame
    StageCollisionFace *bumpFace = nullptr; // FS2 +0x118
    u_char        carHit = 0;       // FS2 +0x120: a car threw it this frame
    float         slideTime = 0.0f;  // FS2 +0x124: seconds left of the throw
    float         slideSpeed = 0.0f; // FS2 +0x128: the floor carries it
    sceVu0FVECTOR slideDirection = {}; // FS2 +0x130
    int           slideLevel = GroundSlide_None; // FS2 +0x140: GroundSlide
    float         nearRadius = 0.0f; // FS2 +0x150: the widest of pushBody's, from the root; 0 without one
    float         radius = 0.0f;    // FS2 +0x154: wallBody's widest, with its bone's reach
    float         radius2 = 0.0f;   // FS2 +0x158: wallBody's widest sphere
    StageList<StageNodeBase *> bodyBones;  // FS2 +0x15c: volume's spheres
    StageList<StageNodeBase *> blowBones;  // FS2 +0x164: the first bone of each blow body; holds its last position
    StageList<StageNodeBase *> blowRoots;  // FS2 +0x16c: the second; null none
    StageList<StageNodeBase *> pushBones;  // FS2 +0x174
    StageList<StageNodeBase *> wallBones;  // FS2 +0x17c
    CollisionBody *volume = nullptr;    // FS2 +0x184: the spheres it is hit on; the camera keeps out of their box
    CollisionBody *nearBody = nullptr;  // FS2 +0x188
    CollisionBody *pushBody = nullptr;  // FS2 +0x18c
    CollisionBody *wallBody = nullptr;  // FS2 +0x190: the motion's 壁判定 turns it on and off
    CollisionBody *meleeBody = nullptr; // FS2 +0x194: the blow's own bone against the map
    CollisionBody *blowBodies[kGroundMaxBlows] = {}; // FS2 +0x198
    int           hitBone = -1;     // FS2 +0x1ac: setHitBone
    int           exceptCount = 0;  // FS2 +0x1b0
    StageObject **exceptList = nullptr; // FS2 +0x1b4: the doors when they do not block, and last the view target
    float         pushedBack = 0.0f; // FS2 +0x1b8: how much a walking character yields, up to half a sphere

private:
    // The jobs of collisionHook0, by the body's category.
    int  bumpWall(StageCollisionFace *face);
    int  meetWall(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                  float depth);
    int  meetCharacter(CollisionContact *contact, const float *normal, float depth);
    int  meetHit(CollisionContact *contact);
    // Whether the wall counts; a car on the way may throw it here.
    bool wallCounts(CollisionContact *contact, StageCollisionFace *face); // FS2 0x001f0170
    // The way from last frame to this one, then a way out of a car.
    void sweepWalls();                                                   // FS2 0x001f1c40
    bool isInsideCar();                                                  // FS2 0x001f2990
    // Out of the view target it stands in. Answers that target, or null when
    // the character is not inside it.
    StageCar *leaveCar();                                                // FS2 0x001f2670
    // Each wall sphere's way since last frame: what it went through pushes
    // it back. level 2 tries beside it too.
    void keepInside(int level, StageCar *car);                           // FS2 0x001f2300
    // The floor under it; a floor found the first time takes the feet.
    void checkFloor();                                                   // FS2 0x001f35b0
    // Moves the character, its pose and its wall spheres.
    void moveBy(const sceVu0FVECTOR step);
    // Which bone of the body table, and the way it faces against the hit.
    void noteHitBone(int index, StageObject *by);
    void updateInBack();
    // syncBody's job for the wall body.
    void syncWall(CollisionBody *body);
};

#endif /* _GAME_CHARACTERGROUND_H */
