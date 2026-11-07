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
#include "game/uidrawer.h"  /* UiVec4 */
#include "sdk/scetypes.h"

class GamePad;              // game/gamesystem.h
class GsTexture;            // game/gsutil.h
class TexAreaList;          // game/gsblockmap.h
class UiFont;               // game/uigame.h
struct sceVif1Packet;

// The pack's items by id (PackFile::getItem).
enum KunitorisItem
{
    KunitorisItem_Level0     = 0x00,    // KunitorisLevel files 0..15 (the level select's lines)
    KunitorisItem_Cursor     = 0x10,    // KunitorisCursor's sprite
    KunitorisItem_Block      = 0x11,    // KunitorisBlock's sprite
    KunitorisItem_Bar        = 0x12,    // the lives bar (KunitorisGame::drawLivesBar / drawBlinkBar)
    KunitorisItem_Highlight  = 0x13,    // KunitorisGame's sprite, KunitorisBlock's highlight pass
    KunitorisItem_FontImage  = 0x15,    // the UiFont's image
    KunitorisItem_FontGlyphs = 0x16,    // the UiFont's glyphs (a FontData)
    KunitorisItem_HowTo      = 0x17,    // kunitorisHowTo's picture (256 x 224)
    KunitorisItem_Title      = 0x18,    // the title picture (256 x 224)
    KunitorisItem_PressStart = 0x19,    // the title's blinking line (128 x 16)
    KunitorisItem_Pose0      = 0x1a,    // 0x1a..0x21: KunitorisEnding's 64 x 64 poses (its scene tables)
    KunitorisItem_Banner     = 0x22,    // KunitorisEnding's quad 0 (224 x 32), both draws
    KunitorisItem_IntroTile0 = 0x23,    // 0x23..0x25: drawIntro's quads 1..3
    KunitorisItem_EndTile0   = 0x26,    // 0x26..0x28: draw's quads 1..3 (64 rows: setTiles)
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

// One cell of a KunitorisPieceData.
struct KunitorisPieceCellData
{
    int    x;                   // FS2 +0x0
    int    y;                   // FS2 +0x4
    u_char _08[8];              // FS2 +0x8
};
static_assert(sizeof(KunitorisPieceCellData) == 0x10, "KunitorisPieceCellData: EE 0x10");
static_assert(offsetof(KunitorisPieceCellData, y) == 0x4, "KunitorisPieceCellData::y");

// A piece of a level file (KunitorisLevel::pieces): its cells, board
// coordinates in cells.
struct KunitorisPieceData
{
    typedef KunitorisPieceCellData Cell;

    u_char _00[4];              // FS2 +0x00
    int    cellCount;           // FS2 +0x04
    u_char _08[8];              // FS2 +0x08
    Cell   cells[1];            // FS2 +0x10: cellCount of them
};
static_assert(offsetof(KunitorisPieceData, cellCount) == 0x4, "KunitorisPieceData::cellCount");
static_assert(offsetof(KunitorisPieceData, cells) == 0x10, "KunitorisPieceData::cells");

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
        outline.set(base + outline.rel);
        relocated = 1;
    }

    static constexpr int kPieceRefs = 64;

    u_char _00[4];              // FS2 +0x0000
    short  width;               // FS2 +0x0004: the board in cells
    short  height;              // FS2 +0x0006
    u_char relocated;           // FS2 +0x0008
    u_char _09[3];
    int    pieceCount;          // FS2 +0x000c: KunitorisBoard::loadLevel
    u_char _10[0x2000];         // FS2 +0x0010
    S2dRef<KunitorisPieceData> pieces[kPieceRefs]; // FS2 +0x2010
    int    outlineCount;        // FS2 +0x2110: lines (two vertices each)
    S2dRef<UiVertex> outline;   // FS2 +0x2114: the pieces' borders, pixels
};
static_assert(offsetof(KunitorisLevel, width) == 0x4, "KunitorisLevel::width");
static_assert(offsetof(KunitorisLevel, relocated) == 0x8, "KunitorisLevel::relocated");
static_assert(offsetof(KunitorisLevel, pieceCount) == 0xc, "KunitorisLevel::pieceCount");
static_assert(offsetof(KunitorisLevel, pieces) == 0x2010, "KunitorisLevel::pieces");
static_assert(offsetof(KunitorisLevel, outlineCount) == 0x2110, "KunitorisLevel::outlineCount");
static_assert(offsetof(KunitorisLevel, outline) == 0x2114, "KunitorisLevel::outline");

// ──────────────────────────────────────────────────────────────────────
// The minigame's objects. Every class here is layout: host (the ROM's
// vptrs sit after the data, at the EE offsets in the comments); nothing
// outside this file reads them. Positions are board cells; one cell is
// kKunitorisCellPixels pixels.
constexpr float kKunitorisCellPixels = 8.0f;
// The game's own objects start lifted by this many cells (drawn up-left
// of their shadow).
constexpr int kKunitorisLift = 4;

// Something an actor draws (vtable 0x319f50: { dtor, draw }; vptr +0x00).
// layout: host
class KunitorisDrawable
{
public:
    virtual ~KunitorisDrawable() {}
    virtual void draw(UiDrawer *drawer) = 0;
};

// A 16 x 16 textured sprite of a pack picture (EE 0x28, vtable 0x319fc0;
// its ctor is inline at each new-expression). layout: host
class KunitorisSprite : public KunitorisDrawable
{
public:
    explicit KunitorisSprite(const void *image);                         // inline in the ROM
    ~KunitorisSprite() override;                                         // FS2 0x002c4e50: deleting (obj, flag)
    void draw(UiDrawer *drawer) override;                                // FS2 0x002cc9d0

    UiTexVertex m_quad[2];      // FS2 +0x04: uv 0..1, xy 0..16
    GsTexture  *m_texture;      // FS2 +0x24: null without the image
};

// What moves an actor and what fades its colour (vptr +0x08 / +0x10, two
// slots each: { dtor, step }). Their classes are later slices' work; step
// answers false once done, and the actor then deletes it. layout: host
class KunitorisMover
{
public:
    virtual ~KunitorisMover() {}
    virtual bool step() = 0;
    float m_pos[2];             // FS2 +0x00
};

// layout: host
class KunitorisFader
{
public:
    virtual ~KunitorisFader() {}
    virtual bool step() = 0;
    float m_rgba[4];            // FS2 +0x00
};

// The draw passes an actor gets (KunitorisGame::draw): the highlight, the
// shadow on the board, then the actor lifted by its height.
enum KunitorisPass
{
    KunitorisPass_Highlight = -1,       // KunitorisBlock only: the 0x13 picture
    KunitorisPass_Shadow    = 0,
    KunitorisPass_Lifted    = 1,        // any positive value
};

// The base of what is on the board (EE 0x28, vtable 0x319f30, vptr +0x24;
// its ctor is inline in the derived ones). layout: host
class KunitorisActor
{
public:
    KunitorisActor();                                                    // inline in the ROM
    virtual ~KunitorisActor();                                           // FS2 0x002cae40: deleting (obj, flag)
    // Steps the mover and the fader into m_pos / m_color; always true.
    virtual bool update();                                               // FS2 0x002cc8c0 (thunk 0x2cdbc0)
    virtual void draw(UiDrawer *drawer, int pass);                       // FS2 0x002cdbd0: nothing

protected:
    // Inline in the ROM (each ctor): the sprite replaced, the old one deleted.
    void setSprite(KunitorisDrawable *sprite);
    // Inline in the ROM (each draw): sprite at m_pos in colour, the
    // drawer's matrix moved there and its colour back to white after.
    void drawSprite(UiDrawer *drawer, KunitorisDrawable *sprite, const UiVec4 &color);

public:
    float              m_pos[2];    // FS2 +0x00: pixels, the cell times 8
    UiVec4             m_color;     // FS2 +0x08: grey (0.5) at first
    KunitorisMover    *m_mover;     // FS2 +0x18
    KunitorisFader    *m_fader;     // FS2 +0x1c
    KunitorisDrawable *m_sprite;    // FS2 +0x20
};

// The player's cursor (EE 0x34, vtable 0x319f00). layout: host
class KunitorisCursor : public KunitorisActor
{
public:
    KunitorisCursor(const int *cell, int height);                        // FS2 0x002cacc0
    ~KunitorisCursor() override;                                         // FS2 0x002cab60: deleting (obj, flag)
    bool update() override;                                              // FS2 0x002c42a0
    void draw(UiDrawer *drawer, int pass) override;                      // FS2 0x002ca870

    int m_cell[2];              // FS2 +0x28
    int m_height;               // FS2 +0x30: cells lifted
};

// A block's life (KunitorisBlock::m_state).
enum KunitorisBlockState
{
    KunitorisBlock_Resting = 0,
    KunitorisBlock_Bounce  = 1,         // a 14-frame hop above its height
    KunitorisBlock_Leaving = 2,         // sinks, placed from m_anchor
    KunitorisBlock_Sinking = 3,         // sinks over 12 frames
    KunitorisBlock_Gone    = 4,         // update answers false
};

// One cell of a shape, kept at an offset from its owner's cell (EE 0x50,
// vtable 0x319f90). layout: host
class KunitorisBlock : public KunitorisActor
{
public:
    KunitorisBlock(const int *origin, const int *offset, int height);    // FS2 0x002cc450
    ~KunitorisBlock() override;                                          // FS2 0x002c7a40: deleting (obj, flag)
    bool update() override;                                              // FS2 0x002cc220 (thunk 0x2c7a30)
    void draw(UiDrawer *drawer, int pass) override;                      // FS2 0x002cb800
    // The passes with the highlight's sprite given (null: a new one).
    void drawWithHighlight(UiDrawer *drawer, int pass, KunitorisDrawable *highlight); // FS2 0x002cbc90

    const int *m_origin;        // FS2 +0x28: the owner's cell
    int        m_offset[2];     // FS2 +0x2c
    int        m_cell[2];       // FS2 +0x34: origin + offset
    int        m_height;        // FS2 +0x3c: cells lifted now
    int        m_restHeight;    // FS2 +0x40
    const int *m_anchor;        // FS2 +0x44: KunitorisBlock_Leaving's (cell = offset - anchor)
    int        m_state;         // FS2 +0x48: KunitorisBlockState
    int        m_timer;         // FS2 +0x4c: frames left in the state
};

// The game's linked list of actors (0x2cde50..0x2ce0b4): each node holds
// its item in a 4-byte box of its own (EE 0x8). layout: host
class KunitorisActorList
{
public:
    // layout: host (EE 0xc)
    struct Node
    {
        explicit Node(KunitorisActor **item);                            // FS2 0x002ce0a0
        KunitorisActor **item;  // FS2 +0x0
        Node            *next;  // FS2 +0x4
        Node            *prev;  // FS2 +0x8
    };

    KunitorisActorList();                                                // FS2 0x002cde50
    ~KunitorisActorList();                                               // FS2 0x002cde60: deleting (obj, flag)
    void  pushBack(KunitorisActor *const &actor);                        // FS2 0x002cdec0
    void  pushFront(KunitorisActor *const &actor);                       // FS2 0x002cdfe0
    // The nodes and their boxes freed (not the actors).
    void  clear();                                                       // FS2 0x002cdf70
    Node *tail() const;                                                  // FS2 0x002ce070

    Node *m_head;               // FS2 +0x0
    int   m_count;              // FS2 +0x4
};

// A set of blocks with an active flag (EE 0xc; new[] of 2 at
// KunitorisGame +0x7c). layout: host
class KunitorisCells
{
public:
    KunitorisCells();                                                    // FS2 0x002cb220
    // Deletes the blocks.
    ~KunitorisCells();                                                   // FS2 0x002cb170: deleting (obj, flag)
    // Inline in the ROM: appended, and the set marked active.
    void add(KunitorisActor *actor);

    KunitorisActorList m_blocks;    // FS2 +0x0
    int                m_active;    // FS2 +0x8
};

// A shape coming in from a side of the view (EE 0x24; 48 at KunitorisGame
// +0x9c). layout: host
class KunitorisGroup : public KunitorisCells
{
public:
    KunitorisGroup();                                                    // FS2 0x002cafd0
    ~KunitorisGroup();                                                   // FS2 0x002caf10: ~KunitorisCells inline
    void setCell(int x, int y);                                          // FS2 0x002c8070
    void setStep(int dx, int dy);                                        // FS2 0x002c8080
    // A block at offset (x y) unless one is there already.
    void addCell(const int *offset);                                     // FS2 0x002c8090

    int m_cell[2];              // FS2 +0x0c: not set by the ctor
    int m_step[2];              // FS2 +0x14: cells per move
    int _1c;                    // FS2 +0x1c
    int _20;                    // FS2 +0x20
};

// A shape: count cells, offsets from its first (data 0x2fbb50..0x2fbe00).
// layout: host (EE 8 bytes: count, cells)
struct KunitorisShape
{
    int count;
    const int (*cells)[2];
};

// The colour cycles of the board's pieces (data 0x2fb9e0: { count, colours },
// the colours at 0x2fb840..0x2fb9e0).
enum KunitorisColors
{
    KunitorisColors_Piece = 0,          // brown, one colour (loadLevel)
    KunitorisColors_Flash = 1,          // 9 frames yellow, then 9 brown
    KunitorisColors_Fade  = 2,          // grey down to black over 6 frames
    KunitorisColors_Green = 3,          // dark green, one colour
    KunitorisColors_Count = 4,
};

// layout: host (EE 8 bytes)
struct KunitorisColorTable
{
    int           count;
    const UiVec4 *colors;
};

// A position and a colour (vtable 0x319f60, vptr +0x18: { update, draw };
// its ctor is inline in the derived ones). layout: host
class KunitorisTinted
{
public:
    KunitorisTinted();                                                   // inline in the ROM: grey
    virtual bool update();                                               // FS2 0x002cdbe0: true
    virtual void draw(UiDrawer *drawer, int pass);                       // FS2 0x002cdbf0: nothing

    float  m_pos[2];            // FS2 +0x00: unset
    UiVec4 m_color;             // FS2 +0x08: grey (0x2fc000)
};

// A colour played from a table, one entry a frame (vtable 0x319f70).
// layout: host
class KunitorisColorCycle : public KunitorisTinted
{
public:
    KunitorisColorCycle();                                               // inline in the ROM: no table
    bool update() override;                                              // FS2 0x002cdc00: the next colour; true
    // Inline in the ROM (loadLevel, selectPiece): the table from its first
    // colour, looping.
    void setColors(KunitorisColors colors);
    // Inline in the ROM (each cycle's update, the board's for its pieces):
    // m_colorIndex moved on, wrapped or held on the last, its colour taken.
    void stepColor();

    int           m_colorCount; // FS2 +0x1c
    const UiVec4 *m_colors;     // FS2 +0x20
    int           m_colorIndex; // FS2 +0x24
    u_char        m_colorLoop;  // FS2 +0x28: wraps; else holds the last
};

// One cell of a piece (EE 0x20). layout: host
struct KunitorisPieceCell
{
    KunitorisPieceCell();                                                // FS2 0x002cd970: all zero
    ~KunitorisPieceCell();                                               // FS2 0x002cd920: deleting (obj, flag)

    int lit;                    // FS2 +0x00: 1 once loaded; 0 draws in the unlit green
    const KunitorisPieceData::Cell *data; // FS2 +0x04
    int cell[2];                // FS2 +0x08: the board cell, x y
    UiVertex rect[2];           // FS2 +0x10: its corners in pixels (a sprite)
};

// A piece of the board, the cells of one KunitorisPieceData (EE 0x38,
// vtable 0x319fe0, vptr +0x18). layout: host
class KunitorisPiece final : public KunitorisColorCycle
{
public:
    KunitorisPiece();                                                    // FS2 0x002cd8a0
    ~KunitorisPiece();                                                   // FS2 0x002cd690: deleting (obj, flag)
    bool update() override;                                              // FS2 0x002cddc0: KunitorisColorCycle's body again
    // Pass 0: every cell white; else lit cells in m_color, the rest green.
    void draw(UiDrawer *drawer, int pass) override;                      // FS2 0x002cd520
    // The cells of data, cell pixels each.
    void load(float cell, const KunitorisPieceData *data);               // FS2 0x002cd730
    // Inline in the ROM (selectPiece): the cells still lit.
    int  litCells() const;

    const KunitorisPieceData *m_data;   // FS2 +0x2c
    int                 m_cellCount;    // FS2 +0x30
    KunitorisPieceCell *m_cells;        // FS2 +0x34: new[]
};

// KunitorisBoard::m_highlight's "every lit piece" (pieces are at most 64).
constexpr int kKunitorisAllPieces = 0x100;

// The level's board: its pieces and their borders (vtable 0x319fd0 over
// 0x319f60 / 0x319f70 at +0x18; the game's 0x319f80 keeps its slots).
// layout: host.
class KunitorisBoard : public KunitorisColorCycle
{
public:
    KunitorisBoard();           // inline in the ROM (KunitorisGame's ctor)
    ~KunitorisBoard();          // inline in the ROM: deletes the pieces
    // Every piece's colour stepped; true.
    bool update() override;                                              // FS2 0x002cdc90
    // The pieces in pass (m_highlight's lifted), then with any pass but 0
    // the borders.
    void draw(UiDrawer *drawer, int pass) override;                      // FS2 0x002cd280
    // Takes the level file and builds its pieces; cell the cell size.
    void loadLevel(float cell, KunitorisLevel *level);                   // FS2 0x002cd390
    // The piece that flashes (KunitorisColors_Flash): piece, every lit one
    // (kKunitorisAllPieces) or, negative, a random lit one; the choice, -1
    // when there is none.
    int  selectPiece(int piece);                                         // FS2 0x002cca40
    // Inline in the ROM (selectPiece): the lit cells of piece, of every
    // piece (kKunitorisAllPieces) or, negative, of m_highlight.
    int  litCells(int piece) const;

    KunitorisLevel *m_level;    // FS2 +0x2c
    int    m_pieceCount;        // FS2 +0x30
    KunitorisPiece *m_pieces;   // FS2 +0x34: new[] (loadLevel)
    int    m_highlight;         // FS2 +0x38: the piece flashing (drawn lifted), -1 none, or kKunitorisAllPieces
};

// The view's sides a group comes in from (the ctor's randNext2() & 3).
enum KunitorisSide
{
    KunitorisSide_Left   = 0,           // moves right
    KunitorisSide_Right  = 1,           // moves left
    KunitorisSide_Top    = 2,           // moves down
    KunitorisSide_Bottom = 3,           // moves up
};
constexpr int kKunitorisGroups = 48;

// The ending's pose animations (data 0x2fc220): one per scene.
enum KunitorisScene
{
    KunitorisScene_Idle   = 0,          // the fallback once a scene's steps run out
    KunitorisScene_Tiles  = 6,          // kunitorisEnding's tiles part
    KunitorisScene_Count  = 8,          // the 8th entry is empty (0x2fc274)
};

// The pictures kunitorisEnding shows, and KunitorisGame's own copy at +0x3c
// (EE 0x1c, on kunitorisEnding's stack). Each draw shows quad 0 and the
// three tiles once m_timer reaches their frames, then the scene's pose.
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

private:
    // Inline in the ROM (both draws): quad i of item items[i] once m_timer
    // reaches show[i]; past end m_timer goes back to -1.
    void drawQuads(float x, float y, UiDrawer *drawer, const int *show, int end, const int *items);
    // Inline in the ROM (both draws): the scene's pose at m_frame, back to
    // KunitorisScene_Idle once the scene has none.
    void drawPose(float x, float y, UiDrawer *drawer);

public:
    int m_frame;                // FS2 +0x00: frames in the scene
    int m_scene;                // FS2 +0x04: KunitorisScene
    int m_timer;                // FS2 +0x08: -1 idle, else counts up (setTiles starts it)
    int _0c;                    // FS2 +0x0c
    int m_tiles[3];             // FS2 +0x10
};
static_assert(sizeof(KunitorisEnding) == 0x1c, "KunitorisEnding: EE 0x1c");
static_assert(offsetof(KunitorisEnding, m_timer) == 0x08, "KunitorisEnding::m_timer");
static_assert(offsetof(KunitorisEnding, m_tiles) == 0x10, "KunitorisEnding::m_tiles");

// One level being played: kunitorisThread's stack object (EE 0x790, vptr
// 0x319f80 at +0x18 over KunitorisBoard). Its destructor is inline in the
// ROM, at the end of kunitorisThread's level.
// layout: host -- EE offsets in comments.
class KunitorisGame : public KunitorisBoard
{
public:
    KunitorisGame(int hiScore, int level);                               // FS2 0x002c8640
    ~KunitorisGame();                                                    // inline in the ROM (kunitorisThread)
    void update(GamePad *pad);                                           // FS2 0x002c64d0
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x002c5ea0
    using KunitorisBoard::draw;

private:
    // Inline in the ROM (the ctor): the first group not active, or null.
    KunitorisGroup *freeGroup();
    // The board four times at the scroll (the level wraps).
    void drawBoard();                                                    // FS2 0x002c81c0
    // m_lives as a bar of pack item 0x12, one pixel a life, at the top.
    void drawLivesBar();                                                 // FS2 0x002c8510
    // _764 as the same bar, dark grey or grey by bit 1 of _760.
    void drawBlinkBar();                                                 // FS2 0x002c83a0

public:
    KunitorisEnding m_ending;   // FS2 +0x03c: draw's drawIntro(0, 0)
    UiDrawer *m_drawer;        // FS2 +0x058
    int    m_hiScore;           // FS2 +0x05c: the level's best at first; recorded when higher
    int    m_score;             // FS2 +0x060
    int    m_scroll[2];         // FS2 +0x064: the board cell at the view's corner, x y
    int    m_playerCell[2];     // FS2 +0x06c: (18, 18) at first; the cursor's cell
    KunitorisCursor *m_cursor;  // FS2 +0x074
    int    m_cellsIndex;        // FS2 +0x078: which of m_cells holds the player's blocks
    KunitorisCells  *m_cells;   // FS2 +0x07c: new[] of 2
    int    m_lives;             // FS2 +0x080: under 1 the game is over (40 .. 32 by level)
    int    _84;                 // FS2 +0x084: 210 .. 120 by level
    int    _88;                 // FS2 +0x088: 120 .. 60 by level (a period: _94 counts it)
    int    _8c;                 // FS2 +0x08c: 20 .. 8 by level (a period: _98 counts it)
    int    _90;                 // FS2 +0x090: 5
    int    _94;                 // FS2 +0x094
    int    _98;                 // FS2 +0x098
    KunitorisGroup m_groups[kKunitorisGroups]; // FS2 +0x09c
    int    _75c;                // FS2 +0x75c
    int    _760;                // FS2 +0x760
    int    _764;                // FS2 +0x764
    int    _768;                // FS2 +0x768
    int    _76c;                // FS2 +0x76c
    u_char m_paused;            // FS2 +0x770: Select flips it while lives are left; draw shows PAUSE
    int    _774;                // FS2 +0x774
    u_char m_cleared;           // FS2 +0x778: the level is cleared
    int    m_shapeCount;        // FS2 +0x77c: of m_shapes, 27 below level 12, else 20
    const KunitorisShape *m_shapes; // FS2 +0x780
    KunitorisSprite *m_sprite;  // FS2 +0x784: the 0x13 picture
};

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
