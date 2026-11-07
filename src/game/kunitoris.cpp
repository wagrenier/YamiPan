// game/kunitoris.cpp -- Kunitoris, the Extra mode minigame: its front end
// (title, level select, the level loop, the ending) and its back thread
// (game/kunitoris.h). The game classes are later sessions' work.
#include "common/yamipan_stub.h"

#include "game/kunitoris.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "game/fileloader.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/gsblockmap.h"
#include "game/gsutil.h"
#include "game/heap.h"
#include "game/seplayer.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "game/uidrawer.h"
#include "game/uigame.h"
#include "game/wstring.h"
#include "sdk/libgraph.h"

static const char kKunitorisPackPath[] = "dataEU/kuniEU.pak";
static const char kKunitorisTitleText[] = "= KUNITORIS =";
static const char kKunitorisClearedMark[] = "c";    // the font's glyph for a cleared level

static const char *const kKunitorisLines[KunitorisLine_Count] = {   /* data 0x2fc340 */
    "LEVEL 1",  "LEVEL 2",  "LEVEL 3",  "LEVEL 4",  "LEVEL 5",
    "LEVEL 6",  "LEVEL 7",  "LEVEL 8",  "LEVEL 9",  "LEVEL 10",
    "LEVEL 11", "LEVEL 12", "LEVEL 13", "LEVEL 14", "LEVEL 15",
    "JAPAN",    "ENDING",
};

static const UiVec4 kKunitorisWhite  = { 1.0f, 1.0f, 1.0f, 1.0f };  /* rodata 0x2fbff0 */
static const UiVec4 kKunitorisGrey   = { 0.5f, 0.5f, 0.5f, 1.0f };  /* rodata 0x2fc000: the pictures' prim colour */
static const UiVec4 kKunitorisYellow = { 1.0f, 1.0f, 0.0f, 1.0f };  /* rodata 0x2fc020: the level select's cursor */

// The pictures: textured gouraud sprites with blending, texel uv.
constexpr u_long kKunitorisPicturePrim =
    UiPrim_UV | UiPrim_Blend | UiPrim_Textured | UiPrim_Gouraud | UiPrim_Sprite;

// The buttons the front end reads by bit (Pad's words, PadBit).
constexpr u_int kPadSelect = 1u << PadBit_Select;
constexpr u_int kPadStart  = 1u << PadBit_Start;
constexpr u_int kPadUp     = 1u << PadBit_Up;
constexpr u_int kPadDown   = 1u << PadBit_Down;
// Any of the face buttons or Start skips the title's scroll-in.
constexpr u_int kPadTitleSkip = kPadStart | (1u << PadBit_Triangle) | (1u << PadBit_Circle)
                              | (1u << PadBit_Cross) | (1u << PadBit_Square);

constexpr int kTitleScrollFrom   = 0x100;   // kunitorisTitle's picture offset at the start
constexpr int kTitleIdleFrames   = 600;     // then the how-to picture ..
constexpr int kTitleHowToFrames  = 300;     // .. for this long
constexpr int kTitleFlashFrames  = 45;      // Start pressed: the line blinks
constexpr int kResultWaitFrames  = 30;      // a level's end shows before Decide / Start leave
constexpr int kEndingIntroFrames = 60;
constexpr int kEndingLastTiles   = 27;
constexpr float kKunitorisCell   = 8.0f;    // KunitorisGame::loadLevel's cell size

static u_char    sClearEnable = 1;      /* sdata 0x31accc (gp-0x7ba4) */
static u_char    sClearR;               /* sbss 0x31aeb0 (gp-0x79c0) */
static u_char    sClearG;               /* sbss 0x31aeb4 (gp-0x79bc) */
static u_char    sClearB;               /* sbss 0x31aeb8 (gp-0x79b8) */
static u_char    sClearA;               /* sbss 0x31aebc (gp-0x79b4) */
static PackFile *sPack;                 /* sbss 0x31aec0 (gp-0x79b0) */
static SePlayer *sSePlayer;             /* sbss 0x31aec4 (gp-0x79ac) */
static u_char    sSePlayerLoaded;       /* sbss 0x31aec8 (gp-0x79a8) */
static UiFont   *sFont;                 /* sbss 0x31aecc (gp-0x79a4) */
static int       sScore;                /* sbss 0x31aed0 (gp-0x79a0): the level's best, then the score */
static int       sSaveChanged;          /* sbss 0x31aed4 (gp-0x799c): runExtraMode's answer */
static SeHandle  sSeHandles[KunitorisSeSlot_Count];    /* bss 0x599800 */

// ──────────────────────────────────────────────────────────────────────
// Inline in the ROM.

static void *kunitorisItem(int id)
{
    return sPack->getItem(id);
}

// The clear colour kunitorisBackThread sets each frame.
static void setClearColor(u_char r, u_char g, u_char b)
{
    sClearEnable = 1;
    sClearR = r;
    sClearG = g;
    sClearB = b;
    sClearA = 0;
}

// The player's table entry; null without a table or past its count.
static const SeEntry *kunitorisSe(int entry)
{
    const SeTable *table = sSePlayer->m_table;
    if (table == nullptr || table->count < entry + 1)
        return nullptr;
    return &table->entries[entry];
}

static void kunitorisPlay(int slot, int entry)
{
    const SeEntry *se = kunitorisSe(entry);
    sSeHandles[slot] = se != nullptr ? sSePlayer->play(se, 0x80, 0x80) : -1;
}

// The music: the playing one stops first.
static void kunitorisPlayBgm(int entry)
{
    SePlayer *player = sSePlayer;
    if (sSeHandles[KunitorisSeSlot_Bgm] >= 0) {
        player->stop(sSeHandles[KunitorisSeSlot_Bgm]);
        sSeHandles[KunitorisSeSlot_Bgm] = -1;
    }
    kunitorisPlay(KunitorisSeSlot_Bgm, entry);
}

static int kunitorisClearedCount()
{
    const SaveData &save = getSystemData()->save;
    int cleared = 0;
    for (int i = 0; i < kKunitorisLevels; i++) {
        if (save._4ac[i] != 0)
            cleared++;
    }
    return cleared;
}

static u_int padPressed()
{
    return gGameSystem->m_pad0.m_pressed;
}

static bool padDecide()
{
    return (padPressed() & bitMask(ButtonMap::defaultButton(0))) != 0;
}

static bool padCancel()
{
    return (padPressed() & bitMask(ButtonMap::defaultButton(1))) != 0;
}

// The drawer's matrix back to identity.
static void resetDrawerMatrix(UiDrawer *drawer)
{
    UiMatrix identity;
    identity.identity();
    drawer->m_stack[drawer->m_top].copy(identity);
    drawer->updateMatrix();
}

// A pack picture as a textured sprite from (x0, y0) to (x1, y1), through a
// temporary GsTexture.
static void drawPicture(UiDrawer *drawer, int item, float x0, float y0, float x1, float y1)
{
    GsTexture *tex = new GsTexture(kunitorisItem(item));
    UiTexVertex quad[2] = {
        { { 0.01f, 0.01f }, { x0, y0 } },
        { { 1.0f, 1.0f }, { x1, y1 } },
    };
    drawer->setTexture(tex);
    drawer->setGsReg(SCE_GS_TEX1_1, 0);
    drawer->setPrimColor(kKunitorisPicturePrim, &kKunitorisGrey);
    drawer->drawTexFlat(quad, 2, 0);
    delete tex;
}

static sceVif1Packet *openFrameList(TexAreaList **texList)
{
    sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_Finish);
    *texList = gfxGetTexArea()->getList(PacketList_Finish);
    return packet;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002c42c0
// callcheck: threadSlotCreate, threadYield, threadSlotState

int runExtraMode(int arg)
{
    threadSlotCreate(kunitorisThread, ThreadSlot_Game,
                     reinterpret_cast<void *>(static_cast<intptr_t>(arg)), nullptr, 0, nullptr);
    do {
        threadYield(1);
    } while (threadSlotState(ThreadSlot_Game) != ThreadState_Free);
    return sSaveChanged;
}

// ──────────────────────────────────────────────────────────────────────
// The mode: the pack, its player and font; then title, level select and
// levels until the title is cancelled.
// FS2 0x002c4320
// callcheck: operator_new, PackFile, SePlayer, UiFont, UiDrawer, ~UiFont, operator_delete, ~UiDrawer, ~SePlayer, KunitorisGame (the new- and delete-expressions, the stack object)
// callcheck: __destroy_new_array, __destroy_arr (the ROM's inline ~KunitorisGame: the stack object's scope end)
// callcheck: bitMask, defaultButton, getList, gfxGetTexArea, openList, play, stop (the inline helpers padDecide, openFrameList, kunitorisPlay, kunitorisPlayBgm)

void kunitorisThread(void *arg)
{
    (void)arg;
    sSaveChanged = 0;
    threadYield(1);
    gfxSelectDisplay(1);
    threadYield(1);
    threadSlotCreate(kunitorisBackThread, ThreadSlot_Dialog, nullptr, nullptr, 0, nullptr);
    setClearColor(0, 0, 0xff);

    delete sPack;
    PackFile *pack = new PackFile(kKunitorisPackPath, 0);
    pack->load(0, false);
    sPack = pack;
    if (sSePlayerLoaded != 1) {
        sSePlayer = new SePlayer();
        sSePlayer->setTable(static_cast<SeTable *>(kunitorisItem(KunitorisItem_SeTable)));
        sSePlayer->loadBank(0, kunitorisItem(KunitorisItem_SeBank));
        sSePlayerLoaded = 1;
        for (int i = 0; i < KunitorisSeSlot_Count; i++)
            sSeHandles[i] = -1;
    }
    UiFont *font = new UiFont(kunitorisItem(KunitorisItem_FontGlyphs), kunitorisItem(KunitorisItem_FontImage));
    font->m_spacing[0] = 0.0f;
    font->m_spacing[1] = 0.0f;
    sFont = font;
    UiDrawer *drawer = new UiDrawer();
    drawer->setPixelMode(1);

    for (;;) {
        setClearColor(0, 0, 0);
        kunitorisPlayBgm(KunitorisSe_TitleBgm);
        if (kunitorisTitle(drawer) < 0)
            break;
        setClearColor(0x11, 0x8d, 0xc1);

        int line = 0;
        for (;;) {
            kunitorisPlayBgm(KunitorisSe_SelectBgm);
            line = kunitorisLevelSelect(drawer, kunitorisClearedCount(), line);
            if (line < 0)
                break;
            if (line == KunitorisLine_Ending) {
                kunitorisEnding(1);
                continue;
            }

            const int level = line;
            sScore = getSystemData()->save._46c[level];
            GamePad *pad = &gGameSystem->m_pad0;
            KunitorisLevel *file = static_cast<KunitorisLevel *>(kunitorisItem(KunitorisItem_Level0 + level));
            file->relocate();
            KunitorisGame game(sScore, level);
            game.loadLevel(kKunitorisCell, file);

            // The view centred on the board, wrapped into it.
            const int width = game.m_level->width;
            const int height = game.m_level->height;
            game.m_scroll[1] = height / 2 - game.m_viewHalf[1];
            game.m_scroll[0] = width / 2 - game.m_viewHalf[0];
            if (game.m_scroll[0] < 0)
                game.m_scroll[0] = width - 1;
            if (width <= game.m_scroll[0])
                game.m_scroll[0] = 0;
            if (game.m_scroll[1] < 0)
                game.m_scroll[1] = height - 1;
            if (height <= game.m_scroll[1])
                game.m_scroll[1] = 0;
            game.start(-1);

            for (;;) {
                threadYield(1);
                game.update(pad);
                TexAreaList *texList;
                sceVif1Packet *packet = openFrameList(&texList);
                game.draw(packet, texList);
                gGameSystem->m_packets.closeList();
                if (game.m_cleared)
                    break;
                if (game.m_lives < 1) {
                    kunitorisPlayBgm(KunitorisSe_GameOver);
                    break;
                }
                if (padPressed() & kPadSelect) {
                    kunitorisPlay(KunitorisSeSlot_Decide, KunitorisSe_Decide);
                    if (!game.m_selectToggle) {
                        if (game.m_lives > 0)
                            game.m_selectToggle = 1;
                    } else if (game.m_lives > 0) {
                        game.m_selectToggle = 0;
                    }
                } else if (padPressed() & kPadStart) {
                    kunitorisPlay(KunitorisSeSlot_Decide, KunitorisSe_Decide);
                    kunitorisHowTo(drawer, 0);
                    kunitorisPlay(KunitorisSeSlot_Decide, KunitorisSe_Decide);
                }
            }

            // The end shows on until Decide or Start, after a wait.
            for (int frame = 0;;) {
                threadYield(1);
                frame++;
                if (frame > kResultWaitFrames && (padDecide() || (padPressed() & kPadStart)))
                    break;
                TexAreaList *texList;
                sceVif1Packet *packet = openFrameList(&texList);
                game.update(pad);
                game.draw(packet, texList);
                gGameSystem->m_packets.closeList();
            }
            kunitorisPlay(KunitorisSeSlot_Decide, KunitorisSe_Decide);

            sScore = game.m_score;
            if (getSystemData()->save._46c[level] < sScore) {
                getSystemData()->save._46c[level] = sScore;
                sSaveChanged = 1;
            }
            if (game.m_cleared && getSystemData()->save._4ac[level] == 0) {
                getSystemData()->save._4ac[level] = 1;
                sSaveChanged = 1;
                if (kunitorisClearedCount() == kKunitorisAllCleared)
                    kunitorisEnding(0);
            }
        }
    }

    delete sFont;
    sFont = nullptr;
    delete drawer;
    delete sSePlayer;
    sSePlayer = nullptr;
    sSePlayerLoaded = 0;
    delete sPack;
    sPack = nullptr;
    threadYield(1);
    setClearColor(0, 0, 0);
    gfxSelectDisplay(0);
    threadYield(1);
    threadSlotKill(ThreadSlot_Dialog);
    threadExit();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002c4ee0
// callcheck: operator_new, UiDrawer, ~UiDrawer (the new- and delete-expressions)
// callcheck: bitMask, defaultButton, copy, identity, updateMatrix, getList, gfxGetTexArea, openList, play, stop (the inline helpers padDecide, resetDrawerMatrix, openFrameList, kunitorisPlayBgm)

void kunitorisEnding(int skippable)
{
    UiDrawer *drawer = new UiDrawer();
    drawer->setPixelMode(1);
    KunitorisEnding ending;
    ending._0c = 0;
    ending.m_tiles[0] = 0;
    ending.m_tiles[1] = 0;
    ending.m_tiles[2] = 0;
    ending.m_timer = -1;
    ending.m_scene = 0;
    ending.m_frame = 0;
    resetDrawerMatrix(drawer);
    kunitorisPlayBgm(KunitorisSe_EndingBgm);

    // The intro scrolls up while the clear colour fades to black.
    for (int i = 0, scroll = 0; i < kEndingIntroFrames; i++, scroll -= 90) {
        ending.m_frame++;
        if (ending.m_timer >= 0)
            ending.m_timer++;
        randNext2();
        TexAreaList *texList;
        sceVif1Packet *packet = openFrameList(&texList);
        drawer->begin(packet, texList);
        ending.drawIntro(0.0f, static_cast<float>(scroll / kEndingIntroFrames), drawer);
        drawer->end();
        gGameSystem->m_packets.closeList();
        const int left = kEndingIntroFrames - i;
        setClearColor(static_cast<u_char>(left * 0x11 / kEndingIntroFrames),
                      static_cast<u_char>(left * 0x8d / kEndingIntroFrames),
                      static_cast<u_char>(left * 0xc1 / kEndingIntroFrames));
        threadYield(1);
    }
    setClearColor(0, 0, 0);

    int tiles = 0;
    ending.setTiles(0);
    ending.setScene(6);
    for (;;) {
        ending.m_frame++;
        if (ending.m_timer >= 0)
            ending.m_timer++;
        randNext2();
        if (ending.m_timer < 0) {
            tiles++;
            if (tiles > kEndingLastTiles)
                break;
            ending.setTiles(tiles);
        }
        if (skippable != 0 && (padDecide() || (padPressed() & kPadStart)))
            break;
        TexAreaList *texList;
        sceVif1Packet *packet = openFrameList(&texList);
        drawer->begin(packet, texList);
        ending.draw(0.0f, -90.0f, drawer);
        drawer->end();
        gGameSystem->m_packets.closeList();
        threadYield(1);
    }

    threadYield(1);
    delete drawer;
    setClearColor(0x11, 0x8d, 0xc1);
    kunitorisPlayBgm(KunitorisSe_SelectBgm);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002c53c0
// callcheck: bitMask, defaultButton, play (the inline helpers padCancel, kunitorisPlay)

int kunitorisTitle(UiDrawer *drawer)
{
    int shown = 0;
    for (;;) {
        if (shown == 0) {
            int offset = kTitleScrollFrom;
            do {
                threadYield(1);
                if (padPressed() & kPadTitleSkip)
                    break;
                kunitorisDrawTitle(drawer, offset, 0);
                offset -= 2;
            } while (offset > 0);
        }
        for (int i = 0; i < kTitleIdleFrames; i++) {
            threadYield(1);
            if (padPressed() & kPadStart) {
                kunitorisPlay(KunitorisSeSlot_Decide, KunitorisSe_Decide);
                for (int frame = 0; frame < kTitleFlashFrames; frame++) {
                    kunitorisDrawTitle(drawer, 0, frame);
                    threadYield(1);
                }
                return 0;
            }
            if (padCancel())
                return -1;
            kunitorisDrawTitle(drawer, 0, 0);
        }
        shown = kunitorisHowTo(drawer, kTitleHowToFrames);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002c5550
// callcheck: operator_new, GsTexture (the new-expression; the delete goes through the vtable)
// callcheck: copy, identity, updateMatrix, drawTexFlat, setGsReg, setPrimColor, setTexture, getList, gfxGetTexArea, openList (the inline helpers resetDrawerMatrix, drawPicture, openFrameList)

int kunitorisHowTo(UiDrawer *drawer, int frames)
{
    int frame = 0;
    for (;;) {
        threadYield(1);
        if (padPressed() & kPadStart)
            return 1;
        if (frames != 0 && ++frame > frames)
            break;
        TexAreaList *texList;
        sceVif1Packet *packet = openFrameList(&texList);
        drawer->begin(packet, texList);
        resetDrawerMatrix(drawer);
        drawPicture(drawer, KunitorisItem_HowTo, -128.0f, -128.0f, 128.0f, 96.0f);
        drawer->end();
        gGameSystem->m_packets.closeList();
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The title picture at offset, and its line unless frame & 3 is 1.
// FS2 0x002c5780
// callcheck: operator_new, GsTexture (the new-expressions; the deletes go through the vtable)
// callcheck: copy, identity, updateMatrix, drawTexFlat, setGsReg, setPrimColor, setTexture, getList, gfxGetTexArea, openList (the inline helpers resetDrawerMatrix, drawPicture, openFrameList)

void kunitorisDrawTitle(UiDrawer *drawer, int offset, int frame)
{
    TexAreaList *texList;
    sceVif1Packet *packet = openFrameList(&texList);
    drawer->begin(packet, texList);
    resetDrawerMatrix(drawer);
    const float top = static_cast<float>(offset) + -128.0f;
    drawPicture(drawer, KunitorisItem_Title, -128.0f, top, 128.0f, top + 224.0f);
    if ((frame & 3) != 1) {
        const float lineTop = static_cast<float>(offset) + 40.0f;
        drawPicture(drawer, KunitorisItem_PressStart, -64.0f, lineTop, 64.0f, lineTop + 16.0f);
    }
    drawer->end();
    gGameSystem->m_packets.closeList();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002c5a70
// callcheck: bitMask, defaultButton, getList, gfxGetTexArea, openList, play (the inline helpers padDecide, padCancel, openFrameList, kunitorisPlay)

int kunitorisLevelSelect(UiDrawer *drawer, int cleared, int cursor)
{
    // The ROM's switch: 3 lines per 3 clears, all of them at 15.
    const int lines = cleared < kKunitorisAllCleared
                    ? (cleared / kKunitorisLinesPerStep + 1) * kKunitorisLinesPerStep
                    : KunitorisLine_Count;
    for (;;) {
        threadYield(1);
        if (padDecide()) {
            kunitorisPlay(KunitorisSeSlot_Decide, KunitorisSe_Decide);
            return cursor;
        }
        if (padCancel())
            break;
        const u_int repeat = gGameSystem->m_pad0.m_repeat;
        if (repeat & kPadUp) {
            kunitorisPlay(KunitorisSeSlot_Cursor, KunitorisSe_Cursor);
            cursor--;
        } else if (repeat & kPadDown) {
            kunitorisPlay(KunitorisSeSlot_Cursor, KunitorisSe_Cursor);
            cursor++;
        }
        if (cursor < 0)
            cursor = lines - 1;
        if (lines <= cursor)
            cursor = 0;

        TexAreaList *texList;
        sceVif1Packet *packet = openFrameList(&texList);
        drawer->begin(packet, texList);
        drawer->m_color = kKunitorisWhite;
        kunitorisPrintf(sFont, drawer, 0x48, 0x14, kKunitorisTitleText);
        for (int i = 0, y = 0x28; i < lines; i++, y += 10) {
            drawer->m_color = i == cursor ? kKunitorisYellow : kKunitorisWhite;
            if (i < kKunitorisLevels && getSystemData()->save._4ac[i] != 0)
                kunitorisPrintf(sFont, drawer, 0x58, y, kKunitorisClearedMark);
            kunitorisPrintf(sFont, drawer, 0x60, y, kKunitorisLines[i]);
        }
        drawer->m_color = kKunitorisWhite;
        drawer->end();
        gGameSystem->m_packets.closeList();
    }
    kunitorisPlay(KunitorisSeSlot_Decide, KunitorisSe_Cancel);
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// The text in the font, grey, in a 256-pixel box from (x - 128, y - 128).
// The font keeps pointing at the freed text, as in the ROM.
// FS2 0x002cd9a0
// callcheck: operator_new[], operator_delete[] (the new- and delete-expressions)
// callcheck: copy, identity, updateMatrix (the inline helper resetDrawerMatrix)

int kunitorisPrintf(UiFont *font, UiDrawer *drawer, int x, int y, const char *format, ...)
{
    char text[600];
    va_list args;
    va_start(args, format);
    vsprintf(text, format, args);
    va_end(args);

    UiTextBox box;
    box.rgba = { 0.5f, 0.5f, 0.5f, 1.0f };
    box.topLeft[0] = static_cast<float>(x) + -128.0f;
    box.topLeft[1] = static_cast<float>(y) + -128.0f;
    box._18[0] = 0.5f;
    box._18[1] = 0.5f;
    box._18[2] = 0.5f;
    box._18[3] = 1.0f;
    box.bottomRight[0] = box.topLeft[0] + 256.0f;
    box.bottomRight[1] = box.topLeft[1] + 256.0f;

    u_short *wide = new u_short[strlen(text) + 1];
    wstrFromAscii(wide, text);
    font->m_text = wide;
    resetDrawerMatrix(drawer);
    int result = drawer->drawText(font, 0, &box, 0.0f, 0.0f);
    delete[] wide;
    return result;
}

// ──────────────────────────────────────────────────────────────────────
// Slot 4 while the mode runs: the clear colour and the sound player.
// FS2 0x002cdb20

void kunitorisBackThread(void *arg)
{
    (void)arg;
    for (;;) {
        gfxSetClearColor(sClearEnable, sClearR, sClearG, sClearB, sClearA);
        SePlayer *player = sSePlayer;
        if (player != nullptr) {
            player->update();
            for (int i = 0; i < KunitorisSeSlot_Count; i++) {
                if (sSeHandles[i] >= 0 && !player->isPlaying(sSeHandles[i]))
                    sSeHandles[i] = -1;
            }
        }
        threadYield(1);
    }
}

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x002c9de0
void KunitorisEnding::draw(float x, float y, UiDrawer *drawer)
{
    YAMIPAN_STUB("KunitorisEnding::draw", 0x002c9de0);
}

// stub of FS2 0x002ca330
void KunitorisEnding::drawIntro(float x, float y, UiDrawer *drawer)
{
    YAMIPAN_STUB("KunitorisEnding::drawIntro", 0x002ca330);
}

// stub of FS2 0x002ca790
void KunitorisEnding::setTiles(int tile)
{
    YAMIPAN_STUB("KunitorisEnding::setTiles", 0x002ca790);
}

// stub of FS2 0x002c8640
KunitorisGame::KunitorisGame(int, int)
{
    YAMIPAN_STUB("KunitorisGame::KunitorisGame", 0x002c8640);
}

// stub of FS2 0x002c5ea0
void KunitorisGame::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("KunitorisGame::draw", 0x002c5ea0);
}

// stub of FS2 0x002cd390
void KunitorisGame::loadLevel(float cell, KunitorisLevel *level)
{
    YAMIPAN_STUB("KunitorisGame::loadLevel", 0x002cd390);
}

// stub of FS2 0x002cca40
void KunitorisGame::start(int arg)
{
    YAMIPAN_STUB("KunitorisGame::start", 0x002cca40);
}

// stub of FS2 0x002c64d0
void KunitorisGame::update(GamePad *pad)
{
    YAMIPAN_STUB("KunitorisGame::update", 0x002c64d0);
}
