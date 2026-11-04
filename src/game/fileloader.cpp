// game/fileloader.cpp -- loading files on the job thread: FileJob,
// FileLoader, PackFile and TextTokenizer (game/fileloader.h).

#include "game/fileloader.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/sldi.h"
#include "game/titlescreen.h"
#include "game/thread.h"
#include "sdk/scetypes.h"

// Loaded files are 64-byte aligned on the global heap.
static const u_int kFileAlign = 0x40;
// A file placed at the end of a caller's buffer starts on 16 bytes.
static const u_int kBufferEndAlign = 0x10;

// The path FileJob::run asks the rx hooks for (one job runs at a time on
// the job thread). Its size is a guess: addFile builds paths in 256 bytes.
static char sJobPath[0x100];        /* bss 0x4c8b50 */

// ──────────────────────────────────────────────────────────────────────
// PackFile: FileLoader's slots over the archive. A pack is complete once
// load answered, so the loading slots do nothing.
// FS2 0x001319f0

bool PackFile::isPack()
{
    return true;
}

// FS2 0x00131a00

bool PackFile::isLoaded()
{
    return true;
}

// FS2 0x00131a10

int PackFile::unload(const char *name)
{
    (void)name;
    return 0;
}

// FS2 0x00131a20

int PackFile::unloadItem(int index)
{
    (void)index;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The job runs once (only from idle); a pack read into the end of the
// unpack buffer is SLDI-unpacked to its start and the block shrunk to the
// unpacked size. Without a buffer the archive opens on the job's data;
// with one it opens on the buffer even when unpacking failed (the open
// then fails on the magic).
// FS2 0x00131a30
// callcheck: jobAdd, loadingBegin, loadingTick, threadYield (start and wait, inline in the ROM)

bool PackJob::load(int arg, bool showLoading)
{
    if (m_state == FileJob_Idle)
        start();
    wait(arg, showLoading);

    void *data = m_data;
    if (data != NULL && m_unpackBuffer != NULL) {
        int size = sldiDecompress(data, m_unpackBuffer);
        data = m_unpackBuffer;
        if (size > 0)
            gGlobalHeap.realloc(data, size);
    }
    if (data != NULL)
        m_pak.open(data, m_variant);
    int count = m_pak.getCount();
    m_result = m_pak.m_result;
    return count != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Shrinks the pack's block to the size its header gives.
// FS2 0x00131c50

void PackFile::compact()
{
    PakHeader *data = m_pak.m_data;
    gGlobalHeap.realloc(data, data == NULL ? 0 : data->size);
}

// FS2 0x00131c70

void PackFile::clear()
{
}

// FS2 0x00131c80

void PackFile::waitLoaded(int arg, bool showLoading)
{
    (void)arg;
    (void)showLoading;
}

// FS2 0x00131c90

int PackFile::getCount()
{
    return m_pak.getCount();
}

// ──────────────────────────────────────────────────────────────────────
// Done for an item of any non-zero size; the archive's -1 for a missing
// item counts as done as well (kept: only a zero size fails).
// FS2 0x00131ca0

int PackFile::getItemState(int id)
{
    return m_pak.getItemSize(id) != 0 ? FileJob_Done : FileJob_Failed;
}

// FS2 0x00131cd0

int PackFile::getState(const char *name)
{
    return m_pak.getSize(name) != 0 ? FileJob_Done : FileJob_Failed;
}

// FS2 0x00131d00

int PackFile::getItemSize(int id)
{
    return m_pak.getItemSize(id);
}

// FS2 0x00131d10

int PackFile::getSize(const char *name)
{
    return m_pak.getSize(name);
}

// FS2 0x00131d20

void *PackFile::getItem(int id)
{
    return m_pak.getItem(id);
}

// FS2 0x00131d30

void *PackFile::getData(const char *name)
{
    return m_pak.getData(name);
}

// FS2 0x00131d40

const char *PackFile::getName(int index)
{
    return m_pak.getName(index);
}

// FS2 0x00131d50

int PackFile::findIndex(const char *name)
{
    return m_pak.findIndex(name);
}

// A pack lists nothing more: adding answers the item's index.
// FS2 0x00131d60

int PackFile::addFile(const char *name)
{
    return m_pak.findIndex(name);
}

// FS2 0x00131d70

bool PackFile::load(int arg, bool showLoading)
{
    return PackJob::load(arg, showLoading);
}

// ──────────────────────────────────────────────────────────────────────
// Frees the pack's buffer; then the PackJob base (archive, job: path, job
// slot, data unless the buffer held it) and FileLoader's clear go.
// FS2 0x00131d80
// callcheck: ~PakArchive, operator_delete[], jobCancel, free, operator_delete (the bases' destructors inline in the ROM; the deleting destructor)

PackFile::~PackFile()
{
    if (m_ownsBuffer)
        gGlobalHeap.free(m_packBuffer);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00131ea0

void FileLoader::clear()
{
    clearFiles();
}

// ──────────────────────────────────────────────────────────────────────
// The job reads the packed file into the end of the buffer (only while it
// has not started), load unpacks it to the start.
// FS2 0x00131eb0

void PackFile::setBuffer(void *buffer, int size, int variant)
{
    m_ownsBuffer = 1;
    m_packBuffer = buffer;
    m_packBufferSize = size;
    m_packVariant = variant;
    m_unpackBuffer = buffer;
    if (FileJob::m_state == FileJob_Idle) {
        m_buffer = buffer;
        m_bufferOffset = -size;
        m_notOwned = m_buffer != NULL;
    }
    m_variant = variant;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00131f00
// callcheck: PakArchive, strlen, operator_new[], strcpy (the PackJob base and its FileJob constructor, inline in the ROM)

PackFile::PackFile(const char *path, int arg)
    : FileLoader(0), PackJob(path, arg),
      m_ownsBuffer(0), m_packBuffer(NULL), m_packBufferSize(0), m_packVariant(-1)
{
}

// Inlined in PackFile::PackFile.

PackJob::PackJob(const char *path, int arg)
    : FileJob(path, 0), _34(arg), m_result(arg), m_unpackBuffer(NULL), m_variant(-1),
      _44(arg), _48(arg)
{
}

// ──────────────────────────────────────────────────────────────────────
// Cancels the list's job and deletes every entry with its file.
// FS2 0x00132000
// callcheck: operator_delete[], jobCancel, free, operator_delete (each entry's delete-expression with its inline ~FileJob)

void FileLoader::clearFiles()
{
    if (m_jobId != 0)
        jobCancel(m_jobId);
    FileEntry *entry = m_head;
    while (entry != NULL) {
        FileEntry *next = entry->m_next;
        delete entry;
        entry = next;
    }
    m_state = FileJob_Idle;
    m_head = NULL;
    m_tail = NULL;
    m_count = 0;
    m_jobId = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001320f0

void FileLoader::waitLoaded(int arg, bool showLoading)
{
    (void)arg;
    if (showLoading)
        loadingBegin();
    while (!isLoaded()) {
        if (showLoading)
            loadingTick();
        threadYield(1);
    }
}

// FS2 0x00132170

bool FileLoader::isLoaded()
{
    return fileJobSettled(m_state);
}

// ──────────────────────────────────────────────────────────────────────
// The list's job: every entry not loaded or running yet, in order.
// FS2 0x001321f0

int FileLoader::loadJob(void *arg)
{
    FileLoader *loader = (FileLoader *)arg;
    loader->m_state = FileJob_Loading;
    for (FileEntry *entry = loader->m_head; entry != NULL; entry = entry->m_next) {
        if ((u_int)entry->m_state < FileJob_Loading)
            FileJob::run(entry);
    }
    loader->m_jobId = 0;
    loader->m_state = FileJob_Done;
    return 0;
}

// FS2 0x00132280

int FileLoader::startLoad()
{
    m_jobId = jobAdd(loadJob, this);
    if (m_jobId != 0)
        m_state = FileJob_Queued;
    return m_jobId;
}

// ──────────────────────────────────────────────────────────────────────
// The entry lookups every slot below inlines: by index, or the first
// entry whose path contains name.

FileEntry *FileLoader::findEntry(int index)
{
    for (FileEntry *entry = m_head; entry != NULL; entry = entry->m_next) {
        if (index == entry->m_index)
            return entry;
    }
    return NULL;
}

FileEntry *FileLoader::findEntry(const char *name)
{
    for (FileEntry *entry = m_head; entry != NULL; entry = entry->m_next) {
        if (entry->m_path != NULL && name != NULL && strstr(entry->m_path, name) != NULL)
            return entry;
    }
    return NULL;
}

// FS2 0x001322d0

int FileLoader::getItemState(int index)
{
    FileEntry *entry = findEntry(index);
    return entry == NULL ? FileJob_NoEntry : entry->m_state;
}

// FS2 0x00132320
// callcheck: strstr (findEntry, inline)

int FileLoader::getState(const char *name)
{
    FileEntry *entry = findEntry(name);
    return entry == NULL ? FileJob_NoEntry : entry->m_state;
}

// ──────────────────────────────────────────────────────────────────────
// Drops a loaded file: its data freed unless a caller's buffer held it,
// the entry back to idle (a later startLoad reads it again).
// FS2 0x001323b0
// callcheck: strstr (findEntry, inline)

int FileLoader::unload(const char *name)
{
    FileEntry *entry = findEntry(name);
    if (entry == NULL)
        return -1;
    if (!entry->m_notOwned && entry->m_data != NULL)
        gGlobalHeap.free(entry->m_data);
    entry->m_data = NULL;
    entry->m_size = 0;
    entry->m_state = FileJob_Idle;
    return 0;
}

// FS2 0x00132470

int FileLoader::unloadItem(int index)
{
    FileEntry *entry = findEntry(index);
    if (entry == NULL)
        return -1;
    if (!entry->m_notOwned && entry->m_data != NULL)
        gGlobalHeap.free(entry->m_data);
    entry->m_data = NULL;
    entry->m_size = 0;
    entry->m_state = FileJob_Idle;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00132500

int FileLoader::getItemSize(int index)
{
    FileEntry *entry = findEntry(index);
    return entry == NULL ? 0 : entry->m_size;
}

// FS2 0x00132550
// callcheck: strstr (findEntry, inline)

int FileLoader::getSize(const char *name)
{
    FileEntry *entry = findEntry(name);
    return entry == NULL ? 0 : entry->m_size;
}

// FS2 0x001325e0

void *FileLoader::getItem(int index)
{
    FileEntry *entry = findEntry(index);
    return entry == NULL ? NULL : entry->m_data;
}

// FS2 0x00132630
// callcheck: strstr (findEntry, inline)

void *FileLoader::getData(const char *name)
{
    FileEntry *entry = findEntry(name);
    return entry == NULL ? NULL : entry->m_data;
}

// FS2 0x001326c0

const char *FileLoader::getName(int index)
{
    FileEntry *entry = findEntry(index);
    return entry == NULL ? NULL : entry->m_path;
}

// FS2 0x00132710
// callcheck: strstr (findEntry, inline)

int FileLoader::findIndex(const char *name)
{
    FileEntry *entry = findEntry(name);
    return entry == NULL ? -1 : entry->m_index;
}

// ──────────────────────────────────────────────────────────────────────
// Appends m_prefix + name to the list; a name already listed answers its
// entry's index instead.
// FS2 0x001327a0
// callcheck: operator_new, strlen, operator_new[], strcpy (the new-expression and the inlined FileJob constructor)

int FileLoader::addFile(const char *name)
{
    int index = findIndex(name);
    if (index >= 0)
        return index;

    char path[0x100];
    sprintf(path, "%s%s", m_prefix, name);
    FileEntry *entry = new FileEntry(path, m_count);
    if (m_tail != NULL)
        m_tail->m_next = entry;
    if (m_count == 0)
        m_head = entry;
    m_count++;
    m_tail = entry;
    entry->m_dir = NULL;
    return m_count - 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001328b0
// callcheck: operator_delete (the deleting destructor)

FileLoader::~FileLoader()
{
    clear();
}

// FS2 0x00132920

FileLoader::FileLoader(const char *dir)
    : m_state(FileJob_Idle), m_head(NULL), m_tail(NULL), m_count(0), m_jobId(0), m_dir(dir)
{
    m_prefix[0] = '\0';
}

// ──────────────────────────────────────────────────────────────────────
// The indicator is asked twice on the first waiting frame (kept).
// FS2 0x00132950

void FileJob::wait(int arg, bool showLoading)
{
    (void)arg;
    bool first = true;
    while (!fileJobSettled(m_state)) {
        if (first) {
            if (showLoading)
                loadingBegin();
            first = false;
        }
        if (showLoading)
            loadingTick();
        threadYield(1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Reads the file whole: m_dir + m_path when that exists, else m_path. An
// empty file is done with no data; a missing one fails. The data goes to
// gGlobalHeap, or into the caller's buffer at m_bufferOffset (negative:
// the file ends that far into the buffer, 16-byte aligned; a buffer too
// small fails with no memory).
// FS2 0x00132a40

int FileJob::run(void *arg)
{
    FileJob *job = (FileJob *)arg;
    int size = 0;
    job->m_state = FileJob_Loading;
    if (job->m_dir != NULL) {
        sprintf(sJobPath, "%s%s", job->m_dir, job->m_path);
        size = rxFileCallSizeHook(sJobPath);
        if (size == 0) {
            job->m_data = NULL;
            job->m_state = FileJob_Done;
            job->m_jobId = 0;
            return 0;
        }
    }
    if (size < 1) {
        sprintf(sJobPath, "%s", job->m_path);
        size = rxFileCallSizeHook(sJobPath);
        if (size == 0) {
            job->m_data = NULL;
            job->m_state = FileJob_Done;
            job->m_jobId = 0;
            return 0;
        }
        if (size < 1) {
            job->m_state = FileJob_Failed;
            return 0;
        }
    }

    job->m_size = size;
    if (job->m_buffer == NULL) {
        job->m_data = gGlobalHeap.alloc(job->m_size, kFileAlign);
    } else {
        job->m_data = NULL;
        int offset = job->m_bufferOffset;
        if (offset < 0) {
            if ((u_int)-offset < (u_int)job->m_size)
                job->m_data = NULL;
            else
                job->m_data = (u_char *)job->m_buffer +
                              ((-offset - job->m_size) & ~(kBufferEndAlign - 1));
        } else {
            job->m_data = (u_char *)job->m_buffer + offset;
        }
    }
    if (job->m_data == NULL) {
        job->m_state = FileJob_NoMemory;
    } else {
        rxFileCallLoadHook(sJobPath, job->m_data);
        job->m_jobId = 0;
        job->m_state = FileJob_Done;
    }
    return 0;
}

// FS2 0x00132bc0

int FileJob::start()
{
    m_jobId = jobAdd(run, this);
    if (m_jobId != 0)
        m_state = FileJob_Queued;
    return m_jobId;
}

// ──────────────────────────────────────────────────────────────────────
// Frees the path, cancels a queued job, frees the data unless a caller's
// buffer held it.
// FS2 0x00132c10
// callcheck: operator_delete[], operator_delete (delete[] m_path; the deleting destructor)

FileJob::~FileJob()
{
    if (m_path != NULL)
        delete[] m_path;
    if (m_jobId != 0)
        jobCancel(m_jobId);
    if (!m_notOwned && m_data != NULL)
        gGlobalHeap.free(m_data);
}

// FS2 0x00132cc0
// callcheck: operator_new[] (the path copy)

FileJob::FileJob(const char *path, int index)
    : m_state(FileJob_Idle), m_index(index), m_size(0), m_notOwned(0), m_data(NULL), m_jobId(0),
      m_dir(NULL), m_buffer(NULL), m_bufferOffset(0)
{
    m_path = new char[strlen(path) + 1];
    strcpy(m_path, path);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00132d40

const char *TextTokenizer::next()
{
    u_int length = 0;
    while (m_cur < m_end) {
        // PC: isspace on the byte as unsigned; the ROM indexes MSL's table
        // with the signed char, so bytes 0x80..0xff read the bytes before
        // it (some of them with the space bit).
        if (!isspace((u_char)*m_cur)) {
            if ((u_int)m_capacity - 1 <= length)
                break;
            m_word[length] = *m_cur;
            length++;
        } else if (length != 0) {
            break;
        }
        m_cur++;
    }
    m_word[length] = '\0';
    return length == 0 ? NULL : m_word;
}

// FS2 0x00132df0

void TextTokenizer::setText(const char *text, int size)
{
    m_text = text;
    m_size = size;
    m_cur = m_text;
    m_end = m_cur + m_size;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00132e20
// callcheck: operator_delete[], operator_delete (delete[] m_word; the deleting destructor)

TextTokenizer::~TextTokenizer()
{
    m_text = NULL;
    m_size = 0;
    m_cur = NULL;
    m_end = NULL;
    // PC: the ROM empties the word before testing it for null.
    if (m_word != NULL) {
        m_word[0] = '\0';
        delete[] m_word;
    }
    m_word = NULL;
    m_capacity = 0;
}

// FS2 0x00132ea0
// callcheck: operator_new[] (the word buffer)

TextTokenizer::TextTokenizer(int capacity)
    : m_text(NULL), m_size(0), m_cur(NULL), m_end(NULL), m_word(NULL), m_capacity(capacity)
{
    m_word = new char[m_capacity];
    m_word[0] = '\0';
}

// ──────────────────────────────────────────────────────────────────────
// FileLoader's defaults for the slots PackFile answers otherwise.
// FS2 0x00132ef0

bool FileLoader::isPack()
{
    return false;
}

// FS2 0x00132f00

int FileLoader::getCount()
{
    return m_count;
}

// FS2 0x00132f10

void FileLoader::compact()
{
}
