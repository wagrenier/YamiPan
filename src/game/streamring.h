/* ==========================================================================
 *  game/streamring.h
 *
 *  The byte rings the movie player streams through (game/streamring.cpp,
 *  0x151f70..0x152750). StreamRing is the interface: the data, its size
 *  and the threshold at which a reader counts as primed; the slots say how
 *  much is there and move it. FileStreamRing fills itself from an rx ring
 *  stream (rxSysStreamOpenRing: the rx file thread reads the file into the
 *  ring); MovieRing (game/movieplayer.h) is filled by hand.
 *
 *  StreamRing's base slots are emitted elsewhere: contiguous / readPtr /
 *  consume / poll / endPos in sound/sound.cpp (0x1461c0..0x146200), filled /
 *  freeSpace / write / readPos in game/spkrom.cpp's range (0x1530d0..
 *  0x153100); all of them answer 0 / null or do nothing.
 *
 *  The file also keeps a table of the open ring streams' ids (8 places,
 *  data 0x2f5a80) and their count (sbss gp-0x7a90); nothing reads either
 *  (debug bookkeeping).
 * ======================================================================== */
#ifndef _GAME_STREAMRING_H
#define _GAME_STREAMRING_H

#include "sdk/scetypes.h"

// layout: host -- EE 0x14, vtable 0x3112c0 (10 slots) at +0x10, after the
// data (CodeWarrior).
class StreamRing
{
public:
    // mem null allocates the ring from gGlobalHeap (64-aligned, owned);
    // either way it is zeroed. threshold < 0 is size * 7 / 8.
    StreamRing(int size, void *mem, int threshold);                      // FS2 0x00152620
    virtual ~StreamRing();                                               // FS2 0x00152590: deleting (obj, flag)
    virtual int     filled();                                            // FS2 0x001530d0: bytes to read
    virtual int     contiguous();                                        // FS2 0x001461f0: of those, before the wrap
    virtual int     freeSpace();                                         // FS2 0x001530e0
    virtual void    write(const void *src, int size);                    // FS2 0x001530f0
    virtual u_char *readPtr();                                           // FS2 0x001461e0
    virtual int     readPos();                                           // FS2 0x00153100
    virtual int     consume(int size);                                   // FS2 0x001461d0: the consumed total
    virtual int     poll();                                              // FS2 0x00146200: fetch more (file rings)
    virtual int     endPos();                                            // FS2 0x001461c0: the consumed total at the end

    // filled() reached the threshold.
    bool isPrimed();                                                     // FS2 0x00151f90
    // Frees the data if owned and zeroes the ring.
    void release();                                                      // FS2 0x00152530
    int  size();                                                         // FS2 0x00152580

    int     m_size;             // FS2 +0x0
    int     m_threshold;        // FS2 +0x4
    u_char *m_data;             // FS2 +0x8
    u_char  m_owned;            // FS2 +0xc: m_data from Heap::alloc

private:
    // Inline in both constructors: release, then take (or allocate) the
    // ring and zero it.
    void setBuffer(int size, void *mem, int threshold);
};

// layout: host -- EE 0x24, vtable 0x311290.
// Every slot answers -1 (null for readPtr) while no stream is open.
class FileStreamRing : public StreamRing
{
public:
    // Paused, no stream.
    FileStreamRing(int size, void *mem, int threshold);                  // FS2 0x00152400
    // Frees the ring, then closes the stream (the ROM's order).
    virtual ~FileStreamRing();                                           // FS2 0x001522a0: deleting (obj, flag)
    virtual int     filled();                                            // FS2 0x00152130: rxFileGetRingFilled
    virtual int     contiguous();                                        // FS2 0x001520b0: up to the ring's end
    virtual int     freeSpace();                                         // FS2 0x00151fe0
    virtual void    write(const void *src, int size);                    // FS2 0x00151fd0: empty (the rx thread writes)
    virtual u_char *readPtr();                                           // FS2 0x00152190: rxFileGetRingData
    virtual int     readPos();                                           // FS2 0x00152160: rxFileGetRingReadPos
    virtual int     consume(int size);                                   // FS2 0x00152050: rxFileRingConsume; answers size
    virtual int     poll();                                              // FS2 0x00152040: 0
    virtual int     endPos();                                            // FS2 0x00152030: m_fileSize

    // Opens name as an rx ring stream over this ring and starts it; the
    // stream id, < 0 failed (-1 when the file is missing).
    int open(const char *name);                                          // FS2 0x001521c0

    int    m_stream;            // FS2 +0x14: rx stream id, -1 none
    u_char m_paused;            // FS2 +0x18: 1 until open starts the stream
    int    m_fileSize;          // FS2 +0x1c: endPos()
    int    m_consumed;          // FS2 +0x20: total

private:
    // Inline in open / the destructor.
    void resume();
    void close();
};

#endif /* _GAME_STREAMRING_H */
