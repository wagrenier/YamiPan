// game/progress.cpp -- save-progress queries: the navigator table's unlock
// conditions and the fresh save's records (game/progress.h).

#include "game/progress.h"

#include "game/gamepaths.h" /* getInitPackItem */
#include "game/sysdata.h"
#include "sdk/scetypes.h"

// The one archive NaviCond_AllArchives does not ask for.
constexpr int kArchiveNotRequired = 98;

// The record kind whose clears NaviCond_AllStagesKind2 asks for.
constexpr int kAllStagesKind = 2;

// A fresh kind-0 record: 3599 = 59:59, the slowest time.
constexpr u_int kDefaultRecordTime = 3599;

// The flag table through the block getSaveData returns (SystemData +0x520).
// The ROM null-tests the pointer first, as a base-class conversion does.
static FlagTable *saveFlags()
{
    SystemData *data = reinterpret_cast<SystemData *>(getSaveData());
    return data != 0 ? &data->flags : 0;
}

// ──────────────────────────────────────────────────────────────────────
// The navi table: the init pack's item 1 (a tail jump to getInitPackItem).
// FS2 0x002955d0
NaviTable *getNaviTable()
{
    return static_cast<NaviTable *>(getInitPackItem(kInitItemNaviTable));
}

// ──────────────────────────────────────────────────────────────────────
// A node's grid position as ints.
// FS2 0x002955e0

int NaviTable::getGridPos(int index, int *x, int *y) const
{
    const NaviNode &n = node(index);
    *x = (int)n.x;
    *y = (int)n.y;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Whether the node has a second goal: not for a single-part node, nor when
// set 2 holds a condition that never holds.
// FS2 0x00295630

bool NaviTable::hasSecondGoal(int index) const
{
    const NaviNode &n = node(index);
    if (n.kind == kNaviKindSinglePart)
        return false;
    for (int i = 0; i < kNaviCondsPerSet; i++) {
        if (n.cond[NaviSet_SecondGoal][i].type == NaviCond_Never)
            return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The node at grid position (x, y), or 0.
// FS2 0x00295690

NaviNode *NaviTable::findNodeAt(int x, int y)
{
    for (int i = 0; i < count; i++) {
        NaviNode &n = node(i);
        if (n.x == (float)x && n.y == (float)y)
            return &n;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Whether the node has the part; a single-part node answers its first.
// FS2 0x00295730

bool NaviTable::hasPart(int index, int part) const
{
    const NaviNode &n = node(index);
    short has;
    if (n.kind == kNaviKindSinglePart)
        has = n.part[0];
    else
        has = n.part[part];
    return has != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Link 0, or link 1 when second, as stored.
// FS2 0x00295780

short NaviTable::getLink(int index, int second) const
{
    const NaviNode &n = node(index);
    if (second == 0)
        return n.link[0];
    return n.link[1];
}

// ──────────────────────────────────────────────────────────────────────
// Trigger n of the node for this part: fired already, ready (its target
// waits for exactly this flag and the target's goal is met, pending links
// aside), or neither.
// callcheck: getSaveData, testConditions (saveFlags and the goal checks inlined)
// FS2 0x002957b0

int NaviTable::getTriggerState(int index, int part, int n) const
{
    int target;
    int secondGoal;
    int flag = getTrigger(index, part, n, &target, &secondGoal);
    if (flag < 0)
        return NaviTrigger_None;
    if (saveFlags()->get(flag) != 0)
        return NaviTrigger_Fired;
    if (!isNextFlag(target, flag))
        return NaviTrigger_None;

    if (secondGoal == 0) {
        if (isFirstGoalDone(target))
            return NaviTrigger_Ready;
    } else {
        if (isSecondGoalDone(target))
            return NaviTrigger_Ready;
    }
    return NaviTrigger_None;
}

// ──────────────────────────────────────────────────────────────────────
// Trigger n's flag with its target and goal; -1 (target kNaviNoTarget, goal
// -1) when there is none or it is part 1's and another part asks.
// FS2 0x002958f0

int NaviTable::getTrigger(int index, int part, int n, int *target, int *secondGoal) const
{
    const NaviTrigger &t = node(index).trigger[n];
    if (t.flag < 0 || (t.part1Only != 0 && part != 1)) {
        *target = kNaviNoTarget;
        *secondGoal = -1;
        return -1;
    }
    *target = t.target;
    *secondGoal = t.secondGoal != 0 ? 1 : 0;
    return t.flag;
}

// ──────────────────────────────────────────────────────────────────────
// Link dir, -1 when below 1.
// FS2 0x00295980

short NaviTable::getLinkOrNone(int index, int dir) const
{
    short link = node(index).link[dir];
    if (link < 1)
        link = -1;
    return link;
}

// ──────────────────────────────────────────────────────────────────────
// Whether link 3 / 2 / 1 / 0 leads to an open node.
// FS2 0x002959b0

bool NaviTable::isLink3Open(int index) const
{
    short link = node(index).link[3];
    if (link < 0)
        return false;
    return testConditions(link, NaviSet_Open, false);
}

// FS2 0x00295a00

bool NaviTable::isLink2Open(int index) const
{
    short link = node(index).link[2];
    if (link < 0)
        return false;
    return testConditions(link, NaviSet_Open, false);
}

// FS2 0x00295a50

bool NaviTable::isLink1Open(int index) const
{
    short link = node(index).link[1];
    if (link < 0)
        return false;
    return testConditions(link, NaviSet_Open, false);
}

// FS2 0x00295aa0

bool NaviTable::isLink0Open(int index) const
{
    short link = node(index).link[0];
    if (link < 0)
        return false;
    return testConditions(link, NaviSet_Open, false);
}

// ──────────────────────────────────────────────────────────────────────
// In one group of the node's pending links, the first whose flag is still
// unset decides: true, with its target and goal, when that goal is open.
// callcheck: getSaveData, testConditions (saveFlags and the goal checks inlined)
// FS2 0x00295af0

bool NaviTable::findPendingLink(int index, int group, int *target, int *secondGoal) const
{
    *target = kNaviNoTarget;
    *secondGoal = -1;
    const NaviPendingLink *link = node(index).pending[group];
    for (int i = 0; i < kNaviLinksPerGroup; i++, link++) {
        if (link->flag < 0)
            continue;
        if (saveFlags()->get(link->flag) != 0)
            continue;

        if (link->secondGoal == 0) {
            if (isFirstGoalOpen(link->target)) {
                *target = link->target;
                *secondGoal = 0;
                return true;
            }
        } else {
            if (isSecondGoalOpen(link->target)) {
                *target = link->target;
                *secondGoal = 1;
                return true;
            }
        }
        return false;
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// Whether the node, its own goal met, has a pending link to fire: the
// first link whose flag is still unset answers whether its target's goal is
// open.
// callcheck: getSaveData, testConditions (saveFlags and the goal checks inlined)
// FS2 0x00295d50

bool NaviTable::canFireLink(int index) const
{
    for (int group = 0; group < kNaviLinkGroups; group++) {
        for (int i = 0; i < kNaviLinksPerGroup; i++) {
            const NaviPendingLink &link = node(index).pending[group][i];
            if (link.flag < 0)
                continue;

            bool goalDone;
            if (hasSecondGoal(index))
                goalDone = isSecondGoalDone(index);
            else
                goalDone = isFirstGoalDone(index);
            if (!goalDone)
                return false;

            if (saveFlags()->get(link.flag) != 0)
                continue;
            if (link.secondGoal != 0)
                return isSecondGoalOpen(link.target);
            return isFirstGoalOpen(link.target);
        }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// Whether flag is the first unset one in one of the node's link groups.
// callcheck: getSaveData (saveFlags inlined)
// FS2 0x00296060

bool NaviTable::isNextFlag(int index, int flag) const
{
    for (int group = 0; group < kNaviLinkGroups; group++) {
        const NaviPendingLink *link = node(index).pending[group];
        for (int i = 0; i < kNaviLinksPerGroup; i++, link++) {
            if (link->flag < 0)
                continue;
            if (saveFlags()->get(link->flag) == 0) {
                if (link->flag == flag)
                    return true;
                break;
            }
        }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// Whether any of the node's pending links has not fired yet.
// callcheck: getSaveData (saveFlags inlined)
// FS2 0x00296140

bool NaviTable::hasPendingFlag(int index) const
{
    for (int group = 0; group < kNaviLinkGroups; group++) {
        const NaviPendingLink *link = node(index).pending[group];
        for (int i = 0; i < kNaviLinksPerGroup; i++, link++) {
            if (link->flag >= 0 && saveFlags()->get(link->flag) == 0)
                return true;
        }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// The goals: "done" leaves the node's own pending links out, "open" asks
// them too.
// FS2 0x00296200

bool NaviTable::isSecondGoalDone(int index) const
{
    return testConditions(index, NaviSet_Open, true) &&
           testConditions(index, NaviSet_SecondGoal, true);
}

// callcheck: get, getSaveData (hasPendingFlag inlined)
// FS2 0x00296260

bool NaviTable::isSecondGoalOpen(int index) const
{
    if (!testConditions(index, NaviSet_Open, false))
        return false;
    if (hasPendingFlag(index))
        return false;
    return testConditions(index, NaviSet_SecondGoal, false);
}

// FS2 0x00296380

bool NaviTable::isFirstGoalOpen(int index) const
{
    return testConditions(index, NaviSet_Open, false) &&
           testConditions(index, NaviSet_FirstGoal, false);
}

// FS2 0x002963e0

bool NaviTable::isOpen(int index) const
{
    return testConditions(index, NaviSet_Open, false);
}

// FS2 0x00296410

bool NaviTable::isSinglePart(int index) const
{
    return node(index).kind == kNaviKindSinglePart;
}

// ──────────────────────────────────────────────────────────────────────
// Whether all four conditions of the node's set hold; skipLinkCheck leaves
// NaviCond_NoPendingLinks out. Node conditions recurse.
// callcheck: get (hasPendingFlag inlined in types 5 and 14)
// FS2 0x00296430

bool NaviTable::testConditions(int index, int set, bool skipLinkCheck) const
{
    const NaviCond *cond = node(index).cond[set];
    for (int i = 0; i < kNaviCondsPerSet; i++, cond++) {
        if (cond->type == NaviCond_NoPendingLinks && skipLinkCheck)
            continue;

        bool holds;
        switch (cond->type) {
        case NaviCond_StageClearedV0:
            holds = getSaveData()->testStageFlags(cond->arg, kClearedVariant0);
            break;
        case NaviCond_StageClearedV1:
            holds = getSaveData()->testStageFlags(cond->arg, kClearedVariant1);
            break;
        case NaviCond_Archive:
            holds = getSaveData()->hasArchive(cond->arg);
            break;
        case NaviCond_AllArchives:
            holds = true;
            for (int id = 1; id <= kArchiveCount; id++) {
                if (id == kArchiveNotRequired)
                    continue;
                if (!getSaveData()->hasArchive(id)) {
                    holds = false;
                    break;
                }
            }
            break;
        case NaviCond_AllRecords:
            holds = getSaveData()->allRecordsSet() != 0;
            break;
        case NaviCond_NoPendingLinks:
            holds = !hasPendingFlag(index);
            break;
        case NaviCond_Never:
            holds = false;
            break;
        case NaviCond_NodeOpen:
        case NaviCond_NodeOpenAlt:
            holds = testConditions(cond->arg, NaviSet_Open, false);
            break;
        case NaviCond_NodeFirstGoalDone:
            holds = isFirstGoalDone(cond->arg);
            break;
        case NaviCond_NodeSecondGoalDone:
            holds = isSecondGoalDone(cond->arg);
            break;
        case NaviCond_KindSelected:
            holds = reinterpret_cast<SystemData *>(getSaveData())->select.kind != 0;
            break;
        case NaviCond_AllStagesKind2: {
            SaveData *save = getSaveData();
            holds = true;
            for (int stage = 1; stage < kStageCount; stage++) {
                if (!save->testStageFlags(stage, save->clearBit(kAllStagesKind, 0))) {
                    holds = false;
                    break;
                }
                if (save->stageHasVariant1(stage) &&
                    !save->testStageFlags(stage, save->clearBit(kAllStagesKind, 1))) {
                    holds = false;
                    break;
                }
            }
            break;
        }
        case NaviCond_NodeFirstGoalOpen:
            holds = isFirstGoalOpen(cond->arg);
            break;
        case NaviCond_NodeSecondGoalOpen:
            holds = isSecondGoalOpen(cond->arg);
            break;
        default:
            holds = true;
            break;
        }
        if (!holds)
            return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// Whether archive id is collected; 0 asks for all of them. CodeWarrior
// inlines the recursion five levels deep.
// FS2 0x00296a00

bool SaveData::hasArchive(int id) const
{
    if (id == 0) {
        for (int i = 1; i <= kArchiveCount; i++) {
            if (!hasArchive(i))
                return false;
        }
        return true;
    }
    return archive[id] != 0;
}

// The records a fresh save holds for kinds 1 and 2, per stage and variant
// (data 0x309ab0 / 0x309ba0). Stage 0 holds 10425 in both; 29 (highest
// wins) 0.
static const u_int sDefaultRecord1[kStageCount][kRecordVariants] = {
    { 10425, 10425 },   // 0
    {   500,  4500 },   // 1
    {  6750,  8500 },   // 2
    {  3000,  5250 },   // 3
    {  5000,  5250 },   // 4
    {  3625,  3750 },   // 5
    {  5250,  6500 },   // 6
    {  5250,  8250 },   // 7
    {  4500,  9000 },   // 8
    {  4500,  9000 },   // 9
    {  9000,  6750 },   // 10
    {  9000,  3000 },   // 11
    {  8250,  7500 },   // 12
    {  6500,  4500 },   // 13
    {  1500,  8250 },   // 14
    {  7250,  6750 },   // 15
    {  4750,  7000 },   // 16
    {  4750,  4500 },   // 17
    {  4500, 13500 },   // 18
    {  3750, 15000 },   // 19
    {  7500,  7500 },   // 20
    {  9750,  8250 },   // 21
    {  7500,  9750 },   // 22
    {  6750,  6750 },   // 23
    {  4500,  7500 },   // 24
    {  6500,  5250 },   // 25
    {  4500, 22500 },   // 26
    { 11250, 11250 },   // 27
    {  3500,  3750 },   // 28
    {     0,     0 },   // 29
};

static const u_int sDefaultRecord2[kStageCount][kRecordVariants] = {
    { 10425, 10425 },   // 0
    {   475,  5450 },   // 1
    {  5025,  6550 },   // 2
    {  2225,  4525 },   // 3
    {  4100,  4000 },   // 4
    {  2325,  2175 },   // 5
    {  1625,  2725 },   // 6
    {  3050,  4400 },   // 7
    {  3150,  4300 },   // 8
    {  3125,  7650 },   // 9
    {  3675,  2425 },   // 10
    {  7250,  2475 },   // 11
    {  5825,  4000 },   // 12
    {  4375,  2450 },   // 13
    {   975,  1700 },   // 14
    {  1525,  3075 },   // 15
    {  3025,  6000 },   // 16
    {  2175,  2425 },   // 17
    {  3000, 10550 },   // 18
    {  1900, 15325 },   // 19
    {  3125,  3800 },   // 20
    {  5625,  5350 },   // 21
    {  5900,  6475 },   // 22
    {  4775,  4550 },   // 23
    {  3600,  6550 },   // 24
    {  4700,  3600 },   // 25
    {  2850, 15475 },   // 26
    {  9100,  9100 },   // 27
    {  3150,  2825 },   // 28
    {     0,     0 },   // 29
};

// ──────────────────────────────────────────────────────────────────────
// The record a fresh save holds: kind 0 the slowest time, kinds 1 and 2
// their tables.
// FS2 0x00296d00

u_int stageDefaultRecord(int kind, int variant, int stage)
{
    if (kind == 0)
        return kDefaultRecordTime;
    if (kind == 1)
        return sDefaultRecord1[stage][variant];
    return sDefaultRecord2[stage][variant];
}
