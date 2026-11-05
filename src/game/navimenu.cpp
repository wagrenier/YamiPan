/* ==========================================================================
 *  game/navimenu.cpp -- the navigator (FS2 0x295470..0x2955d0 and
 *  0x29acb0..0x29eff0): the layout that picks the next node, the answer it
 *  leaves in the save, and its two widgets. See game/navimenu.h.
 *
 *  After a clear currentNaviAnswer is negative, and the ctor jumps along
 *  the cleared node's link, so the cursor lands on the next node by itself.
 *
 *  The route (NaviMenu::m_route) is the path the player took through the
 *  nodes; SaveExtra keeps a copy across screens and in the save.
 * ======================================================================== */
#include "game/navimenu.h"

#include <math.h>
#include <string.h>

#include "game/fileloader.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/layoutpaths.h"
#include "game/memcard.h"
#include "game/pad.h"
#include "game/screen.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "game/uigame.h"
#include "game/uilayout.h"
#include "sdk/scetypes.h"

// On the map at full size: a node's column and row in pixels.
constexpr int kNaviColumnWidth  = 0x4a;
constexpr int kNaviColumnOrigin = 0x48;
constexpr int kNaviRowHeight    = 0x19;
// The mark of the overview, from a node's corner.
constexpr float kNaviMarkX = 38.5f;
constexpr float kNaviMarkY = 14.0f;
constexpr float kNaviNoMark = -1000.0f;
// What fits the whole map on the screen.
constexpr float kNaviZoomScale = 0.353455126f;
// Rows the view scrolls over in the overview.
constexpr int kNaviOverviewFirstRow = 0x10;
constexpr int kNaviOverviewRows     = 0x13;
// Frames of hold and delay of the jump after a clear.
constexpr int kNaviJumpPause = 0x14;
// Pixels a frame.
constexpr float kNaviStepSpeed = 48.0f;
constexpr float kNaviMoveSpeed = 96.0f;
constexpr int kNaviSoundSlot = 3;
// Past the end of the last rectangle's margins in the overview.
constexpr float kNaviMarginNear = 3.0f;
constexpr float kNaviMarginFar  = 5.0f;
// A node's links: the two that go on, the two that came here.
constexpr int kNaviLinkFromA = 2;
constexpr int kNaviLinkFromB = 3;
// The two nodes the overview's flags single out.
constexpr int kNaviNodeFinal = 0x1d;
constexpr int kNaviNodeExtra = 0x40;
// Only the first nodes have hard records.
constexpr int kNaviNodesWithHard = 0x1e;
constexpr u_int kHardClearedVariant0 = 0x10;
constexpr u_int kHardClearedVariant1 = 0x20;
constexpr int kNaviNoNode = -1;

// The pad's bits for each way, the stick's beside the d-pad's.
constexpr u_int kNaviPadUp    = 0x1000 | 0x10000;
constexpr u_int kNaviPadDown  = 0x4000 | 0x40000;
constexpr u_int kNaviPadLeft  = 0x8000 | 0x80000;
constexpr u_int kNaviPadRight = 0x2000 | 0x20000;
constexpr u_int kNaviPadOverview = 0x4;     // L1

// The route a screen hands the next, and the save keeps.
// FS2 0x5995e0
static SaveExtra sSaveExtra;
// FS2 gp-0x79f4
static bool sSaveExtraReady;
// While a navi screen exists.
// FS2 gp-0x79f8
static PrgScaleNavi *sNaviMenu;

static NaviStep *savedSteps()
{
    return reinterpret_cast<NaviStep *>(sSaveExtra.body);
}

static StageSelect &saveSelect()
{
    return reinterpret_cast<SystemData *>(getSaveData())->select;
}


// ──────────────────────────────────────────────────────────────────────
// The screen.

// FS2 0x002954f0
// callcheck: UiLayout, setData, overlayShowScreen, threadYield, advance, update, openList, gfxGetTexArea, getList, draw, closeList, ~UiLayout

static int runNaviLayout(int arg, void *data)
{
    UiLayout layout;
    layout.setData(data, arg);
    overlayShowScreen();
    do {
        if (threadYield(1) != 0)
            return NaviAnswer_Leave;
        layout.advance();
        layout.update();
        sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_Screen);
        TexAreaList *texList = gfxGetTexArea()->getList(PacketList_Screen);
        layout.draw(packet, texList);
        gGameSystem->m_packets.closeList();
    } while (!layout.m_done);
    return layout.m_result;
}

// FS2 0x00295470
// callcheck: naviLayoutPath, FileJob, start, wait, currentNaviAnswer, runNaviLayout, ~FileJob, threadYield

int runNaviFile()
{
    FileJob job(naviLayoutPath(), 0);
    job.start();
    job.wait(1, false);
    int answer = runNaviLayout(currentNaviAnswer(), job.m_data);
    threadYield(2);
    return answer;
}

// The screen's answer into the record: 0 for one that picks no node.
// FS2 0x0029acb0
// callcheck: getSaveData, getSaveData, getSaveData

int applyNaviAnswer(int answer)
{
    if (answer == NaviAnswer_Leave || answer == NaviAnswer_Help)
        return 0;
    StageSelect &select = saveSelect();
    select.variant = (answer & NaviAnswer_Variant1) != 0 ? 1 : 0;
    select._10 = (answer & NaviAnswer_Flag200) != 0;
    select.stage = answer & kNaviAnswerStageMask;
    return 1;
}

// FS2 0x0029ad40
// callcheck: getSaveData, getSaveData, getSaveData

int currentNaviAnswer()
{
    const StageSelect &select = saveSelect();
    int answer = select.stage;
    if (select.variant == 1)
        answer |= NaviAnswer_Variant1;
    if (select.exitKind == StageExit_Cleared)
        answer = -answer;
    return answer;
}

// A new game starts on node 1.
// FS2 0x0029ef70

void saveExtraInit()
{
    sSaveExtraReady = true;
    savedSteps()[0].node = 1;
    savedSteps()[0].side = 0;
    sSaveExtra.head = 1;
}

// FS2 0x0029efd0

SaveExtra *getSaveExtra()
{
    return &sSaveExtra;
}

// FS2 0x0029efe0

NaviMenu *getNaviMenu()
{
    return sNaviMenu;
}


// ──────────────────────────────────────────────────────────────────────
// The header widget.

// FS2 0x0029ae30
// callcheck: UiWidget, getString, findWidgetByName

PrgScaleNaviScroll::PrgScaleNaviScroll(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : UiWidget(seq, layout, data, def, arg), m_navi(nullptr), m_columns(false), m_rows(false)
{
    UiWidget *navi = seq->findWidgetByName(layout->m_data->getString((int)def->param[0]));
    if (navi != nullptr) {
        m_navi = static_cast<PrgScaleNavi *>(navi);
        m_columns = def->param[1] != 0.0f;
        m_rows = def->param[2] != 0.0f;
    }
}

// FS2 0x0029add0
// callcheck: operator_delete (the compiler's)

PrgScaleNaviScroll::~PrgScaleNaviScroll()
{
}

// FS2 0x0029ada0
// callcheck: applyView

void PrgScaleNaviScroll::draw(UiDrawer *drawer)
{
    if (m_navi != nullptr)
        m_navi->applyView(drawer, m_columns, m_rows);
}


// ──────────────────────────────────────────────────────────────────────
// The map: small pieces.

static float nodePixelX(int x)
{
    return (float)(x * kNaviColumnWidth + kNaviColumnOrigin);
}

static float nodePixelY(int y)
{
    return (float)((y + 1) * kNaviRowHeight);
}

// The row the overview scrolls to for a node's row: inline in the ROM, three
// times.
static int overviewRow(int y)
{
    if (y < kNaviOverviewFirstRow)
        return 0;
    if (y < kNaviOverviewFirstRow + kNaviOverviewRows)
        return y - kNaviOverviewFirstRow;
    return kNaviOverviewRows;
}

// Where the view stands for a cursor there.
static void viewFor(bool zoomed, int x, int y, float *out)
{
    if (!zoomed) {
        out[0] = -(float)((x - 2) * kNaviColumnWidth);
        out[1] = -(float)((y - 5) * kNaviRowHeight);
    } else {
        out[0] = 0.0f;
        out[1] = -(float)(overviewRow(y) * kNaviRowHeight) * kNaviZoomScale;
    }
}

// The route's last node, -1 with an empty one.
int PrgScaleNavi::routeTop()
{
    return m_routeCount > 0 ? m_route[m_routeCount - 1].node : kNaviNoNode;
}

void PrgScaleNavi::routePush(int node, int side)
{
    if (m_routeCount < kNaviRouteSteps) {
        m_route[m_routeCount].node = node;
        m_route[m_routeCount].side = side;
        m_routeCount++;
    }
}

void PrgScaleNavi::placeCursor(int x, int y)
{
    m_cursorX = x;
    m_vars[NaviVar_CursorX]->value = x;
    m_cursorY = y;
    m_vars[NaviVar_CursorY]->value = y;
}

// Remembers where the cursor stands, and marks it for the overview.
void PrgScaleNavi::placeOverviewMark()
{
    m_savedX = m_cursorX;
    m_savedY = m_cursorY;
    m_savedNode = m_node;
    if (m_cursorX < 0 || m_cursorY < 0) {
        m_markA[0] = kNaviNoMark;
        m_markA[1] = kNaviNoMark;
    } else {
        m_markA[0] = nodePixelX(m_cursorX) + kNaviMarkX;
        m_markA[1] = nodePixelY(m_cursorY) + kNaviMarkY;
    }
    m_markTrack->m_offset[0] = m_markA[0];
    m_markTrack->m_offset[1] = m_markA[1];
}

void PrgScaleNavi::hideOverviewMark()
{
    m_markA[0] = kNaviNoMark;
    m_markA[1] = kNaviNoMark;
    m_markTrack->m_offset[0] = m_markA[0];
    m_markTrack->m_offset[1] = m_markA[1];
}

void PrgScaleNavi::takeNode(int node)
{
    m_node = node;
    m_vars[NaviVar_Node]->value = node;
    m_savedNode = kNaviNoNode;
}

// Which of its two links back leads to node: -1 neither.
int PrgScaleNavi::sideFrom(int link, int node)
{
    if (node == m_table->getLinkOrNone(link, kNaviLinkFromA))
        return 0;
    if (node == m_table->getLinkOrNone(link, kNaviLinkFromB))
        return 1;
    return -1;
}

// Moves along a link of the node under the cursor.
void PrgScaleNavi::followLink(int link)
{
    const int node = m_node;
    const int target = m_table->getLink(node, link);
    int x, y;
    m_table->getGridPos(target, &x, &y);
    moveCursor(kNaviStepSpeed, m_cursorX, m_cursorY, x, y, 0, 0);
    placeCursor(x, y);
    m_flags |= NaviFlag_Moved;
    routePush(target, sideFrom(target, node));
    updateLinkBits();
}


// ──────────────────────────────────────────────────────────────────────
// What the layout reads.

// FS2 0x0029b830
// callcheck: getSaveData, testStageFlags, isFirstGoalOpen, isOpen, isSinglePart, getSaveData, testStageFlags, isSecondGoalOpen, inStage, getSaveData, getSaveData

u_short PrgScaleNavi::nodeFlagsOf(int node)
{
    NaviTable *table = m_table;
    u_int flags = 0;
    if (getSaveData()->testStageFlags(node, kClearedVariant0))
        flags = NaviNode_FirstCleared;
    if (table->isFirstGoalOpen(node))
        flags |= NaviNode_FirstGoal;
    if (table->isOpen(node))
        flags |= NaviNode_Open;
    if (!table->isSinglePart(node)) {
        if (getSaveData()->testStageFlags(node, kClearedVariant1))
            flags |= NaviNode_SecondCleared;
        if (table->isSecondGoalOpen(node))
            flags |= NaviNode_SecondGoal;
    } else {
        flags |= NaviNode_SinglePart;
        if (flags & NaviNode_FirstCleared)
            flags |= NaviNode_SecondCleared;
        if (flags & NaviNode_FirstGoal)
            flags |= NaviNode_SecondGoal;
    }
    if (inStage() && node == saveSelect().stage)
        flags |= saveSelect().variant == 0 ? NaviNode_HereFirst : NaviNode_HereSecond;
    return (u_short)flags;
}

// The picture of a node: 0 hidden.
// FS2 0x0029b6f0
// callcheck: hasSecondGoal, isSecondGoalDone

int PrgScaleNavi::nodeKindOf(int node, u_short flags)
{
    NaviTable *table = m_table;
    int kind = 0;
    if (flags & NaviNode_SinglePart) {
        if (flags & NaviNode_Open)
            kind = (flags & NaviNode_FirstCleared) ? 2 : 1;
    } else if (flags & (NaviNode_HereFirst | NaviNode_HereSecond)) {
        kind = (flags & NaviNode_FirstCleared) ? 7 : 6;
    } else if (flags & NaviNode_SecondCleared) {
        kind = 5;
    } else if (flags & NaviNode_FirstCleared) {
        kind = table->hasSecondGoal(node) ? 4 : 5;
    } else if (flags & NaviNode_Open) {
        kind = 3;
    }
    if (kind == 4 && !table->isSecondGoalDone(node))
        kind = 5;
    return kind;
}

// The bits of the links of the route's last node. Answers that the layout's
// tab changed.
// FS2 0x0029aef0
// callcheck: isLink0Open, isLink1Open, isLink2Open, isLink3Open

bool PrgScaleNavi::updateLinkBits()
{
    const int previous = m_lastTab;
    const int tab = m_vars[NaviVar_Tab]->value;
    m_lastTab = tab;
    memset(m_bits, 0, sizeof(m_bits));
    m_vars[NaviVar_RouteNode]->value = kNaviNoNode;
    if (m_routeCount != 0) {
        int side = -1;
        if (m_routeCount > 1)
            side = m_route[m_routeCount - 1].side != 0 ? 1 : 0;
        const int node = routeTop();
        if (m_table->isLink0Open(node)) {
            m_bits[node] |= NaviBit_Link0;
            if (tab == 1)
                m_bits[node] |= NaviBit_Shown0;
        }
        if (m_table->isLink1Open(node)) {
            m_bits[node] |= NaviBit_Link1;
            if (tab == 2)
                m_bits[node] |= NaviBit_Shown1;
        }
        if (m_table->isLink2Open(node)) {
            m_bits[node] |= NaviBit_Link2;
            if (tab == 3 && side == 0)
                m_bits[node] |= NaviBit_Shown2;
        }
        if (m_table->isLink3Open(node)) {
            m_bits[node] |= NaviBit_Link3;
            if (tab == 3 && side == 1)
                m_bits[node] |= NaviBit_Shown3;
        }
        m_vars[NaviVar_RouteNode]->value = node;
    }
    return previous != tab;
}

// FS2 0x0029b1a0
// callcheck: isLink0Open, isLink1Open

void PrgScaleNavi::updateLinkVars()
{
    int node = routeTop();
    m_vars[NaviVar_CanLink0]->value = node != kNaviNoNode && m_table->isLink0Open(node) ? 1 : 0;
    node = routeTop();
    m_vars[NaviVar_CanLink1]->value = node != kNaviNoNode && m_table->isLink1Open(node) ? 1 : 0;
    m_vars[NaviVar_CanBack]->value = m_routeCount >= 2 ? 1 : 0;
}

// The route for a node the cursor came to by itself: the saved one when it
// ends there, else a new one from that node.
// FS2 0x0029b2e0

void PrgScaleNavi::syncRoute(int node)
{
    const int saved = sSaveExtra.head > 0 ? savedSteps()[sSaveExtra.head - 1].node : kNaviNoNode;
    const int mine = routeTop();
    if (saved != kNaviNoNode && saved == node) {
        m_routeCount = sSaveExtra.head;
        memcpy(m_route, savedSteps(), sizeof(m_route));
    } else if (node != mine) {
        m_routeCount = 0;
        if (node != kNaviNoNode)
            routePush(node, 0);
    }
}

// FS2 0x0029b430
// callcheck: getGridPos

bool PrgScaleNavi::getPendingRows(int *first, int *last)
{
    *last = -1;
    *first = -1;
    if (m_pendingCount == 0)
        return false;
    for (int i = 0; i < m_pendingCount; i++) {
        int x, y;
        m_table->getGridPos(m_pending[i], &x, &y);
        if (*first < 0) {
            *last = y;
            *first = y;
        } else {
            if (y < *first)
                *first = y;
            if (*last < y)
                *last = y;
        }
    }
    return true;
}

// The pictures again, the nodes the node's links would open lit.
// FS2 0x0029b530
// callcheck: nodeKindOf, canFireLink, hasPendingFlag, findPendingLink

void PrgScaleNavi::markPending(int node)
{
    constexpr int kKindLitFirst  = 5;
    constexpr int kKindLitSecond = 10;
    NaviTable *table = m_table;
    for (int i = 0; i < kNaviNodes; i++)
        m_nodeKind[i] = nodeKindOf(i, m_nodeFlags[i]);
    m_pendingCount = 0;
    for (int i = 0; i < kNaviPending; i++) {
        m_pending[i] = kNaviNoNode;
        m_vars[NaviVar_Pending0 + i]->value = kNaviNoNode;
    }
    if (node == kNaviNoNode || !table->canFireLink(node) || !table->hasPendingFlag(node))
        return;
    for (int group = 0; group < kNaviPending; group++) {
        int target, secondGoal;
        if (!table->findPendingLink(node, group, &target, &secondGoal))
            continue;
        if (m_nodeKind[target] == 0)
            continue;
        m_nodeKind[target] += secondGoal == 0 ? kKindLitFirst : kKindLitSecond;
        m_pending[m_pendingCount] = target;
        m_vars[NaviVar_Pending0 + m_pendingCount]->value = target;
        m_pendingCount++;
    }
}

// FS2 0x0029cb20
// callcheck: nodeFlagsOf, hasSecondGoal

void PrgScaleNavi::updateStatus(int node)
{
    int kind = 0;
    // PC: the ROM asks the flags of no node first, and does not use them.
    if (node != kNaviNoNode) {
        const u_short flags = nodeFlagsOf(node);
        if (flags & NaviNode_Open) {
            if (flags & NaviNode_SinglePart)
                kind = (flags & NaviNode_FirstCleared) ? 6 : 5;
            else if (flags & (NaviNode_HereFirst | NaviNode_HereSecond))
                kind = 1;
            else if ((flags & (NaviNode_FirstCleared | NaviNode_SecondCleared)) == 0)
                kind = 2;
            else if (flags & NaviNode_SecondCleared)
                kind = 4;
            else
                kind = m_table->hasSecondGoal(node) ? 3 : 4;
        }
    }
    m_vars[NaviVar_StatusKind]->value = kind;
}

// FS2 0x0029d460
// callcheck: getSystemData, testStageFlags, getSystemData, stageHasVariant1, getSystemData, testStageFlags, getSystemData, stageHasVariant1

void PrgScaleNavi::updateHardVars(int node)
{
    m_vars[NaviVar_HardCleared]->value = 0;
    m_vars[NaviVar_HardCleared2]->value = 0;
    m_vars[NaviVar_HardKind]->value = 0;
    if (node < 0 || node >= kNaviNodesWithHard)
        return;
    bool first = false, second = false;
    first = getSystemData()->save.testStageFlags(node, kHardClearedVariant0);
    if (first)
        m_vars[NaviVar_HardCleared]->value = 1;
    if (getSystemData()->save.stageHasVariant1(node)
        && getSystemData()->save.testStageFlags(node, kHardClearedVariant1)) {
        second = true;
        m_vars[NaviVar_HardCleared2]->value = 1;
    }
    if (!getSystemData()->save.stageHasVariant1(node)) {
        if (first)
            m_vars[NaviVar_HardKind]->value = 4;
    } else if (first && second) {
        m_vars[NaviVar_HardKind]->value = 3;
    } else if (first) {
        m_vars[NaviVar_HardKind]->value = 1;
    } else if (second) {
        m_vars[NaviVar_HardKind]->value = 2;
    }
}

// Everything that hangs on the node under the cursor. While the overview
// moves (moving), only what the cursor's own place decides.
// PC: on an empty cell node is -1: the ROM reads just before its arrays and
// before the table there, and what it finds decides nothing that shows.
// FS2 0x0029cc50
// callcheck: findNodeAt, updateHardVars, markPending, hasSecondGoal, getSaveData, isSinglePart, getSaveData, testStageFlags, getSaveData, testStageFlags, getSaveData, testStageFlags, getSaveData, testStageFlags, updateStatus, canFireLink, getSaveData, getGridPos, getSaveData

void PrgScaleNavi::updateNode(bool moving)
{
    const int cursorY = m_cursorY;
    const int cursorX = m_cursorX;
    NaviNode *found = m_table->findNodeAt(cursorX, cursorY);
    const int node = found != nullptr ? found->index : kNaviNoNode;
    m_node = node;
    m_vars[NaviVar_Node]->value = node;
    updateHardVars(node);

    if (moving) {
        if (node >= 0 && m_nodeKind[node] == 0) {
            m_node = kNaviNoNode;
            m_vars[NaviVar_Node]->value = kNaviNoNode;
        }
    } else {
        markPending(node);
        if (node >= 0 && m_nodeKind[node] == 0) {
            m_node = kNaviNoNode;
            m_vars[NaviVar_Node]->value = kNaviNoNode;
        }
    }
    const u_short flags = node >= 0 ? m_nodeFlags[node] : 0;

    if (!moving) {
        int goalKind;
        if (node < 0 || !m_table->hasSecondGoal(node)) {
            goalKind = 1;
            if (node >= 0) {
                if (saveSelect().kind != 0 && !m_table->isSinglePart(node)
                    && getSaveData()->testStageFlags(node, kClearedVariant0))
                    goalKind = 2;
                if (node == kNaviNodeFinal && getSaveData()->testStageFlags(kNaviNodeFinal, kClearedVariant0))
                    goalKind = 6;
            }
        } else {
            goalKind = 3;
            if (!(flags & NaviNode_FirstCleared))
                goalKind = 1;
            else if (!(flags & NaviNode_SecondGoal))
                goalKind = 5;
            else if (saveSelect().kind != 0 && getSaveData()->testStageFlags(node, kClearedVariant0)
                     && getSaveData()->testStageFlags(node, kClearedVariant1))
                goalKind = 4;
        }
        m_vars[NaviVar_FirstGoal]->value = (flags & NaviNode_FirstGoal) ? 1 : 0;
        m_vars[NaviVar_SecondGoal]->value = (flags & NaviNode_SecondGoal) ? 1 : 0;
        if (!(flags & (NaviNode_FirstGoal | NaviNode_SecondGoal)))
            goalKind = 0;
        m_vars[NaviVar_GoalKind]->value = goalKind;

        updateStatus(node);

        m_markB[0] = kNaviNoMark;
        m_markB[1] = kNaviNoMark;
        if (m_node != kNaviNoNode && m_table->canFireLink(m_node)) {
            const NaviNode &n = m_table->node(m_node);
            if (n.goalPos[0] >= 0.0f && n.goalPos[1] >= 0.0f) {
                m_markB[0] = n.goalPos[0];
                m_markB[1] = n.goalPos[1];
            }
        }

        m_vars[NaviVar_Node40Clear]->value = (m_nodeFlags[kNaviNodeExtra] & NaviNode_Open) ? 1 : 0;
        m_vars[NaviVar_Node1dClear]->value = (m_nodeFlags[kNaviNodeFinal] & NaviNode_Open) ? 1 : 0;

        memset(m_rowShown, 0, sizeof(m_rowShown));
        memset(m_columnShown, 0, sizeof(m_columnShown));
        int firstRow = kNaviRows, lastRow = -1;
        for (int i = 0; i < kNaviNodes; i++) {
            bool shown;
            if (i == kNaviNodeExtra)
                shown = (m_nodeFlags[i] & NaviNode_FirstCleared) != 0;
            else if (i == kNaviNodeFinal)
                shown = (m_nodeFlags[i] & NaviNode_Open) != 0 && getSaveData()->archive[kArchiveCount] != 0;
            else
                shown = (m_nodeFlags[i] & NaviNode_Open) != 0;
            if (!shown)
                continue;
            int x, y;
            m_table->getGridPos(i, &x, &y);
            if (y < firstRow)
                firstRow = y;
            if (lastRow < y)
                lastRow = y;
            m_columnShown[x] = 1;
        }
        for (int y = firstRow; y <= lastRow; y++)
            m_rowShown[y] = 1;
    }

    m_vars[NaviVar_ColumnShown]->value = m_columnShown[cursorX] != 0 ? 1 : 0;
    m_vars[NaviVar_RowShown]->value = 0;
    if (m_rowShown[cursorY] != 0) {
        m_vars[NaviVar_RowShown]->value = 1;
        if (m_vars[NaviVar_ColumnShown]->value == 1 && (flags & NaviNode_Open))
            m_vars[NaviVar_RowShown]->value = 2;
    }

    if (moving) {
        updateStatus(node);
    } else if (node < 0 || node >= kNaviNodes) {
        m_vars[NaviVar_NodeFlagged]->value = 0;
    } else {
        m_vars[NaviVar_NodeFlagged]->value = getSaveData()->stageFlags[node] != 0 ? 1 : 0;
    }
}

// FS2 0x0029d960
// callcheck: canFireLink, getPendingRows

void PrgScaleNavi::updateHints(int node, int row, bool zoomed, bool busy)
{
    m_vars[NaviVar_NoHint]->value = 0;
    m_vars[NaviVar_UpHint]->value = 0;
    m_vars[NaviVar_DownHint]->value = 0;
    if (busy)
        return;
    if (!zoomed) {
        if (node != kNaviNoNode)
            m_vars[NaviVar_NoHint]->value = m_table->canFireLink(node) ? 2 : 1;
        return;
    }
    const int top = overviewRow(row);
    const bool up = row >= kNaviOverviewFirstRow && top != 0;
    const bool down = row < kNaviOverviewFirstRow + kNaviOverviewRows;
    int first, last;
    if (!getPendingRows(&first, &last)) {
        if (up)
            m_vars[NaviVar_UpHint]->value = 1;
        if (down)
            m_vars[NaviVar_DownHint]->value = 1;
        return;
    }
    if (up)
        m_vars[NaviVar_UpHint]->value = top - first < 1 ? 1 : 2;
    if (down)
        m_vars[NaviVar_DownHint]->value = last - top < 0x21 ? 1 : 2;
}


// ──────────────────────────────────────────────────────────────────────
// The moves.

// FS2 0x0029c7a0
// callcheck: start, start, start, start

bool PrgScaleNavi::moveCursor(float speed, int x, int y, int toX, int toY, int delay, int hold)
{
    if (x == toX && y == toY)
        return false;
    const float fromPixelX = nodePixelX(x), fromPixelY = nodePixelY(y);
    const float toPixelX = nodePixelX(toX), toPixelY = nodePixelY(toY);
    const float dx = toPixelX - fromPixelX, dy = toPixelY - fromPixelY;
    const int frames = (int)(sqrtf(dx * dx + dy * dy) / speed) + 1;
    m_tweens[NaviTween_CursorX].start(fromPixelX, toPixelX, frames, delay, hold, UiTweenCurve_Linear);
    m_tweens[NaviTween_CursorY].start(fromPixelY, toPixelY, frames, delay, hold, UiTweenCurve_Linear);
    float from[2], to[2];
    viewFor(m_zoomed, x, y, from);
    viewFor(m_zoomed, toX, toY, to);
    m_tweens[NaviTween_ScrollX].start(from[0], to[0], frames, delay, hold, UiTweenCurve_Linear);
    m_tweens[NaviTween_ScrollY].start(from[1], to[1], frames, delay, hold, UiTweenCurve_Linear);
    return true;
}

// FS2 0x0029db00
// callcheck: progress, progress

bool PrgScaleNavi::isAtEnd(int count)
{
    if (count == 0)
        return m_tweens[NaviTween_CursorX].progress() == 1.0f
            && m_tweens[NaviTween_CursorY].progress() == 1.0f;
    for (int i = 0; i < NaviTween_Count; i++) {
        if (m_tweens[i].progress() != 1.0f)
            return false;
    }
    return true;
}

// FS2 0x0029dbc0
// callcheck: isDone, isDone

bool PrgScaleNavi::isMoveDone(int count)
{
    if (count == 0)
        return m_tweens[NaviTween_CursorX].isDone() && m_tweens[NaviTween_CursorY].isDone();
    for (int i = 0; i < NaviTween_Count; i++) {
        if (!m_tweens[i].isDone())
            return false;
    }
    return true;
}

// The rectangles, from NaviTween_Scale on: the plain map, the overview.
static const float kNaviViewPlain[NaviTween_Count - NaviTween_Scale] = {
    1.0f,
    72.0f, 442.0f, 0.0f, 75.0f,
    0.0f, 75.0f, 25.0f, 301.0f,
    72.0f, 442.0f, 25.0f, 301.0f,
};
static const float kNaviViewZoomed[NaviTween_Count - NaviTween_Scale] = {
    kNaviZoomScale,
    25.4487686f, 442.0f, 0.0f, 75.0f,
    0.0f, 75.0f, 8.8363781f, 301.0f,
    25.4487686f, 442.0f, 8.8363781f, 301.0f,
};

// FS2 0x0029dc50
// callcheck: start

void PrgScaleNavi::setZoom(bool zoomed, int frames, int x, int y, int toX, int toY)
{
    if (zoomed == m_zoomed)
        return;
    m_zoomed = !m_zoomed;
    const float *from = m_zoomed ? kNaviViewPlain : kNaviViewZoomed;
    const float *to = m_zoomed ? kNaviViewZoomed : kNaviViewPlain;
    for (int i = NaviTween_Scale; i < NaviTween_Count; i++)
        m_tweens[i].start(from[i - NaviTween_Scale], to[i - NaviTween_Scale], frames, 0, 0, UiTweenCurve_Linear);
    m_tweens[NaviTween_CursorX].start(nodePixelX(x), nodePixelX(toX), frames, 0, 0, UiTweenCurve_Linear);
    m_tweens[NaviTween_CursorY].start(nodePixelY(y), nodePixelY(toY), frames, 0, 0, UiTweenCurve_Linear);
    float viewFrom[2], viewTo[2];
    viewFor(!m_zoomed, x, y, viewFrom);
    viewFor(m_zoomed, toX, toY, viewTo);
    m_tweens[NaviTween_ScrollX].start(viewFrom[0], viewTo[0], frames, 0, 0, UiTweenCurve_Linear);
    m_tweens[NaviTween_ScrollY].start(viewFrom[1], viewTo[1], frames, 0, 0, UiTweenCurve_Linear);
    m_vars[NaviVar_Zoomed]->value = 1;
}


// ──────────────────────────────────────────────────────────────────────
// The widget.

// FS2 0x0029e4f0
// callcheck: UiWidget, __construct_array, getNaviTable, getString, findVarIndex, getVar, nodeFlagsOf, nodeKindOf, start, getLink, hasPart, getGridPos, getGridPos, start, findTrack, findTrack, findTrack, value, value, updateNode, updateLinkBits

PrgScaleNavi::PrgScaleNavi(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : UiWidget(seq, layout, data, def, arg), m_flags(0), m_state(NaviState_Free), m_table(getNaviTable())
{
    if (!sSaveExtraReady) {
        sSaveExtra.head = 0;
        sSaveExtraReady = true;
    }
    m_routeCount = sSaveExtra.head;
    memcpy(m_route, savedSteps(), sizeof(m_route));
    m_savedNode = kNaviNoNode;

    const int firstVar = data->findVarIndex(data->getString((int)def->param[0]));
    for (int i = 0; i < NaviVar_Count; i++)
        m_vars[i] = data->getVar(firstVar + i);

    m_pendingCount = 0;
    for (int i = 0; i < kNaviPending; i++) {
        m_pending[i] = kNaviNoNode;
        m_vars[NaviVar_Pending0 + i]->value = kNaviNoNode;
    }
    m_vars[NaviVar_Tab]->value = 0;
    m_lastTab = 0;

    for (int i = 0; i < kNaviNodes; i++) {
        m_nodeFlags[i] = nodeFlagsOf(i);
        m_nodeKind[i] = nodeKindOf(i, m_nodeFlags[i]);
    }

    m_zoomed = m_vars[NaviVar_StartZoomed]->value != 0;
    const float *view = m_zoomed ? kNaviViewZoomed : kNaviViewPlain;
    for (int i = NaviTween_Scale; i < NaviTween_Count; i++)
        m_tweens[i].start(view[i - NaviTween_Scale], view[i - NaviTween_Scale], 0, 0, 0, UiTweenCurve_Linear);

    memset(m_rowShown, 0, sizeof(m_rowShown));
    memset(m_columnShown, 0, sizeof(m_columnShown));

    // After a clear the answer is negative: the screen opens on the cleared
    // node and jumps along its link. Else on the record's node.
    int node;
    const int answer = layout->m_arg;
    if (answer < 0) {
        node = -answer & kNaviAnswerStageMask;
        const bool second = (-answer & NaviAnswer_Variant1) != 0;
        m_linkNode = m_table->getLink(node, second);
        m_linkSecond = second;
        if (m_linkNode > 0) {
            m_flags |= NaviFlag_Jumping;
            if (!m_table->hasPart(node, second))
                m_flags |= NaviFlag_NoPart;
            m_table->getGridPos(m_linkNode, &m_linkX, &m_linkY);
        }
    } else {
        node = answer & kNaviAnswerStageMask;
    }
    int x, y;
    m_table->getGridPos(node, &x, &y);
    m_cursorX = x;
    m_vars[NaviVar_CursorX]->value = x;
    m_cursorY = y;
    m_vars[NaviVar_CursorY]->value = y;
    m_node = node;
    m_vars[NaviVar_Node]->value = node;

    if (m_node != routeTop()) {
        m_routeCount = 0;
        sSaveExtra.head = 0;
        routePush(m_node, 0);
    }

    const float pixelX = nodePixelX(m_cursorX), pixelY = nodePixelY(m_cursorY);
    m_tweens[NaviTween_CursorX].start(pixelX, pixelX, 0, 0, 0, UiTweenCurve_Linear);
    m_tweens[NaviTween_CursorY].start(pixelY, pixelY, 0, 0, 0, UiTweenCurve_Linear);
    float scroll[2];
    viewFor(m_zoomed, m_cursorX, m_cursorY, scroll);
    m_tweens[NaviTween_ScrollX].start(scroll[0], scroll[0], 0, 0, 0, UiTweenCurve_Linear);
    m_tweens[NaviTween_ScrollY].start(scroll[1], scroll[1], 0, 0, 0, UiTweenCurve_Linear);

    m_origin[0] = -222.0f;
    m_origin[1] = -92.0f;

    m_cursorTrack = static_cast<UiOffsetTrack *>(findTrack((int)def->param[1], UiTrackKind_Offset));
    m_markTrack = static_cast<UiOffsetTrack *>(findTrack((int)def->param[2], UiTrackKind_Offset));
    m_goalTrack = static_cast<UiOffsetTrack *>(findTrack((int)def->param[3], UiTrackKind_Offset));

    hideOverviewMark();
    m_markB[0] = kNaviNoMark;
    m_markB[1] = kNaviNoMark;
    if (m_cursorTrack != nullptr) {
        const float cursorY = m_tweens[NaviTween_CursorY].value();
        m_cursorTrack->m_offset[0] = m_tweens[NaviTween_CursorX].value();
        m_cursorTrack->m_offset[1] = cursorY;
    }

    updateNode(false);
    sNaviMenu = this;
    updateLinkBits();
    m_vars[NaviVar_NodeFlagged]->value = 0;
    m_goalTrack->m_offset[0] = m_markB[0];
    m_goalTrack->m_offset[1] = m_markB[1];
    m_vars[NaviVar_Scrolling]->value = 1;
}

// What the screen did to the route stays, unless it picked no node: then
// only the jump's step is taken back.
// FS2 0x0029e3d0
// callcheck: operator_delete, __destroy_arr (both the compiler's)

PrgScaleNavi::~PrgScaleNavi()
{
    sNaviMenu = nullptr;
    const int result = m_layout->m_result;
    if (result == NaviAnswer_Leave || (result & NaviAnswer_Help)) {
        if (m_flags & NaviFlag_Pushed) {
            sSaveExtra.head--;
            if (sSaveExtra.head < 0)
                sSaveExtra.head = 0;
        }
    } else {
        sSaveExtra.head = m_routeCount;
        memcpy(savedSteps(), m_route, sizeof(m_route));
    }
}

// FS2 0x0029d8a0
// callcheck: value, value, setScale, value, value, concat, translate, setTranslate

void PrgScaleNavi::draw(UiDrawer *drawer)
{
    UiMatrix scale;
    const float size = m_tweens[NaviTween_Scale].value();
    scale.setScale(m_tweens[NaviTween_Scale].value(), size);
    const float scroll[2] = { m_tweens[NaviTween_ScrollX].value(), m_tweens[NaviTween_ScrollY].value() };
    m_seq->m_parentXform.concat(scale);
    m_seq->m_parentXform.translate(scroll);
    drawer->setTranslate(m_origin);
}

// FS2 0x0029d5e0
// callcheck: value, value, setScale, value, value, concat, translate, setTranslate, value, setScissorPixels

void PrgScaleNavi::applyView(UiDrawer *drawer, bool columns, bool rows)
{
    UiMatrix scale;
    const float size = m_tweens[NaviTween_Scale].value();
    scale.setScale(m_tweens[NaviTween_Scale].value(), size);
    float scroll[2] = { m_tweens[NaviTween_ScrollX].value(), m_tweens[NaviTween_ScrollY].value() };
    if (columns || rows) {
        if (!columns)
            scroll[0] = 0.0f;
        if (!rows)
            scroll[1] = 0.0f;
    }
    m_seq->m_parentXform.concat(scale);
    m_seq->m_parentXform.translate(scroll);
    drawer->setTranslate(m_origin);

    float rect[4];
    if (!columns && !rows) {
        rect[0] = m_tweens[NaviTween_MapLeft].value() - kNaviMarginNear;
        rect[1] = m_tweens[NaviTween_MapTop].value() - kNaviMarginNear;
        rect[2] = m_tweens[NaviTween_MapRight].value() + kNaviMarginFar;
        rect[3] = m_tweens[NaviTween_MapBottom].value() + kNaviMarginNear;
    } else {
        NaviTween left = NaviTween_MapLeft;
        if (columns && !rows)
            left = NaviTween_ColumnLeft;
        else if (!columns)
            left = NaviTween_RowLeft;
        rect[0] = m_tweens[left].value();
        rect[1] = m_tweens[left + 2].value();
        rect[2] = m_tweens[left + 1].value();
        rect[3] = m_tweens[left + 3].value();
    }
    rect[0] += m_origin[0];
    rect[1] += m_origin[1];
    rect[2] += m_origin[0];
    rect[3] += m_origin[1];
    drawer->setScissorPixels(rect);
}

// Each frame while the layout asks something of it.
// FS2 0x0029b980
// callcheck: updateHints, updateLinkBits, getLink, getGridPos, moveCursor, getLinkOrNone, isMoveDone, isAtEnd, setZoom, defaultButton, bitMask, updateNode, syncRoute, updateLinkVars, playSound, step, value

void PrgScaleNavi::advance()
{
    GameSystem *sys = gGameSystem;
    m_vars[NaviVar_Refresh]->value = 0;
    bool showGoal = false;  // the goal track follows the mark
    bool busy = false;      // no hints this frame
    bool moved = false;     // the player moved the cursor

    if (m_vars[NaviVar_Request]->value == NaviRequest_None) {
        updateHints(m_node, m_cursorY, m_zoomed, false);
        if (updateLinkBits())
            m_vars[NaviVar_Refresh]->value = 1;
        return;
    }

    m_vars[NaviVar_Result]->value = NaviResult_None;
    if (!(m_flags & NaviFlag_Entered)) {
        m_flags |= NaviFlag_Entered;
        if (m_flags & NaviFlag_Jumping) {
            m_state = NaviState_Jump;
        } else {
            switch (m_vars[NaviVar_Request]->value) {
            case NaviRequest_Link0:    m_state = NaviState_Link0;    break;
            case NaviRequest_Link1:    m_state = NaviState_Link1;    break;
            case NaviRequest_Back:     m_state = NaviState_Back;     break;
            case NaviRequest_Overview: m_state = NaviState_Overview; break;
            default:                   m_state = NaviState_Reset;    break;
            }
        }
    }

    switch (m_state) {
    case NaviState_Overview:
        updateLinkBits();
        m_vars[NaviVar_Refresh]->value = 1;
        placeOverviewMark();
        setZoom(true, m_vars[NaviVar_ZoomFrames]->value, m_cursorX, m_cursorY, m_cursorX, m_cursorY);
        m_state = NaviState_Free;
        busy = true;
        m_vars[NaviVar_Scrolling]->value = 0;
        break;

    case NaviState_Back: {
        // PC: the ROM's guards around the entry before the last come to this.
        const int node = m_routeCount < 2 ? kNaviNoNode : m_route[m_routeCount - 2].node;
        int x, y;
        m_table->getGridPos(node, &x, &y);
        moveCursor(kNaviStepSpeed, m_cursorX, m_cursorY, x, y, 0, 0);
        placeCursor(x, y);
        m_routeCount--;
        if (m_routeCount < 0)
            m_routeCount = 0;
        updateLinkBits();
        busy = true;
        m_state = NaviState_Settle;
        break;
    }

    case NaviState_Link1:
        followLink(1);
        busy = true;
        m_state = NaviState_Settle;
        break;

    case NaviState_Link0:
        followLink(0);
        busy = true;
        m_state = NaviState_Settle;
        break;

    case NaviState_Settle:
        if (isMoveDone(NaviTween_Count)) {
            m_vars[NaviVar_Result]->value = NaviResult_Decided;
            m_vars[NaviVar_Request]->value = NaviRequest_None;
            m_flags &= ~(NaviFlag_Entered | NaviFlag_Moved);
            m_vars[NaviVar_Refresh]->value = 1;
            showGoal = true;
            m_vars[NaviVar_Scrolling]->value = m_zoomed ? 0 : 1;
        }
        busy = true;
        break;

    case NaviState_Jump:
        if (!(m_flags & NaviFlag_Moved)) {
            const int node = m_node;
            moveCursor(kNaviStepSpeed, m_cursorX, m_cursorY, m_linkX, m_linkY, kNaviJumpPause, kNaviJumpPause);
            placeCursor(m_linkX, m_linkY);
            m_flags |= NaviFlag_Moved;
            const int side = sideFrom(m_linkNode, node);
            routePush(m_linkNode, side);
            updateLinkBits();
            m_flags |= NaviFlag_Pushed;
            if (sSaveExtra.head < kNaviRouteSteps) {
                savedSteps()[sSaveExtra.head].node = m_linkNode;
                savedSteps()[sSaveExtra.head].side = side;
                sSaveExtra.head++;
            }
        }
        m_state++;
        // Fall through.
    case NaviState_JumpWait:
        if (!isMoveDone(NaviTween_Count)) {
            if (isAtEnd(NaviTween_Count)) {
                showGoal = true;
                m_vars[NaviVar_Refresh]->value = 1;
            }
        } else {
            if (!(m_flags & NaviFlag_NoPart)) {
                m_flags &= ~(NaviFlag_Entered | NaviFlag_Moved | NaviFlag_Jumping | NaviFlag_NoPart);
                m_state = NaviState_Free;
            } else {
                m_vars[NaviVar_Result]->value = NaviResult_Ended;
                m_vars[NaviVar_Request]->value = NaviRequest_None;
            }
            showGoal = true;
            m_vars[NaviVar_Refresh]->value = 1;
        }
        busy = true;
        break;

    case NaviState_Reset:
        m_state = NaviState_Free;
        updateLinkBits();
        m_vars[NaviVar_Refresh]->value = 1;
        // Fall through.
    case NaviState_Free:
    default: {
        if (m_state != NaviState_Free)
            break;
        if (!isMoveDone(NaviTween_Count)) {
            busy = true;
            break;
        }
        m_vars[NaviVar_Scrolling]->value = m_zoomed ? 0 : 1;

        const u_int pressed = sys->m_pad0.m_pressed;
        const u_int overview = pressed & kNaviPadOverview;
        const bool decide = (pressed & bitMask(ButtonMap::defaultButton(0))) != 0;
        if (decide && m_node != kNaviNoNode) {
            if (!m_zoomed) {
                m_vars[NaviVar_Result]->value = NaviResult_Decided;
                m_vars[NaviVar_Request]->value = NaviRequest_None;
                m_flags &= ~NaviFlag_Entered;
            } else {
                setZoom(false, m_vars[NaviVar_ZoomFrames]->value, m_cursorX, m_cursorY, m_cursorX, m_cursorY);
                hideOverviewMark();
                const int previous = m_node;
                const int saved = m_savedNode;
                takeNode(saved);
                if (previous != saved) {
                    updateNode(false);
                    syncRoute(m_node);
                    updateLinkBits();
                    updateLinkVars();
                }
                m_state = NaviState_Settle;
            }
            m_layout->playSound(kNaviSoundSlot, "SYS_FIX");
            break;
        }
        if (overview != 0 && m_node != kNaviNoNode && !m_zoomed) {
            placeOverviewMark();
            setZoom(true, m_vars[NaviVar_ZoomFrames]->value, m_cursorX, m_cursorY, m_cursorX, m_cursorY);
            m_layout->playSound(kNaviSoundSlot, "SYS_FIX");
            m_vars[NaviVar_Scrolling]->value = 0;
            break;
        }
        const bool cancel = (pressed & bitMask(ButtonMap::defaultButton(1))) != 0;
        if (m_zoomed && (overview != 0 || cancel)) {
            setZoom(false, m_vars[NaviVar_ZoomFrames]->value, m_cursorX, m_cursorY, m_savedX, m_savedY);
            placeCursor(m_savedX, m_savedY);
            hideOverviewMark();
            m_vars[NaviVar_Refresh]->value = 1;
            takeNode(m_savedNode);
            showGoal = true;
            m_layout->playSound(kNaviSoundSlot, "SYS_FIX");
            break;
        }
        if (cancel) {
            m_vars[NaviVar_Result]->value = NaviResult_Cancelled;
            m_vars[NaviVar_Request]->value = NaviRequest_None;
            m_layout->playSound(kNaviSoundSlot, "SYS_CANCEL");
            break;
        }
        const u_int repeat = sys->m_pad0.m_repeat;
        int x = m_cursorX, y = m_cursorY;
        if (repeat & kNaviPadUp)
            y--;
        else if (repeat & kNaviPadDown)
            y++;
        if (repeat & kNaviPadLeft)
            x--;
        else if (repeat & kNaviPadRight)
            x++;
        if (x > kNaviColumns - 1)
            x = kNaviColumns - 1;
        if (x < 0)
            x = 0;
        if (y > kNaviRows - 1)
            y = kNaviRows - 1;
        if (y < 0)
            y = 0;
        moved = moveCursor(kNaviMoveSpeed, m_cursorX, m_cursorY, x, y, 0, 0);
        if (moved) {
            m_vars[NaviVar_Refresh]->value = 1;
            m_layout->playSound(kNaviSoundSlot, "SYS_CURSOR");
        }
        showGoal = moved;
        placeCursor(x, y);
        break;
    }
    }

    for (int i = 0; i < NaviTween_Count; i++)
        m_tweens[i].step();
    if (m_cursorTrack != nullptr) {
        const float cursorY = m_tweens[NaviTween_CursorY].value();
        m_cursorTrack->m_offset[0] = m_tweens[NaviTween_CursorX].value();
        m_cursorTrack->m_offset[1] = cursorY;
    }
    if (m_savedNode == kNaviNoNode) {
        updateNode(false);
        if (moved)
            syncRoute(m_node);
        updateLinkBits();
        updateLinkVars();
    } else {
        updateNode(true);
    }
    updateHints(m_node, m_cursorY, m_zoomed, busy);
    if (showGoal) {
        m_goalTrack->m_offset[0] = m_markB[0];
        m_goalTrack->m_offset[1] = m_markB[1];
    }
}
