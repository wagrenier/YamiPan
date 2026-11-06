/* ========================================================================
 *  game/routefilter.h -- the path search of a character's route.
 *
 *  A stage's rute file (RoutePlayer, game/routeplayer.h) is a graph over the
 *  map: nodes, spans between two nodes (links to walk along, gates to walk
 *  through), chains of nodes, and tables of the ways between two floor areas
 *  (StageScene::getFloorAttr). The search asks those tables for a list of
 *  steps, the cheapest over the areas' crossings, then turns the steps into
 *  the nodes to walk to; a run of gates is pulled tight with a funnel. The
 *  walk (RouteWalk, game/characterroute.cpp) asks for the next node each
 *  frame.
 *
 *  RouteFilter is a 4-byte handle on the search's 0x60 record. The names are
 *  the port's: the ROM has no string for any of this (open-items.md).
 * ======================================================================== */
#ifndef _GAME_ROUTEFILTER_H
#define _GAME_ROUTEFILTER_H

#include <stddef.h>

#include <vector>

#include "sdk/scetypes.h"

class RoutePlayer;
class StageCharacter;

enum RouteNodeFlag
{
    RouteNodeFlag_Open = 0x1,   // no corner: the walk may cut toward it
};

// A node of the rute file (0x20).
struct RouteNode
{
    short   crossing;           // FS2 +0x00: the crossing between two areas it belongs to
    u_short flags;              // FS2 +0x02: RouteNodeFlag
    char    name[0x10];         // FS2 +0x04: "Flag_094_s1s3"; read by nothing
    float   position[3];        // FS2 +0x14
};
static_assert(offsetof(RouteNode, position) == 0x14, "RouteNode::position");
static_assert(sizeof(RouteNode) == 0x20, "RouteNode");

// Two nodes: a link walked from one to the other, or a gate walked through
// between them. layout: host: on the EE two pointers and a word, 0xc: a
// gate's is a short, a link's a third node.
struct RouteSpan
{
    RouteNode *first;           // FS2 +0x00
    RouteNode *second;          // FS2 +0x04
    short      _8 = 0;          // FS2 +0x08: a gate's: copied, not read
    RouteNode *third = nullptr; // FS2 +0x08: a link's: relocated, not read
};

enum RouteLegKind
{
    RouteLeg_None  = 0,
    RouteLeg_Link  = 1,        // a span, walked along
    RouteLeg_Gate  = 2,        // a span, walked through
    RouteLeg_Chain = 3,        // a list of nodes with its length
    RouteLeg_Node  = 4,        // one node
};

constexpr short kRouteLegNoIndex = -1;
constexpr int   kRouteLinkReversed = 0x8000;    // in a link's index: walked second to first

// One step of a path: which table of the rute file, and where in it.
struct RouteLeg
{
    short kind  = RouteLeg_None;       // FS2 +0x0: RouteLegKind
    short index = kRouteLegNoIndex;    // FS2 +0x2
};
static_assert(sizeof(RouteLeg) == 4, "RouteLeg");

// What the character must come to, to have passed a step: two steps, or
// the same step twice.
struct RouteLegPair
{
    RouteLeg first;            // FS2 +0x0
    RouteLeg second;           // FS2 +0x4
};
static_assert(sizeof(RouteLegPair) == 8, "RouteLegPair");

// A path; a stored path of the file too, where the vector is a pointer to
// a count and the legs. layout: host: EE 0x10, the vector's cap / size / data at +4;
// FUN_002285a0 is its destructor.
struct RoutePath
{
    void append(const RoutePath &other);                                 // FS2 0x00258870

    float                  length = 0.0f;   // FS2 +0x0: negative no way
    std::vector<RouteLeg> steps;           // FS2 +0x4
};

// The distance on the map, without the height.
float planarDistance(const float *a, const float *b);                    // FS2 0x00258b80

// The record behind RouteFilter (EE 0x60). layout: host.
class RouteSearch
{
public:
    RouteSearch(RoutePlayer *routes, StageCharacter *owner);             // FS2 0x00241c20

    bool  advance();                                                     // FS2 0x0023f560
    float pathLength(const float *from, const float *to);                // FS2 0x002411a0
    float findPath(const float *goal);                                   // FS2 0x00241380
    void  allow(const char *name, bool on);                              // FS2 0x00241a40

    RoutePath                m_path;            // FS2 +0x00
    const RouteLeg         *m_step = nullptr;  // FS2 +0x10: the step being walked
    const RouteLeg         *m_nextStep = nullptr; // FS2 +0x14: past m_nextPair
    RouteLegPair            m_pair;            // FS2 +0x18
    RouteLegPair            m_nextPair;        // FS2 +0x20
    std::vector<RouteNode *> m_nodes;           // FS2 +0x28
    RouteNode              **m_node = nullptr;  // FS2 +0x34: the one walked to
    RouteNode               *m_junction = nullptr; // FS2 +0x38
    int                      m_pathSet = 0;     // FS2 +0x3c: which of the file's sets m_allowed is

private:
    bool  hasReached(const RouteLegPair &pair, const float *pos);       // FS2 0x0023fde0
    RouteLegPair nextPair(const RouteLeg *step, const RouteLeg **next); // FS2 0x00240550
    void  firstPair();                                                   // FS2 0x00240710
    float buildNodes(std::vector<RouteNode *> &out, const std::vector<RouteLeg> &steps,
                     const float *from, const float *to);                // FS2 0x002407f0
    float search(RoutePath &best, const float *from, const float *to, short fromArea,
                 short toArea);                                          // FS2 0x002414c0
    float stepDistance(const RouteLeg &step, const float *pos);         // FS2 0x00241960

    std::vector<const char *> m_allowed;        // FS2 +0x40: the paths open to the character
    std::vector<const char *> m_names;          // FS2 +0x4c: every path the file names
    RoutePlayer    *m_routes;                   // FS2 +0x58
    StageCharacter *m_owner;                    // FS2 +0x5c
};

// The route's paths this character may use. layout: host: the EE's is a
// 4-byte handle on a 0x60 record.
struct RouteFilter
{
    RouteFilter(RoutePlayer *routes, StageCharacter *owner);             // FS2 0x0023f500
    ~RouteFilter();                                                      // FS2 0x0023f380
    // The named paths of the route are open to the character (on) or not.
    void allow(const char *name, bool on);                               // FS2 0x0023f310
    // How far along the route from one point to another; -1 no way.
    float pathLength(const float *from, const float *to);                // FS2 0x0023f290
    void clearPath();                                                    // FS2 0x0023f320
    // A path from the owner to goal: its length, or -1.
    float findPath(const float *goal);                                   // FS2 0x0023f370
    // Along the path from where the owner stands; false at its end.
    bool advance();                                                      // FS2 0x0023f2b0
    // The node it heads for; null past the end.
    RouteNode *currentNode();                                            // FS2 0x0023f2c0
    // Set by advance: a corner to walk to at a walk; null none.
    RouteNode *junctionNode();                                           // FS2 0x0023f2a0

    RouteSearch *m_search = nullptr;    // FS2 +0x0
};

#endif
