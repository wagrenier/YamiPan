/* ==========================================================================
 *  game/stageparty.h
 *
 *  The sight-jack: StageParty (StageScene::party) holds one record of 0x148
 *  bytes, SightJack here, and forwards to it. While the player holds the
 *  jack, the "camjack" camera looks through the eyes of a character near the
 *  line the left stick points along: the stick tunes, the bind buttons keep
 *  a character and call it back. The same record gives the game over's view
 *  (startGameOverView), the foe's warning (warnAttacker) and a fixed vision
 *  of the past (setVisionTarget, the sound KAKOSI).
 *
 *  Names of the class, its modes and its methods are the port's, read from
 *  the code and the sounds JAK_IN, JAK_MODE, JAK_NOISE, JAK_TUNING.
 *
 *  Not written yet, in this file's range: the frame of stage 0x1b's boss.
 * ======================================================================== */
#ifndef _GAME_STAGEPARTY_H
#define _GAME_STAGEPARTY_H

#include <vector>

#include "game/gamehooks.h"

// SightJack::m_mode.
enum JackMode
{
    JackMode_Unset   = 0,   // the record's first frame goes to None
    JackMode_Tuned   = 1,   // the stick is on m_tuned
    JackMode_None    = 2,   // nobody: noise
    JackMode_KeptTurn = 3,  // m_kept, with the stick pushed
    JackMode_Kept    = 4,   // m_kept
    JackMode_KeptLost = 5,  // m_kept out of reach with the stick still pushed
};

// SightJack::m_holdState: a shoulder button held down.
enum JackHold
{
    JackHold_None     = 0,
    JackHold_Counting = 1,  // frames to kJackHoldFrames: a tap
    JackHold_Held     = 2,  // past it, or used
};

// The shoulder buttons of the frame, m_heldButtons: a tap of one steps
// through the foes.
enum JackButton
{
    JackButton_Nearer     = 2,  // PadAction_Zoom
    JackButton_NearestFoe = 4,  // PadAction_Strafe
    JackButton_Farther    = 8,  // PadAction_Ready
};

// JackBinds.
enum { kJackBinds = 4, kJackNoBind = kJackBinds, kJackVisionObjects = 10 };

// "camjack" (kind 11, vtable 0x311ac0, EE 0x340; the kind 8 class under it,
// vtable 0x311d30, adds m_linked).
// layout: host
class JackCamera : public StageCamera
{
public:
    JackCamera();                                                        // inline in StageParty::createCamera
    int getKind() override { return StageObject_Kind11; }                // FS2 vtable +0x1f4
    // The base's, then a kind 0xf target hands over its rotation.
    void onViewTop() override;                                           // FS2 0x00178030
    // The base's cameraStep (0x1775e0) and onLinkChanged (0x178020) are empty.
    void cameraStep() override {}
    void onLinkChanged() override {}
};

// The fill under the jack's picture. Its level is how much noise shows.
// layout: host -- EE 0x2c, vtable 0x316030.
class JackFill : public ScreenFill
{
public:
    JackFill(u_int rgba, int x, int y, int w, int h);                    // FS2 0x002082e0
    // The grain in the fill's colour, then in black, as thick as m_level.
    void draw(sceVif1Packet *packet, TexAreaList *texList) override;     // FS2 0x002081b0
    float m_level = 0.0f;       // FS2 +0x24: 0 clear, 1 noise
    u_int m_rgba;               // FS2 +0x28
};

// The film over a character's eyes (DemoCamera shows it too, EE 0x60). Its
// frames are pictures of grain: four of them, mirrored into the four corners
// of the frame, each corner fading from one random frame to the next; where
// they are bright enough, the colour lies over the picture, the more of them
// the further away the character is.
// layout: host
enum { kJackFilmCorners = 4 };
struct JackFilm
{
    struct Corner
    {
        float progress = 1.0f;  // FS2 +0x0: at 1 the next frame is picked
        float step = 0.0f;      // FS2 +0x4: of progress, each draw
        int   shown = -1;       // FS2 +0x8
        int   leaving = -1;     // FS2 +0xc: fades out over shown
    };

    ~JackFilm();                                                         // FS2 0x002052d0
    JackFilm *clone() const;                                             // FS2 0x00204250
    // level 0..1: how much of the frames covers the picture.
    void draw(float level, sceVif1Packet *packet);                       // FS2 0x00204550

    std::vector<Tim2Texture *> frames;  // FS2 +0x54: size +0x58, data +0x5c
    u_int  band = 0x100;        // FS2 +0x60: brightnesses from clear to solid
    u_int  color = 0;           // FS2 +0x64: RGB
    float  levelLow = 0.2f;     // FS2 +0x68: of the brightness, at level 0
    float  levelHigh = 0.84f;   // FS2 +0x6c: at level 1
    Corner corners[kJackFilmCorners]; // FS2 +0x70
    u_int  seed = 0x44444444;   // FS2 +0xb0: demoFleckRandNext's
};

// layout: host -- EE 0x148, offsets in the comments.
class SightJack
{
public:
    SightJack();                                                         // FS2 0x00203d70
    ~SightJack();                                                        // FS2 0x00203be0

    // The saved ids back to characters on the binds.
    void restoreBinds();                                                 // FS2 0x001fed30
    // The old vision's character leaves the view and the binds.
    void setVisionTarget(StageObject *object);                           // FS2 0x001fef50
    // Everything back to no view, the bindings too.
    void reset();                                                        // FS2 0x00203ae0
    void start(StageObject *viewer);                                     // FS2 0x002039b0
    void stop();                                                         // FS2 0x00203840
    void update();                                                       // FS2 0x00203240
    void draw();                                                         // FS2 0x002024c0
    // Nobody to look through: black under the grain at its thickest.
    void drawNobody(float filmLevel);                                    // FS2 0x00201a40
    // A corpse: black under the grain as it stands.
    void drawCorpse(float filmLevel);                                    // FS2 0x002016d0
    // A live character's eyes, seen through the grain.
    void drawLive(float filmLevel);                                      // FS2 0x00201bc0
    // The film and the flecks of the frame's list; four times in the ROM.
    void drawFilm(float filmLevel, sceVif1Packet *packet);

    // The frame through a ramp of 256 colours, by its brightness.
    void drawPaletteView(sceVif1Packet *packet, const u_int *ramp);      // FS2 0x001ff010
    // A bind's side of the frame lights.
    void drawBindFlash(sceVif1Packet *packet) const;                     // FS2 0x001ffcd0
    // The viewer's cross and the partner's. Inline three times in the ROM.
    void drawOwnMarkers(sceVif1Packet *packet);
    // The crosses of those who have found a foe.
    void drawFoeMarkers(sceVif1Packet *packet);                          // FS2 0x00201520
    // Off the jack, the kept one's cross fades.
    void drawFadingMarker();                                             // FS2 0x00201380
    // What a foe's attack lays over the frame.
    void drawWarning(float filmLevel);                                   // FS2 0x00201840

    // Whom the view is on: m_kept, m_tuned or nobody, by the mode. Inline
    // everywhere in the ROM.
    StageObject *viewed() const;                                         // FS2 0x001fea50

    // The three shoulder buttons of the frame.
    bool anyShoulderHeld() const;                                        // FS2 0x00200400
    // Keeps object on the bind button pressed now; no button, no.
    bool keep(StageObject *object);                                      // FS2 0x002004c0
    // The tap of JackButton_NearestFoe: the nearest foe not down is kept.
    StageObject *keepNearestFoe(int buttons);                            // FS2 0x00200a50
    // The tap of the other two: the next alerted foe farther off or nearer
    // than the kept one, round to the nearest or the farthest.
    StageObject *stepAlertedFoe(int buttons);                            // FS2 0x00200580
    // object may be looked through from here.
    bool canView(StageObject *object) const;                             // FS2 0x00200d10
    // The bound character of the button pressed now becomes the kept one.
    bool recall();                                                       // FS2 0x00200fe0
    // The stick tunes among the viewable characters.
    bool tune();                                                         // FS2 0x00201080
    // A viewable character as the viewer sees it. EE 0xc. layout: host
    struct Bearing
    {
        float        angle;     // +0x0: atan2 from the viewer; pickBearing: off the stick
        float        distance;  // +0x4
        StageObject *object;    // +0x8
    };
    // Every character canView lets through. Inline three times in the ROM.
    std::vector<Bearing> gatherBearings() const;
    // Of the bearings, the nearest to the stick's line within m_angle.
    Bearing pickBearing(const std::vector<Bearing> &bearings) const;     // FS2 0x001fff00
    // The camera to object's eyes (a character's) or its matrix.
    void aimAt(StageObject *object);                                     // FS2 0x00200150
    // The hold's state machine; not 0 while a button counts.
    bool pollButtons();                                                  // FS2 0x00202c70
    // The mode for the frame.
    void stepMode();                                                     // FS2 0x00202f50
    // A vision: the others hidden, its lists' objects on, the viewer off.
    void beginVision();                                                  // FS2 0x001fede0
    // What beginVision did, undone. Inline five times in the ROM.
    void endVision();

    bool stickAtRest() const;   // inline in the ROM

    u_char       m_running = 0;         // FS2 +0x00
    float        m_range = 10000.0f;    // FS2 +0x04
    float        m_angle = 0.34906585f; // FS2 +0x08: radians: 20 degrees
    JackCamera  *m_camera = nullptr;    // FS2 +0x0c
    StageObject *m_viewer = nullptr;    // FS2 +0x10: the scene's player each frame
    StageObject *m_tuned = nullptr;     // FS2 +0x14
    StageObject *m_kept = nullptr;      // FS2 +0x18
    StageObject *m_binds[kJackBinds] = {}; // FS2 +0x1c
    StageCharacter *m_warner = nullptr; // FS2 +0x2c: the foe of warnAttacker
    float        m_warnEnd = 0.0f;      // FS2 +0x30: seconds
    u_char       m_warnPushed = 0;      // FS2 +0x34: warnAttacker pushed the view
    StageObject *m_gameOverOn = nullptr; // FS2 +0x38
    JackFill    *m_fill = nullptr;      // FS2 +0x3c
    Tim2Texture *m_fleckTexture = nullptr; // FS2 +0x40
    void        *m_flecks = nullptr;    // FS2 +0x44
    int          m_screenLevel = 0xfa;  // FS2 +0x48: FUN_002cfb20's
    float        m_filmNear = 400.0f;   // FS2 +0x4c: the film starts past it
    float        m_filmFar = 5500.0f;   // FS2 +0x50: full past it
    JackFilm     m_film;                // FS2 +0x54
    int          m_mode = JackMode_None; // FS2 +0xb8
    int          m_holdState = JackHold_None; // FS2 +0xbc
    int          m_holdFrames = 0;      // FS2 +0xc0
    int          m_heldButtons = 0;     // FS2 +0xc4: JackButton bits
    int          m_markerFrames = 0;    // FS2 +0xc8: 0x201380: the kept one's marker fades over them
    int          m_bindShown = 0;       // FS2 +0xcc
    int          m_bindFlash = 0;       // FS2 +0xd0
    int          m_flashFrames = 0;     // FS2 +0xd4
    u_char       m_noisePlaying = 0;    // FS2 +0xd8
    u_char       m_viewChanged = 0;     // FS2 +0xd9
    int          m_visionFade = 0;      // FS2 +0xdc: 8 at a vision's start
    int          m_visionFrames = 0;    // FS2 +0xe0
    StageObject *m_vision = nullptr;    // FS2 +0xe4
    StageObject *m_visionShow[kJackVisionObjects] = {}; // FS2 +0xe8
    StageObject *m_visionHide[kJackVisionObjects] = {}; // FS2 +0x110
    int          m_visionShowCount = 0; // FS2 +0x138
    int          m_visionHideCount = 0; // FS2 +0x13c
    u_char       m_visionOn = 0;        // FS2 +0x140
    u_int        m_startTime = 0;       // FS2 +0x144: playTime at start
};

// The jack's passes over the frame, and its cross on a character's head.
void gsFrameScale(sceVif1Packet *packet, int level);                     // FS2 0x002cfb20
void gsFrameContrast(sceVif1Packet *packet, u_int rgba);                 // FS2 0x002cf880
void gsFrameAdd(sceVif1Packet *packet, u_int rgba);                      // FS2 0x002058e0
void jackMarker(sceVif1Packet *packet, StageObject *object, StageObject *view, u_int rgba); // FS2 0x00205f50

#endif // _GAME_STAGEPARTY_H
