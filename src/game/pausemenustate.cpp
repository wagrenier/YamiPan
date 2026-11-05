// game/pausemenustate.cpp -- the stage's text notices (FS2 0x207440..0x207ef0): a queue of 11 notices
// and one entry per kind that shows the oldest one, split in two lines, through ScenePool's slots 2 and
// 3. Scripts post them; the name PauseMenuState is older than the reading and stays with the callers.

#include "game/gamehooks.h"

#include <float.h>
#include <string.h>

#include "game/characterinput.h"
#include "game/clock.h"
#include "game/gamesystem.h"
#include "game/stagescene.h"
#include "game/thread.h"

namespace {

constexpr int   kNoticeForever = 0x7fffffff;    // frames of a notice that stays
constexpr float kNoticeFps     = 30.0f;
constexpr u_short kEmptyLine[1] = { 0 };        // data 0x3068b0

constexpr int kFirstControlChar = 0x20;         // below it, a line ends

} // namespace

// FS2 0x00207ef0

PauseMenuState::PauseMenuState()
{
}

// FS2 0x00207ea0
// callcheck: operator_delete

PauseMenuState::~PauseMenuState()
{
}

// FS2 0x00207440

void PauseMenuState::clearList()
{
    if (m_entries[NoticeKind_Event].state != NoticeState_Over)
        m_entries[NoticeKind_Event] = NoticeEntry();
}

// The entries a script pinned on screen are off; true when there was one.
// FS2 0x00207490

bool PauseMenuState::clearPinned()
{
    bool cleared = false;
    for (NoticeEntry &entry : m_entries) {
        if (entry.state == NoticeState_Showing && entry.pinned != 0) {
            entry = NoticeEntry();
            cleared = true;
        }
    }
    return cleared;
}

// FS2 0x002078d0
// callcheck: getSeconds

bool PauseMenuState::isActive()
{
    const float now = gStageScene->clock->getSeconds();
    for (const NoticeEntry &entry : m_entries) {
        if (entry.canDismiss(now))
            return true;
    }
    return false;
}

// The queue's first free place; null with all 11 taken. Inline in the ROM.

PauseNotice *PauseMenuState::freeNotice()
{
    for (PauseNotice &notice : m_notices) {
        if (notice.text == nullptr)
            return &notice;
    }
    return nullptr;
}

bool PauseMenuState::post(int kind, const u_short *text, int frames, int delay, int hold)
{
    PauseNotice *notice = freeNotice();
    if (frames < 0 || delay < 0 || hold < 0 || notice == nullptr)
        return false;
    notice->kind = kind;
    notice->serial = m_serial++;
    notice->frames = frames;
    notice->delay = delay;
    notice->hold = hold;
    notice->text = text;
    return true;
}

// The moment's kind for what stays on the second list.
static int messageKind()
{
    return gStageScene->isEventActive() ? PauseMenuState::NoticeKind_Event : PauseMenuState::NoticeKind_Message;
}

// FS2 0x00207970
// callcheck: isEventActive (its answer is unused)

bool PauseMenuState::addPurpose(const u_short *text, int frames, int delay)
{
    return post(NoticeKind_Purpose, text, frames == 0 ? kNoticeForever : frames, delay, frames);
}

// FS2 0x00207aa0
// callcheck: isEventActive

bool PauseMenuState::addNotice(const u_short *text, int frames, int arg, int level)
{
    return post(messageKind(), text, frames, arg, level);
}

// FS2 0x00207bc0
// callcheck: isEventActive

bool PauseMenuState::addMessage(const u_short *text, int frames, int delay)
{
    return post(messageKind(), text, frames == 0 ? kNoticeForever : frames, delay, frames);
}

// FS2 0x00207cf0
// callcheck: isEventActive (its answer is unused)

bool PauseMenuState::addHint(const u_short *text, int frames, int delay)
{
    return post(NoticeKind_Hint, text, frames == 0 ? kNoticeForever : frames, delay, frames);
}

// FS2 0x00207e20

bool PauseMenuState::hasNotice(int kind)
{
    if (m_entries[kind].state < NoticeState_Over)
        return true;
    for (const PauseNotice &notice : m_notices) {
        if (notice.text != nullptr && notice.kind == kind)
            return true;
    }
    return false;
}

// FS2 0x00207110

void NoticeEntry::show(float now, const PauseNotice &notice)
{
    int line = 0;
    if (*(const char *)notice.text != 0) {
        const u_short *text = notice.text;
        for (; line < kNoticeLines;) {
            int length = 0;
            while (length != kNoticeLineSize - 1 && text[length] >= kFirstControlChar) {
                lines[line][length] = text[length];
                length++;
            }
            lines[line][length] = 0;
            const u_short end = text[length];
            line++;
            const int low = (end | end >> 8) & 0xff;
            if (low == 0 || low >= kFirstControlChar)
                break;
            text += length + 1;
        }
    }
    for (; line < kNoticeLines; line++)
        lines[line][0] = 0;

    start = now + (float)notice.delay / kNoticeFps;
    end = now + (float)notice.delay / kNoticeFps + (float)notice.frames / kNoticeFps;
    hold = start + (float)notice.hold / kNoticeFps;
    dismissed = 0;
    pinned = notice.frames == kNoticeForever && notice.hold == kNoticeForever;
    state = stateAt(now);
}

// FS2 0x00207500
// callcheck: getSeconds, isEventActive, getInputFocus, bitMask, currentMission, setPurposeText, FUN_00207110, FUN_002896a0, setSlot, setSlot, setSlot, setSlot, setSlot, setSlot

void PauseMenuState::update()
{
    StageScene *scene = gStageScene;
    const float now = scene->clock->getSeconds();
    const bool eventActive = scene->isEventActive();
    bool showing = false;
    for (int kind = 0; kind < kNoticeKinds; kind++) {
        if (eventActive != (kind == NoticeKind_Event))
            continue;
        NoticeEntry &entry = m_entries[kind];
        if (scene->getInputFocus() == this && entry.state == NoticeState_Showing) {
            const GamePad &pad = gGameSystem->m_pad0;
            if ((pad.m_pressed & bitMask(pad.actionButton(PadAction_Use))) != 0)
                entry.dismissed |= entry.canDismiss(now);
        }
        entry.state = entry.stateAt(now);
        if (entry.state == NoticeState_Over) {
            PauseNotice *oldest = nullptr;
            for (PauseNotice &notice : m_notices) {
                if (notice.text != nullptr && notice.kind == kind
                    && (oldest == nullptr || notice.serial <= oldest->serial))
                    oldest = &notice;
            }
            if (oldest != nullptr) {
                if (kind == NoticeKind_Purpose)
                    scene->currentMission()->setPurposeText(oldest->text);
                entry.show(now, *oldest);
                *oldest = PauseNotice();
            }
        } else if (entry.state == NoticeState_Showing) {
            showing = true;
        }
    }
    m_showing = showing;

    NoticeEntry *latest = nullptr;
    if (m_showing) {
        float latestStart = FLT_MIN;
        for (int kind = 0; kind < kNoticeKinds; kind++) {
            NoticeEntry &entry = m_entries[kind];
            if (eventActive == (kind == NoticeKind_Event) && entry.state == NoticeState_Showing
                && latestStart < entry.start) {
                latestStart = entry.start;
                latest = &entry;
            }
        }
    }

    ScenePool *pool = scene->pool;
    pool->setColour(m_colour, ScenePool::kAllSlots);
    int lines = 0;
    if (latest != nullptr) {
        while (lines < kNoticeLines && latest->lines[lines][0] != 0)
            lines++;
    }
    if (scene->gameOver != nullptr)
        lines = 0;
    if (lines == 2) {
        pool->setSlot(2, latest->lines[0]);
        pool->setSlot(3, latest->lines[1]);
    } else if (lines == 1) {
        pool->setSlot(2, kEmptyLine);
        pool->setSlot(3, latest->lines[0]);
    } else {
        pool->setSlot(2, nullptr);
        pool->setSlot(3, nullptr);
    }
}
