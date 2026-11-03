/* ==========================================================================
 *  game/progress.h
 *
 *  Save-progress queries (game/progress.cpp, 0x2955e0..0x296c10 and
 *  0x296d00..0x296d60): the navigator table's unlock conditions and the
 *  records a fresh save holds.
 *
 *  The navigator table is the file 2dEU/navi%s.d2b the navi menu
 *  (0x29a700..0x2a0000, not written yet) loads: a 0x10-byte header with the
 *  node count, then one 0x70-byte NaviNode per scenario on the menu's grid.
 *  A node opens, and each of its one or two goals counts as met, when the
 *  conditions of its set hold (NaviTable::testConditions); conditions read
 *  the save data (stage clear flags, archives, records), the flag table and
 *  other nodes. Pending links are event flags a node raises that open
 *  another node's goal.
 * ======================================================================== */
#ifndef _GAME_PROGRESS_H
#define _GAME_PROGRESS_H

#include <stddef.h>

#include "sdk/scetypes.h"

// NaviCond::type. Unused slots hold -1, which (as every unknown type) holds.
enum NaviCondType
{
    NaviCond_StageClearedV0      = 0,   // stage arg cleared in variant 0 by any kind
    NaviCond_StageClearedV1      = 1,   // stage arg cleared in variant 1 by any kind
    NaviCond_Archive             = 2,   // archive arg collected; 0: all of them
    NaviCond_AllArchives         = 3,   // every archive but kArchiveNotRequired
    NaviCond_AllRecords          = 4,   // SaveData::allRecordsSet
    NaviCond_NoPendingLinks      = 5,   // every pending link of this node fired (skippable)
    NaviCond_Never               = 6,
    NaviCond_NodeOpen            = 7,   // node arg's set 0
    NaviCond_NodeFirstGoalDone   = 8,   // node arg's sets 0 + 1, type 5 skipped
    NaviCond_NodeOpenAlt         = 9,   // as 7
    NaviCond_NodeSecondGoalDone  = 10,  // node arg's sets 0 + 2, type 5 skipped
    NaviCond_KindSelected        = 11,  // the play record's kind (SystemData +0x4c0) is not 0
    NaviCond_AllStagesKind2      = 12,  // stages 1..29 cleared by kind 2, in each variant they have
    NaviCond_NodeFirstGoalOpen   = 13,  // node arg's sets 0 + 1
    NaviCond_NodeSecondGoalOpen  = 14,  // node arg: set 0, no pending link, set 2
};

// The condition sets of a node (NaviNode::cond's first index).
enum NaviCondSet
{
    NaviSet_Open       = 0,     // the node is on the grid and playable
    NaviSet_FirstGoal  = 1,
    NaviSet_SecondGoal = 2,     // absent when NaviTable::hasSecondGoal says so
};

// NaviTable::getTriggerState.
enum NaviTriggerState
{
    NaviTrigger_None  = 0,      // no trigger, or its target is not ready
    NaviTrigger_Ready = 1,      // the target's goal is met and waits for this flag
    NaviTrigger_Fired = 2,      // the flag is already set
};

// NaviNode::kind: a node with one part (the others have two, NaviNode::part).
constexpr short kNaviKindSinglePart = 1;

constexpr int kNaviCondSets = 3;
constexpr int kNaviCondsPerSet = 4;
constexpr int kNaviTriggers = 2;        // +0x28..+0x30: the pending links start at +0x30
constexpr int kNaviLinkGroups = 3;
constexpr int kNaviLinksPerGroup = 4;
constexpr int kNaviDirections = 4;

// What getTrigger / findPendingLink answer for "no target".
constexpr int kNaviNoTarget = -2;

struct NaviCond
{
    signed char type;           // FS2 +0: NaviCondType
    signed char arg;            // FS2 +1: a stage, archive or node number
};
static_assert(sizeof(NaviCond) == 0x2, "NaviCond: EE 0x2");

struct NaviTrigger
{
    signed char flag;           // FS2 +0: flag table index, < 0 none
    u_char      part1Only;      // FS2 +1: counts only when part 1 asks
    signed char target;         // FS2 +2: the node it opens
    u_char      secondGoal;     // FS2 +3: nonzero: the target's second goal, else its first
};
static_assert(sizeof(NaviTrigger) == 0x4, "NaviTrigger: EE 0x4");

struct NaviPendingLink
{
    signed char flag;           // FS2 +0: flag table index, < 0 none
    signed char target;         // FS2 +1: the node it opens
    u_char      secondGoal;     // FS2 +2: nonzero: the target's second goal, else its first
    u_char      _3;
};
static_assert(sizeof(NaviPendingLink) == 0x4, "NaviPendingLink: EE 0x4");

struct NaviNode
{
    short    kind;              // FS2 +0x00: kNaviKindSinglePart or two parts
    short    _02;
    short    part[2];           // FS2 +0x04: nonzero when the part exists (hasPart)
    float    x;                 // FS2 +0x08: grid column
    float    y;                 // FS2 +0x0c: grid row
    NaviCond cond[kNaviCondSets][kNaviCondsPerSet];                // FS2 +0x10
    NaviTrigger trigger[kNaviTriggers];                            // FS2 +0x28
    NaviPendingLink pending[kNaviLinkGroups][kNaviLinksPerGroup];  // FS2 +0x30
    u_char   _60[8];            // FS2 +0x60
    short    link[kNaviDirections];  // FS2 +0x68: neighbouring nodes, < 0 (or 0 for getLinkOrNone) none
};
static_assert(offsetof(NaviNode, x) == 0x08, "NaviNode::x");
static_assert(offsetof(NaviNode, cond) == 0x10, "NaviNode::cond");
static_assert(offsetof(NaviNode, trigger) == 0x28, "NaviNode::trigger");
static_assert(offsetof(NaviNode, pending) == 0x30, "NaviNode::pending");
static_assert(offsetof(NaviNode, link) == 0x68, "NaviNode::link");
static_assert(sizeof(NaviNode) == 0x70, "NaviNode: EE 0x70");

// The navi%s.d2b image: this header, then count NaviNodes.
struct NaviTable
{
    int  getGridPos(int index, int *x, int *y) const;                    // FS2 0x002955e0
    bool hasSecondGoal(int index) const;                                 // FS2 0x00295630
    NaviNode *findNodeAt(int x, int y);                                  // FS2 0x00295690
    bool hasPart(int index, int part) const;                             // FS2 0x00295730
    short getLink(int index, int second) const;                          // FS2 0x00295780
    int  getTriggerState(int index, int part, int n) const;              // FS2 0x002957b0
    int  getTrigger(int index, int part, int n, int *target, int *secondGoal) const;  // FS2 0x002958f0
    short getLinkOrNone(int index, int dir) const;                       // FS2 0x00295980
    bool isLink3Open(int index) const;                                   // FS2 0x002959b0
    bool isLink2Open(int index) const;                                   // FS2 0x00295a00
    bool isLink1Open(int index) const;                                   // FS2 0x00295a50
    bool isLink0Open(int index) const;                                   // FS2 0x00295aa0
    bool findPendingLink(int index, int group, int *target, int *secondGoal) const;  // FS2 0x00295af0
    bool canFireLink(int index) const;                                   // FS2 0x00295d50
    bool isNextFlag(int index, int flag) const;                          // FS2 0x00296060
    bool hasPendingFlag(int index) const;                                // FS2 0x00296140
    bool isSecondGoalDone(int index) const;                              // FS2 0x00296200
    bool isSecondGoalOpen(int index) const;                              // FS2 0x00296260
    bool isFirstGoalOpen(int index) const;                               // FS2 0x00296380
    bool isOpen(int index) const;                                        // FS2 0x002963e0
    bool isSinglePart(int index) const;                                  // FS2 0x00296410
    bool testConditions(int index, int set, bool skipLinkCheck) const;   // FS2 0x00296430

    // Inline in the ROM (canFireLink, getTriggerState, testConditions).
    bool isFirstGoalDone(int index) const
    {
        return testConditions(index, NaviSet_Open, true) &&
               testConditions(index, NaviSet_FirstGoal, true);
    }

    NaviNode &node(int index) { return reinterpret_cast<NaviNode *>(this + 1)[index]; }
    const NaviNode &node(int index) const { return reinterpret_cast<const NaviNode *>(this + 1)[index]; }

    int _00;                    // FS2 +0x00
    int _04;                    // FS2 +0x04
    int count;                  // FS2 +0x08: nodes after the header
    int _0c;                    // FS2 +0x0c
};
static_assert(offsetof(NaviTable, count) == 0x08, "NaviTable::count");
static_assert(sizeof(NaviTable) == 0x10, "NaviTable: EE 0x10");

// The record a fresh save holds for (kind, variant, stage).
u_int stageDefaultRecord(int kind, int variant, int stage);              // FS2 0x00296d00

#endif /* _GAME_PROGRESS_H */
