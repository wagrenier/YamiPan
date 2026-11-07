// game/kunitoris.cpp -- Kunitoris, the Extra mode minigame: its front end
// (title, level select, the level loop, the ending) and its back thread
// (game/kunitoris.h). The game classes are later sessions' work.
#include "common/yamipan_stub.h"

#include "game/kunitoris.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
// A file-local wrapper the ROM keeps out of line (j randNext2): only the
// spawnGroup inline calls it; everything else here calls randNext2.

// FS2 0x002c81b0

static u_int kunitorisRandom()
{
    return randNext2();
}

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
            game.m_scroll[1] = height / 2 - game.m_playerCell[1];
            game.m_scroll[0] = width / 2 - game.m_playerCell[0];
            if (game.m_scroll[0] < 0)
                game.m_scroll[0] = width - 1;
            if (width <= game.m_scroll[0])
                game.m_scroll[0] = 0;
            if (game.m_scroll[1] < 0)
                game.m_scroll[1] = height - 1;
            if (height <= game.m_scroll[1])
                game.m_scroll[1] = 0;
            game.selectPiece(-1);

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
                    if (!game.m_paused) {
                        if (game.m_lives > 0)
                            game.m_paused = 1;
                    } else if (game.m_lives > 0) {
                        game.m_paused = 0;
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

            sScore = game.m_hiScore;
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
    ending.setScene(KunitorisScene_Tiles);
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

// ══════════════════════════════════════════════════════════════════════
// The game's objects (game/kunitoris.h).

static const UiVec4 kKunitorisShadow = { 0.01f, 0.01f, 0.01f, 0.8f };   /* rodata 0x2fc030 */

// The view is 32 cells across; groups come in from 8 cells outside it.
constexpr int kKunitorisViewMask  = 31;
constexpr int kKunitorisEnterNear = -8;
constexpr int kKunitorisEnterFar  = 39;
constexpr int kKunitorisStartCell = 18;     // the player's cell, x and y
constexpr int kKunitorisHardLevel = 12;     // from here the second shape table
constexpr int kKunitorisShapeBase = 8;      // level + 8 shapes of the table to pick from
constexpr int kBlockHopFrames     = 7;      // KunitorisBlock_Bounce: half the hop
constexpr int kBlockHopHeight     = 6;      // cells at its top, times 7 / 7
constexpr int kBlockSinkFrames    = 12;

// Per level (16; the second table runs 4 entries past them).
static const int kKunitorisLevelLives[kKunitorisLevels] = {           /* data 0x2fba00 */
    40, 39, 38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 33, 33, 32, 32,
};
static const int kKunitorisLevel84[kKunitorisLevels + 4] = {          /* data 0x2fba40 */
    210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 180, 180, 180, 180,
    180, 150, 120, 120,
};
static const int kKunitorisLevel88[kKunitorisLevels] = {              /* data 0x2fba90 */
    120, 120, 115, 115, 110, 110, 105, 105, 100, 100, 95, 95, 90, 80, 70, 60,
};
static const int kKunitorisLevelGroups[kKunitorisLevels] = {          /* data 0x2fbad0: groups at the start */
    4, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
};
static const int kKunitorisLevel8c[kKunitorisLevels] = {              /* data 0x2fbb10 */
    20, 20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, 14, 12, 10, 8,
};

// The shapes' cells (data 0x2fbb50..0x2fbe00; the dot is sbss 0x31aea8,
// zero).
static const int kShapeDot[][2]     = { { 0, 0 } };
static const int kShapePairH[][2]   = { { 0, 0 }, { 1, 0 } };                                   /* 0x2fbb50 */
static const int kShapePairV[][2]   = { { 0, 0 }, { 0, 1 } };                                   /* 0x2fbb60 */
static const int kShapeBarV[][2]    = { { 0, 0 }, { 0, 1 }, { 0, -1 } };                        /* 0x2fbb70 */
static const int kShapeBarH[][2]    = { { 0, 0 }, { 1, 0 }, { -1, 0 } };                        /* 0x2fbb90 */
static const int kShapeCorner0[][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 } };                         /* 0x2fbbb0 */
static const int kShapeCorner1[][2] = { { 0, 0 }, { 1, 0 }, { 0, -1 } };                        /* 0x2fbbd0 */
static const int kShapeCorner2[][2] = { { 0, 0 }, { -1, 0 }, { 0, 1 } };                        /* 0x2fbbf0 */
static const int kShapeCorner3[][2] = { { 0, 0 }, { -1, 0 }, { 0, -1 } };                       /* 0x2fbc10 */
static const int kShapeSquare[][2]  = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };               /* 0x2fbc30 */
static const int kShapeLineH[][2]   = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { -1, 0 } };              /* 0x2fbc50 */
static const int kShapeLineV[][2]   = { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, -1 } };              /* 0x2fbc70 */
static const int kShapeL[][2]       = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 1 } };               /* 0x2fbc90 */
static const int kShapeJ[][2]       = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 2, 1 } };               /* 0x2fbcb0 */
static const int kShapeLineH2[][2]  = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { -1, 0 } };              /* 0x2fbcd0: kShapeLineH again */
static const int kShapeZ[][2]       = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, -1 } };              /* 0x2fbcf0 */
static const int kShapePlus[][2]    = { { 0, 0 }, { 1, 0 }, { -1, 0 }, { 0, -1 }, { 0, 1 } };   /* 0x2fbd10 */
static const int kShapeRing[][2]    = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 1 }, { 0, 2 },       /* 0x2fbd40 */
                                        { 1, 2 }, { 2, 2 }, { 2, 1 } };
static const int kShapeH[][2]       = { { 0, 0 }, { -1, -1 }, { -1, 0 }, { -1, 1 },             /* 0x2fbd80 */
                                        { 1, -1 }, { 1, 0 }, { 1, 1 } };
static const int kShapeBig[][2]     = { { 0, 0 }, { -1, 0 }, { 0, -2 }, { 0, -1 }, { 0, 1 },    /* 0x2fbdc0 */
                                        { 0, 2 }, { 1, 0 }, { 2, -1 }, { 2, 0 }, { 2, 1 } };

template <int N>
static constexpr KunitorisShape kunitorisShape(const int (&cells)[N][2])
{
    return { N, cells };
}

// Below kKunitorisHardLevel (data 0x2fbe10).
static const KunitorisShape kKunitorisShapesEasy[] = {
    kunitorisShape(kShapeDot),     kunitorisShape(kShapeDot),     kunitorisShape(kShapeDot),
    kunitorisShape(kShapeDot),     kunitorisShape(kShapePairH),   kunitorisShape(kShapePairH),
    kunitorisShape(kShapePairH),   kunitorisShape(kShapePairV),   kunitorisShape(kShapePairV),
    kunitorisShape(kShapePairV),   kunitorisShape(kShapeBarV),    kunitorisShape(kShapeBarV),
    kunitorisShape(kShapeBarV),    kunitorisShape(kShapeBarH),    kunitorisShape(kShapeBarH),
    kunitorisShape(kShapeCorner0), kunitorisShape(kShapeCorner0), kunitorisShape(kShapeCorner1),
    kunitorisShape(kShapeCorner2), kunitorisShape(kShapeCorner3), kunitorisShape(kShapeSquare),
    kunitorisShape(kShapeLineH),   kunitorisShape(kShapeLineV),   kunitorisShape(kShapeL),
    kunitorisShape(kShapeJ),       kunitorisShape(kShapeLineH2),  kunitorisShape(kShapeZ),
};
// From kKunitorisHardLevel (data 0x2fbef0).
static const KunitorisShape kKunitorisShapesHard[] = {
    kunitorisShape(kShapeDot),     kunitorisShape(kShapePairH),   kunitorisShape(kShapePairV),
    kunitorisShape(kShapeBarV),    kunitorisShape(kShapeBarH),    kunitorisShape(kShapeCorner0),
    kunitorisShape(kShapeCorner1), kunitorisShape(kShapeCorner2), kunitorisShape(kShapeCorner3),
    kunitorisShape(kShapeSquare),  kunitorisShape(kShapeLineH),   kunitorisShape(kShapeLineV),
    kunitorisShape(kShapeL),       kunitorisShape(kShapeJ),       kunitorisShape(kShapeLineH2),
    kunitorisShape(kShapeZ),       kunitorisShape(kShapePlus),    kunitorisShape(kShapeRing),
    kunitorisShape(kShapeH),       kunitorisShape(kShapeBig),
};
static int sKunitorisShapeCount[2] = {                                /* sdata 0x31acc0 */
    sizeof(kKunitorisShapesEasy) / sizeof(kKunitorisShapesEasy[0]),   // 27
    sizeof(kKunitorisShapesHard) / sizeof(kKunitorisShapesHard[0]),   // 20
};

// ──────────────────────────────────────────────────────────────────────
// KunitorisSprite. The ctor is inline in the ROM.

KunitorisSprite::KunitorisSprite(const void *image)
    : m_texture(nullptr)
{
    if (image != nullptr)
        m_texture = new GsTexture(image);
    m_quad[0] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    m_quad[1] = { { 1.0f, 1.0f }, { 16.0f, 16.0f } };
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisDrawable: the base (vtable 0x319f50).

// FS2 0x002c7b70
// callcheck: operator_delete (the deleting dtor's flag)

KunitorisDrawable::~KunitorisDrawable()
{
}

// FS2 0x002cab50

void KunitorisDrawable::draw(UiDrawer *)
{
}

// FS2 0x002c4e50
// callcheck: operator_delete (the deleting dtor's flag)
// callcheck: ~GsTexture (the delete-expression, through the vtable)

KunitorisSprite::~KunitorisSprite()
{
    delete m_texture;
    m_texture = nullptr;
}

// FS2 0x002cc9d0

void KunitorisSprite::draw(UiDrawer *drawer)
{
    drawer->setTexture(m_texture);
    drawer->setGsReg(kGsRegTex1_1, 0);
    drawer->setPrimColor(kKunitorisPicturePrim, &kKunitorisWhite);
    drawer->drawTexFlat(m_quad, 2, 0);
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisActor. The ctor, setSprite and drawSprite are inline in the ROM.

KunitorisActor::KunitorisActor()
    : m_mover(nullptr), m_fader(nullptr), m_sprite(nullptr)
{
    m_color = kKunitorisGrey;
}

void KunitorisActor::setSprite(KunitorisDrawable *sprite)
{
    delete m_sprite;
    m_sprite = sprite;
}

void KunitorisActor::drawSprite(UiDrawer *drawer, KunitorisDrawable *sprite, const UiVec4 &color)
{
    const float at[2] = { m_pos[0] + -128.0f, m_pos[1] + -128.0f };
    UiMatrix matrix;
    matrix.identity();
    matrix.translate(at);
    drawer->m_stack[drawer->m_top].copy(matrix);
    drawer->updateMatrix();
    drawer->m_color = color;
    sprite->draw(drawer);
    drawer->m_color = kKunitorisWhite;
}

// FS2 0x002cae40
// callcheck: operator_delete (the deleting dtor's flag)
// callcheck: (the deletes go through the vtables)

KunitorisActor::~KunitorisActor()
{
    delete m_mover;
    m_mover = nullptr;
    delete m_fader;
    m_fader = nullptr;
    delete m_sprite;
    m_sprite = nullptr;
}

// FS2 0x002cdbc0: the vtable slot's thunk (j 0x2cc8c0)
// FS2 0x002cc8c0

bool KunitorisActor::update()
{
    if (m_mover != nullptr) {
        const bool more = m_mover->step();
        m_pos[0] = m_mover->m_pos[0];
        m_pos[1] = m_mover->m_pos[1];
        if (!more) {
            delete m_mover;
            m_mover = nullptr;
        }
    }
    if (m_fader != nullptr) {
        const bool more = m_fader->step();
        m_color = { m_fader->m_rgba[0], m_fader->m_rgba[1], m_fader->m_rgba[2], m_fader->m_rgba[3] };
        if (!more) {
            delete m_fader;
            m_fader = nullptr;
        }
    }
    return true;
}

// FS2 0x002cdbd0

void KunitorisActor::draw(UiDrawer *, int)
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002cacc0
// callcheck: operator_new, GsTexture (the inline KunitorisSprite ctor)
// callcheck: getItem (the inline kunitorisItem)

KunitorisCursor::KunitorisCursor(const int *cell, int height)
{
    m_cell[0] = cell[0];
    m_cell[1] = cell[1];
    m_height = height;
    m_pos[0] = static_cast<float>(m_cell[0]) * kKunitorisCellPixels;
    m_pos[1] = static_cast<float>(m_cell[1]) * kKunitorisCellPixels;
    setSprite(new KunitorisSprite(kunitorisItem(KunitorisItem_Cursor)));
}

// FS2 0x002cab60 (the base's body inline)
// callcheck: operator_delete (the deleting dtor's flag)

KunitorisCursor::~KunitorisCursor()
{
}

// FS2 0x002c42a0

bool KunitorisCursor::update()
{
    KunitorisActor::update();
    return true;
}

// FS2 0x002ca870
// callcheck: copy, identity, translate, updateMatrix (the inline drawSprite)

void KunitorisCursor::draw(UiDrawer *drawer, int pass)
{
    if (pass == KunitorisPass_Shadow) {
        if (m_sprite != nullptr)
            drawSprite(drawer, m_sprite, kKunitorisShadow);
        return;
    }
    const float lift = static_cast<float>(-m_height) * kKunitorisCellPixels;
    const float lifted[2] = { lift, lift };
    drawer->setTranslate(lifted);
    if (m_sprite != nullptr)
        drawSprite(drawer, m_sprite, m_color);
    const float none[2] = { 0.0f, 0.0f };
    drawer->setTranslate(none);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002cc450
// callcheck: operator_new, GsTexture (the inline KunitorisSprite ctor)
// callcheck: getItem (the inline kunitorisItem)

KunitorisBlock::KunitorisBlock(const int *origin, const int *offset, int height)
    : m_anchor(nullptr), m_timer(0)     // the ROM leaves these two
{
    m_origin = origin;
    m_offset[0] = offset[0];
    m_offset[1] = offset[1];
    m_state = KunitorisBlock_Resting;
    m_height = height;
    m_restHeight = height;
    m_cell[0] = m_origin[0] + m_offset[0];
    m_cell[1] = m_origin[1] + m_offset[1];
    m_pos[0] = static_cast<float>(m_cell[0]) * kKunitorisCellPixels;
    m_pos[1] = static_cast<float>(m_cell[1]) * kKunitorisCellPixels;
    setSprite(new KunitorisSprite(kunitorisItem(KunitorisItem_Block)));
}

// FS2 0x002c7a40 (the base's body inline)
// callcheck: operator_delete (the deleting dtor's flag)

KunitorisBlock::~KunitorisBlock()
{
}

// FS2 0x002c7a30: the vtable slot's thunk (j 0x2cc220)
// FS2 0x002cc220

bool KunitorisBlock::update()
{
    switch (m_state) {
    case KunitorisBlock_Gone:
        return false;

    case KunitorisBlock_Leaving:
        if (--m_timer < 1) {
            m_timer = 0;
            m_height = 0;
            m_state = KunitorisBlock_Gone;
        } else {
            m_height = m_timer * m_restHeight / kBlockSinkFrames;
        }
        m_cell[0] = m_offset[0] - m_anchor[0];
        m_cell[1] = m_offset[1] - m_anchor[1];
        m_pos[0] = static_cast<float>(m_cell[0]) * kKunitorisCellPixels;
        m_pos[1] = static_cast<float>(m_cell[1]) * kKunitorisCellPixels;
        KunitorisActor::update();
        return true;

    case KunitorisBlock_Bounce:
        if (--m_timer < 1) {
            m_timer = 0;
            m_height = m_restHeight;
            m_state = KunitorisBlock_Resting;
        } else {
            const int up = kBlockHopFrames - abs(m_timer - kBlockHopFrames);
            m_height = m_restHeight + up * kBlockHopHeight / kBlockHopFrames;
        }
        break;

    case KunitorisBlock_Sinking:
        if (--m_timer < 1) {
            m_timer = 0;
            m_height = 0;
            m_state = KunitorisBlock_Gone;
        } else {
            m_height = m_timer * m_restHeight / kBlockSinkFrames;
        }
        break;
    }
    m_cell[0] = m_origin[0] + m_offset[0];
    m_cell[1] = m_origin[1] + m_offset[1];
    m_pos[0] = static_cast<float>(m_cell[0]) * kKunitorisCellPixels;
    m_pos[1] = static_cast<float>(m_cell[1]) * kKunitorisCellPixels;
    KunitorisActor::update();
    return true;
}

// FS2 0x002cb800
// callcheck: operator_new, GsTexture (the inline KunitorisSprite ctor)
// callcheck: getItem (the inline kunitorisItem)
// callcheck: copy, identity, translate, updateMatrix (the inline drawSprite)

void KunitorisBlock::draw(UiDrawer *drawer, int pass)
{
    if (pass < KunitorisPass_Shadow) {
        KunitorisSprite *highlight = new KunitorisSprite(kunitorisItem(KunitorisItem_Highlight));
        KunitorisDrawable *sprite = highlight != nullptr ? highlight : m_sprite;
        if (sprite != nullptr)
            drawSprite(drawer, sprite, m_color);
        delete highlight;
        return;
    }
    if (pass == KunitorisPass_Shadow) {
        if (m_sprite != nullptr)
            drawSprite(drawer, m_sprite, kKunitorisShadow);
        return;
    }
    const float lift = static_cast<float>(-m_height) * kKunitorisCellPixels;
    const float lifted[2] = { lift, lift };
    drawer->setTranslate(lifted);
    if (m_sprite != nullptr)
        drawSprite(drawer, m_sprite, m_color);
    const float none[2] = { 0.0f, 0.0f };
    drawer->setTranslate(none);
}

// The same passes with the highlight's sprite given (null: a new one of
// the pack's 0x13 picture, as the slot does).
// FS2 0x002cbc90
// callcheck: operator_new, GsTexture (the slot's body: the inline KunitorisSprite ctor)
// callcheck: getItem (the inline kunitorisItem)
// callcheck: copy, identity, translate, updateMatrix, setTranslate (the inline drawSprite; the slot's body)

void KunitorisBlock::drawWithHighlight(UiDrawer *drawer, int pass, KunitorisDrawable *highlight)
{
    if (pass < KunitorisPass_Shadow && highlight != nullptr) {
        drawSprite(drawer, highlight, m_color);
        return;
    }
    // The ROM repeats the slot's body here.
    KunitorisBlock::draw(drawer, pass);
}

// The new slide is made before the old mover goes.
// callcheck: operator_new, KunitorisSlide (the new-expression; the mover's delete goes through its vtable)

void KunitorisBlock::slideFrom(const float *from)
{
    refreshCell();
    const float to[2] = {
        static_cast<float>(m_offset[0]) * kKunitorisCellPixels,
        static_cast<float>(m_offset[1]) * kKunitorisCellPixels,
    };
    KunitorisMover *slide = new KunitorisSlide(from, to, kKunitorisTurnFrames, m_origin);
    delete m_mover;
    m_mover = slide;
}

// FS2 0x002cb6f0
// callcheck: operator_new, KunitorisSlide (the inline slideFrom: the KunitorisSlide ctor)

void KunitorisBlock::turn()
{
    const float from[2] = {
        static_cast<float>(m_offset[0]) * kKunitorisCellPixels,
        static_cast<float>(m_offset[1]) * kKunitorisCellPixels,
    };
    const int x = m_offset[0];
    m_offset[0] = -m_offset[1];
    m_offset[1] = x;
    slideFrom(from);
}

// FS2 0x002cb5e0
// callcheck: operator_new, KunitorisSlide (the inline slideFrom: the KunitorisSlide ctor)

void KunitorisBlock::flip()
{
    const float from[2] = {
        static_cast<float>(m_offset[0]) * kKunitorisCellPixels,
        static_cast<float>(m_offset[1]) * kKunitorisCellPixels,
    };
    m_offset[0] = -m_offset[0];
    slideFrom(from);
}

// FS2 0x002cc600 (the mover's and the old sprite's deletes go through their vtables)
// callcheck: operator_new, GsTexture (the inline KunitorisSprite ctor)
// callcheck: getItem (the inline kunitorisItem)

bool KunitorisBlock::sink()
{
    if (m_state != KunitorisBlock_Resting)
        return false;
    delete m_mover;
    m_mover = nullptr;
    m_timer = kKunitorisSinkFrames;
    m_state = KunitorisBlock_Sinking;
    setSprite(new KunitorisSprite(kunitorisItem(KunitorisItem_Sunk)));
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisMover: the base (vtable 0x319f20).

// FS2 0x002c7bc0
// callcheck: operator_delete (the deleting dtor's flag)

KunitorisMover::~KunitorisMover()
{
}

// FS2 0x002cc9c0

bool KunitorisMover::step()
{
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisSlide.

// FS2 0x002cc830

KunitorisSlide::KunitorisSlide(const float *from, const float *to, int frames, const int *origin)
{
    m_pos[0] = 0.0f;
    m_pos[1] = 0.0f;
    m_origin = origin;
    m_from[0] = from[0];
    m_from[1] = from[1];
    m_to[0] = to[0];
    m_to[1] = to[1];
    m_frame = 0;
    m_frames = frames;
    m_pos[0] = static_cast<float>(m_origin[0]) * kKunitorisCellPixels + m_from[0] + 0.0f;
    m_pos[1] = static_cast<float>(m_origin[1]) * kKunitorisCellPixels + m_from[1] + 0.0f;
}

// FS2 0x002cdd60
// callcheck: operator_delete (the deleting dtor's flag)

KunitorisSlide::~KunitorisSlide()
{
}

// FS2 0x002cc740

bool KunitorisSlide::step()
{
    m_frame++;
    const int frame = m_frame;
    if (frame < m_frames) {
        const float t = static_cast<float>(frame) / static_cast<float>(m_frames);
        m_pos[0] = static_cast<float>(m_origin[0]) * kKunitorisCellPixels + (m_to[0] - m_from[0]) * t + m_from[0] + 0.0f;
        m_pos[1] = static_cast<float>(m_origin[1]) * kKunitorisCellPixels + (m_to[1] - m_from[1]) * t + m_from[1] + 0.0f;
    } else {
        m_frame = m_frames;
        m_pos[0] = static_cast<float>(m_origin[0]) * kKunitorisCellPixels + m_to[0] + 0.0f;
        m_pos[1] = static_cast<float>(m_origin[1]) * kKunitorisCellPixels + m_to[1] + 0.0f;
    }
    return frame < m_frames;
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisActorList.

// FS2 0x002ce0a0

KunitorisActorList::Node::Node(KunitorisActor **item)
    : item(item), next(nullptr), prev(nullptr)
{
}

// FS2 0x002cde50

KunitorisActorList::KunitorisActorList()
    : m_head(nullptr), m_count(0)
{
}

// FS2 0x002cde60
// callcheck: operator_delete (the deleting dtor's flag)

KunitorisActorList::~KunitorisActorList()
{
    clear();
}

// FS2 0x002ce070

KunitorisActorList::Node *KunitorisActorList::tail() const
{
    Node *last = m_head;
    for (Node *node = m_head; node != nullptr; node = node->next)
        last = node;
    return last;
}

// FS2 0x002cdec0
// callcheck: operator_new (the new-expressions: the box and the node)

void KunitorisActorList::pushBack(KunitorisActor *const &actor)
{
    if (m_head == nullptr) {
        pushFront(actor);
        return;
    }
    Node *last = tail();
    Node *node = new Node(new KunitorisActor *(actor));
    node->prev = last;
    last->next = node;
    m_count++;
}

// FS2 0x002cdfe0
// callcheck: operator_new (the new-expressions: the box and the node)

void KunitorisActorList::pushFront(KunitorisActor *const &actor)
{
    Node *old = m_head;
    m_head = new Node(new KunitorisActor *(actor));
    if (old != nullptr) {
        m_head->next = old;
        old->prev = m_head;
    }
    m_count++;
}

// FS2 0x002cdf70
// callcheck: operator_delete (the delete-expressions)

void KunitorisActorList::clear()
{
    for (Node *node = m_head; node != nullptr;) {
        Node *next = node->next;
        delete node->item;
        delete node;
        node = next;
    }
    m_head = nullptr;
    m_count = 0;
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisCells / KunitorisGroup.

// FS2 0x002cb220
// callcheck: KunitorisActorList (the member's ctor)

KunitorisCells::KunitorisCells()
    : m_state(KunitorisCells_Empty)
{
}

// FS2 0x002cb170
// callcheck: operator_delete (the deleting dtor's flag)
// callcheck: ~KunitorisActorList (the member's dtor; the actors' deletes go through the vtable)

KunitorisCells::~KunitorisCells()
{
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next)
        delete *node->item;
    m_blocks.clear();
    m_state = KunitorisCells_Empty;
}

// FS2 0x002c8060

bool KunitorisCells::isEmpty() const
{
    return m_state == KunitorisCells_Empty;
}

// FS2 0x002c7db0

bool KunitorisCells::isGone() const
{
    return m_state == KunitorisCells_Gone;
}

// FS2 0x002cb010 (inline at most uses)
// callcheck: pushBack

void KunitorisCells::add(KunitorisActor *actor)
{
    m_blocks.pushBack(actor);
    if (m_state == KunitorisCells_Empty)
        m_state = KunitorisCells_Active;
}

KunitorisBlock *KunitorisCells::block(int i) const
{
    KunitorisActorList::Node *node = m_blocks.m_head;
    for (int j = 0; j < i; j++)
        node = node->next;
    return static_cast<KunitorisBlock *>(*node->item);
}

bool KunitorisCells::hasBlockAt(int x, int y)
{
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next) {
        KunitorisBlock *other = static_cast<KunitorisBlock *>(*node->item);
        if (x == other->refreshCell()[0] && y == other->refreshCell()[1])
            return true;
    }
    return false;
}

void KunitorisCells::updateBlocks()
{
    // Only the last block's answer counts.
    bool more = true;
    KunitorisActor *last = nullptr;
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next) {
        last = *node->item;
        more = last->update();
    }
    if (last != nullptr && m_state == KunitorisCells_Shot
        && static_cast<KunitorisBlock *>(last)->m_state != KunitorisBlock_Bounce)
        m_state = KunitorisCells_Active;
    if (!more)
        m_state = KunitorisCells_Gone;
}

void KunitorisCells::offsetBounds(int *min, int *max) const
{
    min[0] = min[1] = 0;
    max[0] = max[1] = 0;
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next) {
        const KunitorisBlock *b = static_cast<const KunitorisBlock *>(*node->item);
        if (b->m_offset[0] < min[0])
            min[0] = b->m_offset[0];
        if (max[0] < b->m_offset[0])
            max[0] = b->m_offset[0];
        if (b->m_offset[1] < min[1])
            min[1] = b->m_offset[1];
        if (max[1] < b->m_offset[1])
            max[1] = b->m_offset[1];
    }
}

void KunitorisCells::launch()
{
    if (m_state != KunitorisCells_Empty && m_state != KunitorisCells_Active)
        return;
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next) {
        KunitorisBlock *b = static_cast<KunitorisBlock *>(*node->item);
        if (b->m_state == KunitorisBlock_Resting) {
            b->m_timer = kKunitorisShotFrames;
            b->m_state = KunitorisBlock_Bounce;
        }
    }
    m_state = KunitorisCells_Shot;
}

// FS2 0x002cb100
// callcheck: clear (the actors' deletes go through the vtable)

void KunitorisCells::release()
{
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next)
        delete *node->item;
    m_blocks.clear();
    m_state = KunitorisCells_Empty;
}

// FS2 0x002cb2e0 (the movers' deletes go through their vtable)

bool KunitorisCells::drop(const int *anchor)
{
    if (m_state != KunitorisCells_Empty && m_state != KunitorisCells_Active)
        return false;
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next) {
        KunitorisBlock *b = static_cast<KunitorisBlock *>(*node->item);
        if (b->m_state != KunitorisBlock_Resting)
            continue;
        b->m_timer = kKunitorisDropFrames;
        b->m_state = KunitorisBlock_Leaving;
        b->m_anchor = anchor;
        delete b->m_mover;
        b->m_mover = nullptr;
        b->m_offset[0] = anchor[0] + b->m_origin[0] + b->m_offset[0];
        b->m_offset[1] = anchor[1] + b->m_origin[1] + b->m_offset[1];
        b->m_cell[0] = b->m_offset[0] - anchor[0];
        b->m_cell[1] = b->m_offset[1] - anchor[1];
        b->m_pos[0] = static_cast<float>(b->m_cell[0]) * kKunitorisCellPixels;
        b->m_pos[1] = static_cast<float>(b->m_cell[1]) * kKunitorisCellPixels;
    }
    m_state = KunitorisCells_Dropped;
    return true;
}

// FS2 0x002cb0b0

int KunitorisCells::turn()
{
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next)
        static_cast<KunitorisBlock *>(*node->item)->turn();
    return m_blocks.m_count;
}

// FS2 0x002cb060

int KunitorisCells::flip()
{
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next)
        static_cast<KunitorisBlock *>(*node->item)->flip();
    return m_blocks.m_count;
}

// FS2 0x002cb250

bool KunitorisCells::sink()
{
    if (m_state != KunitorisCells_Empty && m_state != KunitorisCells_Active)
        return false;
    for (KunitorisActorList::Node *node = m_blocks.m_head; node != nullptr; node = node->next)
        static_cast<KunitorisBlock *>(*node->item)->sink();
    m_state = KunitorisCells_Sunk;
    return true;
}

// FS2 0x002c99e0

bool KunitorisCells::hasOffset(const int *offset) const
{
    for (int i = 0; i < m_blocks.m_count; i++) {
        const KunitorisBlock *b = block(i);
        if (b->m_offset[0] == offset[0] && b->m_offset[1] == offset[1])
            return true;
    }
    return false;
}

// FS2 0x002cb460

bool KunitorisCells::touches(KunitorisCells &other)
{
    const int count = other.m_blocks.m_count;
    for (int i = 0; i < count; i++) {
        KunitorisBlock *b = other.block(i);
        const int x = b->refreshCell()[0];
        const int y = b->refreshCell()[1];
        if (hasBlockAt(x, y))
            return true;
    }
    return false;
}

// FS2 0x002cafd0
// callcheck: KunitorisActorList (the member's ctor)

KunitorisGroup::KunitorisGroup()
    : m_moved{ 0, 0 }
{
    // The ROM leaves m_cell; the blocks read it before setCell.
    m_cell[0] = 0;
    m_cell[1] = 0;
    m_step[0] = 0;
    m_step[1] = 0;
}

// FS2 0x002caf10 (~KunitorisCells inline)
// callcheck: operator_delete (the deleting dtor's flag)
// callcheck: clear, ~KunitorisActorList (the inline base dtor)

KunitorisGroup::~KunitorisGroup()
{
}

// FS2 0x002c8070

void KunitorisGroup::setCell(int x, int y)
{
    m_cell[0] = x;
    m_cell[1] = y;
}

// FS2 0x002c8080

void KunitorisGroup::setStep(int dx, int dy)
{
    m_step[0] = dx;
    m_step[1] = dy;
}

// FS2 0x002c8090
// callcheck: operator_new, KunitorisBlock (the new-expression)
// callcheck: pushBack (the inline add)

void KunitorisGroup::addCell(const int *offset)
{
    for (int i = 0; i < m_blocks.m_count; i++) {
        KunitorisActorList::Node *node = m_blocks.m_head;
        for (int j = 0; j < i; j++)
            node = node->next;
        const KunitorisBlock *block = static_cast<const KunitorisBlock *>(*node->item);
        if (block->m_offset[0] == offset[0] && block->m_offset[1] == offset[1])
            return;
    }
    add(new KunitorisBlock(m_cell, offset, kKunitorisLift));
}

// The view is 40 cells; a group leaves 8 cells past a side.
constexpr int   kKunitorisWrapMargin = 8;
constexpr float kKunitorisWrapFar    = 40.0f;

// FS2 0x002c7f00
// The far side is compared and set in float, as the ROM does.

void KunitorisGroup::wrap()
{
    int min[2], max[2];
    offsetBounds(min, max);
    for (int axis = 0; axis < 2; axis++) {
        if (m_cell[axis] < -kKunitorisWrapMargin - max[axis])
            m_cell[axis] = static_cast<int>((kKunitorisWrapFar - 1.0f) - static_cast<float>(min[axis]));
        if (kKunitorisWrapFar - static_cast<float>(min[axis]) <= static_cast<float>(m_cell[axis]))
            m_cell[axis] = -kKunitorisWrapMargin - max[axis];
    }
}

// FS2 0x002c7ee0

int *KunitorisGroup::copyPair(int *dst, const int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    return dst;
}

// FS2 0x002c7dc0

void KunitorisGroup::step(bool move)
{
    if (m_state == KunitorisCells_Shot)
        move = true;
    if (move) {
        m_cell[0] += m_step[0];
        m_cell[1] += m_step[1];
        wrap();
        copyPair(m_moved, m_step);
    } else {
        m_moved[0] = 0;
        m_moved[1] = 0;
    }
    updateBlocks();
}

// FS2 0x002c7c10 (wrap inline)

void KunitorisGroup::scroll(int dx, int dy)
{
    if (m_state == KunitorisCells_Shot)
        return;
    m_cell[0] += dx;
    m_cell[1] += dy;
    wrap();
    m_moved[0] = dx;
    m_moved[1] = dy;
}

// ──────────────────────────────────────────────────────────────────────
// The board's pieces and their colour cycles.

// data 0x2fb840..0x2fb9e0: the cycles' colours, back to back.
static const UiVec4 kKunitorisBrown  = { 0.59375f, 0.41796875f, 0.29296875f, 1.0f };
static const UiVec4 kKunitorisLitYellow = { 0.671875f, 0.67578125f, 0.13671875f, 1.0f };
static const UiVec4 kKunitorisCycleColors[] = {
    kKunitorisBrown,                                                     // 0x2fb840: KunitorisColors_Piece
    kKunitorisLitYellow, kKunitorisLitYellow, kKunitorisLitYellow,       // 0x2fb850: KunitorisColors_Flash
    kKunitorisLitYellow, kKunitorisLitYellow, kKunitorisLitYellow,
    kKunitorisLitYellow, kKunitorisLitYellow, kKunitorisLitYellow,
    kKunitorisBrown, kKunitorisBrown, kKunitorisBrown,                   // 0x2fb8e0
    kKunitorisBrown, kKunitorisBrown, kKunitorisBrown,
    kKunitorisBrown, kKunitorisBrown, kKunitorisBrown,
    { 0.5f, 0.5f, 0.5f, 1.0f },                                          // 0x2fb970: KunitorisColors_Fade
    { 0.4f, 0.4f, 0.4f, 1.0f },
    { 0.3f, 0.3f, 0.3f, 1.0f },
    { 0.2f, 0.2f, 0.2f, 1.0f },
    { 0.1f, 0.1f, 0.1f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.0703125f, 0.28515625f, 0.06640625f, 1.0f },                      // 0x2fb9d0: KunitorisColors_Green
};
static const KunitorisColorTable kKunitorisColorTables[KunitorisColors_Count] = {   /* data 0x2fb9e0 */
    {  1, &kKunitorisCycleColors[0] },
    { 18, &kKunitorisCycleColors[1] },
    {  6, &kKunitorisCycleColors[19] },
    {  1, &kKunitorisCycleColors[25] },
};

// rodata 0x2fbfc0..0x2fbfe0: a piece's cells and the borders.
static const UiVec4 kKunitorisCellWhite = { 1.0f, 1.0f, 1.0f, 1.0f };                  // 0x2fbfc0: pass 0
static const UiVec4 kKunitorisUnlit = { 0.0703125f, 0.640625f, 0.1328125f, 1.0f };    // 0x2fbfd0
static const UiVec4 kKunitorisBorder = { 0.03515625f, 0.28515625f, 0.06640625f, 1.0f }; // 0x2fbfe0

// A piece's cells: flat gouraud sprites with blending; the borders lines.
constexpr u_long kKunitorisCellPrim =
    UiPrim_UV | UiPrim_Blend | UiPrim_Gouraud | UiPrim_Sprite;           // 0x14e
constexpr u_long kKunitorisBorderPrim =
    UiPrim_UV | UiPrim_Blend | UiPrim_Gouraud | UiPrim_Line;             // 0x149

// KunitorisGame::draw's map of the level: an eighth of the board's size.
constexpr float kKunitorisMapScale = 0.125f;

KunitorisTinted::KunitorisTinted()
{
    m_color = kKunitorisGrey;
}

// FS2 0x002cdbe0
bool KunitorisTinted::update()
{
    return true;
}

// FS2 0x002cdbf0
void KunitorisTinted::draw(UiDrawer *drawer, int pass)
{
}

KunitorisColorCycle::KunitorisColorCycle()
    : m_colorCount(0), m_colors(nullptr), m_colorIndex(0), m_colorLoop(0)
{
}

void KunitorisColorCycle::setColors(KunitorisColors colors)
{
    const KunitorisColorTable &table = kKunitorisColorTables[colors];
    m_colors = nullptr;
    m_colorCount = 0;
    m_colorIndex = 0;
    m_colorLoop = 0;
    if (table.colors != nullptr) {
        m_colors = table.colors;
        m_colorCount = table.count;
        m_colorLoop = 1;
        m_color = m_colors[m_colorIndex];
    }
}

void KunitorisColorCycle::stepColor()
{
    m_colorIndex++;
    if (m_colorLoop == 0) {
        if (m_colorIndex >= m_colorCount)
            m_colorIndex = m_colorCount - 1;
    } else {
        m_colorIndex = m_colorIndex % m_colorCount;
    }
    m_color = m_colors[m_colorIndex];
}

// FS2 0x002cdc00
bool KunitorisColorCycle::update()
{
    stepColor();
    return true;
}

// FS2 0x002cd970
KunitorisPieceCell::KunitorisPieceCell()
    : lit(0), data(nullptr), cell{ 0, 0 }, rect{}
{
}

// FS2 0x002cd920
// callcheck: operator_delete (the deleting dtor's flag)

KunitorisPieceCell::~KunitorisPieceCell()
{
}

// FS2 0x002cd8a0
KunitorisPiece::KunitorisPiece()
    : m_data(nullptr), m_cellCount(0), m_cells(nullptr)
{
}

// FS2 0x002cd690
// callcheck: __destroy_new_array (the delete[]-expression)
// callcheck: operator_delete (the deleting dtor's flag)

KunitorisPiece::~KunitorisPiece()
{
    delete[] m_cells;
}

// FS2 0x002cddc0
bool KunitorisPiece::update()
{
    stepColor();
    return true;
}

// FS2 0x002cd730
// callcheck: __destroy_new_array (the delete[]-expression)
// callcheck: operator_new[], FUN_00104d90, KunitorisPieceCell, ~KunitorisPieceCell (the new[]-expression: CodeWarrior's __construct_new_array)

void KunitorisPiece::load(float cell, const KunitorisPieceData *data)
{
    delete[] m_cells;
    m_cells = nullptr;
    m_data = data;
    if (m_data == nullptr)
        return;
    m_cellCount = m_data->cellCount;
    m_cells = new KunitorisPieceCell[m_cellCount];
    for (int i = 0; i < m_data->cellCount; i++) {
        KunitorisPieceCell &c = m_cells[i];
        c.lit = 1;
        c.data = i < m_data->cellCount ? &m_data->cells[i] : nullptr;
        c.rect[0].xy[0] = static_cast<float>(c.data->x) * cell;
        c.rect[0].xy[1] = static_cast<float>(c.data->y) * cell;
        c.rect[1] = c.rect[0];
        c.rect[1].xy[0] += cell;
        c.rect[1].xy[1] += cell;
        c.cell[0] = c.data->x;
        c.cell[1] = c.data->y;
    }
}

// FS2 0x002cd520
// callcheck: setPrimColor, drawFlat

void KunitorisPiece::draw(UiDrawer *drawer, int pass)
{
    if (m_data == nullptr)
        return;
    if (pass == KunitorisPass_Shadow) {
        drawer->setPrimColor(kKunitorisCellPrim, &kKunitorisCellWhite);
        for (int i = 0; i < m_cellCount; i++)
            drawer->drawFlat(m_cells[i].rect, 2, 0);
        return;
    }
    // The colour is set only when it changes: -1 none yet, 0 unlit, 1 lit.
    int current = -1;
    for (int i = 0; i < m_cellCount; i++) {
        if (m_cells[i].lit == 0) {
            if (current != 0) {
                drawer->setPrimColor(kKunitorisCellPrim, &kKunitorisUnlit);
                current = 0;
            }
        } else if (current != 1) {
            drawer->setPrimColor(kKunitorisCellPrim, &m_color);
            current = 1;
        }
        drawer->drawFlat(m_cells[i].rect, 2, 0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisBoard: its ctor and dtor inline in the ROM (KunitorisGame's).

KunitorisBoard::KunitorisBoard()
    : m_level(nullptr), m_pieceCount(0), m_pieces(nullptr), m_highlight(-1)
{
}

KunitorisBoard::~KunitorisBoard()
{
    delete[] m_pieces;
}

// FS2 0x002cdc90

bool KunitorisBoard::update()
{
    for (int i = 0; i < m_pieceCount; i++)
        m_pieces[i].stepColor();
    return true;
}

// FS2 0x002cd280
// callcheck: setPrimColor, drawFlat (the pieces draw through their vtable)

void KunitorisBoard::draw(UiDrawer *drawer, int pass)
{
    for (int i = 0; i < m_pieceCount; i++) {
        if (i == m_highlight)
            m_pieces[i].draw(drawer, KunitorisPass_Lifted);
        else
            m_pieces[i].draw(drawer, pass);
    }
    if (pass != KunitorisPass_Shadow) {
        drawer->setPrimColor(kKunitorisBorderPrim, &kKunitorisBorder);
        drawer->drawFlat(m_level->outline, m_level->outlineCount * 2, 0);
    }
}

int KunitorisPiece::litCells() const
{
    if (m_data == nullptr)
        return 0;
    int lit = 0;
    for (int i = 0; i < m_data->cellCount; i++) {
        if (m_cells[i].lit != 0)
            lit++;
    }
    return lit;
}

int KunitorisBoard::litCells(int piece) const
{
    if (piece < 0)
        piece = m_highlight;
    if (piece < 0)
        return 0;
    if (piece == kKunitorisAllPieces) {
        int lit = 0;
        for (int i = 0; i < m_pieceCount; i++)
            lit += m_pieces[i].litCells();
        return lit;
    }
    return m_pieces[piece].litCells();
}

bool KunitorisPiece::clearCell(int x, int y)
{
    for (int i = 0; i < m_cellCount; i++) {
        KunitorisPieceCell &c = m_cells[i];
        if (c.cell[0] == x && c.cell[1] == y) {
            const bool wasLit = c.lit != 0;
            if (wasLit)
                c.lit = 0;
            return wasLit;
        }
    }
    return false;
}

// FS2 0x002cd120 (litCells inline)

int KunitorisBoard::litPieces() const
{
    int pieces = 0;
    for (int i = 0; i < m_pieceCount; i++) {
        if (litCells(i) != 0)
            pieces++;
    }
    return pieces;
}

int KunitorisBoard::cellCount(int piece) const
{
    if (piece < 0)
        return 0;
    if (piece == kKunitorisAllPieces) {
        int cells = 0;
        for (int i = 0; i < m_pieceCount; i++)
            cells += m_pieces[i].m_cellCount;
        return cells;
    }
    return m_pieces[piece].m_cellCount;
}

// The ROM takes the remainder of the offsets as they are (a negative
// one would read before the grid; the drop's offsets are the scroll
// plus the cursor's cell plus a small offset, so they are not).
// FS2 0x002c8b90 (litCells and clearCell inline)

int KunitorisBoard::clearUnder(KunitorisCells &blocks, int *misses, int *hits)
{
    const int count = blocks.m_blocks.m_count;
    *misses = 0;
    *hits = 0;
    if (m_highlight == kKunitorisAllPieces) {
        for (int i = 0; i < count; i++) {
            const KunitorisBlock *b = blocks.block(i);
            const int x = b->m_offset[0] % m_level->width;
            const int y = b->m_offset[1] % m_level->height;
            const int piece = m_level != nullptr ? m_level->grid[x][y] : -1;
            if (piece < 0) {
                (*misses)++;
                continue;
            }
            if (m_pieces[piece].clearCell(x, y))
                (*hits)++;
            else
                (*misses)++;
        }
        return litCells(m_highlight);
    }

    KunitorisPiece *piece = m_highlight < 0 ? nullptr : &m_pieces[m_highlight];
    if (piece == nullptr) {
        *misses = count;
        *hits = 0;
        return 0;
    }
    for (int i = 0; i < count; i++) {
        const KunitorisBlock *b = blocks.block(i);
        const int x = b->m_offset[0] % m_level->width;
        const int y = b->m_offset[1] % m_level->height;
        if (piece->clearCell(x, y))
            (*hits)++;
        else
            (*misses)++;
    }
    return piece->litCells();
}

// The old choice back to the plain colour first. A negative piece: a
// random one with lit cells (from a random start, round the board once),
// -1 when none is left.
// FS2 0x002cca40
// callcheck: randNext2

int KunitorisBoard::selectPiece(int piece)
{
    if (m_highlight >= 0) {
        if (m_highlight == kKunitorisAllPieces) {
            for (int i = 0; i < m_pieceCount; i++) {
                if (litCells(i) != 0)
                    m_pieces[i].setColors(KunitorisColors_Piece);
            }
        } else {
            m_pieces[m_highlight].setColors(KunitorisColors_Piece);
        }
    }

    if (piece < 0) {
        const int first = static_cast<int>(randNext2()) % m_pieceCount;
        piece = first;
        do {
            if (litCells(piece) != 0) {
                m_pieces[piece].setColors(KunitorisColors_Flash);
                m_highlight = piece;
                return piece;
            }
            piece = (piece + 1) % m_pieceCount;
        } while (first != piece);
        m_highlight = -1;
        return -1;
    }

    if (piece == kKunitorisAllPieces) {
        for (int i = 0; i < m_pieceCount; i++) {
            if (litCells(i) != 0)
                m_pieces[i].setColors(KunitorisColors_Flash);
        }
    } else {
        if (piece >= m_pieceCount)
            return -1;
        m_pieces[piece].setColors(KunitorisColors_Flash);
    }
    m_highlight = piece;
    return piece;
}

// FS2 0x002cd390
// callcheck: __destroy_new_array (the delete[]-expression)
// callcheck: operator_new[], FUN_00104d90, KunitorisPiece, ~KunitorisPiece (the new[]-expression: CodeWarrior's __construct_new_array)
// callcheck: load (each piece)

void KunitorisBoard::loadLevel(float cell, KunitorisLevel *level)
{
    delete[] m_pieces;
    m_level = nullptr;
    m_pieceCount = 0;
    m_pieces = nullptr;
    m_highlight = -1;
    m_level = level;
    if (level == nullptr)
        return;
    m_pieceCount = level->pieceCount;
    m_pieces = new KunitorisPiece[m_pieceCount];
    for (int i = 0; i < m_pieceCount; i++) {
        KunitorisPiece &piece = m_pieces[i];
        piece.load(cell, i < m_level->pieceCount ? m_level->pieces[i].get() : nullptr);
        piece.setColors(KunitorisColors_Piece);
    }
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisGame.

KunitorisGroup *KunitorisGame::freeGroup()
{
    for (int i = 0; i < kKunitorisGroups; i++) {
        if (m_groups[i].m_state == 0)
            return &m_groups[i];
    }
    return nullptr;
}

// The board, the player's cursor and block, the level's rules, and the
// first groups on random shapes from random sides.
// FS2 0x002c8640
// callcheck: __construct_array, KunitorisGroup, ~KunitorisGroup (the member array)
// callcheck: operator_new, UiDrawer, KunitorisCursor, KunitorisBlock, GsTexture (the new-expressions; the inline KunitorisSprite ctor)
// callcheck: operator_new[], FUN_00104d90, KunitorisCells, ~KunitorisCells (the new[]-expression: CodeWarrior's __construct_new_array)
// callcheck: getItem, pushBack (the inline kunitorisItem and add)
// callcheck: kunitorisRandom, addCell, setStep, setCell (the inline spawnGroup)

KunitorisGame::KunitorisGame(int hiScore, int level)
{
    m_ending.m_frame = 0;
    m_ending.m_scene = KunitorisScene_Idle;
    m_ending.m_timer = -1;
    m_ending._0c = 0;
    m_ending.m_tiles[0] = 0;
    m_ending.m_tiles[1] = 0;
    m_ending.m_tiles[2] = 0;
    m_drawer = new UiDrawer();
    m_hiScore = hiScore;
    m_score = 0;
    m_paused = 0;
    m_heldDir = 0;
    m_sprite = new KunitorisSprite(kunitorisItem(KunitorisItem_Highlight));
    m_scroll[0] = 0;
    m_scroll[1] = 0;
    m_playerCell[0] = kKunitorisStartCell;
    m_playerCell[1] = kKunitorisStartCell;
    m_cursor = new KunitorisCursor(m_playerCell, kKunitorisLift);
    m_cellsIndex = 0;
    m_cells = new KunitorisCells[2];
    const int here[2] = { 0, 0 };
    m_cells[m_cellsIndex].add(new KunitorisBlock(m_cursor->m_cell, here, kKunitorisLift));

    m_lives = kKunitorisLevelLives[level];
    m_spawnPeriod = kKunitorisLevel84[level];
    m_drainPeriod = kKunitorisLevel88[level];
    m_stepPeriod = kKunitorisLevel8c[level];
    m_spawnTimer = 5;
    m_drainTimer = m_drainPeriod;
    m_stepTimer = m_stepPeriod;
    m_frame = 0;
    m_feverChain = 0;
    m_feverRate = 0;
    m_fever = 0;
    m_feverBar = 0;
    m_cleared = 0;
    if (level < kKunitorisHardLevel) {
        m_shapeCount = sKunitorisShapeCount[0];
        m_shapes = kKunitorisShapesEasy;
    } else {
        m_shapeCount = sKunitorisShapeCount[1];
        m_shapes = kKunitorisShapesHard;
    }

    const int groups = kKunitorisLevelGroups[level];
    for (int i = 0; i < groups; i++) {
        KunitorisGroup *group = freeGroup();
        if (group == nullptr)
            return;
        int kinds = level + kKunitorisShapeBase;
        if (m_shapeCount < kinds)
            kinds = m_shapeCount;
        spawnGroup(group, kinds);
    }
}

void KunitorisGame::spawnGroup(KunitorisGroup *group, int kinds)
{
    const KunitorisShape &shape = m_shapes[static_cast<int>(kunitorisRandom()) % kinds];
    for (int j = 0; j < shape.count; j++)
        group->addCell(shape.cells[j]);

    const u_int x = kunitorisRandom();
    const u_int y = kunitorisRandom();
    switch (kunitorisRandom() & 3) {
    case KunitorisSide_Top:
        group->setStep(0, 1);
        group->setCell(y & kKunitorisViewMask, kKunitorisEnterNear);
        break;
    case KunitorisSide_Right:
        group->setStep(-1, 0);
        group->setCell(kKunitorisEnterFar, x & kKunitorisViewMask);
        break;
    case KunitorisSide_Left:
        group->setStep(1, 0);
        group->setCell(kKunitorisEnterNear, x & kKunitorisViewMask);
        break;
    default:    // KunitorisSide_Bottom
        group->setStep(0, -1);
        group->setCell(y & kKunitorisViewMask, kKunitorisEnterFar);
        break;
    }
}

int KunitorisGame::activeGroups() const
{
    int active = 0;
    for (int i = 0; i < kKunitorisGroups; i++) {
        if (m_groups[i].m_state != KunitorisCells_Empty)
            active++;
    }
    return active;
}

void KunitorisGame::loseLives(int lives)
{
    if (m_lives > 0 && !m_cleared) {
        m_lives -= lives;
        if (m_lives < 0)
            m_lives = 0;
    }
}

void KunitorisGame::resetPlayerBlocks(KunitorisCells &cells)
{
    cells.release();
    const int here[2] = { 0, 0 };
    cells.add(new KunitorisBlock(m_cursor->m_cell, here, kKunitorisLift));
}

KunitorisGroup *KunitorisGame::detachPlayerBlocks(int dx, int dy)
{
    KunitorisCells &player = m_cells[m_cellsIndex];
    const int count = player.m_blocks.m_count;
    const int cx = m_cursor->m_cell[0];
    const int cy = m_cursor->m_cell[1];
    KunitorisGroup *group = freeGroup();
    if (group == nullptr)
        return nullptr;
    for (int i = 0; i < count; i++) {
        KunitorisBlock *b = player.block(i);
        const int offset[2] = { b->refreshCell()[0] - cx, b->refreshCell()[1] - cy };
        if (!group->hasOffset(offset))
            group->add(new KunitorisBlock(group->m_cell, offset, kKunitorisLift));
    }
    group->m_step[0] = dx;
    group->m_step[1] = dy;
    group->m_cell[0] = cx;
    group->m_cell[1] = cy;
    return group;
}

// The player's blocks cross more than this many cells: they break off.
constexpr int kKunitorisMaxSpan = 16;

// FS2 0x002c9170 (hasBlockAt, release, offsetBounds and kunitorisPlay inline)
// callcheck: operator_new, KunitorisBlock, pushBack (the new-expression and the inline add)
// callcheck: clear, play (the inline release; kunitorisPlay)

void KunitorisGame::absorb()
{
    KunitorisCells &player = m_cells[m_cellsIndex];
    for (int i = 0; i < kKunitorisGroups; i++) {
        KunitorisGroup *group = &m_groups[i];
        const int state = group->m_state;
        if (state == KunitorisCells_Empty || state == KunitorisCells_Shot || state == KunitorisCells_Sunk)
            continue;
        const int count = group->m_blocks.m_count;
        bool touching = false;
        for (int j = 0; j < count; j++) {
            KunitorisBlock *b = group->block(j);
            const int x = b->refreshCell()[0];
            const int y = b->refreshCell()[1];
            if (player.hasBlockAt(x, y)) {
                touching = true;
                break;
            }
        }
        if (!touching)
            continue;
        // Each block joins where it was before the group's last move.
        for (int j = 0; j < count; j++) {
            KunitorisBlock *b = group->block(j);
            const int x = b->refreshCell()[0] - group->m_moved[0];
            const int y = b->refreshCell()[1] - group->m_moved[1];
            if (!player.hasBlockAt(x, y)) {
                const int offset[2] = { x - m_cursor->m_cell[0], y - m_cursor->m_cell[1] };
                player.add(new KunitorisBlock(m_cursor->m_cell, offset, kKunitorisLift));
            }
        }
        group->release();
    }

    int min[2], max[2];
    player.offsetBounds(min, max);
    if ((max[0] - min[0] > kKunitorisMaxSpan || max[1] - min[1] > kKunitorisMaxSpan) && breakOff())
        kunitorisPlay(KunitorisSeSlot_Break, KunitorisSe_BreakOff);
}

// FS2 0x002c9720 (detachPlayerBlocks and resetPlayerBlocks inline)
// callcheck: operator_new, KunitorisBlock, hasOffset, add (the inline detachPlayerBlocks)
// callcheck: clear, pushBack (the inline release and add)

bool KunitorisGame::breakOff()
{
    KunitorisCells &player = m_cells[m_cellsIndex];
    if (player.m_blocks.m_count < 2)
        return false;
    KunitorisGroup *group = detachPlayerBlocks(0, 0);
    if (group == nullptr)
        return false;
    group->sink();
    resetPlayerBlocks(player);
    return true;
}

// FS2 0x002c9a80 (detachPlayerBlocks, launch and resetPlayerBlocks inline)
// callcheck: operator_new, KunitorisBlock, hasOffset, add (the inline detachPlayerBlocks)
// callcheck: clear, pushBack (the inline release and add)

void KunitorisGame::shoot(int dx, int dy)
{
    KunitorisCells &player = m_cells[m_cellsIndex];
    if (player.m_blocks.m_count < 2)
        return;
    KunitorisGroup *group = detachPlayerBlocks(dx, dy);
    if (group == nullptr)
        return;
    group->launch();
    resetPlayerBlocks(player);
}

// FS2 0x002c8ff0

void KunitorisGame::turnTouching()
{
    for (int i = 0; i < kKunitorisGroups - 1; i++) {
        for (int j = i + 1; j < kKunitorisGroups; j++) {
            KunitorisGroup *a = &m_groups[i];
            KunitorisGroup *b = &m_groups[j];
            if (a->m_state == KunitorisCells_Empty || a->m_state == KunitorisCells_Shot
                || a->m_state == KunitorisCells_Sunk)
                continue;
            if (b->m_state == KunitorisCells_Empty || b->m_state == KunitorisCells_Shot
                || b->m_state == KunitorisCells_Sunk)
                continue;
            if (a->m_moved[0] == b->m_moved[0] && a->m_moved[1] == b->m_moved[1] && a->touches(*b)) {
                a->m_step[0] = a->m_moved[1];
                a->m_step[1] = a->m_moved[0];
            }
        }
    }
}

// The bars: pack item 0x12 from (0, -117), value pixels long, its u to
// value / 80. Inline in the ROM (both bar draws).
static void kunitorisDrawBar(UiDrawer *drawer, float value, const UiVec4 *color)
{
    GsTexture *tex = new GsTexture(kunitorisItem(KunitorisItem_Bar));
    const UiTexVertex quad[2] = {
        { { 0.01f, 0.01f }, { 0.0f, -117.0f } },
        { { value / 80.0f, 1.0f }, { value + 0.0f, -101.0f } },
    };
    drawer->setTexture(tex);
    drawer->setGsReg(kGsRegTex1_1, 0);
    drawer->setPrimColor(kKunitorisPicturePrim, color);
    drawer->drawTexFlat(quad, 2, 0);
    delete tex;
}

static const UiVec4 kKunitorisDark = { 0.25f, 0.25f, 0.25f, 1.0f };    /* rodata 0x2fc010 */
constexpr u_int kKunitorisBlinkBit = 2;     // m_frame's bit the blink bar reads

// FS2 0x002c8510
// callcheck: operator_new, GsTexture, getItem, setTexture, setGsReg, setPrimColor, drawTexFlat (the inline kunitorisDrawBar)

void KunitorisGame::drawLivesBar()
{
    const float lives = static_cast<float>(m_lives);
    if (0.0f < lives)
        kunitorisDrawBar(m_drawer, lives, &kKunitorisGrey);
}

// FS2 0x002c83a0
// callcheck: operator_new, GsTexture, getItem, setTexture, setGsReg, setPrimColor, drawTexFlat (the inline kunitorisDrawBar)

void KunitorisGame::drawBlinkBar()
{
    const float value = static_cast<float>(m_feverBar);
    if (0.0f < value)
        kunitorisDrawBar(m_drawer, value, (m_frame & kKunitorisBlinkBit) == 0 ? &kKunitorisDark : &kKunitorisGrey);
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisEnding.

// The four quads both draws show at (x, y) (data 0x2fc280): quad 0 the
// banner, 1..3 the tiles, whose v setTiles narrows to one 64th of the
// picture's rows (the ROM's data, so it stays narrowed for the next draw).
constexpr int kEndingQuads = 4;
static UiTexVertex sEndingQuads[kEndingQuads][2] = {
    { { { 0.0f, 0.0f }, { -75.0f, 75.0f } }, { { 1.0f, 1.0f }, { 149.0f, 107.0f } } },
    { { { 0.0f, 0.0f }, { -69.0f, 76.0f } }, { { 1.0f, 1.0f }, { -5.0f, 92.0f } } },
    { { { 0.0f, 0.0f }, { -6.0f, 76.0f } },  { { 1.0f, 1.0f }, { 58.0f, 92.0f } } },
    { { { 0.0f, 0.0f }, { 58.0f, 76.0f } },  { { 1.0f, 1.0f }, { 122.0f, 92.0f } } },
};
constexpr int   kEndingTileKinds = 63;      // random tiles: 0 .. 62
constexpr float kEndingTileRow   = 1.0f / 64.0f;

// The frames of m_timer each quad shows from, and the frame m_timer stops
// after (rodata 0x2fc300 / 0x2fc320; each draw copies its five ints to the
// stack).
static const int kIntroShow[kEndingQuads] = { 0, 0, 30, 60 };          /* rodata 0x2fc300 */
constexpr int    kIntroShowEnd = 300;                                   /* rodata 0x2fc310 */
static const int kEndShow[kEndingQuads] = { 0, 0, 15, 30 };            /* rodata 0x2fc320 */
constexpr int    kEndShowEnd = 120;                                     /* rodata 0x2fc330 */
// The pictures: the ROM computes 0x22 + i in drawIntro, and picks 0x22 /
// 0x26 / 0x27 / 0x28 by index in draw.
static const int kIntroItems[kEndingQuads] = {
    KunitorisItem_Banner, KunitorisItem_IntroTile0, KunitorisItem_IntroTile0 + 1, KunitorisItem_IntroTile0 + 2,
};
static const int kEndItems[kEndingQuads] = {
    KunitorisItem_Banner, KunitorisItem_EndTile0, KunitorisItem_EndTile0 + 1, KunitorisItem_EndTile0 + 2,
};

// A scene's pose animation: steps of { frames until, pack item }; a looping
// one takes the frame modulo its last step's limit (the ROM does that
// inside the step loop, the same once done).
// layout: host (EE 8 bytes a step, 12 a scene: count, steps, loop)
struct KunitorisPoseStep
{
    int until;
    int item;
};
// layout: host (EE 12 bytes)
struct KunitorisPoseAnim
{
    int                      count;
    const KunitorisPoseStep *steps;
    u_char                   loop;
};

static const KunitorisPoseStep kPoseIdle[] = {                          /* data 0x2fc040 */
    { 30, 0x1a }, { 60, 0x1d },
};
static const KunitorisPoseStep kPose1[] = {                             /* data 0x2fc050 */
    { 10, 0x1d }, { 20, 0x1a }, { 50, 0x1c }, { 60, 0x1a }, { 70, 0x1d }, { 80, 0x1a },
    { 110, 0x1c }, { 115, 0x1a }, { 135, 0x1c }, { 140, 0x1a }, { 175, 0x1c },
};
static const KunitorisPoseStep kPose2[] = {                             /* data 0x2fc0b0 */
    { 10, 0x1a }, { 90, 0x1e },
};
static const KunitorisPoseStep kPose3[] = {                             /* data 0x2fc0c0 */
    { 10, 0x1d }, { 20, 0x1a }, { 50, 0x1b }, { 60, 0x1a }, { 70, 0x1d }, { 80, 0x1a },
    { 110, 0x1b }, { 115, 0x1a }, { 135, 0x1b }, { 140, 0x1a }, { 175, 0x1b },
};
static const KunitorisPoseStep kPose4[] = {                             /* sdata 0x31acd0 */
    { 180, 0x1f },
};
static const KunitorisPoseStep kPose5[] = {                             /* data 0x2fc120 */
    { 5, 0x1a },   { 10, 0x1d },  { 30, 0x1b },  { 35, 0x1d },  { 40, 0x1a },  { 45, 0x1d },
    { 65, 0x1c },  { 70, 0x1a },  { 75, 0x1d },  { 80, 0x1a },  { 100, 0x1f }, { 105, 0x1a },
    { 110, 0x1d }, { 115, 0x1a }, { 135, 0x1e }, { 137, 0x1a }, { 139, 0x1d }, { 141, 0x1b },
    { 143, 0x1d }, { 145, 0x1a }, { 147, 0x1d }, { 149, 0x1c }, { 151, 0x1a }, { 153, 0x1d },
    { 155, 0x1a }, { 157, 0x1f }, { 159, 0x1a }, { 161, 0x1d }, { 163, 0x1a }, { 200, 0x1e },
};
static const KunitorisPoseStep kPoseTiles[] = {                         /* data 0x2fc210 */
    { 30, 0x20 }, { 60, 0x21 },
};
#define KUNITORIS_POSE(steps, loop) { sizeof(steps) / sizeof(steps[0]), steps, loop }
static const KunitorisPoseAnim kPoseAnims[KunitorisScene_Count] = {    /* data 0x2fc220 */
    KUNITORIS_POSE(kPoseIdle, 1),
    KUNITORIS_POSE(kPose1, 0),
    KUNITORIS_POSE(kPose2, 1),
    KUNITORIS_POSE(kPose3, 0),
    KUNITORIS_POSE(kPose4, 0),
    KUNITORIS_POSE(kPose5, 0),
    KUNITORIS_POSE(kPoseTiles, 1),
    { 0, nullptr, 0 },
};
#undef KUNITORIS_POSE

// The scene's picture at frame, -1 past its last step. Inline in the ROM.
static int kunitorisPoseItem(int scene, int frame)
{
    const KunitorisPoseAnim &anim = kPoseAnims[scene];
    for (int i = 0; i < anim.count; i++) {
        if (anim.loop)
            frame %= anim.steps[anim.count - 1].until;
        if (frame < anim.steps[i].until)
            return anim.steps[i].item;
    }
    return -1;
}

// The pose's place: 64 x 64 left of the banner.
constexpr float kPoseLeft = -124.0f;
constexpr float kPoseTop  = 44.0f;
constexpr float kPoseSize = 64.0f;

// FS2 0x002ca790

void KunitorisEnding::setTiles(int tile)
{
    m_timer = 0;
    for (int i = 0; i < 3; i++) {
        m_tiles[i] = tile < 0 ? static_cast<int>(randNext2()) % kEndingTileKinds : tile;
        const float top = static_cast<float>(m_tiles[i]) * kEndingTileRow;
        float bottom = top + kEndingTileRow;
        sEndingQuads[i + 1][0].uv[1] = top;
        if (1.0f <= bottom)
            bottom = 0.99999f;
        sEndingQuads[i + 1][1].uv[1] = bottom;
    }
}

void KunitorisEnding::drawQuads(float x, float y, UiDrawer *drawer, const int *show, int end, const int *items)
{
    if (m_timer < 0)
        return;
    for (int i = 0; i < kEndingQuads; i++) {
        if (m_timer < show[i])
            break;
        GsTexture *tex = new GsTexture(kunitorisItem(items[i]));
        drawer->setTexture(tex);
        drawer->setGsReg(SCE_GS_TEX1_1, 0);
        drawer->setPrimColor(kKunitorisPicturePrim, &kKunitorisGrey);
        UiTexVertex quad[2] = { sEndingQuads[i][0], sEndingQuads[i][1] };
        quad[0].xy[0] += x;
        quad[1].xy[0] += x;
        quad[0].xy[1] += y;
        quad[1].xy[1] += y;
        drawer->drawTexFlat(quad, 2, 0);
        delete tex;
    }
    if (end < m_timer)
        m_timer = -1;
}

void KunitorisEnding::drawPose(float x, float y, UiDrawer *drawer)
{
    int item = kunitorisPoseItem(m_scene, m_frame);
    if (item < 0) {
        setScene(KunitorisScene_Idle);
        item = kunitorisPoseItem(m_scene, m_frame);
    }
    const float left = x + kPoseLeft;
    const float top = y + kPoseTop;
    drawPicture(drawer, item, left, top, left + kPoseSize, top + kPoseSize);
}

// FS2 0x002ca330
// callcheck: operator_new, GsTexture, getItem, setTexture, setGsReg, setPrimColor, drawTexFlat (the inline drawQuads, drawPose, drawPicture)

void KunitorisEnding::drawIntro(float x, float y, UiDrawer *drawer)
{
    drawQuads(x, y, drawer, kIntroShow, kIntroShowEnd, kIntroItems);
    drawPose(x, y, drawer);
}

// FS2 0x002c9de0
// callcheck: operator_new, GsTexture, getItem, setTexture, setGsReg, setPrimColor, drawTexFlat (the inline drawQuads, drawPose, drawPicture)

void KunitorisEnding::draw(float x, float y, UiDrawer *drawer)
{
    drawQuads(x, y, drawer, kEndShow, kEndShowEnd, kEndItems);
    drawPose(x, y, drawer);
}

// The board at the scroll, four times: the level wraps, so the copies one
// board width / height up-left fill what the view shows past its edges.
// FS2 0x002c81c0
// callcheck: identity, setScale, translate, copy, updateMatrix, setPixelMode, setPrimColor, drawFlat (the pieces draw through their vtable)

void KunitorisGame::drawBoard()
{
    const float x = static_cast<float>(m_scroll[0]);
    const float y = static_cast<float>(m_scroll[1]);
    const float left = x - static_cast<float>(m_level->width);
    const float up = y - static_cast<float>(m_level->height);
    const float xs[4] = { x, x, left, left };
    const float ys[4] = { y, up, y, up };
    for (int i = 0; i < 4; i++) {
        const float at[2] = { -128.0f - xs[i] * kKunitorisCellPixels,
                              -128.0f - ys[i] * kKunitorisCellPixels };
        UiMatrix matrix;
        matrix.identity();
        matrix.setScale(1.0f, 1.0f);
        matrix.translate(at);
        m_drawer->m_stack[m_drawer->m_top].copy(matrix);
        m_drawer->updateMatrix();
        m_drawer->setPixelMode(1);
        UiDrawer *drawer = m_drawer;
        for (int j = 0; j < m_pieceCount; j++)
            m_pieces[j].draw(drawer, KunitorisPass_Lifted);
        drawer->setPrimColor(kKunitorisBorderPrim, &kKunitorisBorder);
        drawer->drawFlat(m_level->outline, m_level->outlineCount * 2, 0);
    }
}

// The frame: the board, the blocks' shadows then the blocks lifted (the
// other set first, the groups coming in highlighted), the cursor, the
// level's map small at the top, the ending's pictures, the scores and the
// state line.
// FS2 0x002c5ea0
// callcheck: begin, end, drawBoard, drawWithHighlight, drawIntro, kunitorisPrintf, drawBlinkBar, drawLivesBar
// callcheck: identity, setScale, translate, copy, updateMatrix (the map's and the reset matrices; the cursor and the pieces draw through their vtables)

void KunitorisGame::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    m_drawer->begin(packet, texList);
    drawBoard();

    KunitorisCells &other = m_cells[m_cellsIndex ^ 1];
    KunitorisCells &mine = m_cells[m_cellsIndex];
    for (KunitorisActorList::Node *n = other.m_blocks.m_head; n != nullptr; n = n->next)
        static_cast<KunitorisBlock *>(*n->item)->drawWithHighlight(m_drawer, KunitorisPass_Shadow, nullptr);
    for (KunitorisActorList::Node *n = mine.m_blocks.m_head; n != nullptr; n = n->next)
        static_cast<KunitorisBlock *>(*n->item)->drawWithHighlight(m_drawer, KunitorisPass_Shadow, nullptr);
    for (int i = 0; i < kKunitorisGroups; i++) {
        if (m_groups[i].m_state == 0)
            continue;
        for (KunitorisActorList::Node *n = m_groups[i].m_blocks.m_head; n != nullptr; n = n->next)
            static_cast<KunitorisBlock *>(*n->item)->drawWithHighlight(m_drawer, KunitorisPass_Highlight, nullptr);
    }
    m_cursor->draw(m_drawer, KunitorisPass_Shadow);

    for (KunitorisActorList::Node *n = other.m_blocks.m_head; n != nullptr; n = n->next)
        static_cast<KunitorisBlock *>(*n->item)->drawWithHighlight(m_drawer, KunitorisPass_Lifted, nullptr);
    for (KunitorisActorList::Node *n = mine.m_blocks.m_head; n != nullptr; n = n->next)
        static_cast<KunitorisBlock *>(*n->item)->drawWithHighlight(m_drawer, KunitorisPass_Lifted, nullptr);
    for (int i = 0; i < kKunitorisGroups; i++) {
        if (m_groups[i].m_state == 0)
            continue;
        for (KunitorisActorList::Node *n = m_groups[i].m_blocks.m_head; n != nullptr; n = n->next)
            static_cast<KunitorisBlock *>(*n->item)->drawWithHighlight(m_drawer, KunitorisPass_Lifted, nullptr);
    }
    m_cursor->draw(m_drawer, KunitorisPass_Lifted);

    // The level's map, an eighth of the board's size, at (64, 16).
    const float mapAt[2] = { 64.0f, 16.0f };
    UiMatrix map;
    map.identity();
    map.setScale(kKunitorisMapScale, kKunitorisMapScale);
    map.translate(mapAt);
    m_drawer->m_stack[m_drawer->m_top].copy(map);
    m_drawer->updateMatrix();
    UiDrawer *drawer = m_drawer;
    for (int i = 0; i < m_pieceCount; i++) {
        if (i == m_highlight)
            m_pieces[i].draw(drawer, KunitorisPass_Lifted);
        else
            m_pieces[i].draw(drawer, KunitorisPass_Shadow);
    }
    UiMatrix reset;
    reset.identity();
    m_drawer->m_stack[m_drawer->m_top].copy(reset);
    m_drawer->updateMatrix();
    m_ending.drawIntro(0.0f, 0.0f, m_drawer);

    m_drawer->m_color = kKunitorisWhite;
    kunitorisPrintf(sFont, m_drawer, 8, 10, kKunitorisTitleText);
    kunitorisPrintf(sFont, m_drawer, 8, 0x14, "HISCORE:%d", m_hiScore);
    kunitorisPrintf(sFont, m_drawer, 0x78, 0x14, "SCORE:%d", m_score);
    m_drawer->m_color = kKunitorisWhite;
    if (m_highlight == kKunitorisAllPieces)
        drawBlinkBar();
    else
        drawLivesBar();
    if (!m_cleared) {
        if (m_lives < 1) {
            m_drawer->m_color = kKunitorisWhite;
            kunitorisPrintf(sFont, m_drawer, 0x68, 0x6c, "GAME OVER");
            m_drawer->m_color = kKunitorisWhite;
        } else if (m_paused) {
            m_drawer->m_color = kKunitorisWhite;
            kunitorisPrintf(sFont, m_drawer, 0x78, 0x6c, "PAUSE");
            m_drawer->m_color = kKunitorisWhite;
        }
    } else {
        m_drawer->m_color = kKunitorisWhite;
        kunitorisPrintf(sFont, m_drawer, 0x60, 0x6c, "GAME CLEAR!");
        m_drawer->m_color = kKunitorisWhite;
    }
    m_drawer->end();
}

// Inline in the ROM (the end of kunitorisThread's level); the groups and
// ~KunitorisBoard after it.
// callcheck: ~KunitorisSprite, ~KunitorisCursor, ~UiDrawer (the delete-expressions)

KunitorisGame::~KunitorisGame()
{
    delete m_sprite;
    delete[] m_cells;
    delete m_cursor;
    delete m_drawer;
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisGame::update.

constexpr int   kKunitorisMaxLives    = 80;             // the lives bar's 80 pixels
constexpr int   kKunitorisFeverBar    = 80;
constexpr float kKunitorisFeverStart  = 0.0030769231f;  // 0x3b49a634: a fever lasts 325 frames ..
constexpr float kKunitorisFeverChain  = 0.00015384615f; // 0x392151c3: .. each piece cleared in it 1/6500 shorter
constexpr int   kKunitorisMaxGroups   = 45;             // no new group from this many on
constexpr int   kKunitorisBigClear    = 9;              // cleared clean with more cells: KunitorisScene_Clean

// The lives a piece cleared gives back, by the cells put out in the drop
// that cleared it (data 0x2fbf90, its count at sdata 0x31acc8).
static const int kKunitorisClearLives[] = { 0, 4, 6, 8, 12, 16, 24, 32, 40 };
static int sKunitorisClearLivesCount = 9;               /* sdata 0x31acc8 */

constexpr u_int kPadTriangle = 1u << PadBit_Triangle;
constexpr u_int kPadCircle   = 1u << PadBit_Circle;
constexpr u_int kPadCross    = 1u << PadBit_Cross;
constexpr u_int kPadSquare   = 1u << PadBit_Square;
constexpr u_int kPadLeft     = 1u << PadBit_Left;
constexpr u_int kPadRight    = 1u << PadBit_Right;

// A frame: the timers, lives drained while no fever runs, groups coming in
// and moving, the view scrolled by the d-pad, then a finished drop scored
// or a button's action (Triangle turn, Square flip, Circle drop, Cross
// shoot), the blocks and the cursor stepped and the fever run down.
// FS2 0x002c64d0 (inline: activeGroups, freeGroup, spawnGroup, loseLives,
// cellCount, updateBlocks, release, kunitorisPlay, KunitorisBoard::update)
// callcheck: randNext2, kunitorisRandom, litPieces, absorb, isEmpty, shoot, turnTouching (kunitorisRandom: the inline spawnGroup)
// callcheck: step, isGone, release, scroll, clearUnder, turn, flip, drop (called on members the parser cannot follow)
// callcheck: addCell, setStep, setCell, setTiles, selectPiece, play, stop, clear, operator_new, KunitorisBlock, pushBack, update

bool KunitorisGame::update(GamePad *pad)
{
    if (m_paused)
        return false;
    m_ending.m_frame++;
    if (m_ending.m_timer >= 0)
        m_ending.m_timer++;
    randNext2();
    if (litPieces() == 0)
        m_cleared = 1;
    if (m_cleared)
        return false;

    // A life lasts m_drainPeriod frames, none is lost in a fever.
    if (m_highlight != kKunitorisAllPieces) {
        m_drainTimer--;
        m_feverChain = 0;
        if (m_drainTimer < 1) {
            loseLives(1);
            m_drainTimer = m_drainPeriod;
        }
    }
    if (m_lives < 1) {
        m_ending.setScene(KunitorisScene_Over);
        return false;
    }
    absorb();
    if (m_lives < 1)
        return false;

    // A new group every m_spawnPeriod frames (sooner in a fever), at once
    // when none is left.
    m_spawnTimer--;
    if (m_highlight == kKunitorisAllPieces)
        m_spawnTimer /= 2;
    if (m_spawnTimer < 1 || activeGroups() == 0) {
        if (activeGroups() < kKunitorisMaxGroups) {
            KunitorisGroup *group = freeGroup();
            if (group != nullptr) {
                int kinds = sKunitorisShapeCount[0];
                if (m_shapeCount < kinds)
                    kinds = m_shapeCount;
                spawnGroup(group, kinds);
            }
        }
        m_spawnTimer = m_spawnPeriod;
    }

    // The groups move every m_stepPeriod frames (twice as often in a fever).
    m_stepTimer--;
    if (m_highlight == kKunitorisAllPieces)
        m_stepTimer--;
    const bool move = m_stepTimer < 1;
    for (int i = 0; i < kKunitorisGroups; i++) {
        KunitorisGroup &group = m_groups[i];
        if (group.isEmpty())
            continue;
        group.step(move);
        if (group.isGone())
            group.release();
    }
    if (move) {
        m_stepTimer = m_stepPeriod;
        absorb();
        turnTouching();
    }

    // The d-pad: a press takes the direction, it scrolls while held (on the
    // pad's repeat) and lets go on release.
    const u_int pressed = pad->m_pressed;
    if (pressed & kPadUp)
        m_heldDir = KunitorisDir_Up;
    else if (pressed & kPadDown)
        m_heldDir = KunitorisDir_Down;
    else if (pressed & kPadLeft)
        m_heldDir = KunitorisDir_Left;
    else if (pressed & kPadRight)
        m_heldDir = KunitorisDir_Right;
    switch (m_heldDir) {
    case KunitorisDir_Right:
        if (!(pad->m_held & kPadRight))
            m_heldDir = KunitorisDir_None;
        break;
    case KunitorisDir_Left:
        if (!(pad->m_held & kPadLeft))
            m_heldDir = KunitorisDir_None;
        break;
    case KunitorisDir_Down:
        if (!(pad->m_held & kPadDown))
            m_heldDir = KunitorisDir_None;
        break;
    case KunitorisDir_Up:
        if (!(pad->m_held & kPadUp))
            m_heldDir = KunitorisDir_None;
        break;
    }
    int dx = 0;
    int dy = 0;
    const u_int repeat = pad->m_repeat;
    switch (m_heldDir) {
    case KunitorisDir_Right:
        if (repeat & kPadRight)
            dx = 1;
        break;
    case KunitorisDir_Left:
        if (repeat & kPadLeft)
            dx = -1;
        break;
    case KunitorisDir_Down:
        if (repeat & kPadDown)
            dy = 1;
        break;
    case KunitorisDir_Up:
        if (repeat & kPadUp)
            dy = -1;
        break;
    default:
        if (repeat & kPadUp) {
            m_heldDir = KunitorisDir_Up;
            dy = -1;
        } else if (repeat & kPadDown) {
            m_heldDir = KunitorisDir_Down;
            dy = 1;
        } else if (repeat & kPadLeft) {
            m_heldDir = KunitorisDir_Left;
            dx = -1;
        } else if (repeat & kPadRight) {
            m_heldDir = KunitorisDir_Right;
            dx = 1;
        }
        break;
    }
    m_scroll[1] += dy;
    m_scroll[0] += dx;
    if (m_scroll[0] < 0)
        m_scroll[0] = m_level->width - 1;
    if (m_level->width <= m_scroll[0])
        m_scroll[0] = 0;
    if (m_scroll[1] < 0)
        m_scroll[1] = m_level->height - 1;
    if (m_level->height <= m_scroll[1])
        m_scroll[1] = 0;
    // The groups stay on the board.
    for (int i = 0; i < kKunitorisGroups; i++) {
        if (!m_groups[i].isEmpty())
            m_groups[i].scroll(-dx, -dy);
    }
    if (dx != 0 || dy != 0) {
        kunitorisPlay(KunitorisSeSlot_Scroll, KunitorisSe_Cursor);
        absorb();
    }

    KunitorisCells &dropped = m_cells[m_cellsIndex ^ 1];
    const u_int droppedState = dropped.m_state;
    if (droppedState == KunitorisCells_Gone) {
        // The drop has landed: score it.
        const int before = litPieces();
        int misses;
        int hits;
        const int lit = clearUnder(dropped, &misses, &hits);
        int points = hits;
        int lives = 0;
        if (m_highlight == kKunitorisAllPieces) {
            const int after = litPieces();
            if (after < before) {
                lives = before - after * 2;
                m_feverRate += kKunitorisFeverChain;
                m_fever = 1.0f;
                m_feverBar = kKunitorisFeverBar;
                m_ending.setTiles(-1);
                m_ending.setScene(KunitorisScene_Cheer);
                m_feverChain++;
            }
            const int counted = hits;
            points = hits + m_feverChain * hits * 2;
            if (misses < hits)
                points += (hits - misses) * 3;
            hits = 0;
            misses = 0;
            points += counted * 5;
        } else if (lit == 0) {
            // m_highlight's piece is cleared.
            lives = hits < sKunitorisClearLivesCount ? kKunitorisClearLives[hits]
                                                     : kKunitorisClearLives[sKunitorisClearLivesCount - 1];
            if (hits == cellCount(m_highlight) && misses == 0) {
                lives *= 2;
                if (hits > kKunitorisBigClear)
                    m_ending.setScene(KunitorisScene_Clean);
            }
            points = hits * 4;
            const int size = cellCount(m_highlight);
            if (size < 4 || size > 10) {
                if (size > 19 && (randNext2() & 0xf) == 1)
                    m_ending.setScene(KunitorisScene_Big);
            } else if ((randNext2() & 3) == 1) {
                m_ending.setScene(KunitorisScene_Cheer);
            }
            m_ending.setTiles(-1);
            if (hits == cellCount(m_highlight) && misses == 0) {
                // Cleared in one clean drop: a fever, every lit piece flashes.
                selectPiece(kKunitorisAllPieces);
                m_feverRate = kKunitorisFeverStart;
                m_fever = 1.0f;
                m_feverBar = kKunitorisFeverBar;
                m_ending.setScene(KunitorisScene_Cheer);
                kunitorisPlayBgm(KunitorisSe_FeverBgm);
            } else {
                selectPiece(-1);
            }
        } else {
            lives = (hits - misses) / 2;
        }
        if (lives < 0)
            lives = 0;
        if (m_lives > 0) {
            m_lives += lives;
            if (m_lives > kKunitorisMaxLives)
                m_lives = kKunitorisMaxLives;
        }
        if (m_highlight != kKunitorisAllPieces)
            loseLives(misses * 2 + 2);
        if (points != 1 && misses == 0)
            points *= 2;
        if (m_highlight == kKunitorisAllPieces)
            points *= 2;
        m_score += points;
        if (m_hiScore < m_score)
            m_hiScore = m_score;
        m_cells[m_cellsIndex ^ 1].release();
        if (misses > 1)
            m_ending.setScene(KunitorisScene_Miss);
    } else {
        KunitorisCells &player = m_cells[m_cellsIndex];
        if (pressed & kPadTriangle) {
            if (player.turn() > 1)
                kunitorisPlay(KunitorisSeSlot_Turn, KunitorisSe_Turn);
        } else if (pressed & kPadSquare) {
            if (player.flip() > 1)
                kunitorisPlay(KunitorisSeSlot_Turn, KunitorisSe_Flip);
        } else if (pressed & kPadCircle) {
            // Only once the last drop has been scored.
            if (droppedState < KunitorisCells_Shot) {
                player.drop(m_scroll);
                kunitorisPlay(KunitorisSeSlot_Cursor, KunitorisSe_Drop);
                m_cellsIndex ^= 1;
                const int here[2] = { 0, 0 };
                m_cells[m_cellsIndex].add(new KunitorisBlock(m_cursor->m_cell, here, kKunitorisLift));
            }
        } else if ((pressed & kPadCross) && droppedState < KunitorisCells_Shot) {
            kunitorisPlay(KunitorisSeSlot_Decide, KunitorisSe_Shoot);
            switch (m_heldDir) {
            case KunitorisDir_Right: shoot(1, 0); break;
            case KunitorisDir_Left:  shoot(-1, 0); break;
            case KunitorisDir_Down:  shoot(0, 1); break;
            case KunitorisDir_Up:    shoot(0, -1); break;
            default:                 shoot(0, -1); break;
            }
        }
    }

    for (int i = 0; i < 2; i++)
        m_cells[i].updateBlocks();
    m_cursor->KunitorisActor::update();     // the base's body, not the cursor's slot
    m_frame++;

    // The fever runs down; at its end one piece flashes again.
    if (m_highlight == kKunitorisAllPieces) {
        m_fever -= m_feverRate;
        bool burning;
        if (m_fever > 0.0f) {
            m_feverBar = static_cast<int>(m_fever * 80.0f);
            if (m_feverBar > kKunitorisFeverBar)
                m_feverBar = kKunitorisFeverBar;
            burning = true;
        } else {
            m_feverBar = 0;
            burning = false;
        }
        if (!burning) {
            selectPiece(-1);
            m_feverChain = 0;
            kunitorisPlayBgm(KunitorisSe_SelectBgm);
        }
    }

    if (m_lives < 1)
        return false;
    KunitorisBoard::update();               // inline in the ROM: every piece's colour stepped
    return true;
}
