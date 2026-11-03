// game/perfmeter.cpp -- PerfMeter, the debug frame meter (game/perfmeter.h).

#include "game/perfmeter.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

#include "game/gamesystem.h"
#include "sdk/fileio.h"
#include "sdk/scetypes.h"

// The RcntTimer under the meter: RCNT1 counts PAL H-blanks.
constexpr float kPalHblankRate = 15625.0f;
constexpr float kPalFieldRate = 50.0f;

// writeCsv prints every value as a percentage.
constexpr float kPercent = 100.0f;

// sceOpen's mode for the CSV (rw-rw-rw-).
constexpr int kCsvFileMode = 0666;

// ──────────────────────────────────────────────────────────────────────
// Empty in the retail build (GameSystem::flipFrame calls it in meter mode
// MeterMode_Bars).
// FS2 0x00132f20

void PerfMeter::drawDetail(sceVif1Packet *pkt, int group)
{
}

// ──────────────────────────────────────────────────────────────────────
// Looks for a bar in each group; what it did with one (a debug print, by
// the shape) was compiled out of the retail build.
// FS2 0x00132f30

void PerfMeter::dumpGroups()
{
    for (int group = 0; group < kPerfMeterGroupCount; group++) {
        bool found = false;
        for (u_int i = 0; i < m_barCount; i++) {
            if (group == m_bars[i].group && !found)
                found = true;
        }
    }
}

// One "min,max,mean," triple of a stat cell, as percentages, to the CSV.
// Inline in the original.

void PerfMeter::writeCsvCell(int fd, char *line, int cell, const char *format)
{
    sprintf(line, format, m_statMin[cell] * kPercent, m_statMax[cell] * kPercent,
            m_statSum[cell] * kPercent / (float)m_framesPerSample);
    sceWrite(fd, line, strlen(line));
}

// ──────────────────────────────────────────────────────────────────────
// The recorded samples to host:la_<major>_<minor>.csv, one line a sample:
// a triple per timing mark, then the frame's last mark and the standalone
// sum; then starts over. The callers (0x17f7c0, 0x17f890) stop the
// recording first and pass two ids from 0x23bb10.
// FS2 0x00132f90
// callcheck: sceWrite, strlen, __extendsfdf2 (writeCsvCell, inline in the original, and its float varargs promoted to double)

void PerfMeter::writeCsv(int major, int minor)
{
    char line[1024];

    if (m_sampleCount == 0)
        return;

    sprintf(line, "host:la_%d_%d.csv", major, minor);
    int fd = sceOpen(line, SCE_WRONLY | SCE_CREAT | SCE_TRUNC, kCsvFileMode);
    if (fd >= 0) {
        for (int sample = 0; sample < m_sampleCount; sample++) {
            for (u_int i = 0; i < m_barCount; i++) {
                if (m_bars[i].group == PerfMeterGroup_Detail)
                    writeCsvCell(fd, line, statCell(sample, i), "%.2f,%.2f,%.2f,");
            }
            writeCsvCell(fd, line, statCell(sample, m_maxBars), "%.2f,%.2f,%.2f,");
            writeCsvCell(fd, line, statCell(sample, m_maxBars + 1), "%.2f,%.2f,%.2f\n");
        }
        sceClose(fd);
    }
    resetStats();
}

// A value into a stat cell's maximum, minimum and sum. Inline in the
// original.

void PerfMeter::addSample(int cell, float value)
{
    if (m_statMax[cell] < value)
        m_statMax[cell] = value;
    if (value < m_statMin[cell])
        m_statMin[cell] = value;
    m_statSum[cell] += value;
}

// ──────────────────────────────────────────────────────────────────────
// Once a frame while recording: each timing mark of the group adds the time
// since the previous one to its cell, the frame's last mark goes to the
// m_maxBars cell, standalone bars add to the next; every
// m_framesPerSample frames a new sample row starts.
// FS2 0x00133350
// callcheck: addSample (inline in the original)

void PerfMeter::collectStats(int group)
{
    if (!m_recording || m_sampleCount >= kPerfMeterMaxSamples)
        return;

    float last = 0.0f;
    int marks = 0;
    for (u_int i = 0; i < m_barCount; i++) {
        const PerfMeterBar &bar = m_bars[i];
        if (group == bar.group) {
            addSample(statCell(m_sampleCount, i), bar.value - last);
            last = bar.value;
            marks++;
        } else if (bar.group == PerfMeterGroup_Standalone) {
            addSample(statCell(m_sampleCount, m_maxBars + 1), bar.value);
        }
    }
    if (marks != 0)
        addSample(statCell(m_sampleCount, m_maxBars), last);

    if (++m_sampleFrame >= m_framesPerSample) {
        m_sampleFrame = 0;
        m_sampleCount++;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Empties the sample table: minimums FLT_MAX, maximums and sums 0. Caller
// 0x12cce0 (with Heap::setMark 1).
// FS2 0x00133590

void PerfMeter::resetStats()
{
    m_54 = 0;
    m_sampleCount = 0;
    m_sampleFrame = 0;
    for (int i = 0; i < m_statCells; i++) {
        m_statMin[i] = FLT_MAX;
        m_statMax[i] = 0.0f;
        m_statSum[i] = 0.0f;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Empty in the retail build.
// FS2 0x00133600

void PerfMeter::drawBars(sceVif1Packet *pkt)
{
}

// ──────────────────────────────────────────────────────────────────────
// Empty in the retail build: no bar is ever stored, so m_barCount stays 0.
// FS2 0x00133610

int PerfMeter::addBar(float value, u_int colour, int group, const char *label)
{
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00133620

void PerfMeter::beginFrame()
{
    m_barCount = 0;
    m_38 = 0;
    reset();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00133630
// callcheck: operator_delete[], ~RcntTimer, operator_delete (the delete-expressions, the base destructor and the deleting variant)

PerfMeter::~PerfMeter()
{
    delete[] m_bars;
    delete[] m_statMin;
    delete[] m_statMax;
    delete[] m_statSum;
}

// ──────────────────────────────────────────────────────────────────────
// The stat tables stay unallocated (m_statCells 0): nothing in the retail
// build sizes them.
// FS2 0x001336e0
// callcheck: RcntTimer, operator_new[] (the base constructor, the new-expression)

PerfMeter::PerfMeter(int maxBars, int x, int y, int w, int h)
    : RcntTimer(kPalHblankRate, kPalFieldRate, kVsyncsPerFrame)
{
    m_x = x;
    m_y = y;
    m_w = w;
    m_h = h;
    m_maxBars = maxBars;
    m_barCount = 0;
    m_38 = 0;
    m_bars = new PerfMeterBar[maxBars];
    m_recording = false;
    m_framesPerSample = 0;
    m_statCells = 0;
    m_statMin = 0;
    m_statMax = 0;
    m_statSum = 0;
    resetStats();
}
