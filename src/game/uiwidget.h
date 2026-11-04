/* ==========================================================================
 *  game/uiwidget.h
 *
 *  The S2D script widgets (game/uiwidget.cpp, 0x25b8e0..0x25fa50): the
 *  "prg" types a sequence's script creates by name (S2dWidgetDef::type,
 *  looked up in the class table 0x2facf0), UiWidget::create that builds
 *  them, and the classes behind them -- the map's rotate / zoom / pan
 *  (PrgRot and the widgets that borrow its matrix), the scroll and message
 *  widgets that drive a text track, the movie and the streamed sound with
 *  their subtitles, the layout's return value. Types without a class act
 *  once in create (show / hide a text, reset the options, the map's
 *  ordered moves on another widget).
 *
 *  Each class is a UiWidget (game/uisequence.h; EE vtable after {type_info,
 *  0}: dtor, draw, advance, refresh, getStatus, vptr at +0x1c); the fields
 *  follow at +0x20. The parameters each type reads from S2dWidgetDef::param
 *  are listed with it (p0..p7, floats; an index is a float too).
 *
 *  Also here, as CodeWarrior emitted them after the file's code: UiTween
 *  (an inline class of 30+ callers) and UiFont::countLines.
 *  The class names follow the type names; the file name is the port's.
 * ======================================================================== */
#ifndef _GAME_UIWIDGET_H
#define _GAME_UIWIDGET_H

#include "game/s2ddata.h"
#include "game/uidrawer.h"
#include "game/uisequence.h"
#include "sdk/scetypes.h"

class Clock;                    // game/clock.h
class MoviePlayer;              // game/movieplayer.h
class SoundStream;              // sound/sound.h
class UiTextTrack;              // game/uitrack.h

// The widget types, in the order of the class table 0x2facf0 (its names).
enum PrgType
{
    PrgType_None = -1,                      // not in the table: create reserves a slot and does nothing
    PrgType_Rot = 0x00,                     // "prgRot": PrgRot
    PrgType_RotRev = 0x01,                  // "prgRotRev": PrgRot turned half round
    PrgType_RotEnd = 0x02,                  // "prgRotEnd": PrgRotEnd
    PrgType_ReturnFlag = 0x03,              // "prgReturnFlag": PrgReturnFlag
    PrgType_ReturnValue = 0x04,             // "prgReturnValue": PrgReturnValue
    PrgType_Msg = 0x05,                     // "prgMsg": PrgMsg, once
    PrgType_MsgOnOff = 0x06,                // "prgMsgOnOff": a text track shown / hidden
    PrgType_MsgAniX = 0x07,                 // "prgMsgAniX": PrgMsgAniX
    PrgType_MsgReference = 0x08,            // "prgMsgReference": PrgMsgReference, once
    PrgType_ChangeWeapon = 0x09,            // "prgChangeWeapon": stepWeaponPreview
    // The ordered moves of a PrgRot (this sequence's widget p0); the Call
    // forms name a sequence p0 and the widget p1, their other parameters
    // one later.
    PrgType_RotOrderScale = 0x0a,           // "prgRotOrderScale": scale p1 in p2 frames
    PrgType_RotOrderTrans = 0x0b,           // "prgRotOrderTrans": centre on (p1, p2) in p3 frames
    PrgType_RotOrderTransLabel = 0x0c,      // "prgRotOrderTransLabel": on the anchor of anim p1 in p2 frames
    PrgType_RotOrderTransInit = 0x0d,       // "prgRotOrderTransInit": back to its def's position in p1 frames
    PrgType_RotOrderRotateTarget = 0x0e,    // "prgRotOrderRotateTarget": angle 0 in p1 frames
    PrgType_RotOrderRotate = 0x0f,          // "prgRotOrderRotate": angle p1 in p2 frames
    PrgType_RotCall = 0x10,                 // "prgRotCall": PrgRotCall
    PrgType_RotCallOrderScale = 0x11,       // "prgRotCallOrderScale"
    PrgType_RotCallOrderTrans = 0x12,       // "prgRotCallOrderTrans"
    PrgType_RotCallOrderTransLabel = 0x13,  // "prgRotCallOrderTransLabel": the anim is this sequence's
    PrgType_RotCallOrderTransInit = 0x14,   // "prgRotCallOrderTransInit"
    PrgType_RotCallOrderRotateTarget = 0x15, // "prgRotCallOrderRotateTarget"
    PrgType_RotCallOrderRotate = 0x16,      // "prgRotCallOrderRotate"
    PrgType_TransMsg = 0x17,                // "prgTransMsg": PrgTransMsg
    PrgType_TransEnd = 0x18,                // "prgTransEnd": PrgTransEnd
    PrgType_Movie = 0x19,                   // "prgMovie": PrgMovie
    PrgType_SndStream = 0x1a,               // "prgSndStream": PrgSndStream
    PrgType_ScaleNavi = 0x1b,               // "prgScaleNavi": PrgScaleNavi
    PrgType_ScaleNaviScroll = 0x1c,         // "prgScaleNaviScroll": PrgScaleNaviScroll
    PrgType_McCard = 0x1d,                  // "prgMcCard": PrgMcCard
    PrgType_OptReset = 0x1e,                // "prgOptReset": the options to their defaults
    PrgType_KCReset = 0x1f,                 // "prgKCReset": the options' button map to its defaults
    PrgType_KCResetLocal = 0x20,            // "prgKCResetLocal": the draft button map to its defaults
    PrgType_KCCopyLocal = 0x21,             // "prgKCCopyLocal": the draft from the options
    PrgType_KCCopyMaster = 0x22,            // "prgKCCopyMaster": the draft into the options
    // The moves at a speed: the frames are the distance over p (in pixels
    // per frame, or of the map's scale when the next parameter is set).
    PrgType_RotOrderTransSpd = 0x23,        // "prgRotOrderTransSpd"
    PrgType_RotOrderTransLabelSpd = 0x24,   // "prgRotOrderTransLabelSpd"
    PrgType_RotOrderTransInitSpd = 0x25,    // "prgRotOrderTransInitSpd"
    PrgType_RotCallOrderTransSpd = 0x26,    // "prgRotCallOrderTransSpd"
    PrgType_RotCallOrderTransLabelSpd = 0x27, // "prgRotCallOrderTransLabelSpd"
    PrgType_RotCallOrderTransInitSpd = 0x28, // "prgRotCallOrderTransInitSpd"
    PrgType_Debug = 0x29,                   // "prgDebug": looks variables up, prints nothing in retail
    PrgType_Count = 0x2a,
};

// ──────────────────────────────────────────────────────────────────────
// UiTween: a value moving from one end to the other over a number of
// frames after a delay, then held (0x1c bytes; inline in the ROM, its
// out-of-line copies 0x25f5b0..0x25f990).

// UiTween::curve.
enum UiTweenCurve
{
    UiTweenCurve_Linear = 0,
    UiTweenCurve_EaseIn = 1,    // t squared
};

struct UiTween
{
    UiTween() : from(0.0f), to(0.0f), length(0), frame(0), delay(0), hold(0), curve(0) {} // FS2 0x0025f990

    // Frame 0 of a new move; answers isDone.
    bool  start(float from_, float to_, int length_, int delay_, int hold_, int curve_); // FS2 0x0025f860
    // A frame on (unless done); answers isDone.
    bool  step();                                                        // FS2 0x0025f6e0
    // Done once the frame passes delay + length + hold (length becomes 0).
    bool  isDone();                                                      // FS2 0x0025f8e0
    float value();                                                       // FS2 0x0025f750
    // 0..1 along the move; 1 once done.
    float progress();                                                    // FS2 0x0025f5b0

    float from;                 // FS2 +0x00
    float to;                   // FS2 +0x04
    int   length;               // FS2 +0x08: frames; 0 done
    int   frame;                // FS2 +0x0c
    int   delay;                // FS2 +0x10: frames before it moves
    int   hold;                 // FS2 +0x14: frames at the end before it is done
    int   curve;                // FS2 +0x18: UiTweenCurve
};
static_assert(sizeof(UiTween) == 0x1c, "UiTween: EE 0x1c");

// 17 flags, all set but [10] (0x25f9c0; one caller in another TU, 0x19c180,
// whose use of them is unread). The name is a guess.
struct UiShowFlags
{
    UiShowFlags();                                                       // FS2 0x0025f9c0

    u_char flag[17];            // FS2 +0x00
};
static_assert(sizeof(UiShowFlags) == 17, "UiShowFlags: EE 0x11");

// ──────────────────────────────────────────────────────────────────────
// The map: PrgRot rotates, zooms and pans the sequence's parent matrix
// (UiSequence::m_parentXform) the actors after it draw with; PrgRotCall
// lets another sequence draw with the same matrix, PrgRotEnd ends it.

// PrgRot::startTween's axis.
enum PrgRotAxis
{
    PrgRotAxis_Angle = 0,
    PrgRotAxis_Scale = 1,
    PrgRotAxis_X = 2,
    PrgRotAxis_Y = 3,
    PrgRotAxis_Count = 4,
};

// "prgRot" / "prgRotRev" (0x84 bytes, Vt_00316c10): p0 / p1 the position
// it starts on (negated), p2 / p3 the centre on screen, p4..p7 two corners
// of the area the position stays in (all 0: +-2047). With the map showing
// the player it starts on the player, facing their heading. Pad input
// moves it unless the arg is negative or an ordered move runs.
// layout: host (vptr; EE fields at the offsets noted)
class PrgRot : public UiWidget
{
public:
    PrgRot(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg, float startAngle); // FS2 0x0025e080
    ~PrgRot();                                                           // FS2 0x0025e020: deleting (obj, 1)
    void draw(UiDrawer *drawer);                                         // FS2 0x0025df40
    void advance();                                                      // FS2 0x0025d8a0
    void refresh(bool argChanged);                                       // FS2 0x0025dff0

    // Moves an axis to a value over frames (< 1: at once).
    void startTween(float to, int axis, int frames);                     // FS2 0x0025d580
    // Steps the moves; true while any ran this frame.
    bool stepTweens();                                                   // FS2 0x0025dcf0

    // The value of an axis. Inline in the ROM (startTween).
    float axisValue(int axis) const
    {
        if (axis == PrgRotAxis_Y)
            return m_pos[1];
        if (axis == PrgRotAxis_X)
            return m_pos[0];
        if (axis == PrgRotAxis_Scale)
            return m_scale;
        if (axis == PrgRotAxis_Angle)
            return m_angle;
        return 0.0f;
    }

    float     m_angle;          // FS2 +0x20: radians, kept in -pi..pi
    float     m_scale;          // FS2 +0x24: 0.6..1.8 by the pad, 0.8 at the start
    float     m_pos[2];         // FS2 +0x28: the point shown at the centre, negated
    float     m_center[2];      // FS2 +0x30: on screen (p2, p3)
    UiTween  *m_tweens[PrgRotAxis_Count]; // FS2 +0x38: new / delete, null none
    UiMatrix  m_drawXform;      // FS2 +0x48: the matrix draw made (PrgRotCall)
    float     m_drawCenter[2];  // FS2 +0x60: and its translation
    u_char    m_recentred;      // FS2 +0x68: L1 + R1 recentred it; rotating waits for both released
    float     m_min[2];         // FS2 +0x6c: m_pos's bounds
    float     m_max[2];         // FS2 +0x74
    u_char    m_inputOff;       // FS2 +0x7c: the arg is negative
    float     m_startAngle;     // FS2 +0x80: L1 + R1 turns back to it
};

// "prgRotEnd" (0x20, Vt_00316bd0): the actors after it draw unrotated, in
// pixels.
// layout: host
class PrgRotEnd : public UiWidget
{
public:
    // Inline in the ROM (create).
    PrgRotEnd(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg)
    {
    }
    ~PrgRotEnd();                                                        // FS2 0x0025d460: deleting (obj, 1)
    void draw(UiDrawer *drawer);                                         // FS2 0x0025d3f0
};

// "prgRotCall" (0x24, Vt_00316bf0): p0 a sequence, p1 its PrgRot widget,
// whose matrix the actors after this one draw with.
// layout: host
class PrgRotCall : public UiWidget
{
public:
    // Inline in the ROM (create).
    PrgRotCall(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg), m_target(nullptr)
    {
    }
    ~PrgRotCall();                                                       // FS2 0x0025d520: deleting (obj, 1)
    void draw(UiDrawer *drawer);                                         // FS2 0x0025d4c0

    PrgRot *m_target;           // FS2 +0x20: the ROM takes the widget to be one
};

// ──────────────────────────────────────────────────────────────────────
// Translations and scrolling.

// The base of the translating widgets (0x28, Vt_00316b10): the actors
// after it draw moved by m_translate.
// layout: host
class PrgTrans : public UiWidget
{
public:
    // Inline in the ROM (PrgTransMsg::PrgTransMsg).
    PrgTrans(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg)
    {
        m_translate[0] = 0.0f;
        m_translate[1] = 0.0f;
    }
    ~PrgTrans();                                                         // FS2 0x0025c9e0: deleting (obj, 1)
    void draw(UiDrawer *drawer);                                         // FS2 0x0025c9d0

    float m_translate[2];       // FS2 +0x20
};

// "prgTransMsg" (0x44, Vt_00316af0): scrolls the text track of the anim
// named p0 by whole lines with the pad's Up / Down: p1 the lines shown,
// p2 the line height, p3 the speed (pixels a frame; 4 when not above 0).
// The arg is the line it starts at, also the page the end is kept from
// (scrollEnd). The S2D's rMsgUpLimit / rMsgDownLimit / rMsgUseScroll ask
// it (game/uigame.cpp).
// layout: host
class PrgTransMsg : public PrgTrans
{
public:
    PrgTransMsg(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg); // FS2 0x0025c7f0
    ~PrgTransMsg();                                                      // FS2 0x0025c780: deleting (obj, 1)
    void advance();                                                      // FS2 0x0025c460
    void refresh(bool argChanged);                                       // FS2 0x0025c390

    bool canScroll();                                                    // FS2 0x0025c310: end != 0
    bool atBottom();                                                     // FS2 0x0025c340: end <= scroll
    bool atTop();                                                        // FS2 0x0025c360: scroll <= 0

    // How far the text can scroll: its lines past the ones shown, in
    // pixels, never negative. Inline in the ROM (the ctor, advance,
    // refresh).
    float scrollEnd();

    float        m_scroll;      // FS2 +0x28: pixels
    float        m_speed;       // FS2 +0x2c
    float        m_target;      // FS2 +0x30: where it eases to, a whole line
    float        m_scrollEnd;   // FS2 +0x34: 1000 until the track is found
    float        m_lineHeight;  // FS2 +0x38
    float        m_shownLines;  // FS2 +0x3c
    UiTextTrack *m_text;        // FS2 +0x40
};

// "prgTransEnd" (0x20, Vt_00316ad0): the actors after it draw unmoved.
// layout: host
class PrgTransEnd : public UiWidget
{
public:
    // Inline in the ROM (create).
    PrgTransEnd(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg)
    {
    }
    ~PrgTransEnd();                                                      // FS2 0x0025c2b0: deleting (obj, 1)
    void draw(UiDrawer *drawer);                                         // FS2 0x0025c280
};

// ──────────────────────────────────────────────────────────────────────
// Messages.

// "prgMsg" (0x30, Vt_00316b70), built, run once and deleted by create: the
// text track of the anim named p0 shows message p2 (a strings index; < 0
// none) of the fMsg image p1 at alignment (p3, p4), offset by the value of
// the S2D variable p5 (when > 0). With a non-zero arg the memory-card
// tokens in it (<MC00C> ..: prgMsgCardToken) become the saves' counts.
// layout: host
class PrgMsg : public UiWidget
{
public:
    // Inline in the ROM (create).
    PrgMsg(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg), m_message(-1), m_24(0), m_28(0),
          m_msgFile(data->getImage((int)def->param[1]))
    {
    }
    ~PrgMsg();                                                           // FS2 0x0025cdd0: deleting (obj, 1)
    void advance();                                                      // FS2 0x0025cdc0

    void setText(S2dWidgetDef *def);                                     // FS2 0x0025ce30

    int         m_message;      // FS2 +0x20: setMessageByName's answer; -1 none
    int         m_24;           // FS2 +0x24: 0, unread
    int         m_28;           // FS2 +0x28: 0, unread
    const void *m_msgFile;      // FS2 +0x2c: the fMsg image p1
};

// "prgMsgReference" (0x30, Vt_00316b30), built, run once and deleted by
// create: the text track of anim p0 (a strings index) shows the text
// variable named p2 (UiWidget::expandTextVariable) at alignment (p3, p4):
// as 16-bit text, or as the message of the name it gives in the fMsg
// image p1.
// layout: host
class PrgMsgReference : public UiWidget
{
public:
    // Inline in the ROM (create).
    PrgMsgReference(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg), m_message(-1), m_24(0), m_28(0), m_msgFile(nullptr)
    {
    }
    ~PrgMsgReference();                                                  // FS2 0x0025ca50: deleting (obj, 1)
    void advance();                                                      // FS2 0x0025ca40

    void setText(S2dWidgetDef *def);                                     // FS2 0x0025cab0

    int         m_message;      // FS2 +0x20: setMessageByName / setTextAt's answer
    int         m_24;           // FS2 +0x24: 0, unread
    int         m_28;           // FS2 +0x28: 0, unread
    const void *m_msgFile;      // FS2 +0x2c: the fMsg image p1, once used
};

// "prgMsgAniX" (0x30, Vt_00316b50): slides the x alignment of the text
// track of the anim named p0 from p1 to p2 over p3 frames: forwards while
// the arg is 0, back otherwise.
// layout: host
class PrgMsgAniX : public UiWidget
{
public:
    // Inline in the ROM (create).
    PrgMsgAniX(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg), m_from(def->param[1]), m_to(def->param[2]),
          m_length(def->param[3]), m_frame(0.0f)
    {
    }
    ~PrgMsgAniX();                                                       // FS2 0x0025cd60: deleting (obj, 1)
    void advance();                                                      // FS2 0x0025cc10

    void setTextX(float x, S2dWidgetDef *def);                           // FS2 0x0025ccc0

    float m_from;               // FS2 +0x20
    float m_to;                 // FS2 +0x24
    float m_length;             // FS2 +0x28: frames
    float m_frame;              // FS2 +0x2c
};

// The memory-card tokens a prgMsg text can hold: <MC0nC> save n's cleared
// stages, <MC0nK> its listed flags (n 0..3).
constexpr int kPrgMsgCardTokens = 8;
// Token i (0..7) as 16-bit text and the text it becomes (static buffers;
// "0" without that save); both null past the table.
void prgMsgCardToken(int i, const u_short **token, const u_short **value); // FS2 0x0025d160

// ──────────────────────────────────────────────────────────────────────
// The layout's answer.

// "prgReturnFlag" (0x20, Vt_00316bb0): create ends the layout with the
// value of the S2D variable p0.
// layout: host
class PrgReturnFlag : public UiWidget
{
public:
    // Inline in the ROM (create).
    PrgReturnFlag(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg)
    {
    }
    ~PrgReturnFlag();                                                    // FS2 0x0025d390: deleting (obj, 1)
};

// "prgReturnValue" (0x20, Vt_00316b90): create ends the layout with p0.
// layout: host
class PrgReturnValue : public UiWidget
{
public:
    // Inline in the ROM (create).
    PrgReturnValue(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg)
    {
    }
    ~PrgReturnValue();                                                   // FS2 0x0025d330: deleting (obj, 1)
};

// ──────────────────────────────────────────────────────────────────────
// Movies and streamed sound, with subtitles from an fsm file.

// "prgMovie" (0x34, Vt_00316ab0): plays the movie file p0 (a strings
// index) while the arg is 0, drawn at (256 + p1, 256 + p2); p3 an anim
// whose text track shows the subtitles of the fsm image p4 (when p4 is
// non-zero) at alignment (p5, p6). getStatus: -1 once over.
// layout: host
class PrgMovie : public UiWidget
{
public:
    // Inline in the ROM (create: it then runs refresh(false)'s body).
    PrgMovie(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
        : UiWidget(seq, layout, data, def, arg), m_movie(nullptr), m_subtitles(nullptr), m_text(nullptr),
          m_textX(0.0f), m_textY(0.0f)  // PC: the ROM leaves the position unset until a subtitle track is found
    {
    }
    ~PrgMovie();                                                         // FS2 0x0025c1e0: deleting (obj, 1)
    void draw(UiDrawer *drawer);                                         // FS2 0x0025bf00
    void advance();                                                      // FS2 0x0025c040
    void refresh(bool argChanged);                                       // FS2 0x0025bd30
    int  getStatus();                                                    // FS2 0x0025beb0

    void findSubtitleTrack();                                            // FS2 0x0025c110
    // Opens the movie (the arg is 0). Inline in the ROM (create, refresh).
    void open();
    // Deletes the movie, clears the subtitle. Inline in the ROM.
    void close();

    MoviePlayer *m_movie;       // FS2 +0x20: new / delete
    const void  *m_subtitles;   // FS2 +0x24: the fsm image
    UiTextTrack *m_text;        // FS2 +0x28
    float        m_textX;       // FS2 +0x2c
    float        m_textY;       // FS2 +0x30
};

// "prgSndStream" (0x38, Vt_00316a90): streams the sound p1 (a strings
// index) of the bank image p0 while the arg is 0; p2 an anim whose text
// track shows the subtitles of the fsm image p3 at alignment (p4, p5) by
// its own clock's seconds. getStatus: 1 while streaming, -1 after.
// layout: host
class PrgSndStream : public UiWidget
{
public:
    // Inline in the ROM (create: it then runs refresh(false)'s body);
    // defined in game/uiwidget.cpp (it takes a child of the game clock).
    PrgSndStream(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg);
    ~PrgSndStream();                                                     // FS2 0x0025bc70: deleting (obj, 1)
    void draw(UiDrawer *drawer);                                         // FS2 0x0025bb00
    void advance();                                                      // FS2 0x0025bbc0
    void refresh(bool argChanged);                                       // FS2 0x0025b8e0
    int  getStatus();                                                    // FS2 0x0025bae0

    void findSubtitleTrack();                                            // FS2 0x0025ba10
    // Starts the stream (the arg is 0). Inline in the ROM (create, refresh).
    void open();
    // Deletes the stream, clears the subtitle. Inline in the ROM.
    void close();

    SoundStream *m_stream;      // FS2 +0x20: new / delete
    const void  *m_subtitles;   // FS2 +0x24: the fsm image
    UiTextTrack *m_text;        // FS2 +0x28
    Clock       *m_clock;       // FS2 +0x2c: a child of the game clock, reset at the start
    float        m_textX;       // FS2 +0x30
    float        m_textY;       // FS2 +0x34
};

// ──────────────────────────────────────────────────────────────────────
// Widgets of other TUs create builds (unread; only their constructors).

// "prgScaleNavi" (0x628 bytes; unassigned@00299d90).
// layout: host -- fields unread
class PrgScaleNavi : public UiWidget
{
public:
    PrgScaleNavi(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg); // FS2 0x0029e4f0
};

// "prgScaleNaviScroll" (0x28 bytes; unassigned@00299d90).
// layout: host -- fields unread
class PrgScaleNaviScroll : public UiWidget
{
public:
    PrgScaleNaviScroll(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg); // FS2 0x0029ae30
};

#endif /* _GAME_UIWIDGET_H */
