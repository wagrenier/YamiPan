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
    : m_active(0)
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
    m_active = 0;
}

void KunitorisCells::add(KunitorisActor *actor)
{
    m_blocks.pushBack(actor);
    if (m_active == 0)
        m_active = 1;
}

// FS2 0x002cafd0
// callcheck: KunitorisActorList (the member's ctor)

KunitorisGroup::KunitorisGroup()
    : _1c(0), _20(0)
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

// ──────────────────────────────────────────────────────────────────────
// KunitorisBoard: inline in the ROM (KunitorisGame's ctor and dtor).

KunitorisBoard::KunitorisBoard()
    : _1c(0), _20(0), _24(0), _28(0), m_level(nullptr), _30(0), m_pieces(nullptr), _38(-1)
{
    m_color = kKunitorisGrey;
}

KunitorisBoard::~KunitorisBoard()
{
    delete[] m_pieces;
}

// ──────────────────────────────────────────────────────────────────────
// KunitorisGame.

KunitorisGroup *KunitorisGame::freeGroup()
{
    for (int i = 0; i < kKunitorisGroups; i++) {
        if (m_groups[i].m_active == 0)
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
    m_selectToggle = 0;
    _75c = 0;
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
    _84 = kKunitorisLevel84[level];
    _88 = kKunitorisLevel88[level];
    _8c = kKunitorisLevel8c[level];
    _90 = 5;
    _94 = _88;
    _98 = _8c;
    _760 = 0;
    _774 = 0;
    _768 = 0;
    _76c = 0;
    _764 = 0;
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
        const KunitorisShape &shape = m_shapes[static_cast<int>(randNext2()) % kinds];
        for (int j = 0; j < shape.count; j++)
            group->addCell(shape.cells[j]);

        const u_int x = randNext2();
        const u_int y = randNext2();
        switch (randNext2() & 3) {
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
constexpr u_int kKunitorisBlinkBit = 2;     // _760's bit the blink bar reads

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
    const float value = static_cast<float>(_764);
    if (0.0f < value)
        kunitorisDrawBar(m_drawer, value, (_760 & kKunitorisBlinkBit) == 0 ? &kKunitorisDark : &kKunitorisGrey);
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

// ---- stubs (tools/reverse/stubgen.py) ----

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

// stub of FS2 0x002cd690
KunitorisPiece::~KunitorisPiece()
{
    YAMIPAN_STUB("KunitorisPiece::~KunitorisPiece", 0x002cd690);
}
