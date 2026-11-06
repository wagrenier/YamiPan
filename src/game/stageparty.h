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
 *  Not written yet, in this file's range: the draw's overlays, the rest of
 *  0x1fed30..0x2024c0: they need the host's jack pass.
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
    float m_level = 0.0f;       // FS2 +0x24: 0 clear, 1 noise
    u_int m_rgba;               // FS2 +0x28
};

// The film's state, what demoFilmDraw is given (a pointer to +0x54 of the
// record). layout: host
struct JackFilm
{
    struct Scratch
    {
        float a = 1.0f;
        int   b = 0;
        int   c = -1;
        int   d = -1;
    };
    std::vector<Tim2Texture *> frames;  // FS2 +0x54: size +0x58, data +0x5c
    u_int  rate = 0x100;        // FS2 +0x60
    int    _64 = 0;             // FS2 +0x64
    float  _68 = 0.2f;          // FS2 +0x68
    float  _6c = 0.84f;         // FS2 +0x6c
    Scratch first;              // FS2 +0x70
    Scratch rest[3];            // FS2 +0x80
    u_int  _b0 = 0x44444444;    // FS2 +0xb0
};

// layout: host -- EE 0x148, offsets in the comments.
class SightJack
{
public:
    SightJack();                                                         // FS2 0x00203d70
    ~SightJack();                                                        // FS2 0x00203be0

    // Everything back to no view, the bindings too.
    void reset();                                                        // FS2 0x00203ae0
    void start(StageObject *viewer);                                     // FS2 0x002039b0
    void stop();                                                         // FS2 0x00203840
    void update();                                                       // FS2 0x00203240
    void draw();                                                         // FS2 0x002024c0

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
    int          m_uiColor = 0;         // FS2 +0xc8: the draw's, four bytes
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

#endif // _GAME_STAGEPARTY_H
