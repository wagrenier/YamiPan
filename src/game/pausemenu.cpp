/* ==========================================================================
 *  game/pausemenu.cpp -- the stage's pause menu (start) and map menu
 *  (select), FS2 0x20acb0..0x20b120, between StageMenuList
 *  (game/stagemenu.cpp) and StageCharacter's members (game/playermotion.cpp).
 *
 *  Each start function clears its answer to -1 and starts a thread in
 *  ThreadSlot_Dialog; the thread shows the BG movie screen, runs its layout
 *  modally (runLayoutFile / runLayout, packet list 12) and leaves its answer
 *  for the stage loop, which polls it every frame while it draws the stage
 *  behind (StageScene::drawPaused). The pause menu's layout is the stage's
 *  screen (2dEU/s<stage>m<variant + 1>[b]<lang>.d2b): its answers 0..3 end
 *  the menu, the others open a screen (the navigator, the archive, the
 *  difficulty screen, help, the options) and come back to it with the
 *  cursor on the line they left. The map menu shows the stage's map layout
 *  (...mp<lang>.d2b) from the stage's own files.
 *
 *  The file name is the port's: no source string in the range.
 * ======================================================================== */
#include "game/pausemenu.h"
#include "game/fileloader.h"
#include "game/gamehooks.h"
#include "game/gamepaths.h"
#include "game/menuscene.h"
#include "game/screen.h"
#include "game/stageloop.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/thread.h"

static int sPauseMenuResult;    /* sbss 0x31ae30 (gp-0x7a40): PauseMenuChoice */
static int sMapMenuResult;      /* sbss 0x31ae34 (gp-0x7a3c): -1 while open */

// ──────────────────────────────────────────────────────────────────────
// The answer once the thread has left it (PauseMenu_Running before). With
// an answer the BG movie screen is released again (the thread already did),
// except on PauseMenu_Quit, which keeps it up for the way out.
// FS2 0x0020acb0

int pauseMenuResult()
{
    if (sPauseMenuResult != PauseMenu_Running && sPauseMenuResult != PauseMenu_Quit)
        overlayReleaseScreen();
    return sPauseMenuResult;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0020acf0

void pauseMenuStart()
{
    sPauseMenuResult = PauseMenu_Running;
    threadSlotCreate(pauseMenuThread, ThreadSlot_Dialog, nullptr, nullptr, 0, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// The answer lands one frame after the screen goes.
// FS2 0x0020ad20

void pauseMenuThread(void *)
{
    int choice = runPauseMenu();
    if (choice != PauseMenu_Quit)
        overlayReleaseScreen();
    threadYield(1);
    sPauseMenuResult = choice;
    threadExit();
}

// ──────────────────────────────────────────────────────────────────────
// The player character's sheet has a ctype: on stages 22 and 27 the stage
// screen's "b" layout (stageScreenPath's alt). Inline in the ROM, which picks
// the sheet as stageObjRecord does, but answers no record for a bad index.
// PC: no record reads as false; the ROM would read EE address 0x10.

static bool playerHasAltScreen()
{
    if (gStageScene == nullptr)
        return false;
    int index = gStageScene->player->m_itemDef;
    int set;
    switch (getStageSelect()->kind) {
    case StageKind_Hard: set = 2; break;
    case StageKind_Normal: set = 1; break;
    case StageKind_Easy: set = 0; break;
    default: set = 1; break;
    }
    if (index < 0 || index >= kStageObjRecordCount)
        return false;
    return gStageObjRecords[set][index].kind != 0;
}

// ──────────────────────────────────────────────────────────────────────
// The stage screen until it answers 0..3, or a stage is picked in the
// navigator (PauseMenu_Navi); a missing layout, or an answer it does not
// know, is PauseMenu_Resume. Each run starts with the cursor on the last
// answer (the first on 0).
// FS2 0x0020ad70
// callcheck: overlayShowScreen, getStageSelect, getStageSelect, stageScreenPath, runLayoutFile, getSaveData, runNaviFile, runHelpScreen, runArchiveScreen, runHelpScreen, runStageScreenH, runOptionScreen, runHelpScreen, runHelpScreen, applyNaviAnswer (getStageSelect stands for the ROM's getSaveData + 0x4c8, the record's exitKind)

int runPauseMenu()
{
    overlayShowScreen();
    int answer = 0;
    for (;;) {
        bool alt = playerHasAltScreen();
        const char *path = stageScreenPath(getStageSelect(), alt);
        if (path == nullptr)
            answer = PauseMenu_Resume;
        else
            answer = runLayoutFile(path, answer, false, nullptr, false);

        switch (answer) {
        case PauseMenu_Resume:
            return PauseMenu_Resume;
        case PauseMenu_Restart:
            return PauseMenu_Restart;
        case PauseMenu_Continue:
            return PauseMenu_Continue;
        case PauseMenu_Quit:
            return PauseMenu_Quit;
        case PauseMenuScreen_Navi:
            for (;;) {
                getStageSelect()->exitKind = StageExit_None;
                int navi = runNaviFile();
                if (navi == NaviAnswer_Leave)
                    break;
                if (navi != NaviAnswer_Help) {
                    applyNaviAnswer(navi);
                    return PauseMenu_Navi;
                }
                runHelpScreen(kHelpPageNavi);
            }
            break;
        case PauseMenuScreen_Archive:
            // The ROM asks for 1 or 3 (ArchiveScreen_Answered1, the save
            // changed or not); the change is not saved from here.
            for (;;) {
                int archive = runArchiveScreen(1);
                if (archive != ArchiveScreen_Answered1
                    && archive != (ArchiveScreen_Answered1 | ArchiveScreen_SaveChanged))
                    break;
                runHelpScreen(kHelpPageArchive);
            }
            break;
        case PauseMenuScreen_StageH:
            runStageScreenH();
            break;
        case PauseMenuScreen_Help:
            runHelpScreen(kHelpPageStage);
            break;
        case PauseMenuScreen_Options:
            runOptionScreen(0);
            break;
        case PauseMenuScreen_MapHelp:
            runHelpScreen(kHelpPageMap);
            break;
        default:
            return PauseMenu_Resume;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// -1 while open, then 0; the BG movie screen goes with the answer.
// FS2 0x0020afa0

int mapMenuResult()
{
    if (sMapMenuResult != -1)
        overlayReleaseScreen();
    return sMapMenuResult;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0020afe0

void mapMenuStart()
{
    sMapMenuResult = -1;
    threadSlotCreate(mapMenuThread, ThreadSlot_Dialog, nullptr, nullptr, 0, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// The stage's map layout from the stage loader (FileLoader::getData by
// name), until it answers other than kMapMenuHelp. Then the BG movie
// screen is released twice -- the ROM's way; the second release, and
// mapMenuResult's, find it gone.
// FS2 0x0020b010
// callcheck: overlayShowScreen, getSaveData, stageScreenPathMp, getStageFiles, runLayout, runHelpScreen, overlayReleaseScreen, overlayReleaseScreen, threadYield, threadExit (getStageSelect stands for the ROM's getSaveData + 0x4bc, null-checked there; getData is slot 5 of the loader)

void mapMenuThread(void *)
{
    overlayShowScreen();
    for (;;) {
        int answer = 0;
        const char *path = stageScreenPathMp(getStageSelect());
        if (path != nullptr) {
            StageFiles *files = getStageFiles();
            if (files != nullptr && files->loader != nullptr) {
                void *data = files->loader->getData(path);
                if (data != nullptr)
                    answer = runLayout(data, kLayoutNoArg, false, nullptr);
            }
        }
        if (answer != kMapMenuHelp)
            break;
        runHelpScreen(kHelpPageMap);
    }
    overlayReleaseScreen();
    overlayReleaseScreen();
    threadYield(1);
    sMapMenuResult = 0;
    threadExit();
}
