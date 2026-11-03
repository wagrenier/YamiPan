// game/clock.cpp -- the game clocks and their children list (game/clock.h).

#include "game/clock.h"

#include "game/heap.h"

// A PtrList's array slot. The EE's slots are 4 bytes; the host sizes them
// as pointers.
constexpr u_int kPtrListSlotSize = sizeof(void *);

// Where PtrList::add's array starts (Heap::alloc's alignment).
constexpr u_int kPtrListAlign = 0x10;

// getDeltaSeconds divides by this literal (getSeconds reads
// kClockTimePerSecond from 0x300e48 instead).
constexpr float kClockDeltaPerSecond = 300.0f;

// ──────────────────────────────────────────────────────────────────────
// Inline in the ROM (ClockNode::~ClockNode, twice).

PtrList::~PtrList()
{
    if (m_data != NULL)
        gGlobalHeap.free(m_data);
    m_capacity = 0;
    m_count = 0;
    m_data = NULL;
}

// Inline in the ROM (ClockNode::~ClockNode, twice).

int PtrList::remove(void *p)
{
    for (u_int i = 0; i < m_capacity; i++) {
        if (m_data[i] == p) {
            m_data[i] = NULL;
            m_count--;
            return m_count;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// A new clock under `parent`: running, scale 1, time 0, added to the
// parent's children (the list made on the first child).
// FS2 0x001350d0
// callcheck: operator_new, PtrList::add, operator_new, PtrList::add (new Clock, new PtrList; the two add calls are the two branches)

Clock *Clock::create(Clock *parent)
{
    Clock *clock = new Clock(parent);
    if (parent->m_children == NULL) {
        PtrList *list = new PtrList;
        list->add(clock, kClockChildGrow);
        parent->m_children = list;
    } else {
        parent->m_children->add(clock, kClockChildGrow);
    }
    clock->m_time = 0;          // resetTime, inline
    return clock;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00135190

void Clock::setPaused(bool paused)
{
    m_state = paused ? ClockState_Paused : ClockState_Running;
}

// ──────────────────────────────────────────────────────────────────────
// Nothing of its own: the tree is ClockNode's.
// FS2 0x001351b0
// callcheck: ~ClockNode, operator_delete (the base destructor; the deleting destructor's flag)

Clock::~Clock()
{
}

// ──────────────────────────────────────────────────────────────────────
// A new clock under this one (Clock::create with this as the parent).
// FS2 0x00135210
// callcheck: operator_new, PtrList::add, operator_new, PtrList::add (new Clock, new PtrList; the two add calls are the two branches)

Clock *Clock::createChild()
{
    Clock *clock = new Clock(this);
    if (m_children == NULL) {
        PtrList *list = new PtrList;
        list->add(clock, kClockChildGrow);
        m_children = list;
    } else {
        m_children->add(clock, kClockChildGrow);
    }
    clock->m_time = 0;          // resetTime, inline
    return clock;
}

// ──────────────────────────────────────────────────────────────────────
// Counts the vsyncs and advances the tree: by the fixed ticks per frame,
// or, with m_variableRate, by ticks per frame * vsyncs / m_interval (a
// slow frame then moves the clocks further).
// FS2 0x001352d0

void MasterClock::update(int vsyncs)
{
    m_vsyncTotal += vsyncs;
    if (!m_variableRate)
        advance(m_ticksPerFrame);
    else
        advance((u_int)(m_ticksPerFrame * vsyncs) / (u_int)m_interval);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00135330
// callcheck: ~ClockNode, operator_delete (the base destructor through Clock's inline one; the deleting destructor's flag)

MasterClock::~MasterClock()
{
}

// ──────────────────────────────────────────────────────────────────────
// The root: no parent, running at scale 1. GameSystem passes (12, 2,
// false, true). The ROM stores m_state 0 twice (Clock's inline
// constructor, then its own).
// FS2 0x00135390

MasterClock::MasterClock(int ticksPerFrame, int interval, bool variableRate, bool flag1c)
    : Clock(NULL), m_flag1c(flag1c), m_ticksPerFrame(ticksPerFrame), m_vsyncTotal(0),
      m_interval(interval), m_variableRate(variableRate)
{
    m_state = ClockState_Running;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001353d0

float Clock::getDeltaSeconds()
{
    return (float)m_delta / kClockDeltaPerSecond;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00135420

float Clock::getSeconds()
{
    return (float)m_time / (float)(u_int)kClockTimePerSecond;
}

// ──────────────────────────────────────────────────────────────────────
// ticks * m_scale >> 8 (0 while paused) goes onto m_time and m_time2 and
// becomes m_delta; every child then advances by that delta, so the scales
// multiply down the tree.
// FS2 0x001354a0

void Clock::advance(u_int ticks)
{
    int scale = m_scale;
    if (m_state == ClockState_Paused)
        scale = 0;
    u_int delta = (ticks * scale) >> 8;
    m_time += delta;
    m_delta = delta;
    m_time2 += delta;
    if (m_children != NULL) {
        void *child = NULL;
        while ((child = m_children->next(child)) != NULL)
            static_cast<Clock *>(child)->advance(delta);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Detaches the children (each leaves this list and loses its parent),
// frees the children list, then leaves the parent's list, which is freed
// when it empties.
//
// As the ROM does it: removing a child clears its slot, so next(child)
// no longer finds it and the walk ends after the FIRST child -- any
// others keep m_parent pointing at this clock. When that first child was
// the last one, the list is freed (m_children null) and the ROM calls
// next on the null list (reading EE address 4); the port stops there.
// The ROM also leaves m_children pointing at the freed list after the
// delete (harmless: this is going away).
// FS2 0x00135550
// callcheck: next, free, operator_delete, free, operator_delete, operator_delete, free, operator_delete (the PtrList destructors inline; the deleting destructor's flag)

ClockNode::~ClockNode()
{
    if (m_children != NULL) {
        void *after = NULL;
        ClockNode *child;
        while (m_children != NULL
               && (child = static_cast<ClockNode *>(m_children->next(after))) != NULL) {
            after = child;
            ClockNode *parent = child->m_parent;
            if (parent != NULL) {
                if (parent->m_children != NULL && parent->m_children->remove(child) == 0) {
                    delete parent->m_children;
                    parent->m_children = NULL;
                }
                child->m_parent = NULL;
            }
        }
        if (m_children != NULL)
            delete m_children;
    }
    if (m_parent != NULL && m_parent->m_children != NULL) {
        if (m_parent->m_children->remove(this) == 0) {
            delete m_parent->m_children;
            m_parent->m_children = NULL;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001357b0

void Clock::resetTime()
{
    m_time = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001357c0

void *PtrList::next(void *after)
{
    if (m_data == NULL)
        return NULL;
    u_int i = 0;
    if (after != NULL) {
        for (; i < m_capacity; i++) {
            if (m_data[i] == after) {
                i++;
                break;
            }
        }
    }
    for (; i < m_capacity; i++) {
        if (m_data[i] != NULL)
            return m_data[i];
    }
    return NULL;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00135860
// callcheck: Heap::realloc, Heap::alloc

u_short PtrList::add(void *p, int grow)
{
    if ((int)m_count >= (int)m_capacity) {
        u_int size = (m_capacity + grow) * kPtrListSlotSize;
        if (m_data == NULL)
            m_data = (void **)gGlobalHeap.alloc(size, kPtrListAlign);
        else
            m_data = (void **)gGlobalHeap.realloc(m_data, size);
        for (u_int i = m_capacity; i < (u_int)(m_capacity + grow); i++)
            m_data[i] = NULL;
        m_capacity += (u_short)grow;
    }
    for (u_int i = 0; i < m_capacity; i++) {
        if (m_data[i] == NULL) {
            m_data[i] = p;
            m_count++;
            break;
        }
    }
    return m_count;
}
