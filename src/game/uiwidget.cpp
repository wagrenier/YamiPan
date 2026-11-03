// game/uiwidget.cpp -- the S2D script widgets: UiWidget::create and the prg*
// classes it builds (the map's rotation, scrolling and message texts, the
// movie and the streamed sound with subtitles, the layout's answer), then
// the inline helpers CodeWarrior emitted after them (game/uiwidget.h).

#include "game/uiwidget.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/clock.h"
#include "game/fontdata.h"
#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/memcard.h"
#include "game/movieplayer.h"
#include "game/sysdata.h"
#include "game/uigame.h"
#include "game/uilayout.h"
#include "game/uitrack.h"
#include "game/wstring.h"
#include "sdk/eekernel.h"
#include "sdk/libgraph.h"
#include "sdk/scetypes.h"

// The class table create looks a widget definition's type up in (data
// 0x2facf0, PrgType_Count entries of {id, name}).
// layout: host
struct PrgTypeEntry
{
    int         id;             // PrgType
    const char *name;
};
static const PrgTypeEntry sPrgTypes[PrgType_Count] = {
    { PrgType_Rot, "prgRot" },
    { PrgType_RotRev, "prgRotRev" },
    { PrgType_RotEnd, "prgRotEnd" },
    { PrgType_ReturnFlag, "prgReturnFlag" },
    { PrgType_ReturnValue, "prgReturnValue" },
    { PrgType_Msg, "prgMsg" },
    { PrgType_MsgOnOff, "prgMsgOnOff" },
    { PrgType_MsgAniX, "prgMsgAniX" },
    { PrgType_MsgReference, "prgMsgReference" },
    { PrgType_ChangeWeapon, "prgChangeWeapon" },
    { PrgType_RotOrderScale, "prgRotOrderScale" },
    { PrgType_RotOrderTrans, "prgRotOrderTrans" },
    { PrgType_RotOrderTransLabel, "prgRotOrderTransLabel" },
    { PrgType_RotOrderTransInit, "prgRotOrderTransInit" },
    { PrgType_RotOrderRotateTarget, "prgRotOrderRotateTarget" },
    { PrgType_RotOrderRotate, "prgRotOrderRotate" },
    { PrgType_RotCall, "prgRotCall" },
    { PrgType_RotCallOrderScale, "prgRotCallOrderScale" },
    { PrgType_RotCallOrderTrans, "prgRotCallOrderTrans" },
    { PrgType_RotCallOrderTransLabel, "prgRotCallOrderTransLabel" },
    { PrgType_RotCallOrderTransInit, "prgRotCallOrderTransInit" },
    { PrgType_RotCallOrderRotateTarget, "prgRotCallOrderRotateTarget" },
    { PrgType_RotCallOrderRotate, "prgRotCallOrderRotate" },
    { PrgType_TransMsg, "prgTransMsg" },
    { PrgType_TransEnd, "prgTransEnd" },
    { PrgType_Movie, "prgMovie" },
    { PrgType_SndStream, "prgSndStream" },
    { PrgType_ScaleNavi, "prgScaleNavi" },
    { PrgType_ScaleNaviScroll, "prgScaleNaviScroll" },
    { PrgType_McCard, "prgMcCard" },
    { PrgType_OptReset, "prgOptReset" },
    { PrgType_KCReset, "prgKCReset" },
    { PrgType_KCResetLocal, "prgKCResetLocal" },
    { PrgType_KCCopyLocal, "prgKCCopyLocal" },
    { PrgType_KCCopyMaster, "prgKCCopyMaster" },
    { PrgType_RotOrderTransSpd, "prgRotOrderTransSpd" },
    { PrgType_RotOrderTransLabelSpd, "prgRotOrderTransLabelSpd" },
    { PrgType_RotOrderTransInitSpd, "prgRotOrderTransInitSpd" },
    { PrgType_RotCallOrderTransSpd, "prgRotCallOrderTransSpd" },
    { PrgType_RotCallOrderTransLabelSpd, "prgRotCallOrderTransLabelSpd" },
    { PrgType_RotCallOrderTransInitSpd, "prgRotCallOrderTransInitSpd" },
    { PrgType_Debug, "prgDebug" },
};

// The Call forms' types are the plain forms' this far on.
constexpr int kPrgRotCallOffset = PrgType_RotCallOrderScale - PrgType_RotOrderScale;
constexpr int kPrgRotCallSpdOffset = PrgType_RotCallOrderTransSpd - PrgType_RotOrderTransSpd;

// Pi as the angle checks compare it (doubles: __extendsfdf2 and the
// runtime's compares 0x100230 / 0x100250) and as the floats added.
constexpr double kPi = 3.14159265358979323846;
constexpr float  kPiF = 3.1415927f;
constexpr float  kTwoPiF = 6.2831855f;

// PrgRot: the angle a frame of L1 / R1 or the right stick turns (pi / 64),
// the zoom a frame of the right stick's full tilt, its range, the pan of
// the left stick's full tilt in pixels.
constexpr float kPrgRotTurnStep = 0.049087387f;
constexpr float kPrgRotZoomStep = 0.02f;
constexpr float kPrgRotScaleMin = 0.6f;
constexpr float kPrgRotScaleMax = 1.8f;
constexpr float kPrgRotScaleStart = 0.8f;
constexpr float kPrgRotPanStep = 8.0f;
// The bounds without corners in the definition.
constexpr float kPrgRotBound = 2047.0f;

// PrgTransMsg's speed when its definition gives none above 0; its end
// until the text track is found.
constexpr float kPrgTransMsgSpeed = 4.0f;
constexpr float kPrgTransMsgNoEnd = 1000.0f;

// PrgMovie: the frame size and audio channels it asks of the player, the texture
// base it draws from, where on the 512 x 512 frame its origin is, the
// movies' frame rate.
constexpr int kMovieSize = 0x200;
constexpr int kMovieAudioChannels = 2;    // MoviePlayer: the two language tracks (setVolume mutes one)
constexpr int kMovieDrawTbp = 0x800;
constexpr float kMovieOrigin = 256.0f;
constexpr int kMovieFramesPerSecond = 25;
// PrgSndStream's stream buffer and volume.
constexpr int kSndStreamBufferSize = 0x10000;
constexpr int kSndStreamVolume = 0x80;

// SaveData::testStageFlags masks: the stage cleared in any kind, variant 0
// / variant 1 (the clear bits 0x307350: kind k variant v is 1 << 2k + v).
constexpr u_int kStageClearedVariant0 = 0x15;
constexpr u_int kStageClearedVariant1 = 0x2a;

// The pad words' bits.
constexpr u_int kPadL1 = 1u << PadBit_L1;
constexpr u_int kPadR1 = 1u << PadBit_R1;
constexpr u_int kPadTriangle = 1u << PadBit_Triangle;
constexpr u_int kPadUp = 1u << PadBit_Up;
constexpr u_int kPadDown = 1u << PadBit_Down;
constexpr u_int kPadRStickUp = 1u << PadBit_RStickUp;
constexpr u_int kPadRStickRight = 1u << PadBit_RStickRight;
constexpr u_int kPadRStickDown = 1u << PadBit_RStickDown;
constexpr u_int kPadRStickLeft = 1u << PadBit_RStickLeft;
// Pad::m_stick's indices.
enum PadStick
{
    PadStick_RightX = 0,
    PadStick_RightY = 1,
    PadStick_LeftX = 2,
    PadStick_LeftY = 3,
};

// The text track (kind 3) an anim plays, or null.
// PC: null-checked anim (the ROM calls findTrack on whatever it found).
static inline UiTextTrack *findTextTrack(UiAnim *anim)
{
    if (anim == nullptr)
        return nullptr;
    return static_cast<UiTextTrack *>(anim->findTrack(UiTrackKind_Text));
}

// The text track of the anim named by a strings index of data, played in
// seq; null when any step finds nothing. Inline in the ROM (the message
// widgets, PrgTransMsg, create).
static inline UiTextTrack *findNamedTextTrack(UiSequence *seq, S2dData *data, int nameString)
{
    const char *name = data->getString(nameString);
    if (name == nullptr)
        return nullptr;
    int obj = data->findObject(name);
    if (obj < 0)
        return nullptr;
    return findTextTrack(seq->findAnim(obj));
}

// ──────────────────────────────────────────────────────────────────────
// PrgSndStream.

// Inline in the ROM (create, refresh).
inline void PrgSndStream::close()
{
    if (m_stream != nullptr)
        delete m_stream;
    m_stream = nullptr;
    if (m_text != nullptr)
        m_text->setTextAt(0.0f, 0.0f, nullptr);
}

// Inline in the ROM (create, refresh).
inline void PrgSndStream::open()
{
    close();
    m_stream = new SoundStream();
    m_stream->setBuffer(kSndStreamBufferSize, nullptr);
    S2dWidgetDef *def = m_def;
    const char *name = m_data->getString((int)def->param[1]);
    const void *bank = m_data->getImage((int)def->param[0]);
    m_stream->openBankFile(bank, name);
    m_stream->play(0, 0);
    m_stream->setVolume(kSndStreamVolume, kSndStreamVolume);
    m_clock->resetTime();
}

// Inline in the ROM (create).
// callcheck: createChild

PrgSndStream::PrgSndStream(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : UiWidget(seq, layout, data, def, arg), m_stream(nullptr), m_subtitles(nullptr), m_text(nullptr),
      m_clock(gGameSystem->m_clock.createChild()),
      m_textX(0.0f), m_textY(0.0f)  // PC: the ROM leaves the position unset until a subtitle track is found
{
}

// ──────────────────────────────────────────────────────────────────────
// The arg 0 (re)starts the stream; another stops it.
// FS2 0x0025b8e0
// callcheck: ~SoundStream, setTextAt, operator_new, SoundStream, setBuffer, getString, getImage, openBankFile, play, setVolume, resetTime, stop (open / close inline in the ROM)

void PrgSndStream::refresh(bool argChanged)
{
    if (m_arg == 0) {
        open();
    } else {
        if (m_stream != nullptr)
            m_stream->stop(0);
        if (m_text != nullptr)
            m_text->setTextAt(0.0f, 0.0f, nullptr);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The subtitle track (anim p2) and file (p3) with its position (p4, p5).
// FS2 0x0025ba10
// callcheck: findAnim, findTrack, setTextAt, getImage, fsmRelocate

void PrgSndStream::findSubtitleTrack()
{
    S2dWidgetDef *def = m_def;
    if ((int)def->param[2] == 0)
        return;
    m_text = findTextTrack(findAnim((int)def->param[2]));
    if (m_text == nullptr)
        return;
    m_text->setTextAt(0.0f, 0.0f, nullptr);
    if ((int)def->param[3] == 0)
        return;
    m_subtitles = m_data->getImage((int)def->param[3]);
    if (m_subtitles != nullptr)
        fsmRelocate(const_cast<void *>(m_subtitles));
    m_textX = def->param[4];
    m_textY = def->param[5];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025bae0

int PrgSndStream::getStatus()
{
    return m_stream != nullptr ? 1 : -1;
}

// ──────────────────────────────────────────────────────────────────────
// Feeds the stream (from the draw slot) and deletes it once it is over.
// FS2 0x0025bb00
// callcheck: ~SoundStream, setTextAt, update (close inline in the ROM)

void PrgSndStream::draw(UiDrawer *drawer)
{
    SoundStream *stream = m_stream;
    if (stream == nullptr)
        return;
    int state = stream->m_state;
    bool streaming = state == 1 || state == 2 || state == 3;
    if (!streaming)
        close();
    else
        stream->update();
}

// ──────────────────────────────────────────────────────────────────────
// The subtitle at the stream clock's time.
// FS2 0x0025bbc0
// callcheck: getSeconds, fsmFindText, setTextAt

void PrgSndStream::advance()
{
    if (m_stream == nullptr || m_text == nullptr || m_subtitles == nullptr)
        return;
    float seconds = m_stream == nullptr ? -1.0f : m_clock->getSeconds();
    if (0.0f <= seconds)
        m_text->setTextAt(m_textX, m_textY, fsmFindText(seconds, m_subtitles, 0));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025bc70
// callcheck: ~SoundStream, setTextAt, ~Clock, operator_delete (close inline in the ROM)

PrgSndStream::~PrgSndStream()
{
    close();
    if (m_clock != nullptr)
        delete m_clock;
}

// ──────────────────────────────────────────────────────────────────────
// PrgMovie.

// Inline in the ROM (create, refresh, draw).
inline void PrgMovie::close()
{
    if (m_movie != nullptr)
        delete m_movie;
    m_movie = nullptr;
    if (m_text != nullptr)
        m_text->setTextAt(0.0f, 0.0f, nullptr);
}

// The movie p0 into a new player, its audio channel by the options (the
// port does not know what GameOptions::_04 means beyond this: open-items).
// Inline in the ROM (create, refresh).
inline void PrgMovie::open()
{
    close();
    m_movie = new MoviePlayer(kMovieSize, kMovieSize, kMovieAudioChannels);
    if (getGameOptions()->_04 == 0)
        m_movie->setVolume(1, 0, 0);
    else
        m_movie->setVolume(0, 0, 0);
    m_movie->setWorkArea(getUiWorkArea(), m_movie->workSize());
    const char *name = m_data->getString((int)m_def->param[0]);
    if (m_movie->open(name) < 0) {
        delete m_movie;
        m_movie = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The arg 0 (re)opens the movie; another closes it.
// FS2 0x0025bd30
// callcheck: ~MoviePlayer, setTextAt, operator_new, MoviePlayer, getGameOptions, setVolume, getUiWorkArea, workSize, setWorkArea, getString, open (open / close inline in the ROM)

void PrgMovie::refresh(bool argChanged)
{
    if (m_arg == 0)
        open();
    else
        close();
}

// ──────────────────────────────────────────────────────────────────────
// -1 without a movie or once it is over, else whether it plays.
// FS2 0x0025beb0
// callcheck: testState, isPlaying

int PrgMovie::getStatus()
{
    if (m_movie == nullptr)
        return -1;
    if (m_movie->testState(MovieState_Over))
        return -1;
    return (u_char)m_movie->isPlaying();
}

// ──────────────────────────────────────────────────────────────────────
// Primes the player until a frame is ready, then starts its sound and
// draws each frame at (256 + p1, 256 + p2) of the movie's frame; closes
// it once over. The player uploads the frame into the drawer's packet
// itself (the port's movie module replaces that: no GS upload).
// FS2 0x0025bf00
// callcheck: testState, isReady, prime, startAudio, closeDirect, sceGsSyncPath, FlushCache, drawFrame, ~MoviePlayer, setTextAt (close inline in the ROM)

void PrgMovie::draw(UiDrawer *drawer)
{
    if (m_movie == nullptr)
        return;
    if (m_movie->testState(MovieState_Over)) {
        close();
        return;
    }
    if (!m_movie->isReady()) {
        m_movie->prime();
        return;
    }
    if (m_movie->isReady())
        m_movie->startAudio();
    drawer->closeDirect();
    sceVif1Packet *packet = drawer->m_packet;
    float x = m_def->param[1];
    float y = m_def->param[2];
    sceGsSyncPath(0, 0);
    FlushCache(0);  // WRITEBACK_DCACHE
    m_movie->drawFrame(packet, kMovieDrawTbp, kMovieSize, kMovieSize, (int)(x + kMovieOrigin), (int)(y + kMovieOrigin));
}

// ──────────────────────────────────────────────────────────────────────
// The subtitle at the movie's time (its frame at 25 a second).
// FS2 0x0025c040
// callcheck: getFrame, fsmFindText, setTextAt

void PrgMovie::advance()
{
    if (m_movie == nullptr || m_text == nullptr || m_subtitles == nullptr)
        return;
    float seconds;
    if (m_movie == nullptr) {
        seconds = -1.0f;
    } else {
        int frame = m_movie->getFrame();
        seconds = frame < 0 ? -1.0f : (float)frame / (float)kMovieFramesPerSecond;
    }
    if (0.0f <= seconds)
        m_text->setTextAt(m_textX, m_textY, fsmFindText(seconds, m_subtitles, 0));
}

// ──────────────────────────────────────────────────────────────────────
// The subtitle track (anim p3) and file (p4) with its position (p5, p6).
// FS2 0x0025c110
// callcheck: findAnim, findTrack, setTextAt, getImage, fsmRelocate

void PrgMovie::findSubtitleTrack()
{
    S2dWidgetDef *def = m_def;
    if ((int)def->param[3] == 0)
        return;
    m_text = findTextTrack(findAnim((int)def->param[3]));
    if (m_text == nullptr)
        return;
    m_text->setTextAt(0.0f, 0.0f, nullptr);
    if ((int)def->param[4] == 0)
        return;
    m_subtitles = m_data->getImage((int)def->param[4]);
    if (m_subtitles != nullptr)
        fsmRelocate(const_cast<void *>(m_subtitles));
    m_textX = def->param[5];
    m_textY = def->param[6];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c1e0
// callcheck: ~MoviePlayer, setTextAt, operator_delete (close inline in the ROM)

PrgMovie::~PrgMovie()
{
    close();
}

// ──────────────────────────────────────────────────────────────────────
// PrgTransEnd, PrgTrans, PrgTransMsg.

// FS2 0x0025c280

void PrgTransEnd::draw(UiDrawer *drawer)
{
    float origin[2] = { 0.0f, 0.0f };
    drawer->setTranslate(origin);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c2b0
// callcheck: operator_delete

PrgTransEnd::~PrgTransEnd()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c310

bool PrgTransMsg::canScroll()
{
    return m_scrollEnd != 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c340

bool PrgTransMsg::atBottom()
{
    return m_scrollEnd <= m_scroll;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c360

bool PrgTransMsg::atTop()
{
    return m_scroll <= 0.0f;
}

// Lines past the ones shown, in pixels; when the text is less than two
// pages of the arg's lines (and more than one), the second page's start
// instead. Inline in the ROM.
inline float PrgTransMsg::scrollEnd()
{
    if (m_text == nullptr)
        return 0.0f;
    int page = m_arg;
    int lines = m_text->m_font == nullptr ? 0 : m_text->m_font->countLines();
    float end = (float)lines * m_lineHeight - m_shownLines * m_lineHeight;
    if (1 < page && page < lines && lines < page * 2)
        end = m_lineHeight * ((float)(page * 2) - m_shownLines);
    if (end < 0.0f)
        end = 0.0f;
    return end;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c390
// callcheck: countLines (scrollEnd inline in the ROM)

void PrgTransMsg::refresh(bool argChanged)
{
    m_scroll = 0.0f;
    m_target = m_scroll;
    m_scrollEnd = scrollEnd();
}

// ──────────────────────────────────────────────────────────────────────
// Down / Up held scroll by the speed (the target snapping to the next
// whole line, never above the arg's page); released, the text eases to
// the target. A changed line count starts again at the arg's line.
// FS2 0x0025c460
// callcheck: countLines (scrollEnd inline in the ROM)

void PrgTransMsg::advance()
{
    GamePad &pad = gGameSystem->m_pad0;
    if (m_text == nullptr)
        return;
    float end = scrollEnd();
    if (end != m_scrollEnd) {
        m_scroll = m_lineHeight * (float)m_arg;
        m_scrollEnd = end;
        if (end < m_scroll)
            m_scroll = end;
        m_target = m_scroll;
    }
    if (pad.m_held & kPadDown) {
        m_scroll = m_scroll + m_speed;
        if (m_scroll < m_scrollEnd) {
            int line = (int)m_lineHeight;
            m_target = (float)(line * ((int)((m_scroll + m_lineHeight) - 1.0f) / line));
            float first = m_lineHeight * (float)m_arg;
            if (m_target < first)
                m_target = first;
        } else {
            m_scroll = m_scrollEnd;
            m_target = m_scrollEnd;
        }
    } else if (pad.m_held & kPadUp) {
        float scroll = m_scroll - m_speed;
        m_scroll = scroll;
        if (0.0f <= scroll) {
            m_target = (float)((int)m_lineHeight * ((int)scroll / (int)m_lineHeight));
            if (m_target < m_lineHeight * (float)m_arg)
                m_target = 0.0f;
        } else {
            m_scroll = 0.0f;
            m_target = 0.0f;
        }
    } else if (m_target < m_scroll) {
        m_scroll = m_scroll - m_speed;
        if (m_scroll < m_target)
            m_scroll = m_target;
    } else if (m_scroll < m_target) {
        m_scroll = m_scroll + m_speed;
        if (m_target < m_scroll)
            m_scroll = m_target;
    }
    m_translate[0] = 0.0f;
    m_translate[1] = -m_scroll;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c780
// callcheck: operator_delete

PrgTransMsg::~PrgTransMsg()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c7f0
// callcheck: UiWidget, getString, findObject, findAnim, findTrack, countLines (PrgTrans and scrollEnd inline in the ROM)

PrgTransMsg::PrgTransMsg(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : PrgTrans(seq, layout, data, def, arg)
{
    m_speed = def->param[3];
    if (m_speed <= 0.0f)
        m_speed = kPrgTransMsgSpeed;
    m_lineHeight = def->param[2];
    m_shownLines = def->param[1];
    m_scroll = 0.0f;
    m_target = 0.0f;
    m_text = nullptr;
    m_scrollEnd = kPrgTransMsgNoEnd;
    UiTextTrack *text = findNamedTextTrack(seq, data, (int)def->param[0]);
    if (text == nullptr)
        return;
    m_text = text;
    m_scrollEnd = scrollEnd();
    if (m_scrollEnd < m_scroll) {
        m_scroll = m_scrollEnd;
        m_target = m_scrollEnd;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c9d0

void PrgTrans::draw(UiDrawer *drawer)
{
    drawer->setTranslate(m_translate);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025c9e0
// callcheck: operator_delete

PrgTrans::~PrgTrans()
{
}

// ──────────────────────────────────────────────────────────────────────
// Messages.

// FS2 0x0025ca40
// callcheck: findAnim, findTrack, getString, expandTextVariable, getImage, setMessageByName, getImageEntry, setTextAt (setText inline in the ROM)

void PrgMsgReference::advance()
{
    setText(m_def);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025ca50
// callcheck: operator_delete

PrgMsgReference::~PrgMsgReference()
{
}

// ──────────────────────────────────────────────────────────────────────
// The text variable p2 on the text track of anim p0: as text, or as the
// message of the name it gives in the fMsg image p1.
// FS2 0x0025cab0
// callcheck: findTrack (findTextTrack)

void PrgMsgReference::setText(S2dWidgetDef *def)
{
    if ((int)def->param[0] < 0)
        return;
    UiTextTrack *track = findTextTrack(findAnim((int)def->param[0]));
    if (track == nullptr)
        return;
    const char *name = m_data->getString((int)def->param[2]);
    float x = def->param[3];
    float y = def->param[4];
    char text[0x100];
    if (!expandTextVariable(name, text, nullptr)) {
        expandTextVariable(name, text, const_cast<void *>(m_data->getImage((int)m_def->param[1])));
        m_msgFile = m_data->getImage((int)m_def->param[1]);
        m_message = track->setMessageByName(x, y, text, const_cast<void *>(m_msgFile), 0);
        track->m_msgKey = m_data->getImageEntry((int)def->param[1])->name;
    } else {
        m_message = track->setTextAt(x, y, reinterpret_cast<const u_short *>(text));
    }
}

// ──────────────────────────────────────────────────────────────────────
// A frame towards the end (arg 0) or the start, the text's x alignment
// in between.
// FS2 0x0025cc10

void PrgMsgAniX::advance()
{
    if (m_arg == 0)
        m_frame = m_frame + 1.0f;
    else
        m_frame = m_frame - 1.0f;
    if (m_frame < 0.0f)
        m_frame = 0.0f;
    if (m_length < m_frame)
        m_frame = m_length;
    setTextX((m_frame / m_length) * (m_to - m_from) + m_from + 0.0f, m_def);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025ccc0
// callcheck: getString, findObject, findAnim, findTrack (findNamedTextTrack)

void PrgMsgAniX::setTextX(float x, S2dWidgetDef *def)
{
    UiTextTrack *track = findNamedTextTrack(m_seq, m_data, (int)def->param[0]);
    if (track != nullptr)
        track->m_x = x;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025cd60
// callcheck: operator_delete

PrgMsgAniX::~PrgMsgAniX()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025cdc0
// callcheck: getString, findObject, findAnim, findTrack, setMessage, findVar, setMessageByName, getImageEntry, getText, wstrlen, operator_new[], wstrcpy, prgMsgCardToken, wstrstr, memmove, operator_delete[], setText (setText inline in the ROM)

void PrgMsg::advance()
{
    setText(m_def);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025cdd0
// callcheck: operator_delete

PrgMsg::~PrgMsg()
{
}

// ──────────────────────────────────────────────────────────────────────
// Message p2 of the fMsg image p1 (plus the variable p5) on the text track
// of anim p0; with a non-zero arg its memory-card tokens replaced by the
// saves' counts (in place: the values are shorter than the tokens).
// FS2 0x0025ce30
// callcheck: getString, findObject, findAnim, findTrack, operator_new[], operator_delete[] (findNamedTextTrack; new u_short[] / delete[])

void PrgMsg::setText(S2dWidgetDef *def)
{
    S2dData *data = m_data;
    UiTextTrack *track = findNamedTextTrack(m_seq, data, (int)def->param[0]);
    if (track == nullptr)
        return;
    if ((int)def->param[2] < 0) {
        track->setMessage(-1, nullptr);
        m_message = -1;
        track->m_msgKey = nullptr;
        return;
    }
    const char *name = data->getString((int)def->param[2]);
    float x = def->param[3];
    float y = def->param[4];
    int offset = 0;
    if (0.0f < def->param[5])
        offset = data->findVar(data->getString((int)def->param[5]))->value;
    m_message = track->setMessageByName(x, y, name, const_cast<void *>(m_msgFile), offset);
    track->m_msgKey = data->getImageEntry((int)def->param[1])->name;
    if (m_arg == 0)
        return;
    const u_short *shown = track->getText();
    if (shown == nullptr)
        return;

    bool replaced = false;
    u_short *text = new u_short[wstrlen(shown) + 1];
    wstrcpy(text, shown);
    for (int i = 0; i < kPrgMsgCardTokens; i++) {
        const u_short *token;
        const u_short *value;
        prgMsgCardToken(i, &token, &value);
        bool found = false;
        int valueLen = wstrlen(value);
        int tokenLen = wstrlen(token);
        wstrlen(text);
        for (u_short *at = wstrstr(text, token); at != nullptr;) {
            u_short *rest = at + tokenLen;
            u_short *moved = rest + (valueLen - tokenLen);
            memmove(moved, rest, (wstrlen(rest) + 1) * sizeof(u_short));
            memmove(at, value, valueLen * sizeof(u_short));
            found = true;
            at = wstrstr(moved, token);
        }
        if (found)
            replaced = true;
    }
    if (!replaced) {
        delete[] text;
        text = nullptr;
    }
    if (text != nullptr) {
        track->setText(text, true);
        delete[] text;
    }
}

// The memory-card tokens (data 0x2fae40) and the buffers the token and
// its value are made in (bss 0x599460 / 0x599450).
static const char *const sCardTokens[kPrgMsgCardTokens] = {
    "<MC00C>", "<MC01C>", "<MC02C>", "<MC03C>",
    "<MC00K>", "<MC01K>", "<MC02K>", "<MC03K>",
};
static u_short sCardValue[8];
static u_short sCardToken[8];

// ──────────────────────────────────────────────────────────────────────
// <MC0nC>: the stages save n cleared, a variant-1 clear counting again on
// a stage that has one; <MC0nK>: its listed flags. "0" without the
// memory-card menu's manager or that save.
// FS2 0x0025d160

void prgMsgCardToken(int i, const u_short **token, const u_short **value)
{
    sCardValue[0] = '0';
    sCardValue[1] = 0;
    *value = sCardValue;
    wstrFromAscii(sCardToken, sCardTokens[i]);
    *token = sCardToken;
    if ((u_int)i >= kPrgMsgCardTokens) {
        *token = nullptr;
        *value = nullptr;
        return;
    }
    char text[8];
    switch (i) {
    default: {
        if (gMenuMemcard == nullptr)
            break;
        SaveFile *file = gMenuMemcard->m_files[i];
        if (file == nullptr)
            break;
        int cleared = 0;
        for (int stage = 0; stage < kStageFlagCount; stage++) {
            if (file->save.testStageFlags(stage, kStageClearedVariant0))
                cleared++;
            if (file->save.stageHasVariant1(stage) && file->save.testStageFlags(stage, kStageClearedVariant1))
                cleared++;
        }
        sprintf(text, "%d", cleared);
        wstrFromAscii(sCardValue, text);
        break;
    }
    case 4:
    case 5:
    case 6:
    case 7: {
        if (gMenuMemcard == nullptr)
            break;
        SaveFile *file = gMenuMemcard->m_files[i - 4];
        if (file == nullptr)
            break;
        sprintf(text, "%d", file->flags.countListed());
        wstrFromAscii(sCardValue, text);
        break;
    }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The layout's answer and the map's ends.

// FS2 0x0025d330
// callcheck: operator_delete

PrgReturnValue::~PrgReturnValue()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025d390
// callcheck: operator_delete

PrgReturnFlag::~PrgReturnFlag()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025d3f0

void PrgRotEnd::draw(UiDrawer *drawer)
{
    m_seq->m_parentXform.identity();
    drawer->setPixelMode(1);
    float origin[2] = { 0.0f, 0.0f };
    drawer->setTranslate(origin);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025d460
// callcheck: operator_delete

PrgRotEnd::~PrgRotEnd()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025d4c0

void PrgRotCall::draw(UiDrawer *drawer)
{
    PrgRot *target = m_target;
    m_seq->m_parentXform.copy(target->m_drawXform);
    drawer->setPixelMode(0);
    drawer->setTranslate(target->m_drawCenter);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025d520
// callcheck: operator_delete

PrgRotCall::~PrgRotCall()
{
}

// ──────────────────────────────────────────────────────────────────────
// PrgRot.

// An angle set from a move, brought back towards -pi..pi -- by pi, not
// 2 pi (sic; startTween's turn the short way keeps it in range anyway).
// Inline in the ROM (startTween, stepTweens).
static inline float prgRotWrapSetAngle(float angle)
{
    if ((double)angle < -kPi)
        angle = angle + kPiF;
    if ((double)angle > kPi)
        angle = angle - kPiF;
    return angle;
}

// ──────────────────────────────────────────────────────────────────────
// The angle turns the short way round.
// FS2 0x0025d580
// callcheck: __extendsfdf2, FUN_00100230, FUN_00100250, operator_delete, operator_new (the double compares; UiTween::start inline in the ROM)

void PrgRot::startTween(float to, int axis, int frames)
{
    float current = axisValue(axis);
    if (axis == PrgRotAxis_Angle) {
        float turn = to - current;
        if ((double)turn > kPi)
            to = to - kTwoPiF;
        else if ((double)turn < -kPi)
            to = to + kTwoPiF;
    }
    if (frames < 1 || current == to) {
        if (axis == PrgRotAxis_Y)
            m_pos[1] = to;
        else if (axis == PrgRotAxis_X)
            m_pos[0] = to;
        else if (axis == PrgRotAxis_Scale)
            m_scale = to;
        else if (axis == PrgRotAxis_Angle)
            m_angle = prgRotWrapSetAngle(to);
        if (m_tweens[axis] != nullptr) {
            delete m_tweens[axis];
            m_tweens[axis] = nullptr;
        }
        return;
    }
    if (m_tweens[axis] == nullptr)
        m_tweens[axis] = new UiTween();
    else if (to == m_tweens[axis]->to)
        return;
    m_tweens[axis]->start(current, to, frames, 0, 0, UiTweenCurve_Linear);
}

// ──────────────────────────────────────────────────────────────────────
// Unless input is off or a move runs: L1 / R1 (until both are released
// after a recentre) or the right stick's sides turn, its up / down zooms,
// the left stick pans along the screen; L1 + R1 together recentre (on
// the player when the map shows them), Triangle turns to the player's
// heading. The position stays in the bounds.
// FS2 0x0025d8a0
// callcheck: stepTweens, mapShowsPlayer, getPlayerMapPos, getPlayerHeading, __extendsfdf2, FUN_00100250, FUN_00100230, identity, rotate, uiTransformPoint (the double compares)

void PrgRot::advance()
{
    GamePad &pad = gGameSystem->m_pad0;
    if (m_inputOff || stepTweens())
        return;

    float pan[2] = { 0.0f, 0.0f };
    u_int held = pad.m_held;
    bool l1 = (held & kPadL1) != 0;
    bool r1 = (held & kPadR1) != 0;
    if (l1 && r1 && (pad.m_pressed & (kPadL1 | kPadR1))) {
        m_pos[0] = -m_def->param[0];
        m_pos[1] = -m_def->param[1];
        if (mapShowsPlayer()) {
            float player[2];
            getPlayerMapPos(player);
            m_pos[0] = -player[0];
            m_pos[1] = -player[1];
        }
        m_angle = m_startAngle;
        m_scale = kPrgRotScaleStart;
        m_recentred = 1;
    } else if (pad.m_pressed & kPadTriangle) {
        m_angle = -getPlayerHeading();
    } else {
        if (!l1 && !r1) {
            m_recentred = 0;
            held = pad.m_held;
            if (!(held & kPadRStickUp) && !(held & kPadRStickDown)) {
                if (held & kPadRStickLeft)
                    m_angle = m_angle - kPrgRotTurnStep;
                else if (held & kPadRStickRight)
                    m_angle = m_angle + kPrgRotTurnStep;
            }
        } else if (!m_recentred) {
            if (l1 && !r1)
                m_angle = m_angle - kPrgRotTurnStep;
            else if (r1 && !l1)
                m_angle = m_angle + kPrgRotTurnStep;
        }
        if ((pad.m_held & kPadRStickUp) || (pad.m_held & kPadRStickDown)) {
            m_scale = (m_scale + 0.0f) - pad.m_stick[PadStick_RightY] * kPrgRotZoomStep;
            if (m_scale < kPrgRotScaleMin)
                m_scale = kPrgRotScaleMin;
            if (kPrgRotScaleMax < m_scale)
                m_scale = kPrgRotScaleMax;
        }
        pan[1] = -pad.m_stick[PadStick_LeftY] * kPrgRotPanStep;
        pan[0] = -pad.m_stick[PadStick_LeftX] * kPrgRotPanStep;
    }

    if ((double)m_angle < -kPi)
        m_angle = m_angle + kTwoPiF;
    if ((double)m_angle > kPi)
        m_angle = m_angle - kTwoPiF;

    UiMatrix rot;
    rot.identity();
    rot.rotate(-m_angle);
    float move[2];
    uiTransformPoint(move, &rot, pan, 0);
    m_pos[0] = m_pos[0] + move[0];
    m_pos[1] = m_pos[1] + move[1];
    if (m_pos[0] < m_min[0])
        m_pos[0] = m_min[0];
    if (m_pos[1] < m_min[1])
        m_pos[1] = m_min[1];
    if (m_max[0] < m_pos[0])
        m_pos[0] = m_max[0];
    if (m_max[1] < m_pos[1])
        m_pos[1] = m_max[1];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025dcf0
// callcheck: operator_delete, value, __extendsfdf2, FUN_00100250, FUN_00100230 (UiTween::isDone / step inline in the ROM; the double compares)

bool PrgRot::stepTweens()
{
    bool moved = false;
    for (int axis = 0; axis < PrgRotAxis_Count; axis++) {
        UiTween *tween = m_tweens[axis];
        if (tween == nullptr)
            continue;
        if (tween->isDone()) {
            if (m_tweens[axis] != nullptr) {
                delete m_tweens[axis];
                m_tweens[axis] = nullptr;
            }
            continue;
        }
        moved = true;
        tween->step();
        float value = tween->value();
        if (axis == PrgRotAxis_Y)
            m_pos[1] = value;
        else if (axis == PrgRotAxis_X)
            m_pos[0] = value;
        else if (axis == PrgRotAxis_Scale)
            m_scale = value;
        else if (axis == PrgRotAxis_Angle)
            m_angle = prgRotWrapSetAngle(value);
    }
    return moved;
}

// ──────────────────────────────────────────────────────────────────────
// The sequence's parent matrix moved to the position, rotated, scaled
// (concatenated twice, sic: the scale squared), drawn from the centre;
// both kept for PrgRotCall.
// FS2 0x0025df40

void PrgRot::draw(UiDrawer *drawer)
{
    UiMatrix &xform = m_seq->m_parentXform;
    xform.translate(m_pos);
    xform.rotate(m_angle);
    UiMatrix scale;
    scale.setScale(m_scale, m_scale);
    xform.concat(scale);
    xform.concat(scale);
    drawer->setPixelMode(0);
    drawer->setTranslate(m_center);
    m_drawXform.copy(xform);
    m_drawCenter[0] = m_center[0];
    m_drawCenter[1] = m_center[1];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025dff0

void PrgRot::refresh(bool argChanged)
{
    m_inputOff = m_arg < 0 ? 1 : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025e020
// callcheck: operator_delete

PrgRot::~PrgRot()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025e080
// callcheck: UiWidget, mapShowsPlayer, getPlayerMapPos, getPlayerHeading, identity

PrgRot::PrgRot(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg, float startAngle)
    : UiWidget(seq, layout, data, def, arg)
{
    m_angle = startAngle;
    m_scale = kPrgRotScaleStart;
    m_recentred = 0;
    m_startAngle = startAngle;
    m_pos[0] = -def->param[0];
    m_pos[1] = -def->param[1];
    if (mapShowsPlayer()) {
        float player[2];
        getPlayerMapPos(player);
        m_pos[0] = -player[0];
        m_pos[1] = -player[1];
        m_angle = -getPlayerHeading();
    }
    m_center[0] = def->param[2];
    m_center[1] = def->param[3];
    for (int axis = 0; axis < PrgRotAxis_Count; axis++)
        m_tweens[axis] = nullptr;
    m_drawXform.identity();
    m_drawCenter[0] = 0.0f;
    m_drawCenter[1] = 0.0f;
    const float *corner = &def->param[4];
    if (corner[0] == 0.0f && corner[1] == 0.0f && corner[2] == 0.0f && corner[3] == 0.0f) {
        m_min[0] = -kPrgRotBound;
        m_min[1] = -kPrgRotBound;
        m_max[0] = kPrgRotBound;
        m_max[1] = kPrgRotBound;
    } else {
        m_min[0] = -(corner[0] <= corner[2] ? corner[2] : corner[0]);
        m_min[1] = -(corner[1] <= corner[3] ? corner[3] : corner[1]);
        m_max[0] = -(corner[2] <= corner[0] ? corner[2] : corner[0]);
        m_max[1] = -(corner[3] <= corner[1] ? corner[3] : corner[1]);
    }
    m_inputOff = arg < 0 ? 1 : 0;
}

// ──────────────────────────────────────────────────────────────────────
// UiWidget::create.

// The map moves of the "prgRot{,Call}Order*" types on a PrgRot: order is
// the plain form's type, p its parameters after the widget's name(s), the
// label anim (the TransLabel forms) looked up by the caller. Inline in
// the ROM (create, twice: once per form).
static void prgRotRunOrder(PrgRot *rot, int order, const float *p, UiAnim *label)
{
    switch (order) {
    case PrgType_RotOrderRotate:
        rot->startTween(p[0], PrgRotAxis_Angle, (int)p[1]);
        return;
    case PrgType_RotOrderRotateTarget:
        rot->startTween(0.0f, PrgRotAxis_Angle, (int)p[0]);
        return;
    case PrgType_RotOrderTransInitSpd: {
        const float *home = rot->m_def->param;
        float speed = p[0];
        float dy = -home[1] - rot->m_pos[1];
        float dx = -home[0] - rot->m_pos[0];
        if (p[1] != 0.0f)
            speed = speed / rot->m_scale;
        int frames = (int)(sqrtf(dx * dx + dy * dy) / speed);
        rot->startTween(-home[0], PrgRotAxis_X, frames);
        rot->startTween(-home[1], PrgRotAxis_Y, frames);
        return;
    }
    case PrgType_RotOrderTransInit:
        rot->startTween(-rot->m_def->param[0], PrgRotAxis_X, (int)p[0]);
        rot->startTween(-rot->m_def->param[1], PrgRotAxis_Y, (int)p[0]);
        return;
    case PrgType_RotOrderTransLabelSpd: {
        // PC: null-checked anim (the ROM asks a missing anim for its anchor).
        const float *anchor = label != nullptr ? label->getAnchor() : nullptr;
        if (anchor == nullptr)
            return;
        float dx = -anchor[0] - rot->m_pos[0];
        float dy = -anchor[1] - rot->m_pos[1];
        float speed = p[1];     // p[0]: the label, resolved by the caller
        if (p[2] != 0.0f)
            speed = speed / rot->m_scale;
        int frames = (int)(sqrtf(dx * dx + dy * dy) / speed);
        rot->startTween(-anchor[0], PrgRotAxis_X, frames);
        rot->startTween(-anchor[1], PrgRotAxis_Y, frames);
        return;
    }
    case PrgType_RotOrderTransLabel: {
        const float *anchor = label != nullptr ? label->getAnchor() : nullptr;
        if (anchor == nullptr)
            return;
        rot->startTween(-anchor[0], PrgRotAxis_X, (int)p[1]);
        rot->startTween(-anchor[1], PrgRotAxis_Y, (int)p[1]);
        return;
    }
    case PrgType_RotOrderTransSpd: {
        float dy = -p[1] - rot->m_pos[1];
        float dx = -p[0] - rot->m_pos[0];
        float speed = p[2];
        if (p[3] != 0.0f)
            speed = speed / rot->m_scale;
        int frames = (int)(sqrtf(dx * dx + dy * dy) / speed);
        rot->startTween(-p[0], PrgRotAxis_X, frames);
        rot->startTween(-p[1], PrgRotAxis_Y, frames);
        return;
    }
    case PrgType_RotOrderTrans:
        rot->startTween(-p[0], PrgRotAxis_X, (int)p[2]);
        rot->startTween(-p[1], PrgRotAxis_Y, (int)p[2]);
        return;
    case PrgType_RotOrderScale:
        rot->startTween(p[0], PrgRotAxis_Scale, (int)p[1]);
        return;
    }
}

// The widget a Call form names: sequence p0, its widget p1 (strings
// indices of the layout's data). Inline in the ROM (create).
static UiWidget *prgFindCalledWidget(UiSequence *seq, UiLayout *layout, const S2dWidgetDef *def)
{
    const char *seqName = layout->m_data->getString((int)def->param[0]);
    UiSequence *other = layout->findSequence(seq->m_data->findSequence(seqName));
    if (other == nullptr)
        return nullptr;
    return other->findWidgetByName(layout->m_data->getString((int)def->param[1]));
}

// ──────────────────────────────────────────────────────────────────────
// Runs a widget definition in seq: a widget already there with its id
// gets the new arg (and refresh); otherwise the type builds its widget
// into the next slot, or acts once. Types without a lasting widget leave
// the slot they reserved empty.
// FS2 0x0025e260
// callcheck: findWidget, strcmp, reserveWidgetSlot, operator_new, PrgRot, UiWidget, getString, findVar, getImage, setDef, setText, findObject, findAnim, findTrack, stepWeaponPreview, findWidgetByName, startTween, getAnchor, findSequence, findAnimByName, PrgTransMsg, PrgScaleNavi, PrgScaleNaviScroll, PrgMcCard, getGameOptions, init, applyPadAndScreen, setDefaults, getButtonMapDraft, loadButtonMapDraft, applyButtonMapDraft, ~MoviePlayer, setTextAt, MoviePlayer, setVolume, getUiWorkArea, workSize, setWorkArea, open, findSubtitleTrack, createChild, ~SoundStream, SoundStream, setBuffer, openBankFile, play, resetTime (the classes' ctors and the widgets' open inline in the ROM; the one-shot widgets deleted through the vtable)

void UiWidget::create(UiSequence *seq, S2dWidgetDef *def, UiLayout *layout, int arg)
{
    if (def == nullptr)
        return;
    UiWidget *existing = seq->findWidget(def->id);
    if (existing != nullptr) {
        int old = existing->m_arg;
        existing->m_arg = arg;
        existing->refresh(old != arg);
        return;
    }

    const char *typeName = def->type;
    int type = PrgType_None;
    for (u_int i = 0; i < PrgType_Count; i++) {
        if (strcmp(sPrgTypes[i].name, typeName) == 0) {
            type = sPrgTypes[i].id;
            break;
        }
    }
    UiWidget **slot = seq->reserveWidgetSlot();
    S2dData *data = seq->m_data;
    const float *p = def->param;

    switch (type) {
    case PrgType_Rot:
        *slot = new PrgRot(seq, layout, data, def, arg, 0.0f);
        break;
    case PrgType_RotRev:
        *slot = new PrgRot(seq, layout, data, def, arg, kPiF);
        break;
    case PrgType_RotEnd:
        *slot = new PrgRotEnd(seq, layout, data, def, arg);
        break;
    case PrgType_ReturnFlag: {
        PrgReturnFlag *widget = new PrgReturnFlag(seq, layout, data, def, arg);
        layout->m_result = data->findVar(data->getString((int)p[0]))->value;
        layout->m_done = 1;
        *slot = widget;
        break;
    }
    case PrgType_ReturnValue: {
        PrgReturnValue *widget = new PrgReturnValue(seq, layout, data, def, arg);
        layout->m_result = (int)p[0];
        layout->m_done = 1;
        *slot = widget;
        break;
    }
    case PrgType_Msg: {
        PrgMsg *widget = new PrgMsg(seq, layout, data, def, arg);
        widget->setDef(def);
        widget->setText(def);
        delete widget;
        return;
    }
    case PrgType_MsgOnOff: {
        UiTextTrack *track = findNamedTextTrack(seq, layout->m_data, (int)p[0]);
        if (track == nullptr)
            return;
        track->m_visible = (int)p[1] != 0 ? 1 : 0;
        return;
    }
    case PrgType_MsgAniX:
        *slot = new PrgMsgAniX(seq, layout, data, def, arg);
        break;
    case PrgType_MsgReference: {
        PrgMsgReference *widget = new PrgMsgReference(seq, layout, data, def, arg);
        widget->setDef(def);
        widget->setText(def);
        delete widget;
        return;
    }
    case PrgType_ChangeWeapon:
        stepWeaponPreview();
        return;

    case PrgType_RotOrderScale:
    case PrgType_RotOrderTrans:
    case PrgType_RotOrderTransLabel:
    case PrgType_RotOrderTransInit:
    case PrgType_RotOrderRotateTarget:
    case PrgType_RotOrderRotate:
    case PrgType_RotOrderTransSpd:
    case PrgType_RotOrderTransLabelSpd:
    case PrgType_RotOrderTransInitSpd: {
        // The ROM takes the widget to be a PrgRot.
        PrgRot *rot = static_cast<PrgRot *>(seq->findWidgetByName(layout->m_data->getString((int)p[0])));
        if (rot == nullptr)
            return;
        UiAnim *label = nullptr;
        if (type == PrgType_RotOrderTransLabel || type == PrgType_RotOrderTransLabelSpd)
            label = rot->findAnim((int)p[1]);
        prgRotRunOrder(rot, type, &p[1], label);
        return;
    }

    case PrgType_RotCall: {
        UiWidget *target = prgFindCalledWidget(seq, layout, def);
        if (target == nullptr)
            return;
        PrgRotCall *widget = new PrgRotCall(seq, layout, data, def, arg);
        widget->m_target = static_cast<PrgRot *>(target);
        *slot = widget;
        break;
    }

    case PrgType_RotCallOrderScale:
    case PrgType_RotCallOrderTrans:
    case PrgType_RotCallOrderTransLabel:
    case PrgType_RotCallOrderTransInit:
    case PrgType_RotCallOrderRotateTarget:
    case PrgType_RotCallOrderRotate:
    case PrgType_RotCallOrderTransSpd:
    case PrgType_RotCallOrderTransLabelSpd:
    case PrgType_RotCallOrderTransInitSpd: {
        PrgRot *rot = static_cast<PrgRot *>(prgFindCalledWidget(seq, layout, def));
        if (rot == nullptr)
            return;
        int order = type >= PrgType_RotCallOrderTransSpd ? type - kPrgRotCallSpdOffset : type - kPrgRotCallOffset;
        // The label anim is this sequence's.
        UiAnim *label = nullptr;
        if (order == PrgType_RotOrderTransLabel || order == PrgType_RotOrderTransLabelSpd)
            label = seq->findAnimByName((int)p[2]);
        prgRotRunOrder(rot, order, &p[2], label);
        return;
    }

    case PrgType_TransMsg:
        *slot = new PrgTransMsg(seq, layout, data, def, arg);
        break;
    case PrgType_TransEnd:
        *slot = new PrgTransEnd(seq, layout, data, def, arg);
        break;
    case PrgType_Movie: {
        PrgMovie *widget = new PrgMovie(seq, layout, data, def, arg);
        if (arg == 0)
            widget->open();
        widget->setDef(def);
        widget->findSubtitleTrack();
        *slot = widget;
        break;
    }
    case PrgType_SndStream: {
        PrgSndStream *widget = new PrgSndStream(seq, layout, data, def, arg);
        if (arg == 0)
            widget->open();
        widget->setDef(def);
        widget->findSubtitleTrack();
        *slot = widget;
        break;
    }
    case PrgType_ScaleNavi:
        *slot = new PrgScaleNavi(seq, layout, data, def, arg);
        break;
    case PrgType_ScaleNaviScroll:
        *slot = new PrgScaleNaviScroll(seq, layout, data, def, arg);
        break;
    case PrgType_McCard:
        *slot = new PrgMcCard(seq, layout, data, def, arg);
        break;

    case PrgType_OptReset: {
        GameOptions *options = getGameOptions();
        options->init();
        options->applyPadAndScreen();
        return;
    }
    case PrgType_KCReset: {
        GameOptions *options = getGameOptions();
        options->buttonMap.setDefaults();
        options->applyPadAndScreen();
        return;
    }
    case PrgType_KCResetLocal:
        getButtonMapDraft()->setDefaults();
        return;
    case PrgType_KCCopyLocal:
        loadButtonMapDraft();
        return;
    case PrgType_KCCopyMaster:
        applyButtonMapDraft();
        getGameOptions()->applyPadAndScreen();
        return;

    case PrgType_Debug: {
        // What it printed is stripped: only the lookups are left.
        S2dData *layoutData = layout->m_data;
        layoutData->getString((int)p[0]);
        for (int i = 2; i < 8; i++) {
            if ((int)p[i] < 0)
                return;
            layoutData->findVar(layoutData->getString((int)p[i]));
        }
        return;
    }
    default:
        return;
    }
    seq->m_widgetCount++;
}

// ──────────────────────────────────────────────────────────────────────
// UiTween (inline in the ROM; these are its out-of-line copies).

// FS2 0x0025f5b0

float UiTween::progress()
{
    if (isDone())
        return 1.0f;
    if (length == 0)
        return to;
    float t;
    if (frame < delay)
        t = 0.0f;
    else if (frame < delay + length)
        t = (float)(frame - delay);
    else
        t = (float)length;
    t = t / (float)length;
    if (t < 0.0f)
        t = 0.0f;
    if (1.0f < t)
        t = 1.0f;
    return t;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025f6e0

bool UiTween::step()
{
    if (length == 0)
        return true;
    frame++;
    return isDone();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025f750

float UiTween::value()
{
    if (length == 0)
        return to;
    float t;
    if (frame < delay)
        t = 0.0f;
    else if (frame < delay + length)
        t = (float)(frame - delay);
    else
        t = (float)length;
    t = t / (float)length;
    if (t < 0.0f)
        t = 0.0f;
    if (1.0f < t)
        t = 1.0f;
    if (curve == UiTweenCurve_EaseIn)
        return 0.0f + from + (to - from) * (t * t);
    return 0.0f + from + t * (to - from);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025f860

bool UiTween::start(float from_, float to_, int length_, int delay_, int hold_, int curve_)
{
    from = from_;
    to = to_;
    length = length_;
    frame = 0;
    delay = delay_;
    hold = hold_;
    curve = curve_;
    return isDone();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025f8e0

bool UiTween::isDone()
{
    if (length == 0)
        return true;
    if (hold + length + delay <= frame)
        length = 0;
    return hold + length + delay <= frame;
}

// FS2 0x0025f940: UiTween's deleting destructor (no callers; the class
// has no destructor of its own here). FS2 0x0025f990: UiTween::UiTween,
// inline in the header.

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025f9c0

UiShowFlags::UiShowFlags()
{
    for (int i = 0; i < 17; i++)
        flag[i] = 1;
    flag[10] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0025fa10
// callcheck: countLines

int UiFont::countLines()
{
    if (m_glyphs == nullptr)
        return -1;
    if (m_text == nullptr)
        return 0;
    return static_cast<const FontData *>(m_glyphs)->countLines(m_text);
}
