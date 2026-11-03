/* ==========================================================================
 *  game/perfmeter.h
 *
 *  PerfMeter (game/perfmeter.cpp, not written yet): the debug CPU / heap
 *  meter GameSystem holds at +0x4a8, on an RcntTimer (RCNT1 counting
 *  H-blanks, 64 us a tick). The retail build keeps its bookkeeping
 *  (beginFrame, collectStats, dumpGroups) but its bar and draw functions are
 *  empty, so nothing of it reaches the screen. Minimal.
 * ======================================================================== */
#ifndef _GAME_PERFMETER_H
#define _GAME_PERFMETER_H

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

void rcnt1Start();                                                       // FS2 0x0012f260

// The group numbers callers pass; names are guesses from those callers.
enum PerfMeterGroup
{
    PerfMeterGroup_Thread  = 0, // gameThreadSwitchHook
    PerfMeterGroup_Packet  = 1, // packet buffer use
    PerfMeterGroup_Detail  = 4, // collectStats / drawDetail
    PerfMeterGroup_Heap    = 6, // heap marks 2 and 1
    PerfMeterGroup_HeapAll = 8, // heap mark 0
};

// Bar colours, GS RGBA as a little-endian word (0xAABBGGRR).
enum PerfMeterColour : u_int
{
    PerfMeterColour_Blue    = 0x80ff0000,
    PerfMeterColour_Green   = 0x8000ff00,
    PerfMeterColour_Red     = 0x800000ff,
    PerfMeterColour_Magenta = 0x80ff00ff,
    PerfMeterColour_Cyan    = 0x80ffff00,
    PerfMeterColour_Yellow  = 0x8000ffff,
    PerfMeterColour_White   = 0x80ffffff,
};

// layout: host -- not read yet; EE 0x20 (PerfMeter's own fields start at +0x20).
class RcntTimer
{
public:
    u_char _00[0x20];
};

// layout: host -- holds the bar array (+0x3c, new[maxBars * 0x10]); EE 0x68.
class PerfMeter : public RcntTimer
{
public:
    PerfMeter(int maxBars, int x, int y, int w, int h);                  // FS2 0x001336e0
    ~PerfMeter();                                                        // FS2 0x00133630

    void beginFrame();                                                   // FS2 0x00133620
    void collectStats(int group);                                        // FS2 0x00133350
    void dumpGroups();                                                   // FS2 0x00132f30
    int  addBar(float value, u_int colour, int group, const char *label); // FS2 0x00133610
    void drawBars(sceVif1Packet *pkt);                                   // FS2 0x00133600
    void drawDetail(sceVif1Packet *pkt, int group);                      // FS2 0x00132f20

    u_char _20[0x68 - 0x20];    // FS2 +0x20: x, y, w, h, maxBars +0x30, bars +0x3c, ...
};

#endif /* _GAME_PERFMETER_H */
