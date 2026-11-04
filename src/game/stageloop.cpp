// game/stageloop.cpp -- the stage loop: the thread that plays one stage,
// its menus, restarts and sub-scenes (game/stageloop.h).

#include "game/stageloop.h"

#include "game/clock.h"
#include "game/fileloader.h"
#include "game/gamehooks.h"
#include "game/gamepacket.h"
#include "game/gamepaths.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/gsblockmap.h"
#include "game/screen.h"
#include "game/stageobjlist.h"
#include "game/stageresult.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "sdk/scetypes.h"

// The ALPHA FIX the frame copy draws with while a menu is up: 0x80 = 1.0,
// the frame as it is (the game itself runs at the options' brightness).
constexpr u_char kFrameBrightnessFull = 0x80;
// The ALPHA register's FIX field (bits 32..39): the byte the stage loop
// writes at the copy packet's +0x114.
constexpr int    kAlphaFixShift = 32;
constexpr u_long kAlphaFixMask  = 0xffull << kAlphaFixShift;

StageScene         *gStageScene;    /* sbss 0x31adf4 (gp-0x7a7c) */
static FileLoader  *sStageLoader;   /* sbss 0x31adf8 (gp-0x7a78): setStageLoader */
static StageFiles  *sStageFiles;    /* sbss 0x31adfc (gp-0x7a74): getStageFiles */
static StageScene  *sParentScene;   /* sbss 0x31ae00 (gp-0x7a70): the stage while a sub-scene runs */
static StageSound  *sStageSound;    /* sbss 0x31ae04 (gp-0x7a6c): the stage's sound, for every scene */

// The frame copy's brightness (GfxCopyPacket::alpha's FIX). Inline in the
// ROM (sb to the current copy packet's +0x114).
static inline void setFrameBrightness(u_char fix)
{
    GfxCopyPacket *copy = gfxGetCurrentCopyPacket();
    copy->alpha.DATA = (copy->alpha.DATA & ~kAlphaFixMask) | ((u_long)fix << kAlphaFixShift);
}

// StageSelect::exitKind through the save data's block, as the ROM reaches
// it (getSaveData() + 0x4c8).
static inline void setStageExit(int kind)
{
    reinterpret_cast<SystemData *>(getSaveData())->select.exitKind = kind;
}

// Whether the stage runs a clock: stage 29 always (the countdown), any
// other when the play record's byte +0x10 is set. Inline in the ROM.
static inline bool stageHasClock(const StageSelect *select)
{
    return select->stage == kStageHighestWins ? true : select->_10 != 0;
}

// The stage stops for a menu: the scene's unkept nodes go, the clock and
// the meter stop, the frame shows at full brightness. Inline in the ROM
// (four copies in stageThread).
static inline void stageEnterMenu()
{
    if (gStageScene->keepList != nullptr)
        gStageScene->keepList->removeUnkept();
    getStageResult()->paused = 1;
    gGameSystem->m_perfMeter.m_recording = false;
    setFrameBrightness(kFrameBrightnessFull);
    gStageScene->clock->setPaused(true);
}

// The stage goes on after a menu. Inline in the ROM (three copies in
// stageThread).
static inline void stageLeaveMenu()
{
    getStageResult()->paused = 0;
    gGameSystem->m_perfMeter.m_recording = true;
    setFrameBrightness(getGameOptions()->getBrightnessValue());
    gStageScene->clock->setPaused(false);
}

// The stage's e screen (skipped when skipE) and its h screen when hints
// are on, each followed by two frames. Inline in the ROM (stageThread,
// stageRestart).
static inline void stageRunScreens(bool skipE)
{
    if (!skipE) {
        runStageScreenE();
        threadYield(2);
    }
    if (getGameOptions()->hints == 1)
        runStageScreenH();
    threadYield(2);
}

// The meter's CSV for the stage, then its sound down: the clear and the
// game over. Inline in the ROM (stageCleared, stageGameOverMenu).
static inline void stageFinish()
{
    gGameSystem->m_perfMeter.m_recording = false;
    int kind = getStageSelect()->kind;
    gGameSystem->m_perfMeter.writeCsv(getStageSelect()->stage, kind);
    gStageScene->sound->setMuted(true);
}

static void stageCleared();
static bool stageGameOverMenu();
static int  stagePauseMenu();
static void stageRestart(const char *scriptPath);
static void stageLoad(const char *scriptPath);

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0017f150

StageFiles *getStageFiles()
{
    return sStageFiles;
}

// ──────────────────────────────────────────────────────────────────────
// The stage: its clock mode, sound and scene, the e / h screens, then one
// scene frame per tick until the stage is cleared or left. Each frame
// handles at most one of: game over, clear, a sub-scene to enter, a
// sub-scene that ended, the pause menu (start), the map menu (select).
// FS2 0x0017f160
// callcheck: operator_new, ~StageFiles, ~StageScene, ~StageSound (new StageSound / StageScene; the delete-expressions' deleting destructors)
// callcheck: removeUnkept, gfxGetCurrentCopyPacket, setPaused, runStageScreenE, runStageScreenH (the inline stageEnterMenu / stageLeaveMenu / setFrameBrightness / stageRunScreens)

void stageThread(void *arg)
{
    const char *scriptPath = static_cast<const char *>(arg);

    overlayReleaseScreen();
    if (getStageSelect()->stage == kStageHighestWins)
        getStageResult()->begin(StageClock_Countdown);
    else if (stageHasClock(getStageSelect()))
        getStageResult()->begin(StageClock_CountUp);
    else
        getStageResult()->begin(StageClock_None);

    gGameSystem->beginStage();
    sStageSound = new StageSound();
    stageLoad(scriptPath);
    stageRunScreens(false);
    gStageScene->beginPlay();
    threadYield(1);
    gStageScene->party->resetSaved();
    setFrameBrightness(getGameOptions()->getBrightnessValue());
    gGameSystem->m_perfMeter.m_recording = true;

    do {
        gStageScene->update();
        gStageScene->draw(gStageScene->getTopView());
        sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_StageClock);
        StageResult *result = getStageResult();
        TexAreaList *texList = gfxGetTexArea()->getList(PacketList_StageClock);
        result->update(packet, texList);
        gGameSystem->m_packets.closeList();

        if (gStageScene->isGameOver()) {
            stageEnterMenu();
            if (stageGameOverMenu())
                break;
            stageRestart(scriptPath);
            stageLeaveMenu();
        } else if (gStageScene->isCleared()) {
            stageEnterMenu();
            stageCleared();
            break;
        } else if (gStageScene->subSceneRequest != 0) {
            // The scene asks for a sub-scene: it waits in sParentScene while
            // a new scene over the same files plays.
            sParentScene = gStageScene;
            gStageScene = new StageScene(true, nullptr);
            gStageScene->parent = sParentScene;
            gStageScene->files = sStageFiles;
            sStageFiles->loadSubSceneList();
            sStageFiles->loadCommon();
            sStageFiles->load(scriptPath);
            sParentScene->clearSubSceneRequest();
            gStageScene->enterSubScene(sParentScene->event->getSubSceneData());
            getStageResult()->paused = 1;
            gGameSystem->m_perfMeter.m_recording = false;
        } else if (gStageScene->isSubSceneDone()) {
            delete gStageScene;
            gStageScene = sParentScene;
            sParentScene = nullptr;
            gStageScene->sound->setMuted(false);
            getStageResult()->paused = 0;
            gGameSystem->m_perfMeter.m_recording = true;
        } else if (gStageScene->wantsPauseMenu()) {
            stageEnterMenu();
            int leave = stagePauseMenu();
            if (leave > 0)
                break;
            if (leave == 0)
                stageRestart(scriptPath);
            stageLeaveMenu();
        } else if (gStageScene->wantsMapMenu()) {
            stageEnterMenu();
            gStageScene->sound->pause();
            mapMenuStart();
            while (mapMenuResult() == -1) {
                threadYield(1);
                gStageScene->drawPaused();
            }
            gStageScene->sound->resume();
            stageLeaveMenu();
        }
    } while (threadYield(1) == 0);

    setFrameBrightness(kFrameBrightnessFull);
    gGameSystem->m_perfMeter.m_recording = false;
    delete sStageFiles;
    delete gStageScene;
    sStageFiles = nullptr;
    gStageScene = nullptr;
    if (sStageSound != nullptr)
        delete sStageSound;
    sStageSound = nullptr;
    getStageResult()->releaseLayout();
    threadExit();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0017f7b0

int stageAnswerZero()
{
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The stage is cleared: the meter's CSV, the sound down, the notice dialog
// when the stage has no event movie to go on with.
// FS2 0x0017f7c0
// callcheck: writeCsv, setMuted, getSaveData (the inline stageFinish / setStageExit)

static void stageCleared()
{
    stageFinish();
    StageResult *result = getStageResult();
    if (findStageMovie(getStageSelect(), result->attempt[15]) < 0) {
        noticeDialogStart();
        for (;;) {
            threadYield(1);
            if (noticeDialogState() == 0)
                break;
            gStageScene->drawPaused();
        }
    }
    setStageExit(StageExit_Cleared);
}

// ──────────────────────────────────────────────────────────────────────
// Game over: the dialog, the scene drawn behind it. True to leave the
// stage (quit); a restart or a continue answers false (the caller
// reloads).
// FS2 0x0017f890
// callcheck: getStageSelect, writeCsv, setMuted, getSaveData (the inline stageFinish / setStageExit)

static bool stageGameOverMenu()
{
    stageFinish();
    gameOverDialogStart();
    int choice;
    for (;;) {
        threadYield(1);
        choice = gameOverDialogResult();
        if (choice != GameOverChoice_Running)
            break;
        gStageScene->drawPaused();
    }
    setStageExit(StageExit_GameOver);
    if (choice == GameOverChoice_Restart) {
        getStageResult()->restart();
        return false;
    }
    if (choice == GameOverChoice_Continue) {
        getStageResult()->addContinue();
        return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The pause menu, the sound paused around it. Answers > 0 to leave the
// stage (1 quit, 2 a navigator jump), 0 to reload it (restart or
// continue), -1 to go on.
// FS2 0x0017f970
// callcheck: getSaveData (the inline setStageExit)

static int stagePauseMenu()
{
    gStageScene->sound->pause();
    pauseMenuStart();
    int choice;
    while ((choice = pauseMenuResult()) == PauseMenu_Running) {
        threadYield(1);
        gStageScene->drawPaused();
    }
    gStageScene->sound->resume();

    if (choice == PauseMenu_Quit) {
        setStageExit(StageExit_Quit);
        return 1;
    }
    if (choice == PauseMenu_Navi) {
        setStageExit(StageExit_Navi);
        return 2;
    }
    if (choice == PauseMenu_Restart) {
        getStageResult()->restart();
        return 0;
    }
    if (choice == PauseMenu_Continue) {
        getStageResult()->addContinue();
        return 0;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// A restart or continue: the party's ids kept, the sound stopped, the
// stage built again and the party found again, the screens (the e screen
// only on a first attempt).
// FS2 0x0017fa70
// callcheck: getGameOptions, runStageScreenE, runStageScreenH (the inline stageRunScreens)

static void stageRestart(const char *scriptPath)
{
    gStageScene->party->save();
    gStageScene->sound->stopAll();
    while (!gStageScene->sound->isStopped()) {
        threadYield(1);
        gStageScene->drawPaused();
    }
    stageLoad(scriptPath);
    gStageScene->party->restore();
    stageRunScreens(getStageResult()->hasAttemptFlag());
    gGameSystem->m_perfMeter.m_recording = true;
}

// ──────────────────────────────────────────────────────────────────────
// The stage's files and scene, built anew over the stage loader (the old
// ones deleted).
// FS2 0x0017fb90
// callcheck: operator_new, operator_new, ~StageFiles, ~StageScene (new StageFiles / StageScene; the delete-expressions' deleting destructors)

static void stageLoad(const char *scriptPath)
{
    if (sStageFiles != nullptr)
        delete sStageFiles;
    if (gStageScene != nullptr)
        delete gStageScene;
    sStageFiles = new StageFiles(sStageLoader);
    sStageLoader->setDir(nullptr);
    gStageScene = new StageScene(false, sStageSound);
    sStageFiles->loadCommon();
    if (!gStageScene->isSubScene)
        sStageFiles->loadList();
    else
        sStageFiles->loadSubSceneList();
    sStageFiles->load(scriptPath);
    gStageScene->setupCameras();
    gStageScene->objects->rebalance();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0017fc90

void setStageLoader(FileLoader *loader)
{
    if (sStageLoader != nullptr)
        delete sStageLoader;
    sStageLoader = loader;
}
