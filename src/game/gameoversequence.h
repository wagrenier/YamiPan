/* ==========================================================================
 *  game/gameoversequence.h
 *
 *  What StageScene runs when a character falls for good, and the colour
 *  ramp of the stage's letterbox (0x2b1670..0x2b22f0). The file name is the
 *  port's: no source string. GameOverSequence (+0x118 of the scene) shows the
 *  fall from "CAM_DEAD" (DeathCamera), then what felled it through the party's
 *  view, then fades out; Start skips to the fade. SceneDirector (+0x120) ramps
 *  the bars' colour while the mission asks for them.
 * ======================================================================== */
#ifndef _GAME_GAMEOVERSEQUENCE_H
#define _GAME_GAMEOVERSEQUENCE_H

#include "game/gamehooks.h"

// GameOverSequence::m_state. StageScene::isGameOver waits for the last.
enum GameOverState
{
    GameOverState_Idle    = 0,  // nothing sets it: update stays here
    GameOverState_Start   = 1,  // the ctor's
    GameOverState_Fall    = 2,  // from CAM_DEAD
    GameOverState_Culprit = 3,  // through the party's view
    GameOverState_Fading  = 4,
    GameOverState_Done    = 5,  // read only: state 4 and up with the fade full in mode 2
};
constexpr int kGameOverSequenceDone = GameOverState_Done;

// layout: host -- EE 0x18.
struct GameOverSequence
{
    // Keeps who fell and what felled it.
    GameOverSequence(StageObject *victim, StageObject *cause);           // FS2 0x002b1cb0
    int getState();                                                      // FS2 0x002b1c50
    void update();                                                       // FS2 0x002b1670

    void setState(int state);                                            // FS2 0x002b1940

    int m_state = GameOverState_Start; // FS2 +0x00
    int m_timer = 0;            // FS2 +0x04: frames left in states 2, 3
    u_char m_skipped = 0;       // FS2 +0x08: the fade was started early (Start, or no victim)
    u_char m_viewTaken = 0;     // FS2 +0x09: state 3 was entered; nothing reads it
    StageObject *m_victim;      // FS2 +0x0c
    StageObject *m_cause;       // FS2 +0x10: may be null or the victim
    StageObject *m_camera = nullptr; // FS2 +0x14: the DeathCamera of state 2

private:
    // Whose eyes state 3 shows; null none. Inline three times in setState.
    StageObject *pickViewTarget();
    bool isFadeFull();
};

// "CAM_DEAD" (kind 12, vtable 0x318cf0, EE 0x380). layout: host; the kind 8
// class under it adds m_linked (+0x300). Swings half a turn round the linked
// character over kDeathCameraSwingSteps steps, easing in and out.
class DeathCamera : public StageCamera
{
public:
    // FS2 0x002b1db0
    int getKind() override { return StageObject_Kind12; }
    DeathCamera();                                                       // FS2 0x002b1fe0
    // FS2 0x002b1ce0: deleting; 0x002b2090 the runner's thunk.
    ~DeathCamera() override = default;
    void cameraStep() override;                                          // FS2 0x002b1dc0
    // FS2 0x002b1fd0
    void onLinkChanged() override { resetShot(); }
    // The step after the action's too (FS2 0x002b2080, the ROM's thunk falls
    // into cameraStep).
    void sideHook3() override { cameraStep(); }

    // The shot from its start. A slot of this class alone (+0x264).
    void resetShot();                                                    // FS2 0x002b1f50

    sceVu0FVECTOR m_eyeOffset;  // FS2 +0x340: in the linked character's space, before the turn
    sceVu0FVECTOR m_focusOffset; // FS2 +0x350: from the character's place, in the world
    float m_angle;              // FS2 +0x360: the turn round z, radians
    float m_angleTotal;         // FS2 +0x364: pi; nothing reads it
    float m_swingLeft;          // FS2 +0x368: steps
    float m_swingTotal;         // FS2 +0x36c
    float m_swingSpeed;         // FS2 +0x370: radians per step
    float m_swingAccel;         // FS2 +0x374: radians per step per step
};

// The stage's bars and a sub-scene's: over and under the 4:3 picture's middle,
// a share of the frame and so of the display at any aspect.
void drawLetterbox(sceVif1Packet *packet, u_int rgba);

// +0x120 (StageScene::update deletes it too). layout: host -- EE 0x14.
struct SceneDirector
{
    SceneDirector();                                                     // FS2 0x002b22d0
    ~SceneDirector();                                                    // FS2 0x002b2280: deleting
    // A colour ramp still has frames to go.
    bool isRunning();                                                    // FS2 0x002b2270
    // From colour from to colour to over frames frames.
    void start(int frames, u_int from, u_int to);                        // FS2 0x002b2250
    // Ramps the colour and draws the letterbox in it.
    void draw(sceVif1Packet *packet);                                    // FS2 0x002b20a0

    u_int colour;               // FS2 +0x00: the ramp's colour now
    u_int m_from;               // FS2 +0x04
    u_int m_to;                 // FS2 +0x08
    int   m_frame;              // FS2 +0x0c
    int   m_frames;             // FS2 +0x10: 0 no ramp
};

#endif // _GAME_GAMEOVERSEQUENCE_H
