// game/screen.cpp -- the overlay thread, the modal layout runner and the
// screens and dialogs built on it (game/screen.h).

#include "game/screen.h"

#include "game/gamehooks.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/memcard.h"
#include "game/pad.h"
#include "game/stageresult.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "sdk/scetypes.h"

// The button that cancels a cancelable layout.
constexpr u_int kLayoutCancelButton = 1u << PadBit_Cross;

static const char kBgMovieLayout[] = "2dEU/BGMOVIEEU.d2b";
static const char kArchiveBaseDir[] = "2dEU/";

static int      sGameOverResult = GameOverChoice_Running; /* sdata 0x31abec (gp-0x7c84) */
static int      sNoticeResult = -1;     /* sdata 0x31abf0 (gp-0x7c80): -1 while up */
static UiLayout *sScreenLayout;         /* sbss 0x31ae48 (gp-0x7a28): the BG movie screen */
static u_char   sScreenShown;           /* sbss 0x31ae4c (gp-0x7a24) */
static UiLayout *sLoadingLayout;        /* sbss 0x31ae50 (gp-0x7a20): the loading indicator */
static u_char   sLoadingWanted;         /* sbss 0x31ae54 (gp-0x7a1c): asked for this frame */

// One overlay layout's frame in packet list 11. Inline in the ROM (twice in
// overlayThread).
static inline void drawOverlayLayout(UiLayout *layout)
{
    layout->advance();
    layout->update();
    sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_Overlay);
    void *texList = gfxGetTexArea()->getList(PacketList_Overlay);
    layout->draw(packet, texList);
    gGameSystem->m_packets.closeList();
}

// ──────────────────────────────────────────────────────────────────────
// The stage's e screen, when the play record has one.
// FS2 0x002676e0

void runStageScreenE()
{
    const char *path = stageScreenPathE(getStageSelect());
    if (path != nullptr)
        runLayoutFile(path, kLayoutNoArg, false, nullptr, true);
}

// ──────────────────────────────────────────────────────────────────────
// The stage's h screen for the play record's difficulty, when it has one.
// FS2 0x00267730

void runStageScreenH()
{
    const char *path = stageScreenPathH(getStageSelect());
    if (path != nullptr)
        runLayoutFile(path, kLayoutNoArg, false, nullptr, true);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00267780

void overlayThreadStart()
{
    if (threadSlotState(ThreadSlot_Overlay) == ThreadState_Free)
        threadSlotCreate(overlayThread, ThreadSlot_Overlay, nullptr, overlayThreadCleanup, 0, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// Slot 6's cleanup: both layouts go.
// FS2 0x002677e0
// callcheck: ~UiLayout (the delete-expressions' deleting destructor)

void overlayThreadCleanup(void *arg)
{
    (void)arg;
    if (sScreenLayout != nullptr) {
        delete sScreenLayout;
        sScreenLayout = nullptr;
    }
    sScreenShown = 0;
    if (sLoadingLayout != nullptr) {
        delete sLoadingLayout;
        sLoadingLayout = nullptr;
    }
    sLoadingWanted = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Every frame: the BG movie screen while shown, the loading indicator on
// the frames that asked for it (deleted on the first that did not).
// FS2 0x00267830
// callcheck: advance, update, openList, gfxGetTexArea, getList, draw, closeList (inside the inline drawOverlayLayout)
// callcheck: ~UiLayout (the delete-expression)

void overlayThread(void *arg)
{
    (void)arg;
    for (;;) {
        if (sScreenShown && sScreenLayout != nullptr)
            drawOverlayLayout(sScreenLayout);
        if (!sLoadingWanted) {
            if (sLoadingLayout != nullptr) {
                delete sLoadingLayout;
                sLoadingLayout = nullptr;
            }
        } else if (sLoadingLayout != nullptr) {
            drawOverlayLayout(sLoadingLayout);
        }
        sLoadingWanted = 0;
        threadYield(1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Asks for the loading indicator this frame; builds it on the language
// pack's item 0 once. The slot check is overlayThreadStart inline.
// FS2 0x00267950
// callcheck: operator_new (the new-expression)
// callcheck: threadSlotState, threadSlotCreate (overlayThreadStart, inline in the ROM)

void overlayShowLoading()
{
    sLoadingWanted = 1;
    if (getLangPackItem(LangItem_Loading) != nullptr && sLoadingLayout == nullptr) {
        sLoadingLayout = new UiLayout();
        sLoadingLayout->setData(getLangPackItem(LangItem_Loading), kLayoutNoArg);
    }
    overlayThreadStart();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00267a00
// callcheck: ~UiLayout (the delete-expression's deleting destructor)

void overlayReleaseScreen()
{
    if (sScreenLayout != nullptr) {
        delete sScreenLayout;
        sScreenLayout = nullptr;
    }
    sScreenShown = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Shows the BG movie screen; builds it on the init pack's file once. The
// slot check is overlayThreadStart inline.
// FS2 0x00267a30
// callcheck: operator_new (the new-expression)
// callcheck: threadSlotState, threadSlotCreate (overlayThreadStart, inline in the ROM)

void overlayShowScreen()
{
    sScreenShown = 1;
    if (sScreenLayout == nullptr) {
        sScreenLayout = new UiLayout();
        sScreenLayout->setData(getInitPackData(kBgMovieLayout), kLayoutNoArg);
    }
    overlayThreadStart();
}

// ──────────────────────────────────────────────────────────────────────
// Runs a layout on item in packet list 12 until it ends (its answer) or,
// when cancelable, until cross is pressed (kLayoutCancelled). The first
// frame yields before drawing; the cancel test follows each frame's draw.
// FS2 0x00267ae0
// callcheck: UiLayout, ~UiLayout (the stack object's constructor and destructor)

int runLayout(void *item, int arg, bool cancelable, const char *baseDir)
{
    UiLayout layout;
    layout.setData(item, arg);
    GameSystem *sys = gGameSystem;
    if (baseDir != nullptr)
        layout.setBaseDir(baseDir);
    int result;
    for (;;) {
        threadYield(1);
        layout.advance();
        layout.update();
        if (layout.m_done) {
            result = layout.m_result;
            break;
        }
        sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_Screen);
        void *texList = gfxGetTexArea()->getList(PacketList_Screen);
        layout.draw(packet, texList);
        gGameSystem->m_packets.closeList();
        if (cancelable && (sys->m_pad0.m_pressed & kLayoutCancelButton)) {
            result = kLayoutCancelled;
            break;
        }
    }
    return result;
}

// ──────────────────────────────────────────────────────────────────────
// Loads one layout file on its own (no loading indicator) and runs it;
// kLayoutCancelled when the file is empty. yieldAfter: one more frame
// before the loader goes.
// FS2 0x00267c00
// callcheck: operator_new (the new-expression; addFile, getSize, getData, waitLoaded and the delete go through vtable slots 2 / 7 / 5 / 15 / 0)

int runLayoutFile(const char *path, int arg, bool cancelable, const char *baseDir,
                  bool yieldAfter)
{
    FileLoader *loader = new FileLoader(0);
    loader->addFile(path);
    while (loader->startLoad() == 0)
        threadYield(1);
    loader->waitLoaded(1, false);
    if (loader->getSize(path) < 1) {
        delete loader;
        return kLayoutCancelled;
    }
    int result = runLayout(loader->getData(path), arg, cancelable, baseDir);
    if (yieldAfter)
        threadYield(1);
    delete loader;
    return result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00267d80

int gameOverDialogResult()
{
    switch (sGameOverResult) {
    case GameOverChoice_Continue:
        return GameOverChoice_Continue;
    case GameOverChoice_Quit:
        return GameOverChoice_Quit;
    case GameOverChoice_Restart:
        return GameOverChoice_Restart;
    case GameOverChoice_Running:
        return GameOverChoice_Running;
    default:
        return GameOverChoice_Quit;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The language pack's item 3 while the stage result's attempt flag is set,
// else item 2.
// FS2 0x00267de0

void gameOverDialogStart()
{
    sGameOverResult = GameOverChoice_Running;
    void *item;
    if (!getStageResult()->hasAttemptFlag())
        item = getLangPackItem(LangItem_GameOver);
    else
        item = getLangPackItem(LangItem_GameOverAttempt);
    threadSlotCreate(gameOverDialogThread, ThreadSlot_Dialog, item, nullptr, 0, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// A missing item, or a cancelled answer, reads as Restart.
// FS2 0x00267e50

void gameOverDialogThread(void *item)
{
    if (item != nullptr)
        sGameOverResult = runLayout(item, kLayoutNoArg, false, nullptr);
    if (sGameOverResult == kLayoutCancelled)
        sGameOverResult = GameOverChoice_Restart;
    threadExit();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00267ea0

int noticeDialogState()
{
    if (sNoticeResult == 0)
        return 0;
    if (sNoticeResult == -1)
        return -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00267ee0

void noticeDialogStart()
{
    sNoticeResult = -1;
    threadSlotCreate(noticeDialogThread, ThreadSlot_Dialog, getLangPackItem(LangItem_Notice),
                     nullptr, 0, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00267f30

void noticeDialogThread(void *item)
{
    if (item != nullptr)
        sNoticeResult = runLayout(item, kLayoutNoArg, false, nullptr);
    if (sNoticeResult == -1)
        sNoticeResult = 0;
    threadExit();
}

// ──────────────────────────────────────────────────────────────────────
// The archive screen; tells the caller whether it answered 1 and whether
// the save data changed meanwhile.
// FS2 0x00267f80

int runArchiveScreen(int arg)
{
    SaveData saved = getSystemData()->save;
    int answer = runLayoutFile(archiveLayoutPath(), arg, false, kArchiveBaseDir, true);
    threadYield(2);
    int result = (answer == 1) ? ArchiveScreen_Answered1 : 0;
    if (!saved.equals(&getSystemData()->save))
        result |= ArchiveScreen_SaveChanged;
    return result;
}

// ──────────────────────────────────────────────────────────────────────
// The options screen. Afterwards: the loaded save's stamp is dropped unless
// the options keep it (+0x4d, as SaveFile::apply), the sound output is
// applied when it changed.
// FS2 0x00268050

int runOptionScreen(int arg)
{
    GameOptions saved = *getGameOptions();
    // getSaveData and getSystemData both answer the block 0x598b30: the ROM
    // reads the play record and the options through getSaveData.
    int savedKind = reinterpret_cast<SystemData *>(getSaveData())->select.kind;
    runLayoutFile(optionLayoutPath(), arg, false, nullptr, true);
    if (reinterpret_cast<SystemData *>(getSaveData())->options._4d == 0)
        memcardClearStamp();
    if (saved.soundMode != getGameOptions()->soundMode)
        getGameOptions()->applySound();
    if (!saved.equals(getGameOptions())
        || savedKind != reinterpret_cast<SystemData *>(getSaveData())->select.kind)
        return 1;
    return 0;
}
