/* ========================================================================
 *  game/routeplayer.h -- a stage's rute file and the ways through it.
 *
 *  The file (keyword `rute`, magic "ord\0", 15 words of header) is a graph
 *  over the map. Its header words are offsets from the file's start; the ROM
 *  turns them into EE pointers in place (0x258260) and writes "ORD\0" over
 *  the magic. On the host a pointer does not fit in the file's 4 bytes, so
 *  RouteFile::buildTables reads the same tables into the host records below
 *  and leaves the file as it was read. Nodes hold no pointers: RouteNode *
 *  still points into the file, in the file's order, which is what the tables
 *  keyed by a node are sorted by.
 *
 *  An area is a floor area of StageScene::getFloorAttr. Inside an area that
 *  is not listed (RoutePlayer::isListedArea) a character walks chains of
 *  nodes; RouteAreaSearch finds the way along them. Between areas the file
 *  stores whole paths, by the areas and the crossing between them.
 *
 *  The names are the port's: the ROM has no string for any of this.
 * ======================================================================== */
#ifndef _GAME_ROUTEPLAYER_H
#define _GAME_ROUTEPLAYER_H

#include <stddef.h>

#include <vector>

#include "game/gamehooks.h"
#include "game/routefilter.h"
#include "sdk/scetypes.h"

// The header as it lies in the file: offsets from the file's start, except
// where it says. A table is a count, then its records.
struct RuteHeader
{
    u_int magic;            // FS2 +0x00: "ord\0"
    u_int nodes;            // FS2 +0x04: count, a word, then RouteNode from +8
    u_int nodesSize;        // FS2 +0x08: not an offset: bytes up to gates; read by nothing
    u_int gates;            // FS2 +0x0c: 0xc: two nodes and a short
    u_int links;            // FS2 +0x10: 0xc: three nodes
    u_int chains;           // FS2 +0x14: 8: length, offset of count + nodes
    u_int areas;            // FS2 +0x18: count + the area numbers (shorts); read by nothing
    u_int crossings;        // FS2 +0x1c: area, then area, then count + { crossing, path }
    u_int paths;            // FS2 +0x20: 8: length, offset of count + legs
    u_int areaChains;       // FS2 +0x24: 8: area, offset of count + chain numbers (shorts)
    u_int chainRuns;        // FS2 +0x28: node, then node, then count + chain numbers
    u_int sets;             // FS2 +0x2c: offsets of the sets: count + offsets of names; may be 0
    u_int setCrossings;     // FS2 +0x30: one table like crossings per set
    u_int areaLists;        // FS2 +0x34: like areaChains; relocated only when sets is there
    u_int _38;              // FS2 +0x38
};
static_assert(sizeof(RuteHeader) == 0x3c, "RuteHeader");
static_assert(offsetof(RuteHeader, crossings) == 0x1c, "RuteHeader::crossings");
static_assert(offsetof(RuteHeader, areaLists) == 0x34, "RuteHeader::areaLists");

constexpr u_int kRuteMagic = 0x0064726f;       // "ord\0"
constexpr u_int kRuteMagicCase = 0x00202020;   // the ROM tests with this or'ed in

constexpr float kRouteNoWay = -1.0f;
constexpr float kRouteCrossingLength = 0.1f;   // what standing at a crossing costs

// What nearestSegment says about the point it was given.
enum RouteSegmentSide
{
    RouteSide_None   = 0,      // no chain in the area
    RouteSide_Along  = 1,      // beside the segment
    RouteSide_First  = 2,      // past the segment's first node
    RouteSide_Second = 3,      // past its second
};

// A list of nodes with its length (EE 8). layout: host.
struct RouteChain
{
    float                    length;    // FS2 +0x0
    std::vector<RouteNode *> nodes;     // FS2 +0x4: count + pointers
};

// What a key gives: a list of chain numbers (EE 8). layout: host.
struct RouteChainList
{
    short              key;     // FS2 +0x0: an area
    std::vector<short> chains;  // FS2 +0x4: count + shorts
};

// From one chain end to another (EE 8). layout: host.
struct RouteChainRunTo
{
    RouteNode         *key;     // FS2 +0x0
    std::vector<short> chains;  // FS2 +0x4: count + shorts, the chains to take
};

// layout: host.
struct RouteChainRunFrom
{
    RouteNode                   *key;   // FS2 +0x0
    std::vector<RouteChainRunTo> to;    // FS2 +0x4
};

// The stored path over one crossing, in the file's form (4 bytes on disc).
struct RouteWayRecord
{
    short key;                  // FS2 +0x0: the crossing
    short path;                 // FS2 +0x2: into RouteFile::m_paths
};
static_assert(sizeof(RouteWayRecord) == 4, "RouteWayRecord");

// layout: host.
struct RouteCrossingTo
{
    short                       key;    // FS2 +0x0: the area gone to
    std::vector<RouteWayRecord> ways;   // FS2 +0x4
};

// layout: host.
struct RouteCrossingFrom
{
    short                        key;   // FS2 +0x0: the area left
    std::vector<RouteCrossingTo> to;    // FS2 +0x4
};

typedef std::vector<RouteCrossingFrom> RouteCrossingTable;

class RouteFile;
class RoutePlayer;

// The way along the chains of one area (EE 0x10: the file record and the
// three tables it reads, which here live in the RouteFile). layout: host.
class RouteAreaSearch
{
public:
    explicit RouteAreaSearch(RouteFile *file) : m_file(file) {}

    float areaPath(RoutePath &out, const float *from, const float *to, short area); // FS2 0x00253420
    float wayFromPoint(RoutePath &out, const float *pos, short area, short crossing); // FS2 0x00256b60

private:
    float chainRun(RoutePath &out, RouteNode *from, RouteNode *to);      // FS2 0x00253210
    short chainOf(RouteNode *first, RouteNode *second);                  // FS2 0x002550d0
    short nodeIndex(const RouteNode *node);                              // FS2 0x002550a0
    float wayAlongChains(RoutePath &out, RouteNode *node, short crossing); // FS2 0x00255330
    float wayFromSegment(RoutePath &out, RouteNode *const *segment, short crossing); // FS2 0x002569e0
    float nearestSegment(RouteNode **segment, const float *pos, short area, int *side); // FS2 0x00256db0

    const std::vector<short> *runChains(RouteNode *from, RouteNode *to);

    RouteFile *m_file;          // FS2 +0x0; +4 chains, +8 areaChains, +0xc chainRuns
};

// The record behind RoutePlayer (EE 0x24). layout: host.
class RouteFile
{
public:
    RouteFile(RoutePlayer *owner, void *file);                           // FS2 0x00257fa0
    ~RouteFile();

    bool isOrd() const;

    int   pathSetIndex(const std::vector<const char *> &names);          // FS2 0x00257020
    void  reversePath(RoutePath &path);                                  // FS2 0x00257360
    float wayToCrossing(RoutePath &out, const float *pos, short area, short crossing); // FS2 0x00257680
    float crossingPath(RoutePath &out, short fromArea, short toArea, short crossing, int pathSet); // FS2 0x002578f0
    void  areaList(std::vector<short> &out, short area);                 // FS2 0x00257cb0
    void  areaCrossings(std::vector<short> &out, short fromArea, short toArea); // FS2 0x00257db0
    const RouteLeg *firstLeg(short area, short crossing);                // FS2 0x00257f50
    int   maxStepCount();                                                // FS2 0x00258110
    float legsLength(const std::vector<RouteLeg> &legs);                 // FS2 0x00259230

    RouteAreaSearch            *m_search = nullptr; // FS2 +0x00
    u_char                     *m_bytes;            // FS2 +0x04: the file
    RoutePlayer                *m_owner;            // FS2 +0x08
    std::vector<short>          m_areas;            // FS2 +0x0c: of the nodes with RouteNodeFlag_Open, sorted
    std::vector<const char *>   m_names;            // FS2 +0x18: every path name, once, in file order

    // What the ROM reads through its relocated pointers (buildTables).
    RouteNode                  *m_nodes = nullptr;  // header +0x04
    std::vector<RouteSpan>      m_gates;            // header +0x0c
    std::vector<RouteSpan>      m_links;            // header +0x10
    std::vector<RouteChain>     m_chains;           // header +0x14
    RouteCrossingTable          m_crossings;        // header +0x1c
    std::vector<RoutePath>      m_paths;            // header +0x20
    std::vector<RouteChainList> m_areaChains;       // header +0x24
    std::vector<RouteChainRunFrom> m_chainRuns;     // header +0x28
    bool                        m_hasSets = false;  // header +0x2c is not 0
    std::vector<std::vector<const char *> > m_sets; // header +0x2c
    std::vector<RouteCrossingTable> m_setCrossings; // header +0x30
    std::vector<RouteChainList> m_areaLists;        // header +0x34

private:
    void buildTables();                                                  // FS2 0x00258260
    void collectNames();                                                 // FS2 0x002571e0
    const RoutePath *findWay(short fromArea, short toArea, short crossing, int pathSet); // FS2 0x002579d0
    const RoutePath *findWayIn(short fromArea, short toArea, short crossing,
                               const RouteCrossingTable &table);         // FS2 0x00257ad0
};

// +0x4c of the scene: what answers for the stage's rute file (vtable
// 0x316a78). layout: host (EE 0xc).
class RoutePlayer
{
public:
    explicit RoutePlayer(void *rute);                                    // FS2 0x00253180
    ~RoutePlayer();                                                      // FS2 0x002530a0
    void destroy() { delete this; }

    // The area is in the sorted list of the rute header.
    bool isListedArea(short area);                                       // FS2 0x00252bc0
    // What a leg names (game/routefilter.h).
    RouteNode *endNode(const RouteLeg &step);                           // FS2 0x00252840
    RouteNode *startNode(const RouteLeg &step);                         // FS2 0x002529a0
    RouteNode *node(short index);                                        // FS2 0x00252e20
    const RouteSpan *link(short index);                                  // FS2 0x00252d80
    const RouteSpan *gate(short index);                                  // FS2 0x00252dd0
    float chainLength(short index);                                      // FS2 0x00252c80
    RouteNode *chainNode(short index, int at);                           // FS2 0x00252cd0
    int chainNodeCount(short index);                                     // FS2 0x00252d30
    // The most steps a path of the file has.
    int maxStepCount();                                                  // FS2 0x00252ac0
    // Every path the file names.
    void pathNames(std::vector<const char *> &out);                      // FS2 0x00252b20
    // Which set of the file is exactly these paths: from 1, 0 none, -1
    // when the file has no sets.
    int pathSetIndex(const std::vector<const char *> &names);            // FS2 0x00252e70
    // The file's list for an area (header +0x34); left alone without one.
    void areaList(std::vector<short> &out, short area);                  // FS2 0x00252ec0
    // The crossings between two areas.
    void areaCrossings(std::vector<short> &out, short fromArea, short toArea); // FS2 0x00252f10
    // Over one crossing, by the set of paths.
    float crossingPath(RoutePath &out, short fromArea, short toArea, short crossing, int pathSet); // FS2 0x00253030
    // Inside one area.
    float areaPath(RoutePath &out, const float *from, const float *to, short area); // FS2 0x00252f70
    // From a point of the area to a crossing of it.
    float wayToCrossing(RoutePath &out, const float *pos, short area, short crossing); // FS2 0x00252fd0
    // Turns the path around, its spans and chains into nodes.
    void reversePath(RoutePath &path);                                   // FS2 0x00252b70
    void start(int arg);                                                 // FS2 0x00252b10: empty here

private:
    RouteFile *m_file;          // FS2 +0x4
    void      *m_nodeTable;     // FS2 +0x8: the file's nodes; read by nothing
};

#endif /* _GAME_ROUTEPLAYER_H */
