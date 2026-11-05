/* ==========================================================================
 *  game/objectproperty.h
 *
 *  The scripts' properties of an object (ScriptOp_StoreVar and its load, with
 *  ScriptVar_Property): the numbers both StageObject::setProperty and
 *  SubSceneModelObject's getProperty go by.
 * ======================================================================== */
#ifndef _GAME_OBJECTPROPERTY_H
#define _GAME_OBJECTPROPERTY_H

// What getProperty and setProperty take. The numbers are the scripts'; the
// names are read from SubSceneModelObject's two (ScriptRunner::getProperty's
// own list is unread).
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
    Prop_ResultPaused   = 0x6d,
    Prop_ResultHold     = 0x6e,
    Prop_ClockFrames    = 0x6f,
    Prop_ScoreClock     = 0x70,
    Prop_ViewActive     = 0x72,
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
