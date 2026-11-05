/* ==========================================================================
 *  game/navimenu.h
 *
 *  The navigator (game/navimenu.cpp): the screen between stages, a map of
 *  the story's nodes on a 16 by 52 grid. menuPlayLoop runs it after a stage
 *  and gets a node to play. Its layout does the drawing and the asking;
 *  PrgScaleNavi moves the cursor and the view, and answers the layout
 *  through 34 of its variables and the arrays of NaviMenu (gamehooks.h),
 *  which the layout reads by index.
 *
 *  The class names are the type names; the file name is the port's.
 * ======================================================================== */
#ifndef _GAME_NAVIMENU_H
#define _GAME_NAVIMENU_H

#include "game/gamehooks.h"
#include "game/progress.h"
#include "game/uitrack.h"
#include "game/uiwidget.h"

constexpr int kNaviColumns = 16;

// The layout's variables, in the file's order from the one p0 names. The
// names are read from what the code does with them.
enum NaviVar
{
    NaviVar_CursorX = 0,
    NaviVar_CursorY,
    NaviVar_Node,               // the node under the cursor, -1 none
    NaviVar_CanLink0,
    NaviVar_CanLink1,
    NaviVar_CanBack,
    NaviVar_StartZoomed,        // read once, by the ctor
    NaviVar_ZoomFrames,
    NaviVar_FirstGoal,
    NaviVar_SecondGoal,
    NaviVar_GoalKind,
    NaviVar_StatusKind,
    NaviVar_Request,            // NaviRequest: the layout asks, the widget clears
    NaviVar_Result,             // NaviResult
    NaviVar_Refresh,            // one frame: the layout should look again
    NaviVar_Zoomed,             // set when the view changes
    NaviVar_NoHint,             // updateScrollHints: 1, or 2 where a link can fire
    NaviVar_UpHint,
    NaviVar_DownHint,
    NaviVar_Node40Clear,
    NaviVar_NodeFlagged,
    NaviVar_Node1dClear,
    NaviVar_ColumnShown,
    NaviVar_RowShown,
    NaviVar_Tab,                // 1..3: which of the node's links the layout shows
    NaviVar_RouteNode,          // the route's last node
    NaviVar_Pending0,           // ..3: the nodes a link would open
    NaviVar_Pending1,
    NaviVar_Pending2,
    NaviVar_Pending3,
    NaviVar_HardCleared,
    NaviVar_HardCleared2,
    NaviVar_HardKind,
    NaviVar_Scrolling,
    NaviVar_Count = 0x22,
};

// NaviVar_Request.
enum NaviRequest
{
    NaviRequest_None     = 0,
    NaviRequest_Free     = 1,   // the player moves the cursor (anything not below)
    NaviRequest_Link0    = 2,
    NaviRequest_Link1    = 3,
    NaviRequest_Back     = 4,
    NaviRequest_Overview = 5,
};

// NaviVar_Result.
enum NaviResult
{
    NaviResult_None      = 0,
    NaviResult_Cancelled = 1,
    NaviResult_Ended     = 2,   // the opening jump landed on a node with no part
    NaviResult_Decided   = 3,
};

// m_state. What Jump and JumpWait do comes from the ctor's jump along a link
// after a clear.
enum NaviState
{
    NaviState_Free     = 0,
    NaviState_Jump     = 0x1000,
    NaviState_JumpWait = 0x1001,
    NaviState_Link0    = 0x2000,
    NaviState_Link1    = 0x3000,
    NaviState_Back     = 0x4000,
    NaviState_Settle   = 0x5000,    // until the tweens end
    NaviState_Overview = 0x6000,
    NaviState_Reset    = 0x7000,
};

// m_flags. 2 and 4 are not used.
enum NaviFlag
{
    NaviFlag_Entered  = 0x01,   // a request is being served
    NaviFlag_Moved    = 0x08,
    NaviFlag_Jumping  = 0x10,   // the ctor found a link to follow
    NaviFlag_NoPart   = 0x20,   // ... to a node without that part
    NaviFlag_Pushed   = 0x40,   // ... and the jump is on the saved route
};

// m_tweens. The rectangles are the scissors of the view: the column header,
// the row header, the map.
enum NaviTween
{
    NaviTween_CursorX = 0,
    NaviTween_CursorY,
    NaviTween_ScrollX,
    NaviTween_ScrollY,
    NaviTween_Scale,
    NaviTween_ColumnLeft,       // the first of the 12 of a rectangle set
    NaviTween_ColumnRight,
    NaviTween_ColumnTop,
    NaviTween_ColumnBottom,
    NaviTween_RowLeft,
    NaviTween_RowRight,
    NaviTween_RowTop,
    NaviTween_RowBottom,
    NaviTween_MapLeft,
    NaviTween_MapRight,
    NaviTween_MapTop,
    NaviTween_MapBottom,
    NaviTween_Count,
};

// "prgScaleNavi" (EE 0x628). p0 the first variable's name, p1..p3 three
// offset tracks by name. layout: host; NaviMenu has the EE offsets.
class PrgScaleNavi : public UiWidget, public NaviMenu
{
public:
    PrgScaleNavi(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg); // FS2 0x0029e4f0
    ~PrgScaleNavi() override;                                            // FS2 0x0029e3d0: deleting (obj, 1)
    void draw(UiDrawer *drawer) override;                                // FS2 0x0029d8a0
    void advance() override;                                             // FS2 0x0029b980

    // The view on the drawer, the scissor of one rectangle.
    void applyView(UiDrawer *drawer, bool columns, bool rows);           // FS2 0x0029d5e0

private:
    u_short nodeFlagsOf(int node);                                       // FS2 0x0029b830
    int     nodeKindOf(int node, u_short flags);                         // FS2 0x0029b6f0
    bool    updateLinkBits();                                            // FS2 0x0029aef0
    void    updateLinkVars();                                            // FS2 0x0029b1a0
    void    syncRoute(int node);                                         // FS2 0x0029b2e0
    bool    getPendingRows(int *first, int *last);                       // FS2 0x0029b430
    void    markPending(int node);                                       // FS2 0x0029b530
    void    updateStatus(int node);                                      // FS2 0x0029cb20
    void    updateNode(bool moving);                                     // FS2 0x0029cc50
    void    updateHardVars(int node);                                    // FS2 0x0029d460
    void    updateHints(int node, int row, bool zoomed, bool busy);      // FS2 0x0029d960
    bool    moveCursor(float speed, int x, int y, int toX, int toY, int delay, int hold); // FS2 0x0029c7a0
    bool    isAtEnd(int count);                                          // FS2 0x0029db00
    bool    isMoveDone(int count);                                       // FS2 0x0029dbc0
    void    setZoom(bool zoomed, int frames, int x, int y, int toX, int toY); // FS2 0x0029dc50

    // Inline in the ROM.
    int     routeTop();
    void    routePush(int node, int side);
    void    placeCursor(int x, int y);
    void    placeOverviewMark();
    void    hideOverviewMark();
    void    takeNode(int node);
    void    followLink(int link);
    int     sideFrom(int link, int node);

    u_int          m_flags;     // FS2 +0x20: NaviFlag
    int            m_state;     // FS2 +0x24: NaviState
    NaviTable     *m_table;     // FS2 +0x28
    UiTween        m_tweens[NaviTween_Count]; // FS2 +0x2c
    float          m_origin[2]; // FS2 +0x208: the drawer's translate
    bool           m_zoomed;    // FS2 +0x210
    S2dVar        *m_vars[NaviVar_Count];     // FS2 +0x214: NaviVar
    UiOffsetTrack *m_cursorTrack;             // FS2 +0x29c
    UiOffsetTrack *m_markTrack;               // FS2 +0x2a0
    UiOffsetTrack *m_goalTrack;               // FS2 +0x2a4
};

// "prgScaleNaviScroll" (EE 0x28): a header of the map, drawn with its
// scissor. p0 the map widget's name, p1 columns, p2 rows. layout: host.
class PrgScaleNaviScroll : public UiWidget
{
public:
    PrgScaleNaviScroll(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg); // FS2 0x0029ae30
    ~PrgScaleNaviScroll() override;                                      // FS2 0x0029add0: deleting (obj, 1)
    void draw(UiDrawer *drawer) override;                                // FS2 0x0029ada0

private:
    PrgScaleNavi *m_navi;       // FS2 +0x20: null when it has no such widget
    bool          m_columns;    // FS2 +0x24
    bool          m_rows;       // FS2 +0x25
};

#endif /* _GAME_NAVIMENU_H */
