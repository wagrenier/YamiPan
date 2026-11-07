/* ========================================================================
 *  game/routefilter.cpp -- the path search of a character's route.
 *  See game/routefilter.h.
 * ======================================================================== */
#include "game/routefilter.h"

#include <math.h>
#include <string.h>

#include "game/characterground.h"
#include "game/gamehooks.h"
#include "game/playermotion.h"
#include "game/routeplayer.h"
#include "game/stagescene.h"
#include "game/vu0math.h"

namespace {

// What the two inside tests compare signs of. Not a cross product: both
// tests multiply x by x and y by y (mula.s / msub.s at 0x2404a8), and the
// data lives with that.
inline float insideTerm(float ax, float ay, float bx, float by)
{
    return ax * bx - ay * by;
}

// Which side of the line a..b p is on.
inline float sideOfLine(const float *a, const float *b, const float *p)
{
    return (p[0] - a[0]) * (b[1] - a[1]) + (p[1] - a[1]) * -(b[0] - a[0]);
}

// The turn from a to b, seen from p: negative clockwise.
inline float turnAbout(const float *p, const float *a, const float *b)
{
    return (a[0] - p[0]) * (b[1] - p[1]) + (a[1] - p[1]) * -(b[0] - p[0]);
}

inline float lengthOf(const float *v)
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

inline float dotOf(const float *a, const float *b)
{
    return a[2] * b[2] + a[1] * b[1] + a[0] * b[0];
}

// Is pos inside the four corners, in order? They may cross: the two gates
// of a corridor need not be wound the same way.
// FS2 0x002400c0

bool isInsideQuad(RouteNode *const *corners, const float *pos)
{
    const float *c0 = corners[0]->position;
    const float *c1 = corners[1]->position;
    const float *c2 = corners[2]->position;
    const float *c3 = corners[3]->position;
    const float edge0[2] = {c1[0] - c0[0], c1[1] - c0[1]};
    const float edge1[2] = {c2[0] - c1[0], c2[1] - c1[1]};
    const float edge2[2] = {c3[0] - c2[0], c3[1] - c2[1]};
    const float edge3[2] = {c0[0] - c3[0], c0[1] - c3[1]};
    const float to0[2] = {pos[0] - c0[0], pos[1] - c0[1]};
    const float to1[2] = {pos[0] - c1[0], pos[1] - c1[1]};
    const float to2[2] = {pos[0] - c2[0], pos[1] - c2[1]};
    const float to3[2] = {pos[0] - c3[0], pos[1] - c3[1]};

    const bool side0 = 0.0f < insideTerm(edge0[0], edge0[1], to0[0], to0[1]);
    const bool side1 = 0.0f < insideTerm(edge1[0], edge1[1], to1[0], to1[1]);
    const bool side2 = 0.0f < insideTerm(edge2[0], edge2[1], to2[0], to2[1]);
    if ((insideTerm(edge0[0], edge0[1], edge1[0], edge1[1]) < 0.0f)
        == (insideTerm(edge1[0], edge1[1], edge2[0], edge2[1]) < 0.0f)) {
        if (side0 != side1 || side0 != side2)
            return false;
        return side0 == (0.0f < insideTerm(edge3[0], edge3[1], to3[0], to3[1]));
    }
    if (side0 == side1)
        return side0 != (0.0f < insideTerm(c0[0] - c2[0], c0[1] - c2[1], to2[0], to2[1]));
    if (side0 == side2)
        return false;
    return side0 != (0.0f < insideTerm(c1[0] - c3[0], c1[1] - c3[1], to3[0], to3[1]));
}

// Is pos inside the three corners?
// FS2 0x002403f0

bool isInsideTriangle(RouteNode *const *corners, const float *pos)
{
    const float *c0 = corners[0]->position;
    const float *c1 = corners[1]->position;
    const float *c2 = corners[2]->position;
    const bool side = 0.0f < insideTerm(c1[0] - c0[0], c1[1] - c0[1], pos[0] - c0[0], pos[1] - c0[1]);
    if (side != (0.0f < insideTerm(c2[0] - c1[0], c2[1] - c1[1], pos[0] - c1[0], pos[1] - c1[1])))
        return false;
    return side == (0.0f < insideTerm(c0[0] - c2[0], c0[1] - c2[1], pos[0] - c2[0], pos[1] - c2[1]));
}

// The funnel: the shortest line from from through the gates to to, turning
// only at their corners. The corners go on out; answers its length. A first
// gate that from and the next point are on one side of is not crossed.
// FS2 0x00240d40
// callcheck: insert (out.push_back)

float pullThroughGates(std::vector<RouteNode *> &out, const float *from,
                       const std::vector<RouteSpan> &gates, const float *to)
{
    const RouteSpan *gate = gates.data();
    const RouteSpan *const end = gate + gates.size();
    if (gate != end) {
        const float *a = gate[0].first->position;
        const float *b = gate[0].second->position;
        const float *next = gates.size() < 2 ? to : gate[1].first->position;
        if (0.0f < sideOfLine(a, b, from) * sideOfLine(a, b, next))
            gate++;
        if (gate == end)
            return planarDistance(from, to);
    }

    float length = 0.0f;
    bool fresh = true;
    const float *edge[2] = {nullptr, nullptr};
    const RouteSpan *edgeGate[2] = {nullptr, nullptr};
    while (gate != end || !gates.empty()) {
        const float *left = to;
        const float *right = to;
        bool usable = true;
        if (gate < end) {
            const float *a = gate->first->position;
            const float *b = gate->second->position;
            const float ax = a[0] - from[0];
            const float ay = a[1] - from[1];
            const float bx = b[0] - from[0];
            const float by = b[1] - from[1];
            const float turn = ax * by + ay * -bx;
            usable = turn != 0.0f || ax * bx + ay * by != 0.0f;
            left = 0.0f <= turn ? a : b;
            right = 0.0f <= turn ? b : a;
        }
        if (usable) {
            if (fresh) {
                edge[0] = left;
                edge[1] = right;
                edgeGate[0] = gate;
                edgeGate[1] = gate;
                fresh = false;
            } else {
                int side = -1;
                if (turnAbout(from, edge[0], right) < 0.0f)
                    side = 0;
                if (0.0f < turnAbout(from, edge[1], left))
                    side = 1;
                if (side == -1) {
                    if (end <= gate)
                        break;
                    if (0.0f <= turnAbout(from, edge[0], left)) {
                        edge[0] = left;
                        edgeGate[0] = gate;
                    }
                    if (turnAbout(from, edge[1], right) <= 0.0f) {
                        edge[1] = right;
                        edgeGate[1] = gate;
                    }
                } else {
                    // Always a gate's corner: a funnel edge that is to itself
                    // turns neither way.
                    const float *corner = edge[side];
                    const char *record = reinterpret_cast<const char *>(corner) - offsetof(RouteNode, position);
                    out.push_back(reinterpret_cast<RouteNode *>(const_cast<char *>(record)));
                    length += planarDistance(from, corner);
                    gate = edgeGate[side];
                    fresh = true;
                    from = corner;
                }
            }
        }
        gate++;
    }
    return length + planarDistance(from, to);
}

}  // namespace

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00258b80

float planarDistance(const float *a, const float *b)
{
    return sqrtf((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]));
}

// Appends other's steps and length.
// FS2 0x00258870
// callcheck: FUN_00259760 (the host vector insert)

void RoutePath::append(const RoutePath &other)
{
    length += other.length;
    steps.reserve(steps.size() + other.steps.size());
    steps.insert(steps.end(), other.steps.begin(), other.steps.end());
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023f290

float RouteFilter::pathLength(const float *from, const float *to)
{
    return m_search->pathLength(from, to);
}

// FS2 0x0023f2a0

RouteNode *RouteFilter::junctionNode()
{
    return m_search->m_junction;
}

// FS2 0x0023f2b0

bool RouteFilter::advance()
{
    return m_search->advance();
}

// FS2 0x0023f2c0

RouteNode *RouteFilter::currentNode()
{
    RouteSearch *search = m_search;
    RouteNode **node = search->m_node;
    RouteNode **nodes = search->m_nodes.data();
    if (node < nodes || nodes + search->m_nodes.size() <= node)
        return nullptr;
    return *node;
}

// FS2 0x0023f310

void RouteFilter::allow(const char *name, bool on)
{
    m_search->allow(name, on);
}

// FS2 0x0023f320
// callcheck: resize, resize (the host vector clear)

void RouteFilter::clearPath()
{
    RouteSearch *search = m_search;
    search->m_path.steps.clear();
    search->m_path.length = -1.0f;
    search->m_nodes.clear();
}

// FS2 0x0023f370

float RouteFilter::findPath(const float *goal)
{
    return m_search->findPath(goal);
}

// FS2 0x0023f380
// callcheck: FUN_002285a0, operator_delete, ~RouteNameList, ~RouteNodeList (delete of the record)

RouteFilter::~RouteFilter()
{
    delete m_search;
}

// FS2 0x0023f500
// callcheck: operator_new (new RouteSearch)

RouteFilter::RouteFilter(RoutePlayer *routes, StageCharacter *owner)
{
    m_search = new RouteSearch(routes, owner);
}

// ──────────────────────────────────────────────────────────────────────
// Moves the path on from where the owner stands, and sets m_junction when
// the corner reached is one to go round on foot. false at the path's end.
// FS2 0x0023f560

bool RouteSearch::advance()
{
    constexpr float kNearNode = 40.0f;
    constexpr float kSharpCos2 = 0.75f;     // squared cosine of 30 degrees

    const RouteLeg *const stepsEnd = m_path.steps.data() + m_path.steps.size();
    if (m_path.steps.data() <= m_step && m_step < stepsEnd) {
        if (m_nextStep == stepsEnd) {
            if (!hasReached(m_pair, m_owner->m_xform.position)) {
                m_step = m_nextStep;
                m_pair = m_nextPair;
            }
        } else if (hasReached(m_nextPair, m_owner->m_xform.position)) {
            m_step = m_nextStep;
            m_pair = m_nextPair;
            m_nextPair = nextPair(m_nextStep, &m_nextStep);
        }
    }

    m_junction = nullptr;
    RouteNode **const nodes = m_nodes.data();
    RouteNode **const nodesEnd = nodes + m_nodes.size();
    if (m_node < nodes || nodesEnd <= m_node)
        return false;

    RouteNode *node = *m_node;
    RouteNode *prev = nodes < m_node ? m_node[-1] : nullptr;
    RouteNode *next = m_node < nodesEnd - 1 ? m_node[1] : nullptr;
    const float *pos = m_owner->m_xform.position;

    if (kNearNode <= planarDistance(pos, node->position)) {
        bool passed = false;
        if (next != nullptr && prev != nullptr) {
            float toPrev[3];
            float toNext[3];
            float toNode[3];
            for (int i = 0; i < 3; i++) {
                toPrev[i] = prev->position[i] - node->position[i];
                toNext[i] = next->position[i] - node->position[i];
                toNode[i] = node->position[i] - pos[i];
            }
            passed = dotOf(toNode, toNext) * lengthOf(toPrev) < dotOf(toNode, toPrev) * lengthOf(toNext);
        } else if (next != nullptr) {
            float span[3];
            float left[3];
            for (int i = 0; i < 3; i++) {
                span[i] = next->position[i] - node->position[i];
                left[i] = next->position[i] - pos[i];
            }
            passed = 0.0f < dotOf(left, span) && lengthOf(left) < lengthOf(span);
        } else if (prev != nullptr) {
            passed = planarDistance(prev->position, node->position) < planarDistance(pos, prev->position);
        }
        if (!passed)
            return true;
        m_node++;
        return m_node < nodesEnd;
    }

    if ((node->flags & RouteNodeFlag_Open) == 0) {
        const int motion = m_owner->m_motion->motionId;
        if (isRunMotion(motion) || isCrouchRunMotion(motion) || isWalkMotion(motion)) {
            if (next == nullptr) {
                m_junction = node;
            } else {
                float out[3];
                float in[3];
                const sceVu0FVECTOR *world = prev == nullptr ? m_owner->getWorldMatrix() : nullptr;
                for (int i = 0; i < 3; i++) {
                    out[i] = next->position[i] - node->position[i];
                    in[i] = prev != nullptr ? node->position[i] - prev->position[i] : world[1][i];
                }
                const float dot = dotOf(out, in);
                if (dot * dot < dotOf(in, in) * dotOf(out, out) * kSharpCos2)
                    m_junction = node;
            }
        }
    }
    m_node++;
    return m_node < nodesEnd;
}

// Has pos come to the pair: the end of the step it is on, the room between
// two gates, or the stretch between two steps?
// FS2 0x0023fde0

bool RouteSearch::hasReached(const RouteLegPair &pair, const float *pos)
{
    const RouteLeg &first = pair.first;
    const RouteLeg &second = pair.second;
    if (second.kind == first.kind && second.index == first.index) {
        if (m_node == m_nodes.data() + m_nodes.size())
            return false;
        RouteNode *node = *m_node;
        if (second.kind == RouteLeg_Chain) {
            const int count = m_routes->chainNodeCount(second.index);
            for (int i = 1; i < count; i++) {
                if (node == m_routes->chainNode(second.index, i))
                    return true;
            }
            return false;
        }
        if (second.kind == RouteLeg_Link) {
            const RouteSpan *link = m_routes->link(second.index);
            return (second.index & kRouteLinkReversed) == 0 ? link->second == node : link->first == node;
        }
        return false;
    }

    if (first.kind == RouteLeg_Gate || second.kind == RouteLeg_Gate) {
        RouteNode *corners[4];
        int count = 0;
        if (first.kind == RouteLeg_Gate) {
            const RouteSpan *gate = m_routes->gate(first.index);
            corners[count++] = gate->first;
            corners[count++] = gate->second;
        } else {
            corners[count++] = m_routes->endNode(first);
        }
        if (second.kind == RouteLeg_Gate) {
            const RouteSpan *gate = m_routes->gate(second.index);
            corners[count++] = gate->first;
            corners[count++] = gate->second;
        } else {
            corners[count++] = m_routes->endNode(second);
        }
        return count == 3 ? isInsideTriangle(corners, pos) : isInsideQuad(corners, pos);
    }

    const RouteNode *start = m_routes->startNode(second);
    const RouteNode *end = m_routes->endNode(first);
    const float axisX = end->position[0] - start->position[0];
    const float axisY = end->position[1] - start->position[1];
    if ((pos[0] - start->position[0]) * axisX + (pos[1] - start->position[1]) * axisY < 0.0f)
        return false;
    return (pos[0] - end->position[0]) * axisX + (pos[1] - end->position[1]) * axisY <= 0.0f;
}

// From step on, the next link, or the next two gates of different crossings.
// *next ends up past what was found.
// FS2 0x00240550

RouteLegPair RouteSearch::nextPair(const RouteLeg *step, const RouteLeg **next)
{
    const RouteLeg *const end = m_path.steps.data() + m_path.steps.size();
    RouteLegPair pair;
    *next = step;
    while (step != end) {
        *next = *next + 1;
        if (step->kind == RouteLeg_Link) {
            pair.first = *step;
            pair.second = *step;
            break;
        }
        if (step->kind == RouteLeg_Gate) {
            const RouteLeg *after = *next;
            if (after == end)
                break;
            if (after->kind == RouteLeg_Gate
                && m_routes->gate(step->index)->first->crossing
                    != m_routes->gate(after->index)->first->crossing) {
                pair.first = *step;
                pair.second = *after;
                break;
            }
        }
        step = *next;
    }
    return pair;
}

// The step walked first, when it is a link or a chain.
// FS2 0x00240710

void RouteSearch::firstPair()
{
    if (m_step != m_path.steps.data() + m_path.steps.size()
        && (m_step->kind == RouteLeg_Link || m_step->kind == RouteLeg_Chain)) {
        m_pair.first = *m_step;
        m_pair.second = *m_step;
    } else {
        m_pair = RouteLegPair();
    }
}

// Turns steps into the nodes to walk to and answers the length, from to to.
// FS2 0x002407f0
// callcheck: insert, insert, ~RouteSpanList (the host vectors)

float RouteSearch::buildNodes(std::vector<RouteNode *> &out, const std::vector<RouteLeg> &steps,
                              const float *from, const float *to)
{
    std::vector<RouteSpan> gates;
    out.clear();
    float length = 0.0f;
    const float *pos = from;

    // The way to a step's first node.
    const auto reach = [&](const RouteNode *node) {
        if (!gates.empty())
            return pullThroughGates(out, pos, gates, node->position);
        return pos != nullptr ? planarDistance(pos, node->position) : 0.0f;
    };
    const auto addFirst = [&](RouteNode *node) {
        if (out.empty() || out.back() != node)
            out.push_back(node);
    };

    for (const RouteLeg &step : steps) {
        RouteNode *first = nullptr;
        RouteNode *last = nullptr;
        float along = 0.0f;
        switch (step.kind) {
        case RouteLeg_Node:
            first = m_routes->node(step.index);
            length += reach(first);
            addFirst(first);
            break;
        case RouteLeg_Chain: {
            const u_int count = m_routes->chainNodeCount(step.index);
            first = m_routes->chainNode(step.index, 0);
            length += reach(first);
            last = m_routes->chainNode(step.index, count - 1);
            along = m_routes->chainLength(step.index);
            addFirst(first);
            for (u_int i = 1; i < count; i++)
                out.push_back(m_routes->chainNode(step.index, i));
            break;
        }
        case RouteLeg_Link: {
            const RouteSpan *link = m_routes->link(step.index);
            first = link->first;
            length += reach(first);
            last = link->second;
            along = planarDistance(first->position, last->position);
            addFirst(first);
            out.push_back(last);
            break;
        }
        case RouteLeg_Gate:
            gates.push_back(*m_routes->gate(step.index));
            break;
        }
        if (first != nullptr) {
            length += along;
            pos = (last != nullptr ? last : first)->position;
            gates.clear();
        }
    }

    if (!gates.empty())
        return length + pullThroughGates(out, pos, gates, to);
    if (out.empty())
        return vecDistance(to, from);
    return length + planarDistance(to, out.back()->position);
}

// The way between two points, without keeping it.
// FS2 0x002411a0
// callcheck: ~vector, clear, operator_delete (the host RoutePath and vector)

float RouteSearch::pathLength(const float *from, const float *to)
{
    const short fromArea = gStageScene->getFloorAttr(from);
    const short toArea = gStageScene->getFloorAttr(to);
    if (fromArea < 0 || toArea < 0)
        return -1.0f;

    RoutePath path;
    path.length = -1.0f;
    search(path, from, to, fromArea, toArea);
    if (path.length < 0.0f)
        return path.length;
    std::vector<RouteNode *> nodes;
    return buildNodes(nodes, path.steps, from, to);
}

// A path from the owner to goal, kept to be walked.
// FS2 0x00241380
// callcheck: clear (steps.clear)

float RouteSearch::findPath(const float *goal)
{
    m_path.steps.clear();
    m_path.length = -1.0f;
    const int fromArea = m_owner->m_ground->floorAttr;
    const short toArea = gStageScene->getFloorAttr(goal);
    if (fromArea < 0 || toArea < 0)
        return -1.0f;

    search(m_path, m_owner->m_xform.position, goal, (short)fromArea, toArea);
    m_step = m_path.steps.data();
    firstPair();
    m_nextPair = nextPair(m_step, &m_nextStep);
    if (m_path.length < 0.0f)
        m_nodes.clear();
    else
        m_path.length = buildNodes(m_nodes, m_path.steps, m_owner->m_xform.position, goal);
    m_node = m_nodes.data();
    m_junction = nullptr;
    return m_path.length;
}

// The cheapest way between two areas: over each crossing the file lists for
// them, from from to the crossing's start, along it, and on to to.
// FS2 0x002414c0
// callcheck: ~vector, ~vector, clear (the host RoutePath and vector)

float RouteSearch::search(RoutePath &best, const float *from, const float *to, short fromArea,
                          short toArea)
{
    std::vector<short> crossings;
    m_routes->areaCrossings(crossings, fromArea, toArea);
    if (fromArea == toArea) {
        if (!m_routes->isListedArea(fromArea)) {
            m_routes->areaPath(best, from, to, fromArea);
            return best.length;
        }
        best.length = 1.0f;
        return 1.0f;
    }

    float bestLength = 0.0f;
    short exitCrossing = 0;     // the ROM's stays on the stack from one round to the next too
    for (short crossing : crossings) {
        RoutePath way;
        m_routes->crossingPath(way, fromArea, toArea, crossing, m_pathSet);
        const float wayLength = way.length;
        if (wayLength < 0.0f || way.steps.empty())
            continue;

        RoutePath head;
        RoutePath tail;
        if (!m_routes->isListedArea(fromArea)) {
            const RouteSpan *link = m_routes->link(way.steps.front().index);
            m_routes->wayToCrossing(head, from, fromArea, link->second->crossing);
        } else {
            head.length = stepDistance(way.steps.front(), from);
        }

        const RouteLeg &last = way.steps.back();
        if (last.kind == RouteLeg_Link)
            exitCrossing = m_routes->link(last.index)->first->crossing;
        else if (last.kind == RouteLeg_Gate)
            exitCrossing = m_routes->gate(last.index)->first->crossing;
        m_routes->wayToCrossing(tail, to, toArea, exitCrossing);
        m_routes->reversePath(tail);
        if (m_routes->isListedArea(toArea))
            tail.length = stepDistance(last, to);

        const float length = wayLength + head.length + tail.length;
        if (bestLength == 0.0f || length < bestLength) {
            best.steps.clear();
            best.append(head);
            best.append(way);
            best.append(tail);
            best.length = length;
            bestLength = length;
        }
    }
    return best.length;
}

// How far pos is from the middle of the step, on the map. The ROM has no
// answer for a chain: it reads what its registers hold; here 0.
// FS2 0x00241960

float RouteSearch::stepDistance(const RouteLeg &step, const float *pos)
{
    const RouteNode *a;
    const RouteNode *b;
    if (step.kind == RouteLeg_Gate) {
        const RouteSpan *gate = m_routes->gate(step.index);
        a = gate->first;
        b = gate->second;
    } else if (step.kind == RouteLeg_Link) {
        const RouteSpan *link = m_routes->link(step.index);
        a = link->first;
        b = link->second;
    } else if (step.kind == RouteLeg_Node) {
        a = b = m_routes->node(step.index);
    } else {
        return 0.0f;
    }
    const float y = (a->position[1] + b->position[1]) * 0.5f - pos[1];
    const float x = (a->position[0] + b->position[0]) * 0.5f - pos[0];
    return sqrtf(x * x + y * y);
}

// Opens or closes a path: closing takes the first allowed one whose name
// begins with name, opening adds the one of exactly that name. The set the
// file knows for the new list becomes the search's.
// FS2 0x00241a40
// callcheck: erase, insert (vector erase, push_back)

void RouteSearch::allow(const char *name, bool on)
{
    for (auto it = m_allowed.begin(); it != m_allowed.end(); ++it) {
        if (strncmp(*it, name, strlen(name)) != 0)
            continue;
        if (on)
            return;
        m_allowed.erase(it);
        const int set = m_routes->pathSetIndex(m_allowed);
        if (set != -1)
            m_pathSet = set;
        return;
    }
    if (!on)
        return;
    for (const char *known : m_names) {
        if (strcmp(name, known) != 0)
            continue;
        m_allowed.push_back(known);
        const int set = m_routes->pathSetIndex(m_allowed);
        if (set != -1)
            m_pathSet = set;
        return;
    }
}

// FS2 0x00241c20
// callcheck: RouteNameList, operator_delete (pathNames fills the member in place)

RouteSearch::RouteSearch(RoutePlayer *routes, StageCharacter *owner)
    : m_routes(routes), m_owner(owner)
{
    routes->pathNames(m_names);
    m_allowed.reserve(m_names.size());
    const int count = routes->maxStepCount();
    m_path.steps.reserve(count);
    m_nodes.reserve(count);
}

// The MSL std::vector instances of the search, out of line in the ROM after
// the code above; the host <vector> provides them.
//   FS2 0x0023f420  vector<RouteNode *>::~vector
//   FS2 0x0023f490  vector<const char *>::~vector
//   FS2 0x00241310  vector<RouteLeg>::~vector
//   FS2 0x00241d10  vector<RouteLeg>::resize
//   FS2 0x00241da0  vector<const char *>::reserve
//   FS2 0x00241f80  vector<const char *>::vector (copy)
//   FS2 0x00241fd0  vector<RouteLeg>::erase (first, last)
//   FS2 0x00242170  vector<RouteLeg>::insert (pos, n, value)
//   FS2 0x002425c0  vector<RouteLeg>::reserve
//   FS2 0x00242740  vector<RouteSpan>::insert (pos, n, value)
//   FS2 0x00242c10  vector<RouteSpan>::clear
//   FS2 0x00242c20  vector<RouteSpan>::~vector
//   FS2 0x00242c90  vector<const char *>::assign (first, last)
