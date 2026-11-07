/* ==========================================================================
 *  game/stagemenu.h -- the list window of the stage's menu.
 *
 *  Up to 16 lines the scripts ask for again every frame (addLine), each a
 *  plate with text that slides and changes colour. Two copies of the lines
 *  (pages): the one being drawn, and the next, into which the lines are copied
 *  unmarked after each draw; a line nobody asks for again fades away.
 *  layout: host -- the EE offsets are in the comments.
 * ======================================================================== */
#ifndef _GAME_STAGEMENU_H
#define _GAME_STAGEMENU_H

#include "game/gamehooks.h"

class Clock;
class Tim2Texture;
class WideFontPrinter;

constexpr int kStageMenuLines = 16;
constexpr int kStageMenuPages = 2;

// What a line looks like at rest: the transition table's rows and columns.
enum StageMenuLook
{
    StageMenuLook_Gone        = 0,  // transparent, 3 frames and no text on the way
    StageMenuLook_Closed      = 1,  // transparent, 6 frames with the text
    StageMenuLook_Normal      = 2,  // grey
    StageMenuLook_Off         = 3,  // grey, half transparent: cannot be picked
    StageMenuLook_Selected    = 4,  // orange
    StageMenuLook_SelectedOff = 5,  // orange, half transparent
    StageMenuLook_Count       = 6,
};
// StageMenuFade::setLook's arguments that are no look.
constexpr int kStageMenuFromCurrent = -1;   // from: go on from where it is
constexpr int kStageMenuLookPicked  = -2;   // to: the blink of the picked line

// A run of colours, one a frame, the last one held. layout: host (EE 0xc).
struct StageMenuTrack
{
    int          frames;        // FS2 +0x0
    const u_int *colors;        // FS2 +0x4
    u_char       showsText;     // FS2 +0x8
};

// The colour part of a fade. layout: host (EE 0x10).
struct StageMenuColor
{
    int  frames() const;                                                 // FS2 0x0020a720
    bool finished() const { return frames() <= frame; }

    int   frame;                // FS2 +0x0
    const StageMenuTrack *track; // FS2 +0x4
    int   from;                 // FS2 +0x8: StageMenuLook, -1 none yet
    int   to;                   // FS2 +0xc: StageMenuLook, -1 none yet
};

// Where a line is and what colour. layout: host (EE 0x30).
class StageMenuFade
{
public:
    void clear();
    // A false answer: nothing to play, or it is already there.
    bool setLook(int to, int from = kStageMenuFromCurrent);              // FS2 0x0022fea0
    void draw(sceVif1Packet *packet, TexAreaList *texList, Tim2Texture *plate); // FS2 0x0022ffd0
    // The move starts where the last one was going.
    void settle();                                                       // FS2 0x0020a6e0
    void startMove(int frames);                                          // FS2 0x0020a710
    void aimAtRow(int row, int rows);                                    // FS2 0x0020a740
    void placeAtRow(int row, int rows);                                  // FS2 0x0020a7d0
    void advance();
    bool finished() const { return moveFrames <= moveFrame && color.finished(); }

    float fromX, fromY;         // FS2 +0x00
    float toX, toY;             // FS2 +0x08
    float x, y;                 // FS2 +0x10: y is the plate's middle
    int   moveFrame;            // FS2 +0x18
    int   moveFrames;           // FS2 +0x1c
    StageMenuColor color;       // FS2 +0x20
};

// What StageMenuLine::state says.
enum StageMenuLineState
{
    StageMenuLine_Free    = 0,
    StageMenuLine_New     = 1,  // asked for, not laid out yet
    StageMenuLine_Shown   = 2,
    StageMenuLine_Leaving = 3,  // not asked for and selected: stays until the selection moves
    StageMenuLine_Gone    = 4,  // not asked for: fading, freed at the end
};

// One line. layout: host (EE 0x54).
struct StageMenuLine
{
    void clear();
    void setShown(bool on);     // inline in the ROM
    void setSelected(bool on);  // inline in the ROM
    // The look the flags ask for.
    int  restLook() const;      // inline in the ROM
    bool isFree() const { return state == StageMenuLine_Free; }

    int   state;                // FS2 +0x00: StageMenuLineState
    const u_short *text;        // FS2 +0x04: the script's, also what identifies the line
    int   order;                // FS2 +0x08: the sort's key
    u_char enabled;             // FS2 +0x0c: clear leaves it
    u_char shown;               // FS2 +0x0d
    u_char selected;            // FS2 +0x0e
    u_char changed;             // FS2 +0x0f: set and never read
    StageMenuFade fade;         // FS2 +0x10
    u_char slidesIn;            // FS2 +0x40: comes in at its row, not from the last one
    u_char rowChanged;          // FS2 +0x41
    int   row;                  // FS2 +0x44
    int   rows;                 // FS2 +0x48: how many rows its place was worked out for
    int   asked;                // FS2 +0x4c: addLine this frame
    int   unused50;             // FS2 +0x50: -1 from clear, copied
};

enum StageMenuListState
{
    StageMenuList_Closed  = 0,
    StageMenuList_Opening = 1,
    StageMenuList_Open    = 2,
    StageMenuList_Closing = 3,
};

// The closing's steps.
enum StageMenuClose
{
    StageMenuClose_None   = 0,
    StageMenuClose_Picked = 1,  // the picked line blinks
    StageMenuClose_Fading = 2,
    StageMenuClose_Done   = 3,
};

// layout: host (EE 0xaa8).
class StageMenuList
{
public:
    StageMenuList(WideFontPrinter *printer, int withClock);              // FS2 0x0020ac30
    ~StageMenuList();                                                    // FS2 0x0020aba0: deleting

    void reset();                                                        // FS2 0x0020aaf0
    void setData(void *data);                                            // FS2 0x0020aaa0
    bool addLine(const u_short *text, int order, bool enabled);          // FS2 0x00209d90
    void close(bool picked);                                             // FS2 0x00208f40
    bool closeDone();                                                    // FS2 0x00208d40
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x00209080
    // Picks the selected line: its text, null when it cannot be picked.
    const u_short *pick();                                               // FS2 0x00209710
    bool selectNext();                                                   // FS2 0x00209870
    bool selectPrev();                                                   // FS2 0x00209b00
    int  usedLines();                                                    // FS2 0x002096c0

    int    m_state = StageMenuList_Closed;   // FS2 +0x0: StageMenuListState
    int    m_lineCount = 0;                  // FS2 +0xaa4: at the last draw

private:
    void drawLines(sceVif1Packet *packet, TexAreaList *texList);         // FS2 0x00209260
    void layout();                                                       // FS2 0x00209f90
    void sort();                                                         // FS2 0x0020a870
    StageMenuLine *lines() { return m_pages[m_page]; }
    int  selectedIndex();       // inline in the ROM: -1 none
    bool moveSelection(int step);        // the ROM's two copies

    int    m_closeStep = StageMenuClose_None;        // FS2 +0x4
    u_char m_noClock = 1;                            // FS2 +0x8
    Clock *m_clock = nullptr;                        // FS2 +0xc
    WideFontPrinter *m_printer;                      // FS2 +0x10
    int    m_page = 0;                               // FS2 +0x14
    StageMenuLine m_pages[kStageMenuPages][kStageMenuLines] = {}; // FS2 +0x18
    u_char m_laidOut = 0;                            // FS2 +0xa98: this frame
    void  *m_data = nullptr;                         // FS2 +0xa9c: the plate's TIM2 file
    Tim2Texture *m_plate = nullptr;                  // FS2 +0xaa0
};

#endif /* _GAME_STAGEMENU_H */
