/* ==========================================================================
 *  game/perfmeter.h
 *
 *  PerfMeter (game/perfmeter.cpp): the debug CPU / heap meter GameSystem
 *  holds at +0x4a8, on an RcntTimer (RCNT1 counting H-blanks, 64 us a
 *  tick). The retail build keeps its bookkeeping (beginFrame, collectStats,
 *  writeCsv) but its bar and draw functions are empty, so nothing of it
 *  reaches the screen and the bar list stays empty.
 *
 *  The statistics are a table of samples, each row m_maxBars + 2 cells:
 *  cell i for bar i, cell m_maxBars for the last timing mark of the frame,
 *  cell m_maxBars + 1 for the sum of the standalone bars. Each cell keeps a
 *  minimum, a maximum and a sum (three parallel float arrays) over
 *  m_framesPerSample frames.
 * ======================================================================== */
#ifndef _GAME_PERFMETER_H
#define _GAME_PERFMETER_H

#include "game/rcnttimer.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// The group numbers callers pass; names are guesses from those callers.
enum PerfMeterGroup
{
    PerfMeterGroup_Thread     = 0, // gameThreadSwitchHook
    PerfMeterGroup_Packet     = 1, // packet buffer use
    PerfMeterGroup_Standalone = 2, // collectStats sums these into their own cell
    PerfMeterGroup_Detail     = 4, // timing marks: the game loop's stages (0x198970, 0x199400)
    PerfMeterGroup_Heap       = 6, // heap marks 2 and 1
    PerfMeterGroup_HeapAll    = 8, // heap mark 0
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

// dumpGroups walks the groups below this.
constexpr int kPerfMeterGroupCount = 8;

// collectStats stops recording after this many samples.
constexpr int kPerfMeterMaxSamples = 300;

// layout: host -- EE 0x10 (a pointer). Only the group (+0xc) is read in the
// retail build (addBar is empty); the order of colour and label is a guess.
struct PerfMeterBar
{
    float       value;          // FS2 +0x0: a timing mark's time, or a level
    u_int       colour;         // FS2 +0x4: PerfMeterColour
    const char *label;          // FS2 +0x8
    int         group;          // FS2 +0xc: PerfMeterGroup
};

// layout: host -- EE 0x68 (pointers and the RcntTimer base).
class PerfMeter : public RcntTimer
{
public:
    PerfMeter(int maxBars, int x, int y, int w, int h);                  // FS2 0x001336e0
    ~PerfMeter();                                                        // FS2 0x00133630

    void beginFrame();                                                   // FS2 0x00133620
    void collectStats(int group);                                        // FS2 0x00133350
    void resetStats();                                                   // FS2 0x00133590
    void writeCsv(int major, int minor);                                 // FS2 0x00132f90
    void dumpGroups();                                                   // FS2 0x00132f30
    int  addBar(float value, u_int colour, int group, const char *label); // FS2 0x00133610
    void drawBars(sceVif1Packet *pkt);                                   // FS2 0x00133600
    void drawDetail(sceVif1Packet *pkt, int group);                      // FS2 0x00132f20

    int           m_x;                // FS2 +0x20
    int           m_y;                // FS2 +0x24
    int           m_w;                // FS2 +0x28
    int           m_h;                // FS2 +0x2c
    int           m_maxBars;          // FS2 +0x30
    u_int         m_barCount;         // FS2 +0x34
    int           m_38;               // FS2 +0x38: zeroed with m_barCount, never read in retail
    PerfMeterBar *m_bars;             // FS2 +0x3c: new[m_maxBars]
    bool          m_recording;        // FS2 +0x40: collectStats records only while set
    float        *m_statMin;          // FS2 +0x44: [m_statCells]
    float        *m_statMax;          // FS2 +0x48
    float        *m_statSum;          // FS2 +0x4c
    int           m_sampleFrame;      // FS2 +0x50: frames into the current sample
    int           m_54;               // FS2 +0x54: zeroed with the counters, never read in retail
    int           m_sampleCount;      // FS2 +0x58
    int           m_statCells;        // FS2 +0x5c: 0 in retail (nothing allocates the tables)
    int           m_framesPerSample;  // FS2 +0x60
    u_int         _64;                // FS2 +0x64

private:
    // The cell of a sample's column in the three stat tables.
    int statCell(int sample, int column) const { return column + sample * (m_maxBars + 2); }
    void addSample(int cell, float value);
    void writeCsvCell(int fd, char *line, int cell, const char *format);
};

#endif /* _GAME_PERFMETER_H */
