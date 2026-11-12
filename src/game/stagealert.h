/*
 *  game/stagealert.h -- the ALERT rumble and the red flash of a stage.
 *
 *  StageScene +0xf4, a stage's only. A script names who it shows every frame
 *  it wants the alarm (natives 0xc7, 0xc8); the rumble ALERT plays as long as
 *  a frame goes by with a request, and stops on the first one without.
 *  The flash is one fade, a foe's (CharacterBrain, with the option on flash)
 *  or a cry's (native 0x13e, with the cue and the rumble CAUTION).
 *  The file name is the port's: no source string (architecture.md).
 */
#ifndef GAME_STAGEALERT_H
#define GAME_STAGEALERT_H

#include "sdk/scetypes.h"

class StageFade;
class StageCharacter;
class StageObject;
struct sceVif1Packet;
class TexAreaList;

enum StageAlertState
{
    StageAlert_Idle    = 0,
    StageAlert_Start   = 1,     // passed through in the frame that starts it
    StageAlert_Running = 2,
    StageAlert_Ending  = 3,     // the rumble is off, waits to go idle
};

// layout: host (EE 0x1c).
class StageAlert
{
public:
    StageAlert();                                                        // FS2 0x00264070
    ~StageAlert();                                                       // FS2 0x00264000: deleting
    // Empty in the ROM; StageFileEvent::queueFiles calls it first.
    void beforeFileEvent();                                              // FS2 0x00263b90
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x00263ba0
    // A frame of the state machine; true while the alarm runs.
    bool update();                                                       // FS2 0x00263c10
    // The screen's flash of a foe; answers true.
    bool flash();                                                        // FS2 0x00263ed0
    // The flash for who, with its cue and the CAUTION rumble (native 0x13e).
    bool cry(StageCharacter *who);                                       // FS2 0x00263f10
    // Asks for the alarm this frame, with the option on (natives 0xc7, 0xc8).
    // Whoever it already shows keeps it against another character.
    void show(StageObject *who);                                         // FS2 0x00263f80

private:
    int          m_state  = StageAlert_Idle; // FS2 +0x00: StageAlertState
    u_int        m_frames = 0;       // FS2 +0x04: in this state
    u_int        m_count  = 0;       // FS2 +0x08: frames run, then their time
    StageObject *m_wanted = nullptr; // FS2 +0x0c: this frame's request
    StageObject *m_shown  = nullptr; // FS2 +0x10: who started it
    StageFade   *m_fade;             // FS2 +0x14
    int          m_18     = 0;       // FS2 +0x18: the ctor clears it, nothing reads it
};

#endif
