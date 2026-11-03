/* ==========================================================================
 *  game/clock.h
 *
 *  The game clocks (game/clock.cpp, 0x1350d0..0x135990): a Clock scales its
 *  parent's tick delta by an 8.8 factor (0 while paused) and passes it on to
 *  its children, so a subtree can run slow or stop; MasterClock is the root,
 *  advanced once per frame by GameSystem::flipFrame with the vsyncs the
 *  frame took. PtrList, the children list, is only used here.
 *
 *  Destructors (Metrowerks: one per class, (this, short flags), deleting
 *  when flags > 0): ClockNode::~ClockNode 0x135550 is the real body;
 *  Clock::~Clock 0x1351b0 and MasterClock::~MasterClock 0x135330 only call
 *  it with flags 0 and free this. So the tree lives in a base class (named
 *  ClockNode by the port) holding every Clock field; Clock adds methods
 *  only, MasterClock the frame rate.
 * ======================================================================== */
#ifndef _GAME_CLOCK_H
#define _GAME_CLOCK_H

#include <stddef.h>

#include "sdk/scetypes.h"

// Clock::m_time's units per second (the int at 0x300e48; the play-time
// ticks game/uigame.cpp shows run at 25).
constexpr int kClockTimePerSecond = 300;

// The 8.8 fixed-point scale that runs a clock at its parent's speed.
constexpr int kClockScaleOne = 0x100;

// The slots a children list grows by when full.
constexpr int kClockChildGrow = 16;

// ClockNode::m_state.
enum ClockState
{
    ClockState_Running = 0,
    ClockState_Paused  = 1,     // Clock::advance gives it (and its subtree) a 0 delta
};

// A growable array of pointers whose free slots are null; add fills the
// first free slot, next walks the used ones.
// layout: host -- EE 8 bytes {u16 capacity, u16 count, void **data}.
struct PtrList
{
    PtrList() : m_capacity(0), m_count(0), m_data(NULL) {}
    // Inline in the ROM (ClockNode::~ClockNode): frees the array.
    ~PtrList();

    // The first used entry after `after` (from the start when null); null
    // at the end, and also when `after` is not in the list.
    void   *next(void *after);                                           // FS2 0x001357c0
    // Stores p in the first free slot, first growing the array by `grow`
    // slots when count has reached capacity; answers the count.
    u_short add(void *p, int grow);                                      // FS2 0x00135860
    // Inline in the ROM (ClockNode::~ClockNode): clears p's slot and
    // answers the count left, or -1 when p is not in the list.
    int     remove(void *p);

    u_short m_capacity;         // FS2 +0x0
    u_short m_count;            // FS2 +0x2
    void  **m_data;             // FS2 +0x4: m_capacity slots (gGlobalHeap)
};

// The clock fields and the tree.
// layout: host -- holds the parent and list pointers; EE 0x1c.
class ClockNode
{
public:
    // Inline everywhere (Clock::create / createChild, MasterClock's ctor).
    explicit ClockNode(ClockNode *parent)
        : m_state(ClockState_Running), m_time(0), m_delta(0), m_time2(0),
          m_scale(kClockScaleOne), m_parent(parent), m_children(NULL) {}
    // Takes this out of the tree: detaches the children, frees the list,
    // leaves the parent's list (freeing it when it empties).
    ~ClockNode();                                                        // FS2 0x00135550

    int      m_state;           // FS2 +0x00: ClockState
    u_int    m_time;            // FS2 +0x04: ticks since resetTime
    u_int    m_delta;           // FS2 +0x08: the last advance's ticks
    u_int    m_time2;           // FS2 +0x0c: ticks since creation (never reset here)
    int      m_scale;           // FS2 +0x10: 8.8 fixed point, kClockScaleOne = 1
    ClockNode *m_parent;        // FS2 +0x14
    PtrList *m_children;        // FS2 +0x18: made on the first child
};

// layout: host -- EE 0x1c (ClockNode's fields only).
class Clock : public ClockNode
{
public:
    explicit Clock(Clock *parent) : ClockNode(parent) {}
    ~Clock();                                                            // FS2 0x001351b0

    // A new clock under `parent` (the same code as createChild).
    static Clock *create(Clock *parent);                                 // FS2 0x001350d0
    void   setPaused(bool paused);                                       // FS2 0x00135190
    // A new clock under this one, running at its speed.
    Clock *createChild();                                                // FS2 0x00135210
    // m_delta in seconds (kClockTimePerSecond).
    float  getDeltaSeconds();                                            // FS2 0x001353d0
    // m_time in seconds (kClockTimePerSecond).
    float  getSeconds();                                                 // FS2 0x00135420
    // Adds ticks * m_scale to this clock and passes the result on to the
    // children.
    void   advance(u_int ticks);                                         // FS2 0x001354a0
    void   resetTime();                                                  // FS2 0x001357b0: m_time = 0
};

// layout: host -- EE 0x30.
class MasterClock : public Clock
{
public:
    MasterClock(int ticksPerFrame, int interval, bool variableRate, bool flag1c); // FS2 0x00135390
    ~MasterClock();                                                      // FS2 0x00135330
    // Called once per frame with the vsyncs it took.
    void update(int vsyncs);                                             // FS2 0x001352d0

    u_char  m_flag1c;           // FS2 +0x1c: the last constructor argument; nothing in clock.cpp reads it
    int     m_ticksPerFrame;    // FS2 +0x20
    int     m_vsyncTotal;       // FS2 +0x24
    int     m_interval;         // FS2 +0x28: vsyncs per frame
    u_char  m_variableRate;     // FS2 +0x2c: scale the ticks by vsyncs / m_interval
};

#endif /* _GAME_CLOCK_H */
