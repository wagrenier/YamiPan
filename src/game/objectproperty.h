/* ==========================================================================
 *  game/objectproperty.h
 *
 *  The scripts' properties of an object (ScriptOp_StoreVar and its load, with
 *  ScriptVar_Property): the numbers StageObject's and StageCharacter's two
 *  go by.
 * ======================================================================== */
#ifndef _GAME_OBJECTPROPERTY_H
#define _GAME_OBJECTPROPERTY_H

// What getProperty and setProperty take. The numbers are the scripts'; the
// names are read from StageObject's two (ScriptRunner::getProperty's own list
// is unread).
enum ObjectProperty
{
    Prop_StageCleared   = 0x00,     // sub: stage - 1; 0 no, 1 variant 0, 2 both
    Prop_Archive        = 0x01,     // sub: id - 1
    Prop_Random100      = 0x02,
    Prop_YawDegrees     = 0x03,
    Prop_MissionFrames  = 0x39,
    Prop_Frames30       = 0x3a,     // the object's clock in 30ths
    Prop_Fps            = 0x3b,
    Prop_Result         = 0x3c,
    Prop_ScriptVar3e    = 0x3e,
    Prop_ScriptVar3f    = 0x3f,
    Prop_Group          = 0x40,     // always 4
    Prop_FrameStep      = 0x4d,
    Prop_SwitchedOn     = 0x4f,
    Prop_PlayerId       = 0x50,
    Prop_PartnerId      = 0x51,
    Prop_ActorId        = 0x52,     // who acts for the player, or the player
    Prop_Handle         = 0x54,
    Prop_Restore        = 0x55,
    Prop_ShowFlag       = 0x56,     // sub: an index of kShowFlagByte
    Prop_Shown          = 0x59,
    Prop_KeyValue       = 0x6b,     // a kind 6 object (Seven_Keys) only: StageMapObject
    Prop_ObjectType     = 0x6c,     // the sheet's type of a character or an item, else 0
    Prop_ResultPaused   = 0x6d,
    Prop_ResultHold     = 0x6e,
    Prop_ClockFrames    = 0x6f,
    Prop_ScoreClock     = 0x70,
    Prop_ViewActive     = 0x72,
};

// What a character adds (StageCharacter::getProperty, setProperty; the map
// object and the door have lists of their own, stagedoor.cpp). Sheet: the
// field of StageObjRecord, read only. Max: the sheet's limit of the value
// before it, read only. Percent: stored as value / 100, Thirtieths: value / 30.
enum CharacterProperty
{
    CharProp_Id             = 0x04,
    CharProp_SheetKind      = 0x05,
    CharProp_SheetSeType    = 0x06,
    CharProp_WieldSkills    = 0x07,     // CharacterWieldSkill bits
    CharProp_Sides          = 0x08,     // StageObjSide bits; a store takes 0..15
    CharProp_BrainState     = 0x09,
    CharProp_SheetAiType    = 0x0a,
    CharProp_AiLevel        = 0x0b,
    CharProp_LongWeaponKind = 0x0c,     // read only: CharacterWeaponKind
    CharProp_ShortWeaponKind = 0x0d,    // read only
    CharProp_Sight          = 0x0e,
    CharProp_Hearing        = 0x0f,     // Percent
    CharProp_Health         = 0x10,
    CharProp_HealthMax      = 0x11,
    CharProp_Endurance      = 0x12,
    CharProp_EnduranceMax   = 0x13,
    CharProp_LeftArm        = 0x14,
    CharProp_LeftArmMax     = 0x15,
    CharProp_RightArm       = 0x16,
    CharProp_RightArmMax    = 0x17,
    CharProp_RightLeg       = 0x18,
    CharProp_RightLegMax    = 0x19,
    CharProp_LeftLeg        = 0x1a,
    CharProp_LeftLegMax     = 0x1b,
    CharProp_Recovery       = 0x1c,
    CharProp_LightResist    = 0x1d,     // a store takes 0..2
    CharProp_LightDamage    = 0x1e,
    CharProp_LightFlinch    = 0x1f,
    CharProp_UnarmedAttack  = 0x20,
    CharProp_Bite           = 0x21,
    CharProp_AttackReach    = 0x22,     // read only
    CharProp_WalkRate       = 0x23,     // Percent
    CharProp_RunRate        = 0x24,     // Percent
    CharProp_TurnRate       = 0x25,     // Percent
    CharProp_ReviveTime     = 0x26,     // Thirtieths
    CharProp_ControlResist  = 0x27,
    CharProp_TrapResist     = 0x28,     // Thirtieths
    CharProp_Shot           = 0x29,     // 999 blanks, else Percent the scatter
    CharProp_ShotInterval   = 0x2a,     // Thirtieths
    CharProp_ReloadRate     = 0x2b,     // Percent
    CharProp_SheetStepLevel = 0x2c,
    CharProp_SheetLadder    = 0x2d,
    CharProp_WeaponShown    = 0x2e,     // a store can only show it
    CharProp_WeaponRounds   = 0x2f,
    CharProp_WeaponMagazine = 0x30,     // read only
    CharProp_CarriedAmmo    = 0x31,
    CharProp_BiteTargetId   = 0x32,     // read only
    CharProp_RidingWithId   = 0x33,     // read only
    CharProp_Value5b4       = 0x34,
    CharProp_Flag5b8        = 0x35,
    CharProp_BiteProof      = 0x36,
    CharProp_Flag5ba        = 0x37,
    CharProp_Value62c       = 0x38,     // a store takes 0..9
    CharProp_WieldLight     = 0x41,     // read only: a CharacterWieldSkill bit
    CharProp_WieldHeavy     = 0x42,
    CharProp_WieldShortGun  = 0x43,
    CharProp_WieldRifle     = 0x44,
    CharProp_SidePlayer     = 0x45,     // the four StageObjSide bits, one by one
    CharProp_SideNpc        = 0x46,
    CharProp_SideCorpse     = 0x47,
    CharProp_SideDark       = 0x48,
    CharProp_ScriptValue    = 0x49,     // sub: 0..10 of m_scriptValues
    CharProp_Vector0        = 0x4a,     // x, y, z of m_scriptPoint
    CharProp_Vector1        = 0x4b,
    CharProp_Vector2        = 0x4c,
    CharProp_ReviveFrames   = 0x4e,
    CharProp_Value670       = 0x57,     // read only
    CharProp_Flag3d8        = 0x58,
    CharProp_Answer3a       = 0x5a,     // read only: always 3
    CharProp_Answer3b       = 0x5b,
    CharProp_SheetDecoyResist = 0x60,
    CharProp_Fidgets        = 0x61,
    CharProp_AttackInterval = 0x62,     // Thirtieths
    CharProp_BiteInterval   = 0x63,     // Thirtieths
    CharProp_ShotScale      = 0x65,
    CharProp_MeleeScale     = 0x66,
    CharProp_Value6c8       = 0x67,
    CharProp_Value6cc       = 0x68,
    CharProp_Value6d0       = 0x69,
    CharProp_Value6d4       = 0x6a,
    CharProp_MotionFlagged  = 0x71,     // read only
    CharProp_Flag44c        = 0x73,
};

// CharProp_WieldSkills: what the sheet's columns 軽 重 短 ラ say it can wield.
enum CharacterWieldSkill
{
    CharacterWieldSkill_Light    = 0x1,
    CharacterWieldSkill_Heavy    = 0x2,
    CharacterWieldSkill_ShortGun = 0x4,
    CharacterWieldSkill_Rifle    = 0x8,
};

// Prop_ShowFlag's numbers: the byte of UiShowFlags each one is. The set
// stores "value != 1" there and the get answers the byte's inverse, so 1
// means shown, except 7, which is the other way round.
constexpr int kShowFlagCount = 17;
constexpr signed char kShowFlagByte[kShowFlagCount] = {
    0, 1, 2, 3, 4, 9, 5, 10, 6, 7, 13, 12, 11, 14, 8, 15, 16,
};
constexpr int kShowFlagInverted  = 7;   // the byte holds the value as it is
constexpr int kShowFlagWithFirst = 6;   // read: the byte and'ed with byte 0

constexpr int kArchiveVariantUnlock = 0x40;     // the one id with rules of its own
constexpr int kArchiveWanted   = 1;
constexpr int kArchiveHeld     = 2;
constexpr int kArchiveReported = 3;             // read only
constexpr int kArchiveStates   = 3;

#endif // _GAME_OBJECTPROPERTY_H
