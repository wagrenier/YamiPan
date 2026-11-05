/* ==========================================================================
 *  game/stageitem.h
 *
 *  What a character carries and shoots, 0x17fce0..0x184860: the bullet and
 *  the items of a stage. StageItem is the plain one (a model with a pick
 *  point); four kinds add to it: the thrown items (ItemPhysics, the smoke
 *  flare's fire), the trap (a sphere in the collision world), the weapons
 *  (ItemPhysics, the magazine, the shot) and StageItemE (the lamp: four
 *  lights of its own). The file name and every class name but the ones the
 *  scene's lists gave are the port's: no source string, no symbols.
 * ======================================================================== */
#ifndef _GAME_STAGEITEM_H
#define _GAME_STAGEITEM_H

#include "game/gamehooks.h"
#include "game/seplayer.h"

class PointLight;
class SpotLight;
class StageLight;
class StageItem;

// A bullet's two bodies (bucket, category and mask are the ROM's).
constexpr int   kBulletBodies          = 2;
constexpr int   kBulletBodyHitBucket   = 2;     // the capsule: what it meets
constexpr int   kBulletBodyRayBucket   = 1;     // the segment: the map
constexpr u_int kBulletHitMask         = 0x2002;
constexpr u_int kBulletRayMask         = 1;
constexpr float kBulletRadius          = 8.0f;
constexpr float kBulletUnitsPerSecond  = 20000.0f;
// The segment's map test, what it hands StageScene::raycast.
constexpr u_int kBulletRayFaceMask     = 0x1e42600f;
constexpr u_int kBulletRayFaceFlags    = 0x48000;
// A face with this flag lets a bullet through: always on a door's faces,
// half the time on the others (the ROM then looks on along the segment).
constexpr u_int kFaceBulletPasses      = 0x10000;
// StageCollisionFace::surface values with an effect of their own.
constexpr u_char kSurfaceSplash        = 0x1f;  // GUN_SPLASH
constexpr u_char kSurfaceKyusyu        = 0x1d;  // kyusyu, on stage kStageKyusyu only
constexpr int    kStageKyusyu          = 0x1b;

// The trap's sphere; a character this near and in front of it can spring it.
constexpr float kTrapRadius      = 80.0f;
constexpr float kTrapSpringRange = 90.0f;
// Seconds a thrown item or a sprung trap stays before it goes.
constexpr float kItemLingerSeconds = 10.0f;
// Seconds a smoke flare burns.
constexpr float kFlareBurnSeconds  = 20.0f;

// A surface's sound (StageCollisionFace::surface); null none. The run
// unassigned@002c42a0.
const SeEntry *getSurfaceSound(int surface);                             // FS2 0x002cf0b0

// The first class of the bullet's two, kind 0x18 (vtables 0x312f40 / 0x312f64
// / 0x312f7c; its ctor is inline in StageBullet's). layout: host -- EE: the
// object's to +0x200, then the fields below.
class StageBulletBase : public StageObject
{
public:
    StageBulletBase(StageCharacter *owner, StageItem *weapon);
    ~StageBulletBase() override;                                         // FS2 0x00180670

    StageCharacter *shooter() const { return m_owner; }

    // FS2 0x00183fd0
    int getKind() override { return StageObject_BulletBase; }

protected:
    StageCharacter *m_owner;    // FS2 +0x200: who fired; null for the one setupCameras makes
    StageItem      *m_weapon;   // FS2 +0x204
};

// A bullet (named "bullet", group 4; vtables 0x312ca0 / 0x312cc4 / 0x312cdc /
// 0x312eec; the CollisionOwner at +0x240). It flies a straight line, m_speed
// a frame, until its range is spent or a body stops it: the segment finds the
// map (category kCollisionBulletRay), the capsule whatever else it meets
// (kCollisionBulletHit). setupCameras makes one, off, at the start.
// layout: host -- EE 0x2c0.
class StageBullet : public StageBulletBase, public CollisionOwner
{
public:
    StageBullet(StageCharacter *owner, StageItem *weapon);               // FS2 0x001803e0
    ~StageBullet() override;                                             // FS2 0x00180260

    // FS2 0x0017fce0
    int getKind() override { return StageObject_Bullet; }

    // The side base: the third is the frame.
    // FS2 0x0017fd10
    void sideHook0() override {}
    // FS2 0x0017fd00
    void sideHook1() override {}
    void sideHook2() override;                                           // FS2 0x001801c0
    // FS2 0x0017fcf0
    void sideHook3() override {}

    // FS2 0x00180020
    int  collisionHook0(CollisionContact *, const float *, StageCollisionFace *, float) override { return 0; }
    int  onCollide(CollisionContact *contact, const float *sweep, StageCollisionFace *face,
                   float t, float distance) override;                    // FS2 0x0017fea0
    void syncBody(CollisionBody *body) override;                         // FS2 0x00180170
    void collisionHook3(CollisionBody *body) override;                   // FS2 0x00180030
    // FS2 0x0017fd30
    StageObject *getCollisionObject() override { return this; }
    // The shooter: a bullet never meets who fired it.
    StageObject *getCollisionParent() override;                          // FS2 0x0017fd20

    float         m_speed;      // FS2 +0x248: units a frame
    sceVu0FVECTOR m_velocity;   // FS2 +0x250

private:
    int   m_framesLeft = 0;     // FS2 +0x260: the range in frames (PC: 0 without a weapon; the ROM leaves it)
    CollisionBody *m_bodies[kBulletBodies]; // FS2 +0x264: the capsule, the segment
    u_char m_hit = 0;           // FS2 +0x26c: the map was hit this frame
    sceVu0FVECTOR m_hitPoint;   // FS2 +0x270
    float  m_hitDistance = 0.0f; // FS2 +0x280
    StageCollisionFace *m_hitFace = nullptr; // FS2 +0x284
};

// What throws, drops and shows a carried item (class of vtable 0x316510, a
// CollisionOwner at +0x240 in the thrown items and the weapons; the run
// unassigned@0023c020). Read here only as far as its users.
// layout: host -- EE: the owner's fields at +0x240, 0x210 bytes.
class ItemPhysics : public CollisionOwner
{
public:
    ItemPhysics(bool arg0, bool arg1);                                   // FS2 0x0023db50
    ~ItemPhysics();                                                      // FS2 0x0023db00

    // The bones of motion, which is owner's.
    void setup(StageItem *owner, void *motion);                          // FS2 0x0023d600
    void update();                                                       // FS2 0x0023ca50
    void draw();                                                         // FS2 0x0023c910

    int  collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                        float depth) override;                                      // FS2 0x0023c020
    // FS2 0x0023c010
    int  onCollide(CollisionContact *, const float *, StageCollisionFace *, float, float) override { return 0; }
    void syncBody(CollisionBody *body) override;                         // FS2 0x0023c300
    void collisionHook3(CollisionBody *body) override;                   // FS2 0x0023c2f0
    // FS2 0x001840b0
    StageObject *getCollisionParent() override { return m_parent; }

    u_char       m_moving = 0;  // FS2 +0x1fc (the owner's +0x43c): still falling or thrown
    StageObject *m_parent = nullptr; // FS2 +0x200 (the owner's +0x440)
};

// The item classes (StageFiles::placeItem, by ItemDef::kind and name). The
// base: vtables 0x313bc0 / 0x313be4 / 0x313bfc; the object's slots go on at
// +0x24c: setupItem, the four side hooks, canPick.
// layout: host -- EE sizes in the comments.
class StageItem : public StageObject
{
public:
    StageItem();                                                         // FS2 0x00183f40: EE 0x240
    ~StageItem() override;                                               // FS2 0x00183ec0

    // FS2 0x00184250
    int getKind() override { return StageObject_Item; }

    // Slot +0x24c: joins the scene under name: list, script, model, its
    // motions, its count and pick point. Runs with m_itemDef already set.
    virtual void setupItem(const char *name, ModelInstance *instance,
                           StringMap<Tim2Texture *> *textures,
                           StageList<void *> *motions);                  // FS2 0x00183bf0

    // FS2 0x001840d0
    void sideHook0() override {}
    // FS2 0x001840c0
    void sideHook1() override {}
    // The motion's frame.
    void sideHook2() override;                                           // FS2 0x00183b80
    // The model's frozen flag.
    void sideHook3() override;                                           // FS2 0x00183af0

    // Slot +0x260 (0x183780 jumps here): who is close and facing the pick
    // point, the short test.
    virtual bool canPick(StageObject *who);                              // FS2 0x001838d0
    // The same with a wider reach.
    bool canPickFar(StageObject *who);                                   // FS2 0x00183790

    // Sets the magazine full, once m_weaponDef is set.
    void initWeapon();                                                   // FS2 0x001823f0
    // The mission's text for the item, or the file's "????".
    const char *getDisplayName();                                        // FS2 0x00183a40
    // GETITEMn / WEAPONn: null none.
    const char *getPickSound();                                          // FS2 0x00183a90

protected:
    friend struct SceneRender;      // game/scenerender.cpp

    // The definition; PC: the first for a bad index, where the ROM reads
    // from address 0.
    const ItemDef &itemDef() const;

    void         *m_holder;     // FS2 +0x204: 0 from the ctor, unread: set, the model stays frozen
    u_char        m_inUse;      // FS2 +0x208: a thrown flare's and an open trap's
    int           m_maxCount;   // FS2 +0x210
    void         *m_itemMotion; // FS2 +0x214: the motion list's first
    u_char        m_sprung;     // FS2 +0x218: a trap that closed
    sceVu0FVECTOR m_pickPoint;  // FS2 +0x220: bn_pic in the motion, the object's space, w 1
};

// The lamp's lights (0x18 bytes; StageItemE +0x240): a spot and a wide spot,
// two points, and the switched part "nl". "Lamp" is a guess from T_TORCH2.
// layout: host -- EE offsets in the comments.
class ItemLamp
{
public:
    explicit ItemLamp(StageObject *owner);                               // FS2 0x00182f40
    ~ItemLamp();                                                         // FS2 0x00182ee0

    // Shadows from the two spots.
    void setCastsShadow(bool on);                                        // FS2 0x00182b80
    // +0x2b0 of the first spot and the last point (unread).
    void setValue2b0(int value);                                         // FS2 0x00182ba0
    // Hangs the four lights' nodes under parent, or only detaches them.
    void attachTo(StageNodeBase *parent);                                // FS2 0x00182bc0
    // The owner's tree: on or off.
    void setSwitchedOn(bool on);                                         // FS2 0x00182eb0
    // Shows the dark part (0) or the lit one (1).
    void setDark(bool dark);                                             // FS2 0x00182ec0

    SpotLight   *m_spot;        // FS2 +0x00
    SpotLight   *m_wideSpot;    // FS2 +0x04
    PointLight  *m_point;       // FS2 +0x08
    PointLight  *m_point2;      // FS2 +0x0c
    PartSwitch  *m_parts;       // FS2 +0x10
    StageObject *m_owner;       // FS2 +0x14
};

// ItemKind_E (vtables 0x313950 / 0x313974 / 0x31398c): the lamp.
// layout: host -- EE 0x280.
class StageItemE : public StageItem
{
public:
    StageItemE();                                                        // FS2 0x00182ad0
    ~StageItemE() override;                                              // FS2 0x00182a00

    // FS2 0x001841f0
    int getKind() override { return StageObject_ItemE; }
    void setupItem(const char *name, ModelInstance *instance, StringMap<Tim2Texture *> *textures,
                   StageList<void *> *motions) override;                 // FS2 0x00182970

    // Slot +0x264.
    virtual void setDark(bool dark);                                     // FS2 0x00184230
    // Slot +0x268.
    virtual void setSwitchedOn(bool on);                                 // FS2 0x00184210
    // Slot +0x26c.
    virtual bool isSwitchedOn();                                         // FS2 0x00184200

    // The lamp's first spot is on: the map shows the player (the S2D's rMapP).
    bool isSpotOn() const;
    // light is one of the four.
    bool ownsLight(StageObject *light) const;                            // FS2 0x001828b0
    void setShadowOwner(StageObject *owner);                             // FS2 0x00182930
    // The matrix a shadow is thrown from: the last point's, pulled back
    // from a wall between the holder's head and it.
    void getLightMatrix(sceVu0FMATRIX out);                              // FS2 0x00182770

private:
    friend struct SceneRender;      // game/scenerender.cpp
    friend struct PlayerMotion;     // stepUpper reads m_isTorch2
    ItemLamp *m_lamp;           // FS2 +0x240
    u_char    m_isTorch2;       // FS2 +0x244: T_TORCH2 (unread)
};

// Kinds below 7 (vtables 0x3136c0 / 0x3136e4 / 0x3136fc / 0x313924): blades,
// clubs and guns; ItemPhysics at +0x240.
// layout: host -- EE 0x480.
class StageWeaponItem : public StageItem, public ItemPhysics
{
public:
    StageWeaponItem();                                                   // FS2 0x00182670
    ~StageWeaponItem() override;                                         // FS2 0x00184110

    // FS2 0x001841d0
    int getKind() override { return StageObject_WeaponItem; }
    void setupItem(const char *name, ModelInstance *instance, StringMap<Tim2Texture *> *textures,
                   StageList<void *> *motions) override;                 // FS2 0x001825a0
    void sideHook2() override;                                           // FS2 0x00181400
    void sideHook3() override;                                           // FS2 0x00181340
    // FS2 0x001841e0
    StageObject *getCollisionObject() override { return this; }

    // A shot from shooter, from .. to: a round out of the magazine; a
    // bullet unless blank; the flash and the shell, the light, the report.
    void fire(StageCharacter *shooter, const sceVu0FVECTOR from, const sceVu0FVECTOR to,
              bool blank);                                               // FS2 0x00181f40

    // The sound names, by the weapon's name and ItemDef::kind.
    const char *getPickSound();                                          // FS2 0x00181490
    const char *getReloadSetSound();                                     // FS2 0x001814f0
    const char *getMagazineSetSound();                                   // FS2 0x00181540
    const char *getMagazineOutSound();                                   // FS2 0x001816d0
    const char *getShotSound();                                          // FS2 0x00181860
    const char *getHitSound();                                           // FS2 0x00181960
    const char *getSwingSound(bool light);                               // FS2 0x00181b50

private:
    // G_NN38, R_MURATA and G_26NEN load round by round; the rest by the
    // magazine (and throw shells). Inline in the ROM.
    bool loadsByMagazine() const;
    // A samurai's blade: kind 0 LW_SAMURAI_B, kind 1 LW_SAMURAI_B2.
    bool isSamuraiBlade() const;

    u_char        m_45c;        // FS2 +0x45c: 0 after setupItem (unread)
    sceVu0FVECTOR m_aimPoint;   // FS2 +0x460: bn_point in the motion
    sceVu0FVECTOR m_firePoint;  // FS2 +0x470: bn_fire in the motion
};

// I_WANA (vtables 0x313420 / 0x313444 / 0x31345c / 0x313684): a trap, a
// sphere of the collision world while it is open.
// layout: host -- EE 0x280.
class StageTrapItem : public StageItem, public CollisionOwner
{
public:
    StageTrapItem();                                                     // FS2 0x00181200
    ~StageTrapItem() override;                                           // FS2 0x00181150

    // FS2 0x001840f0
    int getKind() override { return StageObject_TrapItem; }
    void sideHook2() override;                                           // FS2 0x00180e70

    // FS2 0x00180e20
    int  collisionHook0(CollisionContact *, const float *, StageCollisionFace *, float) override { return 0; }
    // FS2 0x00180e10
    int  onCollide(CollisionContact *, const float *, StageCollisionFace *, float, float) override { return 0; }
    void syncBody(CollisionBody *body) override;                         // FS2 0x00180e40
    // FS2 0x00180e30
    void collisionHook3(CollisionBody *) override {}
    // FS2 0x00184100
    StageObject *getCollisionObject() override { return this; }

    // It goes kItemLingerSeconds from now.
    void startLinger();                                                  // FS2 0x00180f40
    // Sprung shut: the close motion, the sphere off.
    void spring();                                                       // FS2 0x00180f50
    void disarm();                                                       // FS2 0x00180f90
    // Open again, with the open motion.
    void arm();                                                          // FS2 0x00180fa0
    // who is near and the trap is in front of it.
    bool canSpring(StageObject *who);                                    // FS2 0x00180fe0
    // Where a character stands to work the trap: pos, rot, a lift of 60.
    void getWorkPlace(sceVu0FVECTOR pos, sceVu0FVECTOR rot, sceVu0FVECTOR lift,
                      StageObject *who);                                 // FS2 0x001810b0

private:
    CollisionBody *m_body;      // FS2 +0x248
    float  m_lingered;          // FS2 +0x24c: seconds since startLinger
    u_char m_lingering;         // FS2 +0x250
};

// I_HATUEN, I_AKIKAN, I_PET, I_NUT (vtables 0x313190 / 0x3131b4 / 0x3131cc /
// 0x3133f4): ItemPhysics at +0x240. An item at rest goes after
// kItemLingerSeconds; I_HATUEN, the flare, burns for kFlareBurnSeconds first.
// layout: host -- EE 0x480.
class StageThrownItem : public StageItem, public ItemPhysics
{
public:
    StageThrownItem();                                                   // FS2 0x00180d30
    ~StageThrownItem() override;                                         // FS2 0x00183fe0

    // FS2 0x001840a0
    int getKind() override { return StageObject_ThrownItem; }
    void setupItem(const char *name, ModelInstance *instance, StringMap<Tim2Texture *> *textures,
                   StageList<void *> *motions) override;                 // FS2 0x00180c40
    void sideHook2() override;                                           // FS2 0x001808f0
    void sideHook3() override;                                           // FS2 0x001808b0
    // FS2 0x001840e0
    StageObject *getCollisionObject() override { return this; }

    // The flare is at rest in a SMOKE_A box (asked once, then kept).
    bool isInSmokeArea();                                                // FS2 0x00180710
    // SMOKE_LAND .. KNIFE_LAND: null none.
    const char *getLandSound();                                          // FS2 0x001807c0
    void setThrowVector(const sceVu0FVECTOR vector);                     // FS2 0x00180820
    // The flare still burns.
    bool isSmoking();                                                    // FS2 0x00180830

private:
    // Is the flare (inline in the ROM).
    bool isFlare() const;

    // m_smokeArea before the first answer.
    static constexpr int kSmokeAreaUnasked = 1;

    sceVu0FVECTOR m_throwVector; // FS2 +0x450
    float  m_restTime;          // FS2 +0x460
    StageParticle *m_smoke;     // FS2 +0x464: "hatsuen", where it lies
    StageParticle *m_roomSmoke; // FS2 +0x468: "heyasmoke", a SMOKE_A room's
    float  m_burnTime;          // FS2 +0x46c
    int    m_smokeArea;         // FS2 +0x470: kSmokeAreaUnasked, then whether contains found a box
    StageSoundSource *m_sound;  // FS2 +0x474: SMOKE_ON
};

#endif /* _GAME_STAGEITEM_H */
