// game/menuscene.cpp -- the menu scene: the player's way from the language
// screen through the title, the top menu and the navi screen into a stage
// (game/menuscene.h).

#include "game/menuscene.h"

#include "game/gamehooks.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/heap.h"
#include "game/kunitoris.h"
#include "game/memcard.h"
#include "game/progress.h"
#include "game/screen.h"
#include "game/stageloop.h"
#include "game/stageresult.h"
#include "game/sysdata.h"
#include "game/teleport.h"
#include "game/thread.h"
#include "game/titlescreen.h"
#include "sdk/scetypes.h"

static const char kLanguageLayout[] = "2dEU/languageEU.d2b";

// Which of StageResult::attempt the ending movie's branch is.
constexpr int kAttemptMovieBranch = 15;

constexpr int kStagePackPathSize = 0x40;

// stageThread's argument (extent unread; 0x40 like the packs' stack paths).
static char sStageScriptPath[kStagePackPathSize];   /* bss 0x5997c0 */

static void menuFlow(bool usePackFile);
static int  runAttractDemo();
static void menuPlayLoop(bool usePackFile, bool newGame);
static bool menuPlayStage(bool usePackFile);
static void menuRunStage(bool looseFiles, PackFile *pack);
static bool menuTopMenu();
static int  runMenuLayoutFile(const char *path, int arg);
static bool menuChooseKind();
static bool menuLoadGame();
static bool menuSaveDialog(int reason);
static void menuBootScreens(bool chooseLanguage);
static void menuCheckCard();
static int  menuPlayMovie(int event, int branch, void *workArea, PackFile *pack);

// getSaveData and getSystemData answer the same block.
static inline SystemData *savedSystem()
{
    return reinterpret_cast<SystemData *>(getSaveData());
}

// The whole system data to its defaults. Inline in the ROM (menuFlow,
// menuLoadGame).
static inline void resetSystemData()
{
    SystemData *data = savedSystem();
    data->save.setDefaults();
    data->select.setDefaults();
    data->options.init();
    data->flags.clear();
}

// The record's stage has been cleared on some kind in the record's variant.
// Inline in the ROM (twice in menuPlayLoop).
static inline bool recordStageCleared()
{
    if (getStageSelect()->variant == 0) {
        StageSelect *select = getStageSelect();
        return getSystemData()->save.testStageFlags(select->stage, kClearedVariant0);
    }
    StageSelect *select = getStageSelect();
    return getSystemData()->save.testStageFlags(select->stage, kClearedVariant1);
}

// A stage pack's variant: the language's first plus the record's kind.
// Inline in the ROM (menuPlayStage, menuRunStage).
static inline int stagePackVariant()
{
    int base = 0;
    getGameOptions();
    int language = getLanguage();
    if (language == Language_IT)
        base = 4 * kStagePackVariantsPerLanguage;
    else if (language == Language_FR)
        base = 3 * kStagePackVariantsPerLanguage;
    else if (language == Language_ES)
        base = 2 * kStagePackVariantsPerLanguage;
    else if (language == Language_DE)
        base = 1 * kStagePackVariantsPerLanguage;
    else if (language == Language_EN)
        base = 0;
    return base + getStageSelect()->kind;
}

// A new stage pack over its heap buffer. Inline in the ROM (menuPlayStage,
// menuRunStage): only the alignment differs, and the pack's owner sets it.
static inline PackFile *newStagePack(u_int align, void **bufferOut)
{
    char path[kStagePackPathSize];
    stagePackPath(getStageSelect(), path);
    PackFile *pack = new PackFile(path, 0);
    void *buffer = gGlobalHeap.alloc(kStagePackBufferSize, align);
    pack->setBuffer(buffer, kStagePackBufferSize, stagePackVariant());
    *bufferOut = buffer;
    return pack;
}

// ──────────────────────────────────────────────────────────────────────
// Slot 2: the menu flow, on pack files.
// FS2 0x002b5740

void menuSceneThread(void *arg)
{
    (void)arg;
    menuFlow(true);
    threadExit();
}

// ──────────────────────────────────────────────────────────────────────
// The game outside a stage; never returns. On a new game the card is
// checked for a save with the first game's bonus (SaveData::unlock468)
// unless stepScan already found one of the first game itself.
// FS2 0x002b5770
// callcheck: setDefaults, init, clear (resetSystemData, inline in the ROM)
// callcheck: operator_new, Memcard, ~Memcard (the new- and delete-expressions)

static void menuFlow(bool usePackFile)
{
    teleportBootRequest();      // PC
    menuBootScreens(true);
    threadYield(1);
    loadLangPack();
    runLayoutFile(sceLogoLayoutPath(), kLayoutNoArg, false, nullptr, true);

    bool withIntro = true;
    for (;;) {
        loadMenuPack();
        overlayReleaseScreen();

        bool intro = withIntro;
        int answer;
        for (;;) {
            answer = runTitleScreen(intro);
            // PC: a teleport from the title is a new game with no questions.
            if (teleportPending())
                answer = TitleAnswer_NewGame;
            if (answer == TitleAnswer_NewGame || answer == TitleAnswer_LoadGame)
                break;
            intro = runAttractDemo() != 0;
        }
        memcardClearStamp();

        bool newGame;
        if (answer == TitleAnswer_LoadGame) {
            overlayShowScreen();
            if (!menuLoadGame()) {
                withIntro = false;
                continue;
            }
            savedSystem()->options.applyPadAndScreen();
            newGame = false;
        } else {
            u_char bonus = memcardGetState();
            if (bonus == 0) {
                Memcard *card = new Memcard();
                card->startCheckAll();
                bool done;
                do {
                    threadYield(1);
                    done = card->update();
                } while (!done);
                if (done) {
                    for (int i = 0; i < kSaveFiles; i++) {
                        const SaveFile *file = card->m_files[i];
                        int status = file->status;
                        bool missing = status == SaveStatus_Unknown || status == SaveStatus_NoDir;
                        if (missing)
                            continue;
                        bool broken = status == SaveStatus_Empty || status == SaveStatus_Broken
                                   || status == SaveStatus_Denied;
                        if (broken)
                            continue;
                        if (file->save.unlock468 != 0) {
                            bonus = 1;
                            break;
                        }
                    }
                }
                delete card;
            }

            resetSystemData();
            savedSystem()->options.applyPadAndScreen();
            getStageSelect()->stage = 1;
            getStageSelect()->variant = 0;
            getStageSelect()->exitKind = StageExit_None;
            if (bonus != 0)
                getSaveData()->markUnlock468();
            runOptionScreen(1);
            overlayShowScreen();
            if (!menuChooseKind()) {
                withIntro = false;
                continue;
            }
            newGame = true;
        }

        memcardSetFlagAgain();
        for (;;) {
            if (!newGame && !menuTopMenu())
                break;
            menuPlayLoop(usePackFile, newGame);
            newGame = false;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Answers 1, nothing else: menuFlow asks it when the title ended without
// an answer, so read as the attract demo retail left out (a guess).
// FS2 0x002b5aa0

static int runAttractDemo()
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Navi screen, stage, navi screen ... until the navi is left or a stage is
// quit. A new game goes straight to its first stage; so does a stage left
// for the navi's jump (StageExit_Navi: the jump's answer is already in the
// record).
// FS2 0x002b5ab0
// callcheck: getStageSelect, getSystemData, testStageFlags (recordStageCleared, inline in the ROM)
// callcheck: getSaveData (savedSystem, the ROM's getter calls)

static void menuPlayLoop(bool usePackFile, bool newGame)
{
    for (;;) {
        currentNaviAnswer();    // the answer is dropped
        teleportTake();         // PC: into the record, as the navi's jump
        if (newGame) {
            saveExtraInit();
            newGame = false;
        } else if (savedSystem()->select.exitKind != StageExit_Navi) {
            int answer = runNaviFile();
            if (teleportPending())      // PC
                continue;
            if (answer == NaviAnswer_Leave)
                return;
            if (answer == NaviAnswer_Help) {
                runHelpScreen(kHelpPageNavi);
                continue;
            }
            applyNaviAnswer(answer);
        }

        savedSystem()->select.exitKind = StageExit_None;
        overlayReleaseScreen();

        // PC: a teleport's event movie plays alone, then the navi.
        int movieBranch;
        if (teleportTakeMovie(&movieBranch)) {
            menuPlayMovie(1, movieBranch, nullptr, nullptr);
            continue;
        }

        bool wasCleared = recordStageCleared();
        bool played = menuPlayStage(usePackFile);
        if (savedSystem()->select.exitKind == StageExit_Navi)
            continue;

        bool saveOffered = !played;
        NaviTable *navi = static_cast<NaviTable *>(getInitPackItem(kInitItemNaviTable));
        int variant = savedSystem()->select.variant;
        if (navi->hasPart(savedSystem()->select.stage, variant))
            saveOffered = true;
        if (saveOffered) {
            overlayShowScreen();
            menuSaveDialog(SaveDialog_NaviNode);
        }

        if (!wasCleared && recordStageCleared()) {
            int stage = getStageSelect()->stage;
            if (stage == kNaviTuStageB)
                runLayoutFile(naviTuLayoutPath(), 1, false, nullptr, true);
            else if (stage == kNaviTuStageA)
                runLayoutFile(naviTuLayoutPath(), 0, false, nullptr, true);
        }

        if (savedSystem()->select.exitKind == StageExit_Quit) {
            if (!saveOffered && memcardGetFlag() == 0) {
                overlayShowScreen();
                menuSaveDialog(SaveDialog_StageQuit);
            }
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The record's stage: a single-part node is only its movie, and counts as
// cleared; any other gives up the menu pack for the stage's opening movie
// and the stage, with the ending movie and the notice after a clear. The
// pack starts loading behind the opening movie, on the movie's work area.
// True when the stage counts as played to its end.
// FS2 0x002b5dc0
// callcheck: getGameOptions, getLanguage (stagePackVariant, inline in the ROM)
// callcheck: operator_new, PackFile, alloc, stagePackPath, setBuffer (newStagePack, inline in the ROM)
// callcheck: getSaveData (savedSystem, the ROM's getter calls)

static bool menuPlayStage(bool usePackFile)
{
    NaviTable *navi = getNaviTable();
    threadYield(2);
    if (navi->isSinglePart(savedSystem()->select.stage)) {
        menuPlayMovie(0, 0, nullptr, nullptr);
        savedSystem()->recordStageResult();
        savedSystem()->select.exitKind = StageExit_Cleared;
        savedSystem()->recordStageResult();
        return true;
    }

    unloadMenuPack();
    if (usePackFile) {
        void *buffer;
        PackFile *pack = newStagePack(kStagePackAlignMovie, &buffer);
        threadYield(2);
        if (teleportSkipsIntro() || stageMoviePath(getStageSelect(), 0, 0) == nullptr) // PC: skip
            pack->start();
        else
            menuPlayMovie(0, 0, buffer, pack);
        menuRunStage(false, pack);
    } else {
        if (!teleportSkipsIntro())      // PC
            menuPlayMovie(0, 0, nullptr, nullptr);
        threadYield(2);
        // The record over itself: the ROM's three copies, kept.
        int variant = savedSystem()->select.variant;
        int kind = savedSystem()->select.kind;
        int stage = savedSystem()->select.stage;
        getStageSelect()->stage = stage;
        getStageSelect()->kind = kind;
        getStageSelect()->variant = variant;
        menuRunStage(true, nullptr);
    }

    if (savedSystem()->select.exitKind == StageExit_Cleared) {
        int branch = getStageResult()->attempt[kAttemptMovieBranch];
        if (menuPlayMovie(1, branch, nullptr, nullptr) == 0) {
            noticeDialogStart();
            do {
                threadYield(1);
            } while (noticeDialogState() != 0);
        }
        savedSystem()->recordStageResult();
        loadMenuPack();
        return true;
    }
    loadMenuPack();
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// Gives the stage its loader, runs stageThread in slot 3 and waits. A
// message to this thread (threadYield's answer) is passed on to the stage.
// FS2 0x002b60b0
// callcheck: operator_new, PackFile, alloc, stagePackPath, setBuffer (newStagePack, inline in the ROM)
// callcheck: getGameOptions, getLanguage (stagePackVariant, inline in the ROM)
// callcheck: FileLoader (the new-expression)

static void menuRunStage(bool looseFiles, PackFile *pack)
{
    if (looseFiles) {
        setStageLoader(new FileLoader(0));
    } else {
        if (pack == nullptr) {
            void *buffer;
            pack = newStagePack(kStagePackAlignPlain, &buffer);
        }
        pack->load(1, true);
        setStageLoader(pack);
    }

    threadYield(2);
    stageScriptPath(getStageSelect(), sStageScriptPath);
    threadSlotCreate(stageThread, ThreadSlot_Game, sStageScriptPath, nullptr, 0, nullptr);
    do {
        if (threadYield(1) != 0)
            threadSwapMessage(ThreadSlot_Game, 1);
    } while (threadSlotState(ThreadSlot_Game) != ThreadState_Free);
    threadSwapMessage(ThreadSlot_02, 0);
    setStageLoader(nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// The top menu until the player plays (true) or goes back (false). What
// changes the save offers the save dialog.
// FS2 0x002b62d0

static bool menuTopMenu()
{
    for (;;) {
        int answer = runMenuLayoutFile(topMenuLayoutPath(), kLayoutNoArg);
        if (teleportPending())          // PC: as Play
            return true;
        if (answer == TopMenuAnswer_Extra) {
            overlayReleaseScreen();
            int changed = runExtraMode(0);
            overlayShowScreen();
            if (changed != 0)
                menuSaveDialog(SaveDialog_Extra);
        } else if (answer == TopMenuAnswer_Options) {
            if (runOptionScreen(0) == 1)
                menuSaveDialog(SaveDialog_Options);
        } else if (answer == TopMenuAnswer_Archive) {
            bool changed = false;
            for (;;) {
                int archive = runArchiveScreen(0);
                if (archive & ArchiveScreen_SaveChanged)
                    changed = true;
                if (!(archive & ArchiveScreen_Answered1))
                    break;
                runHelpScreen(kHelpPageArchive);
            }
            // The ROM passes no reason here: a0 still holds the 3 of the
            // compare above, on the only path that reaches this call.
            if (changed)
                menuSaveDialog(SaveDialog_Archive);
        } else if (answer == TopMenuAnswer_Play) {
            return true;
        } else if (answer == TopMenuAnswer_Title3 || answer == TopMenuAnswer_Title) {
            return false;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// runLayoutFile without a cancel or a base directory; the loader goes
// first, one frame after the layout's last.
// FS2 0x002b6410
// callcheck: operator_new, UiLayout, ~UiLayout (the new-expression and the stack object; addFile, waitLoaded, getSize, getData and the delete go through vtable slots 2 / 15 / 7 / 5 / 0)

static int runMenuLayoutFile(const char *path, int arg)
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

    UiLayout layout;
    layout.setData(loader->getData(path), arg);
    for (;;) {
        threadYield(1);
        layout.advance();
        layout.update();
        if (layout.m_done || teleportPending())     // PC: the second
            break;
        sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_Screen);
        TexAreaList *texList = gfxGetTexArea()->getList(PacketList_Screen);
        layout.draw(packet, texList);
        gGameSystem->m_packets.closeList();
    }
    threadYield(1);
    delete loader;
    return layout.m_result;
}

// ──────────────────────────────────────────────────────────────────────
// The new game's difficulty into the record; false to go back.
// FS2 0x002b65e0

static bool menuChooseKind()
{
    if (memcardGetState() != 0)
        getSaveData()->markUnlock468();
    for (;;) {
        int answer = runLayoutFile(newGameLayoutPath(), kLayoutNoArg, false, nullptr, true);
        if (teleportPending())          // PC: the record's kind stands
            return true;
        if (answer == NewGameAnswer_Options) {
            runOptionScreen(0);
        } else if (answer == NewGameAnswer_Kind2) {
            savedSystem()->select.kind = StageKind_Hard;
            return true;
        } else if (answer == NewGameAnswer_Kind1) {
            savedSystem()->select.kind = StageKind_Normal;
            return true;
        } else if (answer == NewGameAnswer_Kind0) {
            savedSystem()->select.kind = StageKind_Easy;
            return true;
        } else if (answer == NewGameAnswer_Back) {
            return false;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The load screen over fresh defaults; true when a save came in.
// FS2 0x002b66e0
// callcheck: getSaveData, setDefaults, init, clear (resetSystemData, inline in the ROM)

static bool menuLoadGame()
{
    resetSystemData();
    int answer = runLayoutFile(mcBaseLayoutPath(), SaveDialog_Load, false, nullptr, true);
    if (answer != 0)
        getGameOptions()->applySound();
    return answer != 0;
}

// ──────────────────────────────────────────────────────────────────────
// The save dialog; answer 0 sets the memcard flag that keeps the next
// dialog away (memcardGetFlag in menuPlayLoop).
// FS2 0x002b6770

static bool menuSaveDialog(int reason)
{
    memcardClearFlag();
    int answer = runLayoutFile(mcBaseLayoutPath(), reason, false, nullptr, true);
    if (answer == 0)
        memcardSetFlag();
    return answer != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002b67d0

static void menuBootScreens(bool chooseLanguage)
{
    if (chooseLanguage)
        runLayoutFile(kLanguageLayout, kLayoutNoArg, false, nullptr, true);
    menuCheckCard();
}

// ──────────────────────────────────────────────────────────────────────
// The boot's card check: memAlt%s.d2b says what is wrong and waits until
// the player goes on without it, or the next scan is fine.
// FS2 0x002b6810
// callcheck: operator_new, Memcard, ~Memcard (the new- and delete-expressions; addFile, waitLoaded, getSize, getData and the loader's delete go through vtable slots 2 / 15 / 7 / 5 / 0)

static void menuCheckCard()
{
    FileLoader *loader = new FileLoader(0);
    const char *path = memAltLayoutPath();
    loader->addFile(path);
    while (loader->startLoad() == 0)
        threadYield(1);
    loader->waitLoaded(1, false);
    if (loader->getSize(path) < 1) {
        delete loader;
        return;
    }

    Memcard *card = new Memcard();
    int answer;
    do {
        card->startScan();
        do {
            threadYield(1);
        } while (!card->update());
        int outcome = card->m_outcome;
        if (outcome == MemcardOutcome_Ok)
            break;
        if (outcome == MemcardOutcome_NoCard)
            answer = runLayout(loader->getData(path), kMemAltNoCard, false, nullptr);
        else
            answer = runLayout(loader->getData(path), kMemAltNoRoom, false, nullptr);
    } while (answer != kMemAltContinue && !teleportPending());  // PC: the second
    delete card;
    delete loader;
}

// ──────────────────────────────────────────────────────────────────────
// One of the stage's movies as a modal layout; 1 when it has none. The
// movie widget borrows workArea (getUiWorkArea), and pack starts reading
// behind the movie once the movie's file is in.
// FS2 0x002b6a00
// callcheck: operator_new, start (the new-expression; start through the pack's PackJob base; addFile, waitLoaded, getSize, getData and the delete go through vtable slots 2 / 15 / 7 / 5 / 0)

static int menuPlayMovie(int event, int branch, void *workArea, PackFile *pack)
{
    const char *path = stageMoviePath(getStageSelect(), event, branch);
    if (path == nullptr)
        return 1;

    setUiWorkArea(workArea);
    FileLoader *loader = new FileLoader(0);
    loader->addFile(path);
    while (loader->startLoad() == 0)
        threadYield(1);
    loader->waitLoaded(1, false);
    if (pack != nullptr)
        pack->start();
    if (loader->getSize(path) < 1) {
        delete loader;
        // PC: the ROM deletes it a second time below, through the freed
        // object's vtable; harmless on the EE, not here.
        loader = nullptr;
    } else {
        runLayout(loader->getData(path), 0, false, nullptr);
    }
    threadYield(1);
    if (loader != nullptr)
        delete loader;
    setUiWorkArea(nullptr);
    return 0;
}
