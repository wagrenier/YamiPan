/*
 *  game/stagedoor.h -- a door of the map (FS2 0x1723c0..0x174a10, vtable
 *  0x3112f0, EE 0x2c0): the models oXX_dp_nn and oXX_ds_nn.
 *
 *  A door is told what to do by its own script (one entry per door): each
 *  frame the script looks at what the Door condition found for the player
 *  (StageCharacter::doorActions), puts a label on the Use button
 *  (UiItemList::post) and, when the button goes, asks the player's motion
 *  through natives 0x87..0x89. The motion's Begin and End events then move
 *  the door (sideHook1), so door and hand stay together, and DoorLink plays
 *  the door's half of the motion on the door's own pose.
 *
 *  The faces of a door are groups of the Root's mesh (attachToRoot): two for
 *  it shut and two for it open; the door's own mesh takes over while it swings.
 *
 *  Layout: host (the mode, the record and the state are StageObject's, others
 *  read them there), EE offsets in the comments.
 */
#ifndef GAME_STAGEDOOR_H
#define GAME_STAGEDOOR_H

#include "game/mapobject.h"

// PC: off until the stage crash of 2026-11-05 is explained (open-items.md,
// game/playermotion.cpp fifth run): createMapObject makes plain objects and
// neither the motion lists nor the door natives are bound, as before this file.
constexpr bool kDoorsEnabled = false;

// StageObject::m_doorMode; the sheet's 初期状態 starts it.
enum StageDoorMode
{
    StageDoorMode_Closed  = 0,
    StageDoorMode_Opening = 1,
    StageDoorMode_Open    = 2,
    StageDoorMode_Closing = 3,
    StageDoorMode_Locked  = 4,
    StageDoorMode_Barred  = 5,  // shut, and the script only shows a notice
};

// The sheet's 鍵属性: from which side a key works. 0 none, 1 and 3 the front,
// 2 and 4 the back, 5 both.
constexpr int kDoorKeyBothSides = 5;

// The sheet's 窓状態.
enum StageDoorWindow
{
    StageDoorWindow_None   = 0,
    StageDoorWindow_Whole  = 1, // forcing the door breaks the glass
    StageDoorWindow_Broken = 2, // one can reach through
};

// What StageCharacter::doorActions answers (natives 0x11b, 0x11c, 0x11d, 0x12c).
enum CharacterDoorAction
{
    CharacterDoor_Open    = 0x01,
    CharacterDoor_Close   = 0x02,
    CharacterDoor_Unlock  = 0x04,
    CharacterDoor_Lock    = 0x08,
    CharacterDoor_Nothing = 0x10,
};

// CharacterAllows::doorHand and doorKey.
enum { DoorHand_None = 0, DoorHand_Open = 1, DoorHand_Close = 2 };
enum { DoorKey_None = 0, DoorKey_Lock = 1, DoorKey_Unlock = 2, DoorKey_Force = 3 };

// layout: host -- EE 0x2c0, offsets in the comments.
class StageDoor : public StageMapObject
{
public:
    StageDoor(int type, int number);                                     // FS2 0x00174940
    ~StageDoor() override {}                                             // FS2 0x00171a30

    // FS2 0x00171ac0
    int getKind() override { return StageObject_Door; }

    // The motions, the mode of the sheet, and where one stands to use it.
    void setMotionList(void *list, int arg, int arg2) override;          // FS2 0x001747b0
    void sideHook1() override;                                           // FS2 0x00174390
    void sideHook2() override;                                           // FS2 0x001741c0
    void sideHook3() override;                                           // FS2 0x00174180

    int  getProperty(int id, int sub) override;                          // FS2 0x00172d80
    void setProperty(int id, int sub, int value) override;               // FS2 0x00172620
    bool scriptIsFacing(StageObject *other) override;                    // FS2 0x001723c0
    // FS2 0x001722e0
    StageDoor *getScriptDoor() override { return this; }

    // Snaps to mode: the motion's end pose for a shut or open door, the
    // light's fade for a moving one, then the faces.
    void setMode(int mode);                                              // FS2 0x00173fb0
    // What the S2D shows of the door from here on (m_doorState).
    void noteUse(bool on);                                               // FS2 0x00172530
    // Shut, locked or barred.
    bool isShut() const;                                                 // FS2 0x001725e0
    // Within reach of either side's point.
    bool isWithinReach(const float *point) const;                        // FS2 0x00172480
    // One can reach the lock through: the sheet's 2, 4 and 5.
    bool canReachThrough() const;                                        // FS2 0x00173830
    // Nobody is at it and it is not moving.
    bool isFree() const;                                                 // FS2 0x001739b0
    // The key works from where point is.
    bool keyWorksFrom(const float *point) const;                         // FS2 0x001739f0
    // point is on the front.
    bool isInFront(const float *point) const;                            // FS2 0x00173c30
    // Where who stands to use it, and how it faces.
    void getUsePlace(sceVu0FVECTOR place, sceVu0FVECTOR angles, const StageObject *who) const; // FS2 0x00173b30
    // Its sound of that name (DR_%02d_...), in buffer.
    const char *soundName(char *buffer, const char *format) const;       // 0x1736e0, 0x173750, 0x1737c0 and inline

    int              m_hp = 0;       // FS2 +0x29c: ドアHP
    int              m_keyKind = 0;  // FS2 +0x2a0: 鍵属性
    int              m_window = 0;   // FS2 +0x2a4: StageDoorWindow
    StageCharacter  *m_user = nullptr; // FS2 +0x288: who has the motion on it

private:
    // How far the turn from the front's yaw to point is, as addAngle wraps it.
    float sideOf(const float *point) const;

    sceVu0FVECTOR m_pivot = {};      // FS2 +0x250: where one stands
    sceVu0FVECTOR m_sideA = {};      // FS2 +0x260: the two reach points
    sceVu0FVECTOR m_sideB = {};      // FS2 +0x270
    float  m_yaw = 0.0f;             // FS2 +0x280: the front
    float  m_yawBack = 0.0f;         // FS2 +0x284
    float  m_lightTime = 0.0f;       // FS2 +0x290: how long the light takes
    float  m_lightElapsed = 0.0f;    // FS2 +0x294
};

#endif // GAME_STAGEDOOR_H
