// game/stagealert.cpp -- the ALERT rumble and the red flash of a stage
// (game/stagealert.h).

#include "game/stagealert.h"

#include <limits.h>

#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/rumblelist.h"
#include "game/stagescene.h"
#include "game/sysdata.h"

static const char kRumbleAlert[]   = "ALERT";   /* data 0x308820 */
static const char kRumbleCaution[] = "CAUTION"; /* data 0x308828 */

// The flash's fill: the 512-pixel screen at the 1792 offset, as the scene's.
constexpr int   kFlashX = 0x700;
constexpr int   kFlashY = 0x700;
constexpr int   kFlashW = 0x200;
constexpr int   kFlashH = 0x200;
constexpr u_int kFlashColour  = 0x80;   // half red
constexpr float kFlashSeconds = 0.01f;
constexpr float kFlashLevel   = 0.25f;

constexpr u_int kEndingFrames30 = 30;

// To whole frames at 30 a second, rounded. An overflow is cvt.w.s's: a clock
// paused by a menu gives a delta of 0.
static int toFrames30(float value)
{
    float frames = 30.0f * value + 0.5f;
    return frames < (float)INT_MAX ? (int)frames : INT_MAX;
}

// The ROM's inline, three times.
static void stopAlertRumble()
{
    RumbleList *rumble = gStageScene->rumble;
    if (rumble != nullptr && rumble->isPlaying(kRumbleAlert) != 0)
        rumble->removeNamed(kRumbleAlert);
}

// FS2 0x00264070
// callcheck: operator_new, StageFade, setColour

StageAlert::StageAlert()
{
    m_fade = new StageFade(kFlashX, kFlashY, kFlashW, kFlashH, nullptr);
    m_fade->setColour(kFlashColour);
}

// FS2 0x00264000
// callcheck: operator_delete

StageAlert::~StageAlert()
{
    delete m_fade;
}

// FS2 0x00263b90

void StageAlert::beforeFileEvent()
{
}

// FS2 0x00263ba0
// callcheck: isNotRunning

void StageAlert::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    if (!m_fade->isNotRunning())
        m_fade->draw(packet, texList);
}

// FS2 0x00263c10
// callcheck: getDeltaSeconds, isPlaying, addNamed, isPlaying, removeNamed, getDeltaSeconds, isPlaying, removeNamed

bool StageAlert::update()
{
    m_frames++;
    float seconds = gStageScene->clock->getDeltaSeconds();
    u_int ending = (u_int)toFrames30((float)m_frames / (1.0f / seconds));

    switch (m_state) {
    case StageAlert_Idle: {
        if (m_wanted == nullptr)
            break;
        RumbleList *rumble = gStageScene->rumble;
        if (rumble != nullptr && rumble->isPlaying(kRumbleAlert) == 0)
            rumble->addNamed(kRumbleAlert, nullptr);
        m_state = StageAlert_Start;
        m_frames = 0;
        m_shown = m_wanted;
    }
        // fall through
    case StageAlert_Start:
        m_state = StageAlert_Running;
        m_frames = 0;
        m_count = 0;
        // fall through
    case StageAlert_Running:
        if (m_wanted == nullptr) {
            stopAlertRumble();
            m_state = StageAlert_Ending;
            m_frames = 0;
            // The ROM divides by the delta here where it means to multiply, so
            // any frame run is far past kEndingFrames30 and the next update
            // goes idle.
            seconds = gStageScene->clock->getDeltaSeconds();
            m_count = (u_int)toFrames30((float)m_count / seconds);
        } else {
            m_count++;
        }
        break;
    case StageAlert_Ending:
        if (ending + m_count >= kEndingFrames30) {
            stopAlertRumble();
            m_state = StageAlert_Idle;
            m_frames = 0;
        }
        break;
    }

    m_wanted = nullptr;
    return m_state == StageAlert_Start || m_state == StageAlert_Running;
}

// FS2 0x00263ed0
// callcheck: start

bool StageAlert::flash()
{
    m_fade->start(kFlashSeconds, kFlashLevel, kFlashLevel);
    return true;
}

// FS2 0x00263f10
// callcheck: start, playQuestionVoice, addNamed

bool StageAlert::cry(StageCharacter *who)
{
    m_fade->start(kFlashSeconds, kFlashLevel, kFlashLevel);
    who->playQuestionVoice();
    if (gStageScene->rumble != nullptr)
        gStageScene->rumble->addNamed(kRumbleCaution, nullptr);
    return true;
}

// FS2 0x00263f80
// callcheck: getGameOptions

void StageAlert::show(StageObject *who)
{
    if (getGameOptions()->alertMode == GameAlert_Off)
        return;
    if (m_wanted == nullptr || who == m_shown || m_wanted != m_shown)
        m_wanted = who;
}
