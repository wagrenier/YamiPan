/* ==========================================================================
 *  game/fileloader.h
 *
 *  Loading files on the job thread (game/fileloader.cpp, 0x1319f0..
 *  0x132f20):
 *  - FileJob: one file, read whole through the rx file hooks into
 *    gGlobalHeap or a caller's buffer (FileJob::start queues it with
 *    jobAdd, the caller polls m_state or waits);
 *  - FileLoader: a list of files loaded by one job (addFile each,
 *    startLoad, waitLoaded, then getData / getSize by name: the first
 *    listed path containing it, or by index);
 *  - PackFile: the same interface over one pack file (dataEU/pack*.pak),
 *    loaded by its PackJob base (load) and read through a PakArchive,
 *    SLDI-decompressed first when the caller gave it a buffer;
 *  - TextTokenizer: whitespace-separated words of a text in memory.
 *  FileJobState is also the answer of the loaders' getState slots.
 * ======================================================================== */
#ifndef _GAME_FILELOADER_H
#define _GAME_FILELOADER_H

#include <stddef.h>

#include "game/pakarchive.h"
#include "sdk/scetypes.h"

// FileJob::m_state, FileLoader::m_state and the getState answers.
enum FileJobState
{
    FileJob_Idle = 0,
    FileJob_Queued = 1,
    FileJob_Loading = 2,
    FileJob_Done = 3,           // m_data holds the file (null when it is empty)
    FileJob_Failed = 0x80,      // bit: 0x80 not found, 0x81 no memory
    FileJob_NoMemory = 0x81,
    FileJob_Failed82 = 0x82,    // never set in this file; counted as settled
    FileJob_NoEntry = 0x83,     // getState / getItemState: nothing listed under it
};

// True once a job has stopped: idle, done or failed (1 / 2 and any value
// not listed above are still running).
inline bool fileJobSettled(int state)
{
    return state == FileJob_NoEntry || state == FileJob_Failed82 || state == FileJob_NoMemory ||
           state == FileJob_Failed || state == FileJob_Done || state == FileJob_Idle;
}

// One file loaded by a job: m_dir + m_path first, then m_path, through the
// rx file hooks; an empty file is done with no data.
// layout: host -- EE 0x28.
class FileJob
{
public:
    FileJob(const char *path, int index);                                // FS2 0x00132cc0: path copied to new[]
    ~FileJob();                                                          // FS2 0x00132c10: deleting (obj, 1)
    // jobAdd(run, this); queued (state 1) when it answers non-zero.
    int start();                                                         // FS2 0x00132bc0
    // threadYield(1) until the job settles, with the loading indicator
    // when showLoading (arg unused).
    void wait(int arg, bool showLoading);                                // FS2 0x00132950
    static int run(void *job);                                           // FS2 0x00132a40

    int         m_state;        // FS2 +0x00: FileJobState
    char       *m_path;         // FS2 +0x04
    int         m_index;        // FS2 +0x08: FileLoader's entry index (standalone jobs: 0)
    int         m_size;         // FS2 +0x0c
    u_char      m_notOwned;     // FS2 +0x10: the destructor leaves m_data alone
    void       *m_data;         // FS2 +0x14: gGlobalHeap unless m_buffer is given
    int         m_jobId;        // FS2 +0x18
    const char *m_dir;          // FS2 +0x1c: tried first as a prefix
    void       *m_buffer;       // FS2 +0x20: the caller's buffer, if any
    int         m_bufferOffset; // FS2 +0x24: negative: the file ends that far into it
};

// A FileLoader's list entry: the job and the next entry.
// layout: host -- EE 0x2c.
struct FileEntry : public FileJob
{
    FileEntry(const char *path, int index) : FileJob(path, index), m_next(NULL) {}

    FileEntry *m_next;          // FS2 +0x28
};

// A list of files loaded together by one job (vtable 0x3110b0, 18 slots).
// layout: host -- EE 0x9c with the vptr at +0x98.
class FileLoader
{
public:
    explicit FileLoader(int arg);                                        // FS2 0x00132920
    virtual ~FileLoader();                                               // FS2 0x001328b0: slot 0, deleting
    virtual bool        isPack();                                        // FS2 0x00132ef0: slot 1
    // Lists m_prefix + name (once: a name findIndex knows answers its
    // index); answers the index.
    virtual int         addFile(const char *name);                       // FS2 0x001327a0: slot 2
    virtual int         findIndex(const char *name);                     // FS2 0x00132710: slot 3, -1 without one
    virtual const char *getName(int index);                              // FS2 0x001326c0: slot 4
    virtual void       *getData(const char *name);                       // FS2 0x00132630: slot 5
    virtual void       *getItem(int index);                              // FS2 0x001325e0: slot 6
    virtual int         getSize(const char *name);                       // FS2 0x00132550: slot 7, 0 without one
    virtual int         getItemSize(int index);                          // FS2 0x00132500: slot 8, 0 without one
    virtual int         unloadItem(int index);                           // FS2 0x00132470: slot 9, -1 without one
    virtual int         unload(const char *name);                        // FS2 0x001323b0: slot 10, -1 without one
    virtual int         getState(const char *name);                      // FS2 0x00132320: slot 11, FileJobState
    virtual int         getItemState(int index);                         // FS2 0x001322d0: slot 12, FileJobState
    virtual int         getCount();                                      // FS2 0x00132f00: slot 13
    virtual bool        isLoaded();                                      // FS2 0x00132170: slot 14
    virtual void        waitLoaded(int arg, bool showLoading);           // FS2 0x001320f0: slot 15 (arg unused)
    virtual void        clear();                                         // FS2 0x00131ea0: slot 16
    virtual void        compact();                                       // FS2 0x00132f10: slot 17
    // Queues the load job; 0 while no job slot is free.
    int startLoad();                                                     // FS2 0x00132280

    int        m_state;         // FS2 +0x00: FileJobState of the whole list
    FileEntry *m_head;          // FS2 +0x04
    FileEntry *m_tail;          // FS2 +0x08
    int        m_count;         // FS2 +0x0c
    int        m_jobId;         // FS2 +0x10
    int        m_arg;           // FS2 +0x14: the constructor's argument (unread)
    char       m_prefix[0x80];  // FS2 +0x18: put before every added name (set outside this file)

private:
    void clearFiles();                                                   // FS2 0x00132000
    static int loadJob(void *loader);                                    // FS2 0x001321f0
    FileEntry *findEntry(int index);                                     // inlined in the ROM
    FileEntry *findEntry(const char *name);                              // inlined in the ROM
};

// The loading half of a PackFile (its second base, at +0x9c): the job
// reading the file and the archive over its data. EE 0x4c.
// layout: host
class PackJob : public FileJob
{
public:
    PackJob(const char *path, int arg);
    // Starts the job when idle and waits for it (the loading indicator when
    // showLoading), decompresses into m_unpackBuffer when there is one,
    // opens the archive; true when it holds any item (arg unused).
    bool load(int arg, bool showLoading);                                // FS2 0x00131a30

    PakArchive m_pak;           // FS2 +0x28 (PackFile +0xc4)
    int        _34;             // FS2 +0x34: the constructor's argument (unread)
    int        m_result;        // FS2 +0x38: the constructor's argument, then the archive's m_result
    void      *m_unpackBuffer;  // FS2 +0x3c: sldiDecompress's destination
    int        m_variant;       // FS2 +0x40: PakArchive::open's variant (-1 every item)
    int        _44;             // FS2 +0x44: the constructor's argument (unread)
    int        _48;             // FS2 +0x48: the constructor's argument (unread)
};

// A pack file (dataEU/pack*.pak; Vt_00311060, 18 slots): FileLoader's slots
// answered from the archive. A caller-given buffer (setBuffer) is the
// pack's: the job reads the packed file into its end and load unpacks it
// to its start; the destructor frees it.
// layout: host -- EE 0xf8: FileLoader 0..0x9c, PackJob +0x9c, own fields
// from +0xe8.
class PackFile : public FileLoader, public PackJob
{
public:
    PackFile(const char *path, int arg);                                 // FS2 0x00131f00
    virtual ~PackFile();                                                 // FS2 0x00131d80: slot 0, deleting
    virtual bool        isPack();                                        // FS2 0x001319f0: slot 1
    virtual int         addFile(const char *name);                       // FS2 0x00131d60: slot 2, only finds it
    virtual int         findIndex(const char *name);                     // FS2 0x00131d50: slot 3
    virtual const char *getName(int index);                              // FS2 0x00131d40: slot 4
    virtual void       *getData(const char *name);                       // FS2 0x00131d30: slot 5, the first item whose name contains it
    virtual void       *getItem(int id);                                 // FS2 0x00131d20: slot 6, null without one
    virtual int         getSize(const char *name);                       // FS2 0x00131d10: slot 7, -1 without one
    virtual int         getItemSize(int id);                             // FS2 0x00131d00: slot 8, -1 without one
    virtual int         unloadItem(int index);                           // FS2 0x00131a20: slot 9, nothing
    virtual int         unload(const char *name);                        // FS2 0x00131a10: slot 10, nothing
    virtual int         getState(const char *name);                      // FS2 0x00131cd0: slot 11
    virtual int         getItemState(int id);                            // FS2 0x00131ca0: slot 12
    virtual int         getCount();                                      // FS2 0x00131c90: slot 13
    virtual bool        isLoaded();                                      // FS2 0x00131a00: slot 14, always
    virtual void        waitLoaded(int arg, bool showLoading);           // FS2 0x00131c80: slot 15, nothing
    virtual void        clear();                                         // FS2 0x00131c70: slot 16, nothing
    virtual void        compact();                                       // FS2 0x00131c50: slot 17
    // PackJob::load on the second base; every loader passes (0, false).
    bool load(int arg, bool showLoading);                                // FS2 0x00131d70
    // The pack's buffer (gGlobalHeap, freed with the pack) and the variant
    // its items are kept for; the buffer only reaches the job while it is
    // idle.
    void setBuffer(void *buffer, int size, int variant);                 // FS2 0x00131eb0

    u_char m_ownsBuffer;        // FS2 +0xe8
    void  *m_packBuffer;        // FS2 +0xec
    int    m_packBufferSize;    // FS2 +0xf0
    int    m_packVariant;       // FS2 +0xf4
};

// Splits a text in memory into whitespace-separated words, one per next
// into the word buffer (longer words are cut at capacity - 1 and the rest
// is the next word). Its one user is 0x1a3a60.
// layout: host -- EE 0x18.
class TextTokenizer
{
public:
    explicit TextTokenizer(int capacity);                                // FS2 0x00132ea0
    ~TextTokenizer();                                                    // FS2 0x00132e20: deleting (obj, short)
    void        setText(const char *text, int size);                     // FS2 0x00132df0
    // The next word in the buffer, null at the end of the text.
    const char *next();                                                  // FS2 0x00132d40

    const char *m_text;         // FS2 +0x00
    int         m_size;         // FS2 +0x04
    const char *m_cur;          // FS2 +0x08
    const char *m_end;          // FS2 +0x0c
    char       *m_word;         // FS2 +0x10: new[m_capacity]
    int         m_capacity;     // FS2 +0x14
};

#endif /* _GAME_FILELOADER_H */
