// game/streamring.cpp -- the movie player's byte rings (game/streamring.h).
//
// The game calls rx through jump thunks of its own in this file
// (0x151f70 rxThEnableDispatch, 0x151f80 rxThDisableDispatch, 0x152730
// rxFileCallLoadHook, 0x152740 rxFileCallSizeHook); the port calls the rx
// functions (rx/rxthread.h, rx/rxfile.h), so they have no code here.

#include "game/streamring.h"

#include <string.h>

#include "game/heap.h"
#include "rx/rxfile.h"
#include "rx/rxsys.h"
#include "sdk/scetypes.h"

enum
{
    kRingAlign      = 0x40,     // gGlobalHeap alignment of an owned ring
    kMaxRingStreams = 8,
};

// The open ring streams' ids, -1 a free place; a ninth stream is counted
// but not listed. Nothing reads them.
static int sRingStreams[kMaxRingStreams] = {                             // FS2 data 0x002f5a80
    -1, -1, -1, -1, -1, -1, -1, -1,
};
static int sRingStreamCount;                                             // FS2 sbss gp-0x7a90

static void addRingStream(int stream)
{
    for (int i = 0; i < kMaxRingStreams; i++) {
        if (sRingStreams[i] < 0) {
            sRingStreams[i] = stream;
            break;
        }
    }
    sRingStreamCount++;
}

static void removeRingStream(int stream)
{
    for (int i = 0; i < kMaxRingStreams; i++) {
        if (sRingStreams[i] == stream) {
            sRingStreams[i] = -1;
            break;
        }
    }
    sRingStreamCount--;
}

// ──────────────────────────────────────────────────────────────────────
// StreamRing

// FS2 0x00151f90
// callcheck: filled (a vtable slot)

bool StreamRing::isPrimed()
{
    return m_threshold <= filled();
}

// FS2 0x00152530

void StreamRing::release()
{
    if (m_owned && m_data != NULL) {
        gGlobalHeap.free(m_data);
    }
    m_owned = 0;
    m_size = 0;
    m_threshold = 0;
    m_data = NULL;
}

// FS2 0x00152580

int StreamRing::size()
{
    return m_size;
}

// FS2 0x00152590
// callcheck: free, operator_delete (release inline; the deleting destructor)

StreamRing::~StreamRing()
{
    release();
}

// Inline in both constructors.
void StreamRing::setBuffer(int size, void *mem, int threshold)
{
    release();
    if (mem == NULL) {
        mem = gGlobalHeap.alloc(size, kRingAlign);
        m_owned = 1;
    }
    memset(mem, 0, size);
    m_size = size;
    if (threshold < 0) {
        m_threshold = m_size * 7 / 8;
    } else {
        m_threshold = threshold;
    }
    m_data = (u_char *)mem;
}

// FS2 0x00152620
// callcheck: free, alloc, memset (setBuffer inline)

StreamRing::StreamRing(int size, void *mem, int threshold)
    : m_size(0), m_threshold(0), m_data(NULL), m_owned(0)
{
    setBuffer(size, mem, threshold);
}

// ──────────────────────────────────────────────────────────────────────
// FileStreamRing

// FS2 0x00151fd0

void FileStreamRing::write(const void *src, int size)
{
}

// FS2 0x00151fe0
// callcheck: rxFileGetRingFilled

int FileStreamRing::freeSpace()
{
    if (m_stream < 0) {
        return -1;
    }
    return m_size - rxFileGetRingFilled(m_stream);
}

// FS2 0x00152030

int FileStreamRing::endPos()
{
    return m_fileSize;
}

// FS2 0x00152040

int FileStreamRing::poll()
{
    return 0;
}

// FS2 0x00152050
// callcheck: rxFileRingConsume

int FileStreamRing::consume(int size)
{
    if (m_stream < 0) {
        return -1;
    }
    rxFileRingConsume(m_stream, size);
    m_consumed += size;
    return size;
}

// What can be read in one piece: the filled bytes, cut at the ring's end.
// FS2 0x001520b0
// callcheck: rxFileGetRingFilled, rxFileGetRingReadPos

int FileStreamRing::contiguous()
{
    if (m_stream < 0) {
        return -1;
    }
    int n = rxFileGetRingFilled(m_stream);
    int toEnd = m_size - rxFileGetRingReadPos(m_stream);
    if (toEnd < n) {
        n = toEnd;
    }
    if (n < 1) {
        n = 0;
    }
    return n;
}

// FS2 0x00152130
// callcheck: rxFileGetRingFilled

int FileStreamRing::filled()
{
    if (m_stream < 0) {
        return -1;
    }
    return rxFileGetRingFilled(m_stream);
}

// FS2 0x00152160
// callcheck: rxFileGetRingReadPos

int FileStreamRing::readPos()
{
    if (m_stream < 0) {
        return -1;
    }
    return rxFileGetRingReadPos(m_stream);
}

// FS2 0x00152190
// callcheck: rxFileGetRingData

u_char *FileStreamRing::readPtr()
{
    if (m_stream < 0) {
        return NULL;
    }
    return rxFileGetRingData(m_stream);
}

// Inline in open.
void FileStreamRing::resume()
{
    if (m_stream >= 0) {
        rxFilePause(m_stream, 0);
        m_paused = 0;
    }
}

// The size first (a missing file fails before any stream is opened), then
// the ring stream; the stream starts at once.
// FS2 0x001521c0
// callcheck: rxFileCallSizeHook, rxSysStreamOpenRing, rxFilePause (resume inline)

int FileStreamRing::open(const char *name)
{
    m_fileSize = rxFileCallSizeHook(name);
    if (m_fileSize < 0) {
        return -1;
    }
    m_stream = rxSysStreamOpenRing(name, m_data, m_size, m_threshold);
    if (m_stream >= 0) {
        resume();
    }
    if (m_stream >= 0) {
        addRingStream(m_stream);
    }
    return m_stream;
}

// Inline in the destructor.
void FileStreamRing::close()
{
    if (m_stream >= 0) {
        rxFileClose(m_stream);
        m_paused = 0;
        m_fileSize = 0;
        m_consumed = 0;
        if (m_stream >= 0) {
            removeRingStream(m_stream);
        }
        m_stream = -1;
    }
}

// The ROM frees an owned ring before it closes the stream reading into
// it (kept; the memory stays in gGlobalHeap until reused).
// FS2 0x001522a0
// callcheck: free, rxFileClose, free, operator_delete (release / close inline, then the base's destructor)

FileStreamRing::~FileStreamRing()
{
    release();
    close();
}

// FS2 0x00152400
// callcheck: free, alloc, memset (the base's constructor with setBuffer inline)

FileStreamRing::FileStreamRing(int size, void *mem, int threshold)
    : StreamRing(size, mem, threshold),
      m_stream(-1), m_paused(1), m_fileSize(0), m_consumed(0)
{
}
