/* ==========================================================================
 *  game/prgmccard.h
 *
 *  "prgMcCard" (game/prgmccard.cpp, 0x2b85a0..0x2b9ef0), the widget that is
 *  the logic of 2dEU/mcBase%s.d2b, the load and save screens: it drives the
 *  menu's save manager gMenuMemcard and talks to the layout through a block
 *  of S2D variables only. The layout draws from them and writes the player's
 *  choices back. The file name is the port's.
 * ======================================================================== */
#ifndef _GAME_PRGMCCARD_H
#define _GAME_PRGMCCARD_H

#include "game/s2ddata.h"
#include "game/uisequence.h"

// S2D variables in a row, from the one named by the widget's p0: the file's
// own names in the comments. "rf" the widget writes, "af" the layout answers
// in, "s2s" belong to the layout's selector widgets.
enum McVar
{
    McVar_Message      = 0,     // rfMSG_FLAG: McMessage
    McVar_SaveBar      = 1,     // rfSAVEBAR: 1 the save screen's header and bar
    McVar_LoadBar      = 2,     // rfLOADBAR: 1 the "Load Game" header and bar
    McVar_Select       = 3,     // rfSELECT: cleared by the ctor only
    McVar_SelectCtrl   = 4,     // rfSELECT_CTRL: McSelectCtrl
    McVar_SelectAnswer = 5,     // afSELECT_CTRL: the slot picked, -1 none
    McVar_YesNo        = 6,     // rfYESNO: cleared by the ctor only
    McVar_YesNoCtrl    = 7,     // rfYESNO_CTRL: 0 hidden, kMcYesNoShown
    McVar_YesNoAnswer  = 8,     // afYESNO_CTRL: -1 none, 0 no, else yes
    McVar_SlotFlag     = 9,     // SCN01_FLAG..04: StageSelect::stage, or kMcSlotFree / kMcSlotBroken
    McVar_SlotType     = 13,    // SCN01_TYPE..04: StageKind, + kMcSlotTypeAutoSave
    McVar_SelectCursor = 17,    // s2sSELECT_CTRL_SEL: the file's value kept
    McVar_SelectStatus = 18,    // s2sSELECT_STATUS: the file's value kept
    McVar_YesNoCursor  = 19,    // s2sYESNO_CTRL_SEL: the answer the yes / no opens on: 0, 1
    McVar_YesNoStatus  = 20,    // s2sYESNO_STATUS: the file's value kept
    McVar_BarCtrl      = 21,    // rfBAR_CTRL: the bar, m_shownPercent
    McVar_Count        = 22,
};

// McVar_SelectCtrl.
enum McSelectCtrl
{
    McSelect_Hidden = 0,
    McSelect_Saved  = 1,        // a save just went in: the list stays, with no cursor
    McSelect_Load   = 2,
    McSelect_Save   = 3,
};

constexpr int kMcYesNoShown = 2;
constexpr int kMcSlotFree = 0xff;
constexpr int kMcSlotBroken = 0xfe;

// McVar_SlotType: the text is "Difficulty: <StageKind>", with this added
// "Autosave: ON" (save the layout's 0x18..0x1d below).
constexpr int kMcSlotTypeAutoSave = 3;

// McVar_Message: what the screen says (each value seen on screen, one text
// image of the layout each). The values the code never sets are the layout's:
// 0xa "The saved data is corrupt. Erase ...", 0xe "Format successful.", 0x16
// "Data is corrupt.", 0x17 "No data.", and 0x15, 0x18..0x1d, 0x21 a save's
// Difficulty / Autosave / Stages cleared lines.
enum McMessage
{
    McMessage_None           = 0,
    McMessage_SavePrompt     = 1,       // "Save play data up to this point?"
    McMessage_ArchivePrompt  = 2,       // "Archive conditions have been changed. Save data?"
    McMessage_OptionsPrompt  = 3,       // "Game settings have been changed. Save data?"
    McMessage_KunitoriPrompt = 4,       // "Kunitoris" play data has been updated. Save data?
    McMessage_NoCard         = 5,       // "No memory card ... inserted in MEMORY CARD slot 1."
    McMessage_AutoSaveFailed = 6,       // "Autosave failed on memory card ... To proceed"
    McMessage_NoStamp        = 7,       // "It is necessary to manually save data after activating the Autosave feature."
    McMessage_FormatAsk      = 8,       // "... is unformatted. Format?"
    McMessage_NoSpace        = 9,       // "Insufficient space available ..."
    McMessage_Formatting     = 0xb,     // "Formatting. Do not remove ..."
    McMessage_Loading        = 0xc,     // "Loading data. Do not remove ..."
    McMessage_LoadFailed     = 0xd,     // "Load failed. Please check ..."
    McMessage_FormatFailed   = 0xf,     // "Format failed. Please check ..."
    McMessage_OverwriteAsk   = 0x10,    // "Existing play data will be overwritten. Okay?"
    McMessage_Saving         = 0x11,    // "Saving data. Do not remove ..."
    McMessage_SaveFailed     = 0x12,    // "Save failed. Please check ..."
    McMessage_Saved          = 0x13,    // "Save successful."
    McMessage_InsertSaveCard = 0x14,    // "Insert a memory card ... with FORBIDDEN SIREN 2 save data"
    McMessage_Checking       = 0x1f,    // "Checking the save data. Do not remove ..."
    McMessage_CheckFailed    = 0x20,    // "Checking failed. Please check ..."
};

// PrgMcCard::m_state of both screens.
constexpr int kMcStateStart = 0;
constexpr int kMcStateClose = 7000;     // clears the variables, then kMcCloseFrames
constexpr int kMcStateClosing = 7010;

// PrgMcCard::m_state on the load screen. The numbers are the ROM's.
enum McLoadState
{
    McLoad_WaitInfo  = 1,
    McLoad_Check     = 5,
    McLoad_WaitCheck = 10,
    McLoad_List      = 20,
    McLoad_Confirm   = 30,
    McLoad_Message   = 1000,            // until decide, then the layout answers 0
};

// PrgMcCard::m_state on the save screen. The numbers are the ROM's.
enum McSaveState
{
    McSave_Choose        = 10,          // autoSave or the prompt
    McSave_NoStampWait   = 1000,        // McMessage_NoStamp until decide, then the ordinary save
    McSave_NoStampEnd    = 1010,        // leaves at once
    // autoSave: over the loaded save.
    McSave_AutoInfo      = 2000,
    McSave_AutoWaitInfo  = 2005,
    McSave_AutoRead      = 2008,
    McSave_AutoWaitRead  = 2010,
    McSave_AutoDead      = 2020,        // nothing enters it
    McSave_AutoWrite     = 2500,
    McSave_AutoWaitWrite = 2510,
    McSave_AutoShowDone  = 2900,
    // The ordinary save.
    McSave_WaitPrompt    = 2999,        // back from the list: the card idle, then the prompt
    McSave_Prompt        = 3000,
    McSave_WaitAnswer    = 3010,
    McSave_Info          = 3020,
    McSave_WaitInfo      = 3021,
    McSave_Check         = 3025,
    McSave_WaitCheck     = 3030,
    McSave_Message       = 3040,        // until decide, then the prompt again
    McSave_ListInfo      = 3100,
    McSave_List          = 3105,        // polling the card while the player picks
    McSave_SlotInfo      = 3120,
    McSave_SlotAnswer    = 3125,
    McSave_FormatInfo    = 3200,
    McSave_FormatAnswer  = 3205,
    McSave_WaitFormat    = 3210,
    McSave_OverwriteInfo = 3300,
    McSave_OverwriteAnswer = 3305,
    McSave_CheckSpace    = 3400,
    McSave_Write         = 3500,
    McSave_WaitWrite     = 3510,
    McSave_ShowDone      = 5000,
    McSave_DoneWait      = 5010,        // decide or the timer, then the layout answers 1
};

// The frames each wait lasts.
constexpr int kMcCloseFrames = 8;
constexpr int kMcSavedFrames = 125;     // the saved message
constexpr int kMcAutoSavedFrames = 50;  // the same after an auto-save

// "prgMcCard" (0x94 bytes, Vt_00319520). p0: the first McVar's name. The
// layout's arg is SaveDialog_Load (the load screen) or a SaveDialogReason.
// layout: host -- the base grows on a 64-bit host; the EE offsets are in
// the comments.
class PrgMcCard : public UiWidget
{
public:
    PrgMcCard(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg); // FS2 0x002b9d60
    ~PrgMcCard();                                                        // FS2 0x002b9ce0: deleting (obj, 1)
    void draw(UiDrawer *drawer);                                         // FS2 0x002b85a0
    void advance();                                                      // FS2 0x002b85b0

    // One frame of a screen; true when the layout has been answered.
    // cardDone: Memcard::update's answer. prompt: added to McMessage_SavePrompt.
    bool stepSave(bool cardDone, int prompt);                            // FS2 0x002b86e0
    bool stepLoad(bool cardDone);                                        // FS2 0x002b97b0

    int  getVar(int var) const { return m_vars[var]->value; }
    void setVar(int var, int value) { m_vars[var]->value = value; }

private:
    // Inline in the ROM, each several times.
    bool decidePressed() const;
    bool cancelPressed() const;
    void resetPercent();
    void followProgress();
    void setPercentDone();
    void askYesNo(int cursor);
    void showSlots(int ctrl);
    void showPrompt(int prompt);
    void showSaveMessage(int message);
    void saveCardLost();
    void closeScreen(int barVar);
    bool stepClosing();
    void close(int result);

    int     m_mode;             // FS2 +0x20: the layout's arg
    int     m_state;            // FS2 +0x24: McLoadState, McSaveState
    int     m_28;               // FS2 +0x28: -1, never read
    S2dVar *m_vars[McVar_Count]; // FS2 +0x2c
    int     m_shownPercent;     // FS2 +0x84: eases up to m_targetPercent
    int     m_targetPercent;    // FS2 +0x88
    int     m_timer;            // FS2 +0x8c
    int     m_result;           // FS2 +0x90: the layout's answer: 1 loaded / saved
};

#endif /* _GAME_PRGMCCARD_H */
