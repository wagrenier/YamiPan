/* ========================================================================
 *  game/routeplayer.cpp -- a stage's rute file and the ways through it.
 *  See game/routeplayer.h.
 *
 *  0x254e10..0x255080 and 0x258bc0..0x25b850 are MSL inlines the ROM kept
 *  out of line; they are the host's <vector>, <set> and <algorithm> here
 *  (the ledger after the includes).
 * ======================================================================== */
#include "game/routeplayer.h"

#include <math.h>
#include <string.h>

#include <algorithm>
#include <set>

#include "rx/rxsys.h"

/*
 * CodeWarrior's MSL template instances emitted in this object file (batch
 * 247). The host has no body for any of them: each is the standard library
 * on the right, inline at its caller. RouteLegList is the ROM's
 * std::vector<RouteLeg>, WordVector its std::vector<RouteNode *>, NameSet
 * collectNames' std::set<const char *>, AreaNumberList RouteFile +0x0c (a
 * second 2-byte vector instance beside vector<short>, so another element
 * type in the ROM; std::vector<short> here). A vector is {capacity, count,
 * data}; a tree node is left, right, parent | colour, value at +0xc.
 *
 * areaPath's reversed copies: std::reverse_copy(first, last,
 * back_inserter(v)) and std::copy, their iterator ops kept out of line:
 *   FS2 0x00254f10 WordVector::iterator::operator!=          first != last
 *   FS2 0x00254fc0 back_insert_iterator<WordVector>::operator*   *result
 *   FS2 0x00254fa0 WordVector::iterator::operator--          --last
 *   FS2 0x00254f90 WordVector::iterator::operator*           *last
 *   FS2 0x00254f40 back_insert_iterator<WordVector>::operator=   push_back
 *   FS2 0x00254f30 back_insert_iterator<WordVector>::operator++  ++result
 *   FS2 0x00254f00 back_insert_iterator<WordVector>::back_insert_iterator  the result's copy
 *   FS2 0x00254e40 RouteLegList::iterator::operator!=        the same for legs
 *   FS2 0x00254ef0 back_insert_iterator<RouteLegList>::operator*
 *   FS2 0x00254ed0 RouteLegList::iterator::operator--
 *   FS2 0x00254ec0 RouteLegList::iterator::operator*
 *   FS2 0x00254e70 back_insert_iterator<RouteLegList>::operator=
 *   FS2 0x00254e60 back_insert_iterator<RouteLegList>::operator++
 *   FS2 0x00254e30 back_insert_iterator<RouteLegList>::back_insert_iterator
 *   FS2 0x00254e10 RouteLegList::iterator::operator++        the run's forward copy
 *   FS2 0x00254fd0 WordVector::WordVector                    leaving / arriving
 *   FS2 0x00255080 RouteLegList::RouteLegList                way.steps, runs[4]
 *   FS2 0x00254ff0 RouteLegList::operator=                   out.steps = way.steps
 *
 * The legs (RouteLegList, 4-byte RouteLeg):
 *   FS2 0x00259430 RouteLegList::assign                      inside operator=
 *   FS2 0x002592f0 RouteLegList::initRange                   reversePath's copy of the steps
 *   FS2 0x00259760 RouteLegList::insertRange                 RoutePath::append
 *   FS2 0x00259d60 RouteLegList::erase(iterator)             wayFromSegment's erase(begin)
 *
 * The rute file's tables, std::lower_bound on the first field (findRecord):
 *   FS2 0x00258bc0 std::lower_bound(short)                   isListedArea
 *   FS2 0x00258c70 std::lower_bound(RouteCrossingFrom)       areaCrossings, findWayIn
 *   FS2 0x00258ce0 std::lower_bound(RouteCrossingTo)         areaCrossings, findWayIn
 *   FS2 0x00258dc0 std::lower_bound(RouteWayRecord)          findWayIn
 *   FS2 0x00258d50 std::lower_bound(RouteChainList)          areaList (header +0x34)
 *   FS2 0x00258e30 std::lower_bound(RouteAreaChains)         m_areaChains (header +0x24)
 *   FS2 0x00258ea0 std::lower_bound(RouteChainRunFrom)       chainRun, chainOf, wayAlongChains
 *   FS2 0x00258f10 std::lower_bound(RouteChainRunTo)         the same, second level
 *
 * RouteFile's area list and the short lists handed out:
 *   FS2 0x0025a7d0 AreaNumberList::clear                     m_areas before its push_backs
 *   FS2 0x00259ef0 AreaNumberList::insert                    m_areas.push_back
 *   FS2 0x0025a7e0 AreaNumberList::~AreaNumberList           m_areas going away
 *   FS2 0x00258f80 std::sort(short)                          std::sort(m_areas): the ROM's
 *                  pivot is a pseudo-random median of three (a gp counter); equal
 *                  shorts cannot be told apart, so the order is the same
 *   FS2 0x0025ac20 std::sortMedian(short)                    inside sort
 *   FS2 0x0025ad10 std::sortPartition(short)                 inside sort (x < pivot)
 *   FS2 0x0025af80 std::sortPartitionEqual(short)            inside sort (!(pivot < x))
 *   FS2 0x0025b1f0 std::sortSmall(short)                     inside sort, under 21: selection
 *   FS2 0x0025a320 vector<short>::insert                     areaCrossings' push_back
 *   FS2 0x0025a750 vector<short>::resize                     areaList's out = list
 *   FS2 0x0025b310 vector<short>::eraseRange                 inside resize
 *
 * collectNames' std::set<const char *> (NameSet):
 *   FS2 0x0025ac00 NameSet::NameSet                          the local set
 *   FS2 0x0025ab40 NameSet::insert                           names.insert
 *   FS2 0x0025b460 NameSet::insertNode                       inside insert
 *   FS2 0x0025a8f0 NameSet::eraseTree                        the set's dtor
 *   FS2 0x0025ab30 NameSet::getValueAllocator                inside eraseTree
 *   FS2 0x0025ab20 NameSet::destroyValue                     inside eraseTree (a pointer: empty)
 *   FS2 0x0025ab10 NameSet::getNodeAllocator                 inside eraseTree
 *   FS2 0x0025ab00 NameSet::freeNode                         inside eraseTree
 *
 * MSL's red-black tree, shared by every set and map of the game (ClipSet,
 * IntSet, ListMap, NameMap, NameSet, VcmNameSet, VcmStateMap, VcmTokenSet):
 *   FS2 0x0025a850 mslTreeIncrement                          ++ on a tree iterator
 *   FS2 0x0025b590 mslTreeBalanceInsert                      every insertNode's rebalance
 *   FS2 0x0025b7c0 mslTreeRotateRight                        inside the rebalance
 *   FS2 0x0025b850 mslTreeRotateLeft                         inside the rebalance
 */

// PC: the ROM's libc strcasecmp (0x127ea0); MSVC has no such name.
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

namespace {

// std::lower_bound over a table keyed by its first field, and the record
// only if it is that key's: 0x258bc0..0x258f10, one instance a table.
template <class Record, class Key>
const Record *findRecord(const std::vector<Record> &table, Key key)
{
    const auto at = std::lower_bound(table.begin(), table.end(), key,
                                     [](const Record &record, Key wanted) { return record.key < wanted; });
    return at != table.end() && at->key == key ? &*at : nullptr;
}

inline bool inSegment(const RouteNode *node, RouteNode *const *segment)
{
    return node == segment[0] || node == segment[1];
}

// sqrt.s takes the absolute value; a rounding error must not make a NaN.
inline float eeSqrt(float value)
{
    return sqrtf(fabsf(value));
}

inline float dot3(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// The nodes of a chain up to the first of the segment's, that one included.
std::vector<RouteNode *> nodesUpTo(const RouteChain &chain, RouteNode *const *segment)
{
    std::vector<RouteNode *> out;
    for (RouteNode *node : chain.nodes) {
        out.push_back(node);
        if (inSegment(node, segment))
            break;
    }
    return out;
}

// The nodes of a chain after the first of the segment's.
std::vector<RouteNode *> nodesAfter(const RouteChain &chain, RouteNode *const *segment)
{
    std::vector<RouteNode *> out;
    size_t at = 0;
    while (at < chain.nodes.size() && !inSegment(chain.nodes[at], segment))
        at++;
    for (at++; at < chain.nodes.size(); at++)
        out.push_back(chain.nodes[at]);
    return out;
}

std::vector<RouteNode *> reversed(const std::vector<RouteNode *> &nodes)
{
    return std::vector<RouteNode *>(nodes.rbegin(), nodes.rend());
}

}  // namespace

// The distance on the map from a point to a node.
// FS2 0x00255040

static float distanceToNode(const float *pos, const RouteNode *node)
{
    return planarDistance(pos, node->position);
}

// From pos to the segment a..b, with the height: side says which end is
// nearest, RouteSide_Along when neither (or exactly at the end).
// FS2 0x00258900

static float distanceToSegment(const float *pos, const RouteNode *a, const RouteNode *b, int *side)
{
    if (side != nullptr)
        *side = RouteSide_None;

    const float along[3] = {b->position[0] - a->position[0], b->position[1] - a->position[1],
                            b->position[2] - a->position[2]};
    const float fromA[3] = {pos[0] - a->position[0], pos[1] - a->position[1], pos[2] - a->position[2]};
    const float pastA = dot3(along, fromA);
    if (pastA <= 0.0f) {
        if (side != nullptr)
            *side = pastA == 0.0f ? RouteSide_Along : RouteSide_First;
        return eeSqrt(dot3(fromA, fromA));
    }

    const float back[3] = {-along[0], -along[1], -along[2]};
    const float fromB[3] = {pos[0] - b->position[0], pos[1] - b->position[1], pos[2] - b->position[2]};
    const float pastB = dot3(back, fromB);
    if (pastB <= 0.0f) {
        if (side != nullptr)
            *side = pastB == 0.0f ? RouteSide_Along : RouteSide_Second;
        return eeSqrt(dot3(fromB, fromB));
    }

    if (side != nullptr)
        *side = RouteSide_Along;
    const float scale = 1.0f / eeSqrt(dot3(along, along));
    const float unit[3] = {along[0] * scale, along[1] * scale, along[2] * scale};
    const float projected = dot3(unit, fromA);
    return eeSqrt(dot3(fromA, fromA) - projected * projected);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00252840

RouteNode *RoutePlayer::endNode(const RouteLeg &step)
{
    switch (step.kind) {
    case RouteLeg_Node:
        return node(step.index);
    case RouteLeg_Chain:
        return m_file->isOrd() ? m_file->m_chains[step.index].nodes.back() : nullptr;
    case RouteLeg_Link:
        return link(step.index)->second;
    default:
        return nullptr;
    }
}

// FS2 0x002529a0

RouteNode *RoutePlayer::startNode(const RouteLeg &step)
{
    switch (step.kind) {
    case RouteLeg_Node:
        return node(step.index);
    case RouteLeg_Chain:
        return m_file->isOrd() ? m_file->m_chains[step.index].nodes.front() : nullptr;
    case RouteLeg_Link:
        return link(step.index)->first;
    default:
        return nullptr;
    }
}

// FS2 0x00252ac0

int RoutePlayer::maxStepCount()
{
    return m_file->isOrd() ? m_file->maxStepCount() : 0;
}

// FS2 0x00252b10

void RoutePlayer::start(int)
{
}

// FS2 0x00252b20
// callcheck: RouteNameList (the host vector assignment)

void RoutePlayer::pathNames(std::vector<const char *> &out)
{
    out = m_file->m_names;
}

// FS2 0x00252b70

void RoutePlayer::reversePath(RoutePath &path)
{
    if (m_file->isOrd())
        m_file->reversePath(path);
}

// FS2 0x00252bc0
// callcheck: lower_bound (std::lower_bound)

bool RoutePlayer::isListedArea(short area)
{
    if (!m_file->isOrd())
        return false;
    return std::binary_search(m_file->m_areas.begin(), m_file->m_areas.end(), area);
}

// FS2 0x00252c80

float RoutePlayer::chainLength(short index)
{
    return m_file->isOrd() ? m_file->m_chains[index].length : kRouteNoWay;
}

// FS2 0x00252cd0

RouteNode *RoutePlayer::chainNode(short index, int at)
{
    return m_file->isOrd() ? m_file->m_chains[index].nodes[at] : nullptr;
}

// FS2 0x00252d30

int RoutePlayer::chainNodeCount(short index)
{
    return m_file->isOrd() ? (int)m_file->m_chains[index].nodes.size() : 0;
}

// The ROM indexes with the short as it is, so a leg with kRouteLinkReversed
// would land before the table. No file looked at has one (open-items.md).
// FS2 0x00252d80

const RouteSpan *RoutePlayer::link(short index)
{
    return m_file->isOrd() ? &m_file->m_links[index] : nullptr;
}

// FS2 0x00252dd0

const RouteSpan *RoutePlayer::gate(short index)
{
    return m_file->isOrd() ? &m_file->m_gates[index] : nullptr;
}

// FS2 0x00252e20

RouteNode *RoutePlayer::node(short index)
{
    return m_file->isOrd() ? &m_file->m_nodes[index] : nullptr;
}

// FS2 0x00252e70

int RoutePlayer::pathSetIndex(const std::vector<const char *> &names)
{
    return m_file->isOrd() ? m_file->pathSetIndex(names) : -1;
}

// FS2 0x00252ec0

void RoutePlayer::areaList(std::vector<short> &out, short area)
{
    if (m_file->isOrd())
        m_file->areaList(out, area);
}

// FS2 0x00252f10

void RoutePlayer::areaCrossings(std::vector<short> &out, short fromArea, short toArea)
{
    if (m_file->isOrd())
        m_file->areaCrossings(out, fromArea, toArea);
}

// FS2 0x00252f70

float RoutePlayer::areaPath(RoutePath &out, const float *from, const float *to, short area)
{
    return m_file->isOrd() ? m_file->m_search->areaPath(out, from, to, area) : kRouteNoWay;
}

// FS2 0x00252fd0

float RoutePlayer::wayToCrossing(RoutePath &out, const float *pos, short area, short crossing)
{
    return m_file->isOrd() ? m_file->wayToCrossing(out, pos, area, crossing) : kRouteNoWay;
}

// FS2 0x00253030

float RoutePlayer::crossingPath(RoutePath &out, short fromArea, short toArea, short crossing, int pathSet)
{
    return m_file->isOrd() ? m_file->crossingPath(out, fromArea, toArea, crossing, pathSet) : kRouteNoWay;
}

// FS2 0x002530a0
// callcheck: operator_delete, ~AreaNumberList (the members and the delete of m_file)

RoutePlayer::~RoutePlayer()
{
    delete m_file;
}

// FS2 0x00253180
// callcheck: operator_new (new RouteFile)

RoutePlayer::RoutePlayer(void *rute)
{
    m_file = new RouteFile(this, rute);
    m_nodeTable = m_file->isOrd() ? (void *)m_file->m_nodes : rute;
}

// ──────────────────────────────────────────────────────────────────────
// The chains to take from one chain end to another, or null.

const std::vector<short> *RouteAreaSearch::runChains(RouteNode *from, RouteNode *to)
{
    const RouteChainRunFrom *row = findRecord(m_file->m_chainRuns, from);
    if (row == nullptr)
        return nullptr;
    const RouteChainRunTo *run = findRecord(row->to, to);
    return run != nullptr ? &run->chains : nullptr;
}

// Those chains as a path of chain legs.
// FS2 0x00253210
// callcheck: lower_bound, lower_bound, reserve, insert (std::lower_bound, the host vector)

float RouteAreaSearch::chainRun(RoutePath &out, RouteNode *from, RouteNode *to)
{
    const std::vector<short> *chains = runChains(from, to);
    if (chains == nullptr)
        return kRouteNoWay;

    out.length = 0.0f;
    out.steps.reserve(chains->size());
    for (short index : *chains) {
        out.length += m_file->m_chains[index].length;
        RouteLeg leg;
        leg.kind = RouteLeg_Chain;
        leg.index = index;
        out.steps.push_back(leg);
    }
    return out.length;
}

// From from to to, two points of one area, along its chains.
// The length it writes counts a stored run twice when it goes through one,
// and what it answers then is the run's alone; both are the ROM's. The
// callers read out.length.
// FS2 0x00253420
// callcheck: clear, WordVector, ~RouteLegList, RouteLegList, insert, operator!=, operator*, operator--, operator*, operator=, operator++, back_insert_iterator, reserve, insert, operator=, FUN_00228610, operator!=, operator*, operator--, operator*, operator=, operator++, back_insert_iterator, operator++, ~vector, chainRun (the host vector and its reversed copies)

float RouteAreaSearch::areaPath(RoutePath &out, const float *from, const float *to, short area)
{
    out.length = 0.0f;
    out.steps.clear();

    RouteNode *fromSegment[2] = {nullptr, nullptr};
    RouteNode *toSegment[2] = {nullptr, nullptr};
    int fromSide;
    int toSide;
    nearestSegment(fromSegment, from, area, &fromSide);
    nearestSegment(toSegment, to, area, &toSide);
    if (fromSide == RouteSide_None || toSide == RouteSide_None)
        return kRouteNoWay;

    if (fromSegment[0] == toSegment[0] && fromSegment[1] == toSegment[1])
        return planarDistance(from, to);

    const short fromIndex = chainOf(fromSegment[0], fromSegment[1]);
    const short toIndex = chainOf(toSegment[0], toSegment[1]);
    const RouteChain &fromChain = m_file->m_chains[fromIndex];
    const RouteChain &toChain = m_file->m_chains[toIndex];

    // Meant to be "the same chain walked the other way". It compares the
    // first chain with itself (t2 twice at 0x253598), so it is true for two
    // chains of a size when the first reads the same both ways.
    bool sameChain = true;
    if (fromIndex != toIndex) {
        const int count = (int)fromChain.nodes.size();
        if (count == (int)toChain.nodes.size()) {
            for (int i = 0; i < count; i++) {
                if (fromChain.nodes[i] != fromChain.nodes[count - i - 1]) {
                    sameChain = false;
                    break;
                }
            }
        } else {
            sameChain = false;
        }
    }

    if ((fromIndex & ~kRouteLinkReversed) == (toIndex & ~kRouteLinkReversed) || sameChain) {
        const int count = (int)fromChain.nodes.size();
        int at = 0;
        bool backwards = false;
        RouteNode *const *until = toSegment;
        for (; at < count; at++) {
            RouteNode *node = fromChain.nodes[at];
            if (inSegment(node, fromSegment))
                break;
            if (inSegment(node, toSegment)) {
                backwards = true;
                until = fromSegment;
                break;
            }
        }

        RoutePath way;
        for (at++; at < count; at++) {
            RouteNode *node = fromChain.nodes[at];
            if (!way.steps.empty())
                way.length += distanceToNode(node->position, &m_file->m_nodes[way.steps.back().index]);
            RouteLeg leg;
            leg.kind = RouteLeg_Node;
            leg.index = nodeIndex(node);
            way.steps.push_back(leg);
            if (inSegment(node, until))
                break;
        }
        if (backwards)
            m_file->reversePath(way);
        out.length = way.length;
        out.steps = way.steps;
        return out.length;
    }

    RouteNode *const fromFront = fromChain.nodes.front();
    RouteNode *const fromBack = fromChain.nodes.back();
    RouteNode *const toFront = toChain.nodes.front();
    RouteNode *const toBack = toChain.nodes.back();

    // Chains that share an end meet there.
    std::vector<RouteNode *> leaving;
    std::vector<RouteNode *> arriving;
    bool meet = true;
    if (fromFront == toFront) {
        leaving = reversed(nodesUpTo(fromChain, fromSegment));
        arriving = nodesUpTo(toChain, toSegment);
    } else if (fromFront == toBack) {
        leaving = reversed(nodesUpTo(fromChain, fromSegment));
        arriving = reversed(nodesAfter(toChain, toSegment));
    } else if (fromBack == toFront) {
        leaving = nodesAfter(fromChain, fromSegment);
        arriving = nodesUpTo(toChain, toSegment);
    } else if (fromBack == toBack) {
        leaving = nodesAfter(fromChain, fromSegment);
        arriving = reversed(nodesAfter(toChain, toSegment));
    } else {
        meet = false;
    }

    if (meet) {
        out.length = 0.0f;
        out.steps.reserve(leaving.size() + arriving.size());
        RouteNode *previous = nullptr;
        const auto walkTo = [&](RouteNode *node) {
            if (previous != nullptr)
                out.length += distanceToNode(node->position, previous);
            previous = node;
            RouteLeg leg;
            leg.kind = RouteLeg_Node;
            leg.index = nodeIndex(node);
            out.steps.push_back(leg);
        };
        for (RouteNode *node : leaving)
            walkTo(node);
        for (size_t i = 1; i < arriving.size(); i++)
            walkTo(arriving[i]);
        return out.length;
    }

    // Otherwise by a stored run, the cheapest of the four pairs of ends.
    RouteNode *const ends[4][2] = {{fromFront, toFront}, {fromFront, toBack}, {fromBack, toFront}, {fromBack, toBack}};
    RoutePath runs[4];
    float lengths[4];
    lengths[0] = chainRun(runs[0], ends[0][0], ends[0][1]);
    float best = lengths[0];
    int bestRun = 0;
    for (int i = 1; i < 4; i++) {
        lengths[i] = chainRun(runs[i], ends[i][0], ends[i][1]);
        if (best < 0.0f || lengths[i] < best) {
            best = lengths[i];
            bestRun = i;
        }
    }
    if (best < 0.0f)
        return kRouteNoWay;

    out.length = best;
    const RoutePath &run = runs[bestRun];

    const auto nodeLeg = [&](RouteNode *node) {
        RouteLeg leg;
        leg.kind = RouteLeg_Node;
        leg.index = nodeIndex(node);
        return leg;
    };
    // The nodes on the way from the chain's front to the segment, without
    // the front.
    const auto legsFromFront = [&](const RouteChain &chain, RouteNode *const *segment, std::vector<RouteLeg> &legs) {
        RouteNode *previous = nullptr;
        for (size_t i = 0; i < chain.nodes.size(); i++) {
            RouteNode *node = chain.nodes[i];
            if (i != 0) {
                legs.push_back(nodeLeg(node));
                out.length += distanceToNode(node->position, previous);
            }
            if (inSegment(node, segment))
                break;
            previous = node;
        }
    };
    // The nodes after the segment, to the chain's back.
    const auto legsToBack = [&](const RouteChain &chain, RouteNode *const *segment, std::vector<RouteLeg> &legs) {
        bool past = false;
        RouteNode *previous = nullptr;
        for (RouteNode *node : chain.nodes) {
            if (past) {
                if (previous != nullptr)
                    out.length += distanceToNode(node->position, previous);
                legs.push_back(nodeLeg(node));
                previous = node;
            } else if (inSegment(node, segment)) {
                past = true;
            }
        }
    };

    if (fromFront == ends[bestRun][0]) {
        std::vector<RouteLeg> head;
        legsFromFront(fromChain, fromSegment, head);
        out.steps.insert(out.steps.end(), head.rbegin(), head.rend());
    } else {
        legsToBack(fromChain, fromSegment, out.steps);
        if (!out.steps.empty())
            out.steps.pop_back();
    }
    out.steps.insert(out.steps.end(), run.steps.begin(), run.steps.end());
    out.length += lengths[bestRun];

    if (toFront == ends[bestRun][1]) {
        legsFromFront(toChain, toSegment, out.steps);
    } else {
        std::vector<RouteLeg> tail;
        legsToBack(toChain, toSegment, tail);
        out.steps.insert(out.steps.end(), tail.rbegin(), tail.rend());
    }
    return best;
}

// The node's index in the file's table.
// FS2 0x002550a0

short RouteAreaSearch::nodeIndex(const RouteNode *node)
{
    return (short)(node - m_file->m_nodes);
}

// The chain the segment first..second lies on: the stored run between them,
// or the first chain of first's area with whichever had no row.
// FS2 0x002550d0
// callcheck: lower_bound, lower_bound, lower_bound (std::lower_bound)

short RouteAreaSearch::chainOf(RouteNode *first, RouteNode *second)
{
    RouteNode *wanted = first;
    const RouteChainRunFrom *row = findRecord(m_file->m_chainRuns, first);
    if (row != nullptr) {
        wanted = second;
        const RouteChainRunTo *run = findRecord(row->to, second);
        if (run != nullptr)
            return run->chains.empty() ? (short)0 : run->chains.front();
    }

    const RouteChainList *list = findRecord(m_file->m_areaChains, first->crossing);
    if (list == nullptr)    // the ROM reads past the table
        return 0;
    for (short index : list->chains) {
        const std::vector<RouteNode *> &nodes = m_file->m_chains[index].nodes;
        if (std::find(nodes.begin(), nodes.end(), wanted) != nodes.end())
            return index;
    }
    return 0;
}

// From a node to the crossing, along the chains of the node's area. Only a
// node at the crossing itself answers without setting out.length.
// FS2 0x00255330
// callcheck: lower_bound, lower_bound, lower_bound, reserve, insert, resize, firstLeg, legsLength (std::lower_bound, the host vector)

float RouteAreaSearch::wayAlongChains(RoutePath &out, RouteNode *node, short crossing)
{
    const short area = node->crossing;
    const RouteChainList *list = findRecord(m_file->m_areaChains, area);
    if (list == nullptr)
        return kRouteNoWay;

    const RouteLeg *firstLeg = m_file->firstLeg(area, crossing);
    if (firstLeg == nullptr)
        return kRouteNoWay;
    RouteNode *const junction = m_file->m_links[firstLeg->index].first;

    const auto nodeLeg = [&](RouteNode *at) {
        RouteLeg leg;
        leg.kind = RouteLeg_Node;
        leg.index = nodeIndex(at);
        return leg;
    };
    const auto chainsLength = [&](const std::vector<short> &chains) {
        float length = 0.0f;
        for (short index : chains)
            length += m_file->m_chains[index].length;
        return length;
    };

    if (junction == node) {
        out.steps.push_back(nodeLeg(node));
        return kRouteCrossingLength;
    }

    if (findRecord(m_file->m_chainRuns, node) != nullptr) {
        // The node is a chain's end: a stored run, the first chain as nodes.
        const std::vector<short> *chains = runChains(node, junction);
        if (chains == nullptr || chains->empty())   // the ROM reads past the table
            return kRouteNoWay;
        const RouteChain &first = m_file->m_chains[chains->front()];
        out.length = first.length;
        out.steps.reserve(first.nodes.size() + chains->size() - 1);
        for (RouteNode *at : first.nodes)
            out.steps.push_back(nodeLeg(at));
        for (size_t i = 1; i < chains->size(); i++) {
            RouteLeg leg;
            leg.kind = RouteLeg_Chain;
            leg.index = (*chains)[i];
            out.steps.push_back(leg);
            out.length += m_file->m_chains[leg.index].length;
        }
        return out.length;
    }

    // Inside a chain. The chain itself may end at the junction.
    const RouteChain *chain = nullptr;
    int at = 0;
    for (short index : list->chains) {
        const std::vector<RouteNode *> &nodes = m_file->m_chains[index].nodes;
        const auto found = std::find(nodes.begin(), nodes.end(), node);
        if (found == nodes.end())
            continue;
        chain = &m_file->m_chains[index];
        at = (int)(found - nodes.begin());
        break;
    }
    if (chain == nullptr)
        return kRouteNoWay;

    const auto walkToFront = [&]() {
        out.steps.resize(at + 1);
        for (int i = 0; i <= at; i++)
            out.steps[i] = nodeLeg(chain->nodes[at - i]);
    };
    const auto walkToBack = [&]() {
        const int count = (int)chain->nodes.size() - at;
        out.steps.resize(count);
        for (int i = 0; i < count; i++)
            out.steps[i] = nodeLeg(chain->nodes[at + i]);
    };

    if (chain->nodes.front() == junction) {
        walkToFront();
        out.length = m_file->legsLength(out.steps);
        return out.length;
    }
    if (chain->nodes.back() == junction) {
        walkToBack();
        out.length = m_file->legsLength(out.steps);
        return out.length;
    }

    // Out of either end, then a stored run: the shorter run decides, not
    // what it costs to reach its end. No row reads past the table in the
    // ROM; here it is an empty run.
    static const std::vector<short> kNoChains;
    const std::vector<short> *viaFront = runChains(chain->nodes.front(), junction);
    const std::vector<short> *viaBack = runChains(chain->nodes.back(), junction);
    if (viaFront == nullptr)
        viaFront = &kNoChains;
    if (viaBack == nullptr)
        viaBack = &kNoChains;
    const float frontLength = chainsLength(*viaFront);
    const float backLength = chainsLength(*viaBack);

    const std::vector<short> *chains;
    if (frontLength < backLength) {
        walkToFront();
        out.length = frontLength + m_file->legsLength(out.steps);
        chains = viaFront;
    } else {
        walkToBack();
        out.length = backLength + m_file->legsLength(out.steps);
        chains = viaBack;
    }
    for (short index : *chains) {
        RouteLeg leg;
        leg.kind = RouteLeg_Chain;
        leg.index = index;
        out.steps.push_back(leg);
    }
    return out.length;
}

// From a segment to the crossing: from its first node, without that node
// when the way runs over the second anyway.
// FS2 0x002569e0
// callcheck: erase (the host vector erase)

float RouteAreaSearch::wayFromSegment(RoutePath &out, RouteNode *const *segment, short crossing)
{
    const short second = nodeIndex(segment[1]);
    wayAlongChains(out, segment[0], crossing);

    bool found = false;
    for (const RouteLeg &leg : out.steps) {
        if (leg.kind == RouteLeg_Node && leg.index == second) {
            found = true;
            break;
        }
    }
    if (found && out.steps.size() != 1) {
        out.length -= planarDistance(segment[0]->position, segment[1]->position);
        out.steps.erase(out.steps.begin());
    }
    return out.length;
}

// From a point of the area to the crossing.
// FS2 0x00256b60
// callcheck: insert (the host vector)

float RouteAreaSearch::wayFromPoint(RoutePath &out, const float *pos, short area, short crossing)
{
    RouteNode *segment[2] = {nullptr, nullptr};
    int side;
    const float distance = nearestSegment(segment, pos, area, &side);
    if (segment[0] == nullptr)
        return 0.0f;

    wayFromSegment(out, segment, crossing);
    if (side == RouteSide_Along)
        return out.length;

    // Past an end of the segment: that end comes first.
    RouteLeg end;
    end.kind = RouteLeg_Node;
    if (side == RouteSide_First)
        end.index = nodeIndex(segment[0]);
    else if (side == RouteSide_Second)
        end.index = nodeIndex(segment[1]);
    else
        rxSysPrintf("そんな馬鹿な\n");

    bool found = false;
    for (const RouteLeg &leg : out.steps) {
        if (leg.kind == end.kind && leg.index == end.index) {
            found = true;
            break;
        }
    }
    if (!found) {
        out.steps.insert(out.steps.begin(), end);
        out.length += planarDistance(segment[0]->position, segment[1]->position);
    }
    out.length += distance;
    return out.length;
}

// The nearest segment of the area's chains to pos, its two nodes in file
// order, and which end of it (side, may be null). -1 when there is none.
// FS2 0x00256db0
// callcheck: lower_bound, distanceToSegment (std::lower_bound, distanceToSegment)

float RouteAreaSearch::nearestSegment(RouteNode **segment, const float *pos, short area, int *side)
{
    segment[0] = nullptr;
    segment[1] = nullptr;
    if (side != nullptr)
        *side = RouteSide_None;

    const RouteChainList *list = findRecord(m_file->m_areaChains, area);
    if (list == nullptr)
        return kRouteNoWay;

    float best = kRouteNoWay;
    for (short index : list->chains) {
        RouteNode *previous = nullptr;
        for (RouteNode *node : m_file->m_chains[index].nodes) {
            if (previous != nullptr) {
                int where;
                const float distance = distanceToSegment(pos, node, previous, &where);
                if (best < 0.0f || !(distance < 0.0f || best <= distance)) {
                    segment[0] = node;
                    segment[1] = previous;
                    best = distance;
                    if (side != nullptr)
                        *side = where;
                }
            }
            previous = node;
        }
    }

    if (segment[0] != nullptr && segment[1] != nullptr && segment[1] < segment[0]) {
        std::swap(segment[0], segment[1]);
        if (side != nullptr) {
            if (*side == RouteSide_First)
                *side = RouteSide_Second;
            else if (*side == RouteSide_Second)
                *side = RouteSide_First;
        }
    }
    return best;
}

// ──────────────────────────────────────────────────────────────────────
// Which set of the file is exactly these names (any case): every name of
// the set among them, and as many. From 1; 0 none, -1 no sets in the file.
// FS2 0x00257020

int RouteFile::pathSetIndex(const std::vector<const char *> &names)
{
    if (!m_hasSets)
        return -1;

    int index = 1;
    for (const std::vector<const char *> &set : m_sets) {
        if (names.size() == set.size()) {
            bool all = true;
            for (const char *name : set) {
                bool found = false;
                for (const char *wanted : names) {
                    if (strcasecmp(wanted, name) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    all = false;
                    break;
                }
            }
            if (all)
                return index;
        }
        index++;
    }
    return 0;
}

// Every name of every set once: the ROM's tree is ordered by the pointers,
// so by where the strings lie in the file, and so is this one.
// FS2 0x002571e0
// callcheck: NameSet, insert, insert, mslTreeIncrement, eraseTree (the host set and vector)

void RouteFile::collectNames()
{
    if (!m_hasSets)
        return;

    std::set<const char *> names;
    for (const std::vector<const char *> &set : m_sets)
        names.insert(set.begin(), set.end());
    m_names.assign(names.begin(), names.end());
}

// The path walked the other way, its links and chains made nodes.
// FS2 0x00257360
// callcheck: initRange, clear, insert, ~vector (the host vector)

void RouteFile::reversePath(RoutePath &path)
{
    const std::vector<RouteLeg> steps = path.steps;
    path.steps.clear();

    const auto pushNode = [&](const RouteNode *node) {
        RouteLeg leg;
        leg.kind = RouteLeg_Node;
        leg.index = (short)(node - m_nodes);
        path.steps.push_back(leg);
    };

    for (auto step = steps.rbegin(); step != steps.rend(); ++step) {
        if (step->kind == RouteLeg_Node || step->kind == RouteLeg_Gate) {
            path.steps.push_back(*step);
        } else if (step->kind == RouteLeg_Link) {
            const RouteSpan &link = m_links[step->index];
            pushNode(link.second);
            pushNode(link.first);
        } else if (step->kind == RouteLeg_Chain) {
            const std::vector<RouteNode *> &nodes = m_chains[step->index].nodes;
            for (auto node = nodes.rbegin(); node != nodes.rend(); ++node)
                pushNode(*node);
        }
    }
}

// From a point of the area to the crossing, by what the area's own stored
// path over it starts with: a gate is right there, a link's start without
// a corner is walked to straight, the rest goes along the chains.
// FS2 0x00257680
// callcheck: clear, insert (the host vector)

float RouteFile::wayToCrossing(RoutePath &out, const float *pos, short area, short crossing)
{
    out.length = 0.0f;
    out.steps.clear();

    const RoutePath *way = findWay(area, crossing, crossing, 0);
    if (way == nullptr || way->steps.empty())
        return kRouteNoWay;

    const RouteLeg &first = way->steps.front();
    if (first.kind == RouteLeg_Chain) {
        m_search->wayFromPoint(out, pos, area, crossing);
    } else if (first.kind == RouteLeg_Link) {
        const RouteSpan &link = m_links[first.index];
        RouteNode *node = (first.index & kRouteLinkReversed) == 0 ? link.first : link.second;
        if ((node->flags & RouteNodeFlag_Open) == 0) {
            m_search->wayFromPoint(out, pos, area, crossing);
        } else {
            out.length = planarDistance(pos, node->position);
            RouteLeg leg;
            leg.kind = RouteLeg_Node;
            leg.index = (short)(node - m_nodes);
            out.steps.push_back(leg);
        }
    } else if (first.kind == RouteLeg_Gate) {
        out.length = kRouteCrossingLength;
        out.steps.push_back(first);
    }
    return out.length;
}

// The stored path, into out.
// FS2 0x002578f0
// callcheck: resize, clear (the host vector)

float RouteFile::crossingPath(RoutePath &out, short fromArea, short toArea, short crossing, int pathSet)
{
    const RoutePath *way = findWay(fromArea, toArea, crossing, pathSet);
    if (way == nullptr) {
        out.steps.clear();
        out.length = kRouteNoWay;
        return 0.0f;
    }
    out.length = way->length;
    out.steps = way->steps;
    return out.length;
}

// By the set's table first; a set with no entry falls to the file's own, and
// one whose entry has a negative length closes the way.
// FS2 0x002579d0

const RoutePath *RouteFile::findWay(short fromArea, short toArea, short crossing, int pathSet)
{
    if (pathSet == 0)
        return findWayIn(fromArea, toArea, crossing, m_crossings);

    const RoutePath *way = findWayIn(fromArea, toArea, crossing, m_setCrossings[pathSet - 1]);
    if (way == nullptr)
        return findWayIn(fromArea, toArea, crossing, m_crossings);
    return way->length < 0.0f ? nullptr : way;
}

// FS2 0x00257ad0
// callcheck: lower_bound, lower_bound, lower_bound (std::lower_bound)

const RoutePath *RouteFile::findWayIn(short fromArea, short toArea, short crossing, const RouteCrossingTable &table)
{
    const RouteCrossingFrom *from = findRecord(table, fromArea);
    if (from == nullptr)
        return nullptr;
    const RouteCrossingTo *to = findRecord(from->to, toArea);
    if (to == nullptr)
        return nullptr;
    const RouteWayRecord *way = findRecord(to->ways, crossing);
    return way != nullptr ? &m_paths[way->path] : nullptr;
}

// FS2 0x00257cb0
// callcheck: lower_bound, resize (std::lower_bound, the host vector)

void RouteFile::areaList(std::vector<short> &out, short area)
{
    const RouteChainList *list = findRecord(m_areaLists, area);
    if (list != nullptr)
        out = list->chains;
}

// Adds to out what it finds.
// FS2 0x00257db0
// callcheck: lower_bound, lower_bound, insert (std::lower_bound, the host vector)

void RouteFile::areaCrossings(std::vector<short> &out, short fromArea, short toArea)
{
    const RouteCrossingFrom *from = findRecord(m_crossings, fromArea);
    if (from == nullptr)
        return;
    const RouteCrossingTo *to = findRecord(from->to, toArea);
    if (to == nullptr)
        return;
    for (const RouteWayRecord &way : to->ways)
        out.push_back(way.key);
}

// The first leg of the area's stored way out by the crossing.
// FS2 0x00257f50

const RouteLeg *RouteFile::firstLeg(short area, short crossing)
{
    const RoutePath *way = findWayIn(area, crossing, crossing, m_crossings);
    if (way == nullptr || way->steps.empty())
        return nullptr;
    return &way->steps.front();
}

// FS2 0x00257fa0
// callcheck: operator_new, clear, insert, sort (new, the host vector, std::sort)

RouteFile::RouteFile(RoutePlayer *owner, void *file)
    : m_bytes((u_char *)file), m_owner(owner)
{
    if (!isOrd())
        return;

    buildTables();
    collectNames();
    m_search = new RouteAreaSearch(this);

    const u_int count = *(const u_int *)(m_bytes + ((const RuteHeader *)m_bytes)->nodes);
    for (u_int i = 0; i < count; i++) {
        if ((m_nodes[i].flags & RouteNodeFlag_Open) != 0)
            m_areas.push_back(m_nodes[i].crossing);
    }
    std::sort(m_areas.begin(), m_areas.end());
}

RouteFile::~RouteFile()
{
    delete m_search;
}

bool RouteFile::isOrd() const
{
    return (*(const u_int *)m_bytes | kRuteMagicCase) == kRuteMagic;
}

// The most steps a stored path comes to once its links and chains are nodes.
// A leg of kind node counts for nothing, as in the ROM.
// FS2 0x00258110

int RouteFile::maxStepCount()
{
    int most = 0;
    for (const RoutePath &path : m_paths) {
        int steps = 0;
        for (const RouteLeg &leg : path.steps) {
            if (leg.kind == RouteLeg_Chain)
                steps += (int)m_chains[leg.index].nodes.size();
            else if (leg.kind == RouteLeg_Gate)
                steps += 1;
            else if (leg.kind == RouteLeg_Link)
                steps += 2;
        }
        most = std::max(most, steps);
    }
    return most;
}

// What 0x258260 relocates, read into host records: see the header's comment.
// FS2 0x00258260

void RouteFile::buildTables()
{
    const u_char *const bytes = m_bytes;
    const RuteHeader &header = *(const RuteHeader *)bytes;

    const auto wordAt = [bytes](u_int offset) { return *(const u_int *)(bytes + offset); };
    const auto shortAt = [bytes](u_int offset) { return *(const short *)(bytes + offset); };
    const auto nodeAt = [this](u_int offset) { return (RouteNode *)(m_bytes + offset); };
    const auto shortList = [&](u_int offset) {
        const short *first = (const short *)(bytes + offset + 4);
        return std::vector<short>(first, first + wordAt(offset));
    };
    const auto chainLists = [&](u_int offset) {
        std::vector<RouteChainList> out(wordAt(offset));
        for (u_int i = 0; i < out.size(); i++) {
            const u_int record = offset + 4 + i * 8;
            out[i].key = shortAt(record);
            out[i].chains = shortList(wordAt(record + 4));
        }
        return out;
    };
    const auto crossingTable = [&](u_int offset) {
        RouteCrossingTable out(wordAt(offset));
        for (u_int i = 0; i < out.size(); i++) {
            const u_int record = offset + 4 + i * 8;
            const u_int rows = wordAt(record + 4);
            out[i].key = shortAt(record);
            out[i].to.resize(wordAt(rows));
            for (u_int j = 0; j < out[i].to.size(); j++) {
                const u_int row = rows + 4 + j * 8;
                const u_int ways = wordAt(row + 4);
                const RouteWayRecord *first = (const RouteWayRecord *)(bytes + ways + 4);
                out[i].to[j].key = shortAt(row);
                out[i].to[j].ways.assign(first, first + wordAt(ways));
            }
        }
        return out;
    };

    m_nodes = nodeAt(header.nodes + 8);

    m_gates.resize(wordAt(header.gates));
    for (u_int i = 0; i < m_gates.size(); i++) {
        const u_int record = header.gates + 4 + i * 0xc;
        m_gates[i].first = nodeAt(wordAt(record));
        m_gates[i].second = nodeAt(wordAt(record + 4));
        m_gates[i]._8 = shortAt(record + 8);
    }

    m_links.resize(wordAt(header.links));
    for (u_int i = 0; i < m_links.size(); i++) {
        const u_int record = header.links + 4 + i * 0xc;
        m_links[i].first = nodeAt(wordAt(record));
        m_links[i].second = nodeAt(wordAt(record + 4));
        m_links[i].third = nodeAt(wordAt(record + 8));
    }

    m_chains.resize(wordAt(header.chains));
    for (u_int i = 0; i < m_chains.size(); i++) {
        const u_int record = header.chains + 4 + i * 8;
        const u_int nodes = wordAt(record + 4);
        m_chains[i].length = *(const float *)(bytes + record);
        m_chains[i].nodes.resize(wordAt(nodes));
        for (u_int j = 0; j < m_chains[i].nodes.size(); j++)
            m_chains[i].nodes[j] = nodeAt(wordAt(nodes + 4 + j * 4));
    }

    m_crossings = crossingTable(header.crossings);

    m_paths.resize(wordAt(header.paths));
    for (u_int i = 0; i < m_paths.size(); i++) {
        const u_int record = header.paths + 4 + i * 8;
        const u_int legs = wordAt(record + 4);
        const RouteLeg *first = (const RouteLeg *)(bytes + legs + 4);
        m_paths[i].length = *(const float *)(bytes + record);
        m_paths[i].steps.assign(first, first + wordAt(legs));
    }

    m_areaChains = chainLists(header.areaChains);

    m_chainRuns.resize(wordAt(header.chainRuns));
    for (u_int i = 0; i < m_chainRuns.size(); i++) {
        const u_int record = header.chainRuns + 4 + i * 8;
        const u_int rows = wordAt(record + 4);
        m_chainRuns[i].key = nodeAt(wordAt(record));
        m_chainRuns[i].to.resize(wordAt(rows));
        for (u_int j = 0; j < m_chainRuns[i].to.size(); j++) {
            const u_int row = rows + 4 + j * 8;
            m_chainRuns[i].to[j].key = nodeAt(wordAt(row));
            m_chainRuns[i].to[j].chains = shortList(wordAt(row + 4));
        }
    }

    m_hasSets = header.sets != 0;
    if (!m_hasSets)
        return;

    m_sets.resize(wordAt(header.sets));
    for (u_int i = 0; i < m_sets.size(); i++) {
        const u_int names = wordAt(header.sets + 4 + i * 4);
        m_sets[i].resize(wordAt(names));
        for (u_int j = 0; j < m_sets[i].size(); j++)
            m_sets[i][j] = (const char *)(bytes + wordAt(names + 4 + j * 4));
    }

    m_setCrossings.resize(wordAt(header.setCrossings));
    for (u_int i = 0; i < m_setCrossings.size(); i++)
        m_setCrossings[i] = crossingTable(wordAt(header.setCrossings + 4 + i * 4));

    if (header.areaLists != 0)
        m_areaLists = chainLists(header.areaLists);
}

// How far a path of nodes runs, on the map.
// FS2 0x00259230

float RouteFile::legsLength(const std::vector<RouteLeg> &legs)
{
    float length = 0.0f;
    const RouteNode *previous = nullptr;
    for (const RouteLeg &leg : legs) {
        const RouteNode *node = &m_nodes[leg.index];
        if (previous != nullptr)
            length += planarDistance(previous->position, node->position);
        previous = node;
    }
    return length;
}
