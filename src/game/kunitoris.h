/* ==========================================================================
 *  game/kunitoris.h
 *
 *  Kunitoris, the Extra mode behind the top menu (game/kunitoris.cpp,
 *  0x2c42a0..0x2ce0c0): a falling-piece minigame with its own pack
 *  (dataEU/kuniEU.pak: levels, font, title pictures, its sound table and
 *  bank). runExtraMode runs kunitorisThread in slot 3 and waits for it; a
 *  back thread in slot 4 keeps the clear colour and the sound player.
 *  Each level's best score and its clear go into SaveData::_46c / _4ac
 *  (16 levels: LEVEL 1..15 and JAPAN); 15 cleared unlock JAPAN and the
 *  ENDING line, the 15th clear plays the ending once.
 *
 *  The file name is the port's (no source string); the title the game
 *  prints is "= KUNITORIS =".
 * ======================================================================== */
#ifndef _GAME_KUNITORIS_H
#define _GAME_KUNITORIS_H

#include <stddef.h>

#include "game/s2ddata.h"   /* S2dRef */
#include "sdk/scetypes.h"

class GamePad;              // game/gamesystem.h
class TexAreaList;          // game/gsblockmap.h
class UiDrawer;             // game/uidrawer.h
class UiFont;               // game/uigame.h
struct sceVif1Packet;

// The pack's items by id (PackFile::getItem).
enum KunitorisItem
{
    KunitorisItem_Level0     = 0x00,    // KunitorisLevel files 0..15 (the level select's lines)
    KunitorisItem_FontImage  = 0x15,    // the UiFont's image
    KunitorisItem_FontGlyphs = 0x16,    // the UiFont's glyphs (a FontData)
    KunitorisItem_HowTo      = 0x17,    // kunitorisHowTo's picture (256 x 224)
    KunitorisItem_Title      = 0x18,    // the title picture (256 x 224)
    KunitorisItem_PressStart = 0x19,    // the title's blinking line (128 x 16)
    KunitorisItem_SeTable    = 0x29,    // the SEDT table of the minigame's SePlayer
    KunitorisItem_SeBank     = 0x2a,    // its bank 0 (XVAB)
};

// The SePlayer's entries the front end plays (its table's order).
enum KunitorisSe
{
    KunitorisSe_Decide    = 0,
    KunitorisSe_Cancel    = 1,
    KunitorisSe_Cursor    = 6,
    KunitorisSe_SelectBgm = 7,          // the level select (and after the ending)
    KunitorisSe_TitleBgm  = 8,
    KunitorisSe_GameOver  = 9,
    KunitorisSe_EndingBgm = 10,
};

// The handles kept at 0x599800 (bss): kunitorisBackThread clears each once
// it stops playing.
enum KunitorisSeSlot
{
    KunitorisSeSlot_Decide = 0,         // Decide / Cancel / Start clicks
    KunitorisSeSlot_Cursor = 1,         // the level select's cursor
    KunitorisSeSlot_Bgm    = 5,         // the music (stopped before the next)
    KunitorisSeSlot_Count  = 6,
};

// The level select's lines: LEVEL 1..15, JAPAN, ENDING (0x2fc340).
enum KunitorisLine
{
    KunitorisLine_Japan  = 15,          // the 16th level
    KunitorisLine_Ending = 16,          // kunitorisEnding, skippable
    KunitorisLine_Count  = 17,
};
constexpr int kKunitorisLevels = 16;    // SaveData::_46c / _4ac
// Clears that open three more lines; at 15 every line is open.
constexpr int kKunitorisLinesPerStep = 3;
constexpr int kKunitorisAllCleared = 15;

// A level file (a pack item), relocated in place once.
struct KunitorisLevel
{
    // Inline in the ROM (kunitorisThread): the 64 piece refs and the one at
    // +0x2114 become pointers into the file.
    void relocate()
    {
        if (relocated != 0)
            return;
        char *base = reinterpret_cast<char *>(this);
        for (int i = 0; i < kPieceRefs; i++)
            pieces[i].set(base + pieces[i].rel);
        _2114.set(base + _2114.rel);
        relocated = 1;
    }

    static constexpr int kPieceRefs = 64;

    u_char _00[4];              // FS2 +0x0000
    short  width;               // FS2 +0x0004: the board in cells
    short  height;              // FS2 +0x0006
    u_char relocated;           // FS2 +0x0008
    u_char _09[3];
    int    pieceCount;          // FS2 +0x000c: KunitorisGame::loadLevel
    u_char _10[0x2000];         // FS2 +0x0010
    S2dRef<void> pieces[kPieceRefs]; // FS2 +0x2010: loadLevel's piece records
    u_char _2110[4];            // FS2 +0x2110
    S2dRef<void> _2114;         // FS2 +0x2114
};
static_assert(offsetof(KunitorisLevel, width) == 0x4, "KunitorisLevel::width");
static_assert(offsetof(KunitorisLevel, relocated) == 0x8, "KunitorisLevel::relocated");
static_assert(offsetof(KunitorisLevel, pieceCount) == 0xc, "KunitorisLevel::pieceCount");
static_assert(offsetof(KunitorisLevel, pieces) == 0x2010, "KunitorisLevel::pieces");
static_assert(offsetof(KunitorisLevel, _2114) == 0x2114, "KunitorisLevel::_2114");

// One level being played: kunitorisThread's stack object (EE 0x790; three
// class levels, vptrs 0x319f60 / 0x319fd0 / 0x319f80 at +0x18). Its
// destructor is inline in the ROM, at the end of kunitorisThread's level.
// layout: host -- only what the front end reads; EE offsets in comments.
class KunitorisGame
{
public:
    KunitorisGame(int bestScore, int level);                             // FS2 0x002c8640
    ~KunitorisGame();                                                    // inline in the ROM (kunitorisThread)
    // Takes the level file and builds its pieces; cell the cell size.
    void loadLevel(float cell, KunitorisLevel *level);                   // FS2 0x002cd390
    void start(int arg);                                                 // FS2 0x002cca40
    void update(GamePad *pad);                                           // FS2 0x002c64d0
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x002c5ea0

    KunitorisLevel *m_level;    // FS2 +0x02c
    int    m_score;             // FS2 +0x05c: recorded as the level's best when higher
    int    m_scroll[2];         // FS2 +0x064: the board cell at the view's corner, x y
    int    m_viewHalf[2];       // FS2 +0x06c: kunitorisThread centres m_scroll by it
    int    m_lives;             // FS2 +0x080: under 1 the game is over
    u_char m_selectToggle;      // FS2 +0x770: Select flips it while lives are left
    u_char m_cleared;           // FS2 +0x778: the level is cleared
};

// kunitorisEnding's state (EE 0x1c, on its stack).
struct KunitorisEnding
{
    // Inline in the ROM (kunitorisEnding).
    void setScene(int scene)
    {
        if (m_scene != scene) {
            m_scene = scene;
            m_frame = 0;
        }
    }

    void drawIntro(float x, float y, UiDrawer *drawer);                  // FS2 0x002ca330
    // The three tiles to tile (negative: random ones); restarts m_timer.
    void setTiles(int tile);                                             // FS2 0x002ca790
    void draw(float x, float y, UiDrawer *drawer);                       // FS2 0x002c9de0

    int m_frame;                // FS2 +0x00: frames in the scene
    int m_scene;                // FS2 +0x04
    int m_timer;                // FS2 +0x08: -1 idle, else counts up (setTiles starts it)
    int _0c;                    // FS2 +0x0c
    int m_tiles[3];             // FS2 +0x10
};
static_assert(sizeof(KunitorisEnding) == 0x1c, "KunitorisEnding: EE 0x1c");
static_assert(offsetof(KunitorisEnding, m_timer) == 0x08, "KunitorisEnding::m_timer");
static_assert(offsetof(KunitorisEnding, m_tiles) == 0x10, "KunitorisEnding::m_tiles");

// The mode behind the top menu's answer 10 (game/menuscene.cpp), in slot 3:
// 1 when it changed the save (a best score or a clear).
int  runExtraMode(int arg);                                              // FS2 0x002c42c0

// The front end (game/kunitoris.cpp).
void kunitorisThread(void *arg);                                         // FS2 0x002c4320
void kunitorisBackThread(void *arg);                                     // FS2 0x002cdb20
// The title: 0 Start, -1 Cancel.
int  kunitorisTitle(UiDrawer *drawer);                                   // FS2 0x002c53c0
void kunitorisDrawTitle(UiDrawer *drawer, int offset, int frame);        // FS2 0x002c5780
// The how-to picture until Start (1) or frames frames (0); 0: until Start.
int  kunitorisHowTo(UiDrawer *drawer, int frames);                       // FS2 0x002c5550
// The level select from line cursor: the line chosen, -1 Cancel.
int  kunitorisLevelSelect(UiDrawer *drawer, int cleared, int cursor);    // FS2 0x002c5a70
int  kunitorisPrintf(UiFont *font, UiDrawer *drawer, int x, int y, const char *format, ...); // FS2 0x002cd9a0
// skippable: Decide / Start leave it (the ENDING line); 0 after the 15th clear.
void kunitorisEnding(int skippable);                                     // FS2 0x002c4ee0

#endif /* _GAME_KUNITORIS_H */
