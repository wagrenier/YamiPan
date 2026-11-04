/* ==========================================================================
 *  game/prgmccard.cpp
 *
 *  The load and save screens' logic, 0x2b85a0..0x2b9ef0 (game/prgmccard.h).
 *  Both are state machines over gMenuMemcard: start a request, wait for
 *  Memcard::update, show the outcome through the layout's variables. The
 *  save screen keeps asking for the card's info while the player chooses,
 *  so that a card pulled at any question lands on McMessage_NoCard.
 * ======================================================================== */
#include "game/prgmccard.h"

#include <string.h>

#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/memcard.h"
#include "game/pad.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "game/uilayout.h"
#include "sdk/libmc.h"

// The sound slot the screens' decide and cancel play on.
static const int kMcSoundSlot = 3;

// ──────────────────────────────────────────────────────────────────────
// The inlines both screens share.

bool PrgMcCard::decidePressed() const
{
    return (gGameSystem->m_pad0.m_pressed & bitMask(ButtonMap::defaultButton(0))) != 0;
}

bool PrgMcCard::cancelPressed() const
{
    return (gGameSystem->m_pad0.m_pressed & bitMask(ButtonMap::defaultButton(1))) != 0;
}

void PrgMcCard::resetPercent()
{
    m_targetPercent = 0;
    m_shownPercent = m_targetPercent;
}

// The bar never runs ahead of the request.
void PrgMcCard::followProgress()
{
    float progress = gMenuMemcard->m_progress;
    if (progress > 1.0f)
        progress = 1.0f;
    m_targetPercent = (int)(progress * 100.0f);
    if (m_targetPercent < m_shownPercent)
        m_shownPercent = m_targetPercent;
}

void PrgMcCard::setPercentDone()
{
    m_shownPercent = 100;
    m_targetPercent = m_shownPercent;
}

void PrgMcCard::askYesNo(int cursor)
{
    setVar(McVar_YesNoCursor, cursor);
    setVar(McVar_YesNoCtrl, kMcYesNoShown);
    setVar(McVar_YesNoAnswer, -1);
}

// The list of saves, each slot's variables from the card's files.
void PrgMcCard::showSlots(int ctrl)
{
    setVar(McVar_SelectCtrl, ctrl);
    setVar(McVar_SelectAnswer, -1);
    for (int i = 0; i < kSaveFiles; i++) {
        const SaveFile *file = gMenuMemcard->m_files[i];
        if (file->isFree()) {
            setVar(McVar_SlotFlag + i, kMcSlotFree);
        } else if (file->isBroken()) {
            setVar(McVar_SlotFlag + i, kMcSlotBroken);
        } else {
            setVar(McVar_SlotFlag + i, file->select.stage);
            int kind = StageKind_Easy;
            if (file->select.kind == StageKind_Hard)
                kind = StageKind_Hard;
            else if (file->select.kind == StageKind_Normal)
                kind = StageKind_Normal;
            setVar(McVar_SlotType + i, file->options.autoSave == 0 ? kind : kind + kMcSlotTypeAutoSave);
        }
    }
}

void PrgMcCard::showPrompt(int prompt)
{
    setVar(McVar_Message, McMessage_SavePrompt + prompt);
}

void PrgMcCard::showSaveMessage(int message)
{
    setVar(McVar_Message, message);
    m_state = McSave_Message;
}

// The card went away under one of the save screen's questions.
void PrgMcCard::saveCardLost()
{
    showSaveMessage(McMessage_NoCard);
    setVar(McVar_SelectCtrl, McSelect_Hidden);
    setVar(McVar_YesNoCtrl, 0);
    setVar(McVar_YesNoAnswer, -1);
}

void PrgMcCard::closeScreen(int barVar)
{
    m_timer = kMcCloseFrames;
    setVar(barVar, 0);
    setVar(McVar_SelectCtrl, McSelect_Hidden);
    setVar(McVar_SelectAnswer, -1);
    setVar(McVar_YesNoCtrl, 0);
    setVar(McVar_YesNoAnswer, -1);
    setVar(McVar_Message, McMessage_None);
    m_state = kMcStateClosing;
}

bool PrgMcCard::stepClosing()
{
    m_timer--;
    if (m_timer < 0) {
        m_layout->m_result = m_result;
        m_layout->m_done = 1;
        return true;
    }
    return false;
}

void PrgMcCard::close(int result)
{
    m_result = result;
    m_state = kMcStateClose;
}

// ──────────────────────────────────────────────────────────────────────
// Nothing to draw: the layout shows the variables.
// FS2 0x002b85a0

void PrgMcCard::draw(UiDrawer *drawer)
{
    (void)drawer;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002b85b0

void PrgMcCard::advance()
{
    int step = (m_targetPercent - m_shownPercent) / 4;
    if (step < 1)
        step = 1;
    m_shownPercent += step;
    if (m_targetPercent < m_shownPercent)
        m_shownPercent = m_targetPercent;
    setVar(McVar_BarCtrl, m_shownPercent);

    bool cardDone = gMenuMemcard->update();
    switch (m_mode) {
    case 0:
        stepLoad(cardDone);
        break;
    case 1:
    case 2:
        stepSave(cardDone, 0);
        break;
    case 3:
        stepSave(cardDone, 1);
        break;
    case 4:
        stepSave(cardDone, 2);
        break;
    case 5:
        stepSave(cardDone, 3);
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002b86e0
// callcheck: bitMask, defaultButton (decidePressed, cancelPressed)

bool PrgMcCard::stepSave(bool cardDone, int prompt)
{
    Memcard *card = gMenuMemcard;

    switch (m_state) {
    case kMcStateClosing:
        return stepClosing();

    case kMcStateClose:
        closeScreen(McVar_SaveBar);
        break;

    case McSave_DoneWait:
        if (decidePressed()) {
            m_layout->playSound(kMcSoundSlot, "SYS_FIX");
            close(1);
        } else {
            m_timer--;
            if (m_timer < 0)
                close(1);
        }
        break;

    case McSave_ShowDone:
        m_timer = kMcSavedFrames;
        m_state = McSave_DoneWait;
        break;

    case McSave_WaitWrite:
        if (!cardDone) {
            followProgress();
            break;
        }
        setPercentDone();
        if (card->m_outcome == MemcardOutcome_Ok) {
            m_state = McSave_ShowDone;
            setVar(McVar_Message, McMessage_Saved);
            SaveFile *image = card->m_image;
            memcardSetStamp(image->stamp.value);
            int index = getVar(McVar_SelectAnswer);
            SaveFile *file = card->m_files[index];
            if (file != nullptr) {
                // The list shows the new save without another read.
                file->assignHead(*image);
                memcpy(file->reserved, image->reserved, sizeof(file->reserved));
                file->magic = image->magic;
                file->sum = image->sum;
                setVar(McVar_SlotFlag + index, file->select.stage);
            }
            setVar(McVar_SelectCtrl, McSelect_Saved);
        } else {
            showSaveMessage(McMessage_SaveFailed);
        }
        break;

    case McSave_CheckSpace:
        if (card->m_info.free < memcardRequiredKB()) {
            showSaveMessage(McMessage_NoSpace);
            break;
        }
        m_state = McSave_Write;
        [[fallthrough]];
    case McSave_Write:
        resetPercent();
        setVar(McVar_Message, McMessage_Saving);
        card->startSave(getVar(McVar_SelectAnswer));
        m_state = McSave_WaitWrite;
        break;

    case McSave_OverwriteAnswer: {
        int answer = getVar(McVar_YesNoAnswer);
        if (!cardDone)
            break;
        if (card->m_info.type != sceMcTypePS2) {
            saveCardLost();
        } else if (answer == -1) {
            card->startCardInfo();
        } else {
            setVar(McVar_YesNoCtrl, 0);
            m_state = answer == 0 ? McSave_Prompt : McSave_Write;
        }
        break;
    }

    case McSave_OverwriteInfo:
        m_state = McSave_OverwriteAnswer;
        card->startCardInfo();
        break;

    case McSave_WaitFormat:
        if (!cardDone)
            break;
        if (card->m_outcome == MemcardOutcome_Ok || card->m_outcome == MemcardOutcome_Unusable)
            m_state = McSave_Write;
        else
            showSaveMessage(McMessage_FormatFailed);
        break;

    case McSave_FormatAnswer: {
        int answer = getVar(McVar_YesNoAnswer);
        if (!cardDone)
            break;
        if (card->m_info.type != sceMcTypePS2) {
            saveCardLost();
        } else if (answer == -1) {
            card->startCardInfo();
        } else {
            setVar(McVar_YesNoCtrl, 0);
            if (answer == 0) {
                m_state = McSave_Prompt;
            } else {
                setVar(McVar_Message, McMessage_Formatting);
                card->startFormat();
                m_state = McSave_WaitFormat;
            }
        }
        break;
    }

    case McSave_FormatInfo:
        m_state = McSave_FormatAnswer;
        card->startCardInfo();
        break;

    case McSave_SlotAnswer: {
        int answer = getVar(McVar_YesNoAnswer);
        if (!cardDone)
            break;
        if (card->m_info.type != sceMcTypePS2) {
            saveCardLost();
        } else if (answer == -1) {
            card->startCardInfo();
        } else if (answer == 0) {
            setVar(McVar_SelectCtrl, McSelect_Save);
            setVar(McVar_SelectAnswer, -1);
            setVar(McVar_YesNoCtrl, 0);
            m_state = McSave_ListInfo;
        } else {
            setVar(McVar_YesNoCtrl, 0);
            if ((u_char)card->m_info.format == 0) {
                m_state = McSave_FormatInfo;
                setVar(McVar_Message, McMessage_FormatAsk);
                askYesNo(0);
            } else if (card->m_files[getVar(McVar_SelectAnswer)]->isFree()) {
                m_state = McSave_CheckSpace;
            } else {
                setVar(McVar_Message, McMessage_OverwriteAsk);
                askYesNo(0);
                m_state = McSave_OverwriteInfo;
            }
        }
        break;
    }

    case McSave_SlotInfo:
        m_state = McSave_SlotAnswer;
        card->startCardInfo();
        break;

    case McSave_List: {
        int selected = getVar(McVar_SelectAnswer);
        if (selected == -1 && cancelPressed()) {
            m_state = McSave_WaitPrompt;
            m_layout->playSound(kMcSoundSlot, "SYS_CANCEL");
            break;
        }
        if (!cardDone)
            break;
        if (card->m_info.type != sceMcTypePS2) {
            saveCardLost();
        } else if (selected == -1) {
            card->startCardInfo();
        } else {
            askYesNo(1);
            m_state = McSave_SlotInfo;
        }
        break;
    }

    case McSave_ListInfo:
        m_state = McSave_List;
        card->startCardInfo();
        break;

    case McSave_Message:
        if (decidePressed()) {
            m_layout->playSound(kMcSoundSlot, "SYS_FIX");
            m_state = McSave_Prompt;
        }
        break;

    case McSave_WaitCheck:
        if (!cardDone)
            break;
        if (card->m_outcome == MemcardOutcome_Failed) {
            showSaveMessage(McMessage_CheckFailed);
        } else if (card->m_outcome != MemcardOutcome_Unusable && card->m_outcome != MemcardOutcome_Ok) {
            showSaveMessage(McMessage_NoCard);
        } else {
            m_state = McSave_ListInfo;
            showSlots(McSelect_Save);
        }
        break;

    case McSave_WaitInfo:
        if (!cardDone)
            break;
        if (card->m_info.type != sceMcTypePS2) {
            showSaveMessage(McMessage_NoCard);
            setVar(McVar_SelectCtrl, McSelect_Hidden);
            break;
        }
        m_state = McSave_Check;
        [[fallthrough]];
    case McSave_Check:
        setVar(McVar_Message, McMessage_Checking);
        card->startCheckAll();
        m_state = McSave_WaitCheck;
        break;

    case McSave_WaitAnswer: {
        int answer = getVar(McVar_YesNoAnswer);
        if (answer == -1)
            break;
        if (answer == 0) {
            close(0);
            break;
        }
        m_state = McSave_Info;
    }
        [[fallthrough]];
    case McSave_Info:
        resetPercent();
        card->startCardInfo();
        m_state++;
        break;

    case McSave_WaitPrompt:
        if (!cardDone)
            break;
        m_state = McSave_Prompt;
        [[fallthrough]];
    case McSave_Prompt:
        resetPercent();
        showPrompt(prompt);
        setVar(McVar_SelectCtrl, McSelect_Hidden);
        askYesNo(1);
        m_state = McSave_WaitAnswer;
        break;

    case McSave_AutoShowDone:
        m_timer = kMcAutoSavedFrames;
        m_state = McSave_DoneWait;
        break;

    case McSave_AutoWaitWrite:
        if (!cardDone) {
            followProgress();
            break;
        }
        setPercentDone();
        if (card->m_outcome == MemcardOutcome_Ok) {
            m_state = McSave_AutoShowDone;
            setVar(McVar_Message, McMessage_Saved);
            memcardSetStamp(card->m_image->stamp.value);
        } else {
            showSaveMessage(McMessage_SaveFailed);
        }
        break;

    case McSave_AutoWrite:
        resetPercent();
        setVar(McVar_Message, McMessage_Saving);
        card->startSave(getVar(McVar_SelectAnswer));
        m_state = McSave_AutoWaitWrite;
        break;

    case McSave_AutoDead: {
        SaveStamp stamp;
        memcardGetStamp(&stamp.value);
        break;
    }

    case McSave_AutoWaitRead: {
        if (!cardDone)
            break;
        if (card->m_outcome == MemcardOutcome_Failed) {
            showSaveMessage(McMessage_CheckFailed);
            break;
        }
        if (card->m_outcome == MemcardOutcome_Unusable) {
            showSaveMessage(McMessage_AutoSaveFailed);
            break;
        }
        if (card->m_outcome != MemcardOutcome_Ok) {
            showSaveMessage(McMessage_NoCard);
            break;
        }
        SaveStamp stamp;
        memcardGetStamp(&stamp.value);
        int index = stamp.f.index;
        SaveFile *file = card->m_files[index];
        if (!file->isValid() || stamp.value != file->stamp.value) {
            showSaveMessage(McMessage_AutoSaveFailed);
            break;
        }
        setVar(McVar_SelectAnswer, index);
        m_state = McSave_AutoWrite;
        break;
    }

    case McSave_AutoWaitInfo:
        if (!cardDone)
            break;
        if (card->m_info.type != sceMcTypePS2) {
            showSaveMessage(McMessage_NoCard);
            break;
        }
        m_state = McSave_AutoRead;
        [[fallthrough]];
    case McSave_AutoRead: {
        setVar(McVar_Message, McMessage_Checking);
        SaveStamp stamp;
        memcardGetStamp(&stamp.value);
        card->startLoad(stamp.f.index);
        m_state = McSave_AutoWaitRead;
        break;
    }

    case McSave_AutoInfo:
        resetPercent();
        card->startCardInfo();
        m_state = McSave_AutoWaitInfo;
        break;

    case McSave_NoStampWait:
        if (!decidePressed())
            break;
        m_layout->playSound(kMcSoundSlot, "SYS_FIX");
        m_state = McSave_NoStampEnd;
        [[fallthrough]];
    case McSave_NoStampEnd:
        m_state = McSave_Info;
        break;

    case kMcStateStart:
        resetPercent();
        setVar(McVar_SaveBar, 1);
        m_state = McSave_Choose;
        [[fallthrough]];
    case McSave_Choose: {
        if (getGameOptions()->autoSave != 0) {
            SaveStamp stamp;
            memcardGetStamp(&stamp.value);
            if (stamp.value == (u_long)-1) {
                setVar(McVar_Message, McMessage_NoStamp);
                m_state = McSave_NoStampWait;
            } else {
                m_state = McSave_AutoInfo;
            }
            break;
        }
        showPrompt(prompt);
        m_state = McSave_Prompt;
        break;
    }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// The copy of a SaveFile's sum-covered part: the implicit assignment of a
// struct the ROM has here, emitted out of line for stepSave.
// FS2 0x002b9710
// callcheck: operator= (the implicit copy assignments)

void SaveFile::assignHead(const SaveFile &src)
{
    save = src.save;
    select = src.select;
    options = src.options;
    flags = src.flags;
    extra = src.extra;
    stamp = src.stamp;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002b97b0
// callcheck: bitMask, defaultButton (decidePressed, cancelPressed)

bool PrgMcCard::stepLoad(bool cardDone)
{
    Memcard *card = gMenuMemcard;

    switch (m_state) {
    case kMcStateClosing:
        return stepClosing();

    case kMcStateClose:
        closeScreen(McVar_LoadBar);
        break;

    case McLoad_Message:
        if (decidePressed()) {
            m_layout->playSound(kMcSoundSlot, "SYS_FIX");
            close(0);
        }
        break;

    case McLoad_Confirm: {
        int answer = getVar(McVar_YesNoAnswer);
        if (answer == -1)
            break;
        if (answer == 0) {
            setVar(McVar_SelectCtrl, McSelect_Load);
            setVar(McVar_SelectAnswer, -1);
            setVar(McVar_YesNoCtrl, 0);
            m_state = McLoad_List;
        } else {
            card->m_files[getVar(McVar_SelectAnswer)]->apply();
            close(1);
        }
        break;
    }

    case McLoad_List:
        if (getVar(McVar_SelectAnswer) == -1) {
            if (cancelPressed()) {
                m_layout->playSound(kMcSoundSlot, "SYS_CANCEL");
                close(0);
            }
        } else {
            askYesNo(1);
            m_state = McLoad_Confirm;
        }
        break;

    case McLoad_WaitCheck:
        if (!cardDone) {
            followProgress();
            break;
        }
        setPercentDone();
        if (card->m_outcome == MemcardOutcome_Failed) {
            setVar(McVar_Message, McMessage_LoadFailed);
            m_state = McLoad_Message;
        } else if (card->m_outcome == MemcardOutcome_Unusable) {
            setVar(McVar_Message, McMessage_InsertSaveCard);
            m_state = McLoad_Message;
        } else if (card->m_outcome == MemcardOutcome_Ok) {
            m_state = McLoad_List;
            showSlots(McSelect_Load);
        } else {
            setVar(McVar_Message, McMessage_NoCard);
            m_state = McLoad_Message;
        }
        break;

    case McLoad_WaitInfo:
        if (!cardDone)
            break;
        if (card->m_info.type != sceMcTypePS2) {
            setVar(McVar_Message, McMessage_NoCard);
            m_state = McLoad_Message;
            break;
        }
        if ((u_char)card->m_info.format == 0) {
            setVar(McVar_Message, McMessage_InsertSaveCard);
            m_state = McLoad_Message;
            break;
        }
        m_state = McLoad_Check;
        [[fallthrough]];
    case McLoad_Check:
        setVar(McVar_Message, McMessage_Loading);
        card->startCheckAll();
        m_state = McLoad_WaitCheck;
        break;

    case kMcStateStart:
        resetPercent();
        setVar(McVar_LoadBar, 1);
        card->startCardInfo();
        m_state++;
        break;
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002b9ce0
// callcheck: ~Memcard, operator_delete (the delete-expression, the deleting destructor)

PrgMcCard::~PrgMcCard()
{
    delete gMenuMemcard;
    gMenuMemcard = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002b9d60
// callcheck: UiWidget, operator_new, Memcard (the base, the new-expression)

PrgMcCard::PrgMcCard(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : UiWidget(seq, layout, data, def, arg)
{
    const char *first = data->getString((int)def->param[0]);
    int index = data->findVarIndex(first);
    for (int i = 0; i < McVar_Count; i++)
        m_vars[i] = data->getVar(index + i);

    // McVar_SelectCursor, McVar_SelectStatus and McVar_YesNoStatus keep the
    // file's values.
    for (int i = McVar_Message; i < McVar_SlotType + kSaveFiles; i++)
        setVar(i, 0);
    setVar(McVar_BarCtrl, 0);
    setVar(McVar_YesNoCursor, 0);

    m_mode = layout->m_arg;
    m_state = kMcStateStart;
    m_28 = -1;
    gMenuMemcard = new Memcard();
    m_shownPercent = 0;
    m_targetPercent = 0;
}
