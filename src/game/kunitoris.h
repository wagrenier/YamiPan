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
    KunitorisItem_Sunk       = 0x14,    // KunitorisBlock::sink's sprite
    KunitorisItem_FontImage  = 0x15,   // the UiFont's image
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
    KunitorisSe_Drop      = 2,          // KunitorisGame::update: Circle
    KunitorisSe_Shoot     = 3,          // Cross
    KunitorisSe_Turn      = 4,          // Triangle
    KunitorisSe_Flip      = 5,          // Square
    KunitorisSe_Cursor    = 6,          // the level select's cursor; a scroll in the game
    KunitorisSe_SelectBgm = 7,          // the level select (and after the ending); the game's music after a fever
    KunitorisSe_TitleBgm  = 8,
    KunitorisSe_GameOver  = 9,
    KunitorisSe_EndingBgm = 10,
    KunitorisSe_FeverBgm  = 11,         // every lit piece flashes (KunitorisGame::update)
    KunitorisSe_BreakOff  = 12,         // KunitorisGame::breakOff
};

// The handles kept at 0x599800 (bss): kunitorisBackThread clears each once
// it stops playing.
enum KunitorisSeSlot
{
    KunitorisSeSlot_Decide = 0,         // Decide / Cancel / Start clicks; the game's shot
    KunitorisSeSlot_Cursor = 1,         // the level select's cursor; the game's drop
    KunitorisSeSlot_Turn   = 2,         // the game's turn and flip
    KunitorisSeSlot_Scroll = 3,         // the game's scroll
    KunitorisSeSlot_Break  = 4,         // the game's break-off
    KunitorisSeSlot_Bgm    = 5,        // the music (stopped before the next)
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
    short  grid[64][64];        // FS2 +0x0010: the piece on each cell, [x][y]; -1 none
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
    virtual ~KunitorisDrawable();                                        // FS2 0x002c7b70: deleting (obj, flag)
    virtual void draw(UiDrawer *drawer);                                 // FS2 0x002cab50: nothing
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
// slots each: { dtor, step }). step answers false once done, and the actor
// then deletes it. The base mover (vtable 0x319f20) never ends; its only
// derived class is KunitorisSlide. No fader class is in this file's range
// (KunitorisActor::update is the only code that reads one). layout: host
class KunitorisMover
{
public:
    virtual ~KunitorisMover();                                           // FS2 0x002c7bc0: deleting (obj, flag)
    virtual bool step();                                                 // FS2 0x002cc9c0: true
    float m_pos[2];             // FS2 +0x00
};

// A straight move over frames frames from one offset to another, both
// pixels from origin's cell (EE 0x28, vtable 0x319fb0 over the base mover's
// 0x319f20 { 0x2c7bc0, 0x2cc9c0 }, vptr +0x08). layout: host
class KunitorisSlide final : public KunitorisMover
{
public:
    KunitorisSlide(const float *from, const float *to, int frames, const int *origin); // FS2 0x002cc830
    ~KunitorisSlide() override;                                          // FS2 0x002cdd60: deleting (obj, flag)
    // m_pos one frame on; false (at to) once frames have passed.
    bool step() override;                                                // FS2 0x002cc740

    float      m_from[2];       // FS2 +0x0c
    float      m_to[2];         // FS2 +0x14
    int        m_frame;         // FS2 +0x1c
    int        m_frames;        // FS2 +0x20
    const int *m_origin;        // FS2 +0x24: a cell, times 8 under both ends
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
    KunitorisBlock_Bounce  = 1,         // a hop above its height (KunitorisGame::shoot: 15 frames)
    KunitorisBlock_Leaving = 2,         // sinks, placed from m_anchor (KunitorisCells::drop: 12 frames)
    KunitorisBlock_Sinking = 3,         // sinks (KunitorisBlock::sink: 15 frames, the height over 12)
    KunitorisBlock_Gone    = 4,         // update answers false
};
constexpr int kKunitorisShotFrames = 15;    // KunitorisBlock_Bounce from a shot
constexpr int kKunitorisDropFrames = 12;    // KunitorisBlock_Leaving
constexpr int kKunitorisSinkFrames = 15;    // KunitorisBlock_Sinking from sink
constexpr int kKunitorisTurnFrames = 3;     // turn / flip slide this long

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
    // A quarter turn about the owner's cell, (x y) -> (-y x), slid there.
    void turn();                                                         // FS2 0x002cb6f0
    // Mirrored left to right about the owner's cell, slid there.
    void flip();                                                         // FS2 0x002cb5e0
    // A resting block sinks on the 0x14 picture; false otherwise.
    bool sink();                                                         // FS2 0x002cc600
    // Inline in the ROM (at every compare): m_cell from origin and offset.
    const int *refreshCell()
    {
        m_cell[0] = m_origin[0] + m_offset[0];
        m_cell[1] = m_origin[1] + m_offset[1];
        return m_cell;
    }

private:
    // Inline in the ROM (turn, flip): m_cell refreshed and a slide from
    // from (pixels) to the new offset replaces the mover.
    void slideFrom(const float *from);

public:
    const int *m_origin;       // FS2 +0x28: the owner's cell
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

// A set of blocks' life (KunitorisCells::m_state).
enum KunitorisCellsState
{
    KunitorisCells_Empty   = 0,
    KunitorisCells_Active  = 1,         // the player's, or a group moving every m_stepPeriod
    KunitorisCells_Shot    = 2,         // a shot group: its blocks hop, it moves every frame, scrolls do not move it
    KunitorisCells_Dropped = 3,         // drop: its blocks sink onto the board
    KunitorisCells_Sunk    = 4,         // a broken-off group: its blocks sink in place
    KunitorisCells_Gone    = 5,         // the last block's update answered false
};

// A set of blocks and its state (EE 0xc; new[] of 2 at KunitorisGame +0x7c:
// the player's and the dropped). Its blocks are KunitorisBlocks.
// layout: host
class KunitorisCells
{
public:
    KunitorisCells();                                                    // FS2 0x002cb220
    // Deletes the blocks.
    ~KunitorisCells();                                                   // FS2 0x002cb170: deleting (obj, flag)
    // Appended, and an empty set made active (inline at most uses).
    void add(KunitorisActor *actor);                                     // FS2 0x002cb010
    bool isEmpty() const;                                                // FS2 0x002c8060
    bool isGone() const;                                                 // FS2 0x002c7db0
    // The blocks deleted, the set empty.
    void release();                                                      // FS2 0x002cb100
    // Every resting block sinks onto the board at anchor (its offset made
    // the board cell); an empty or active set only. The set is dropped.
    bool drop(const int *anchor);                                        // FS2 0x002cb2e0
    // Each block turned / flipped; the count of blocks.
    int  turn();                                                         // FS2 0x002cb0b0
    int  flip();                                                         // FS2 0x002cb060
    // An empty or active set sinks in place (KunitorisCells_Sunk).
    bool sink();                                                         // FS2 0x002cb250
    // A block at this offset already.
    bool hasOffset(const int *offset) const;                             // FS2 0x002c99e0
    // A block of other on a cell of a block of this set.
    bool touches(KunitorisCells &other);                                 // FS2 0x002cb460
    // Inline in the ROM: block i (the list walked from its head).
    KunitorisBlock *block(int i) const;
    // Inline in the ROM: a block of this set on cell (x y).
    bool hasBlockAt(int x, int y);
    // Inline in the ROM: every block updated; a shot set is active again
    // once its last block stopped hopping, gone once that block is.
    void updateBlocks();
    // Inline in the ROM: the blocks' offsets' extent, 0 0 included.
    void offsetBounds(int *min, int *max) const;
    // Inline in the ROM: an empty or active set's resting blocks hop, the
    // set shot.
    void launch();

    KunitorisActorList m_blocks;    // FS2 +0x0
    int                m_state;     // FS2 +0x8: KunitorisCellsState
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
    // One frame: moved by m_step when move (a shot set always) and
    // wrapped, then the blocks updated.
    void step(bool move);                                                // FS2 0x002c7dc0
    // Off one side of the 40-cell view by 8, back in from the other.
    void wrap();                                                         // FS2 0x002c7f00
    // The view scrolled: moved by (dx dy) unless shot, wrapped.
    void scroll(int dx, int dy);                                         // FS2 0x002c7c10
    // An int pair copied (step's m_moved = m_step, out of line in the ROM).
    static int *copyPair(int *dst, const int *src);                      // FS2 0x002c7ee0

    int m_cell[2];           // FS2 +0x0c: not set by the ctor
    int m_step[2];              // FS2 +0x14: cells per move
    int m_moved[2];             // FS2 +0x1c: the last frame's move (step or scroll), 0 0 when it stood
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
    // Inline in the ROM (KunitorisBoard::clearUnder): the cell on board
    // cell (x y) put out; true when it was lit.
    bool clearCell(int x, int y);

    const KunitorisPieceData *m_data;  // FS2 +0x2c
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
    // The pieces with a cell still lit; none left clears the level.
    int  litPieces() const;                                              // FS2 0x002cd120
    // Inline in the ROM (KunitorisGame::update): the cells of piece, of
    // every piece (kKunitorisAllPieces); 0 when negative.
    int  cellCount(int piece) const;
    // The dropped blocks put out the cells under them (their offsets are
    // board cells, wrapped by the level's size): of any piece while every
    // one flashes, else of m_highlight's only. Counts the cells put out
    // (hits) and the rest (misses); the lit cells left of m_highlight.
    int  clearUnder(KunitorisCells &blocks, int *misses, int *hits);     // FS2 0x002c8b90

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

// KunitorisGame::m_heldDir: the direction button that scrolls while held.
enum KunitorisDir
{
    KunitorisDir_None  = 0,
    KunitorisDir_Up    = 1,             // the view up a cell (m_scroll y - 1)
    KunitorisDir_Down  = 2,
    KunitorisDir_Left  = 3,
    KunitorisDir_Right = 4,
};

// The ending's pose animations (data 0x2fc220): one per scene.
enum KunitorisScene
{
    KunitorisScene_Idle   = 0,          // the fallback once a scene's steps run out
    KunitorisScene_Miss   = 1,          // KunitorisGame::update: a drop missed more than one cell
    KunitorisScene_Over   = 2,          // the lives ran out
    KunitorisScene_Cheer  = 3,          // a piece cleared (one in four), a fever started or chained
    KunitorisScene_Clean  = 4,          // a piece of 10 cells or more cleared in one drop
    KunitorisScene_Big    = 5,          // a piece of 20 cells or more cleared (one in sixteen)
    KunitorisScene_Tiles  = 6,         // kunitorisEnding's tiles part
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
    // One frame of play: false while paused, cleared or out of lives.
    bool update(GamePad *pad);                                           // FS2 0x002c64d0
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x002c5ea0
    using KunitorisBoard::draw;
    using KunitorisBoard::update;

private:
    // Inline in the ROM (the ctor): the first group not active, or null.
    KunitorisGroup *freeGroup();
    // Inline in the ROM (the ctor, update): group on one of the first
    // kinds shapes, coming in from a random side.
    void spawnGroup(KunitorisGroup *group, int kinds);
    // Inline in the ROM (update): the groups not empty.
    int  activeGroups() const;
    // Inline in the ROM (update): lives lost, none below 0, none once cleared.
    void loseLives(int lives);
    // Groups that touch the player's blocks join them, from where they were
    // before their last move; a set grown past 16 cells across breaks off.
    void absorb();                                                       // FS2 0x002c9170
    // The player's blocks (two or more) as a free group at the cursor that
    // sinks there; the player keeps one block. False when nothing broke off.
    bool breakOff();                                                     // FS2 0x002c9720
    // The same as a shot group moving (dx dy) every frame.
    void shoot(int dx, int dy);                                          // FS2 0x002c9a80
    // Two groups that moved alike and touch: the first turns, its step
    // the move swapped.
    void turnTouching();                                                 // FS2 0x002c8ff0
    // Inline in the ROM (breakOff, shoot): the player's blocks copied into
    // a free group at the cursor, moving (dx dy); null without one free.
    KunitorisGroup *detachPlayerBlocks(int dx, int dy);
    // Inline in the ROM (breakOff, shoot, update's drop): the player's set
    // back to the one block under the cursor.
    void resetPlayerBlocks(KunitorisCells &cells);
    // The board four times at the scroll (the level wraps).
    void drawBoard();                                                    // FS2 0x002c81c0
    // m_lives as a bar of pack item 0x12, one pixel a life, at the top.
    void drawLivesBar();                                                 // FS2 0x002c8510
    // m_feverBar as the same bar, dark grey or grey by bit 1 of m_frame.
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
    int    m_spawnPeriod;       // FS2 +0x084: 210 .. 120 by level: frames between new groups
    int    m_drainPeriod;       // FS2 +0x088: 120 .. 60 by level: frames a life lasts (not in a fever)
    int    m_stepPeriod;        // FS2 +0x08c: 20 .. 8 by level: frames between group moves
    int    m_spawnTimer;        // FS2 +0x090: 5 at first; halved each fever frame
    int    m_drainTimer;        // FS2 +0x094
    int    m_stepTimer;         // FS2 +0x098: two a fever frame
    KunitorisGroup m_groups[kKunitorisGroups]; // FS2 +0x09c
    int    m_heldDir;           // FS2 +0x75c: KunitorisDir, the scroll's button while held
    int    m_frame;             // FS2 +0x760: frames played
    int    m_feverBar;          // FS2 +0x764: m_fever as 0..80 pixels (drawBlinkBar)
    float  m_feverRate;         // FS2 +0x768: m_fever lost a frame
    float  m_fever;             // FS2 +0x76c: 1 when a fever starts or chains; at 0 it ends
    u_char m_paused;            // FS2 +0x770: Select flips it while lives are left; draw shows PAUSE
    int    m_feverChain;        // FS2 +0x774: pieces cleared in this fever (0 outside one)
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
