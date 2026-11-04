/* ==========================================================================
 *  game/memcard.h
 *
 *  The memory card module (game/memcard.cpp, 0x2a9c70..0x2acff0):
 *  - McDevice, a thin wrapper over libmc for one port / slot: each call
 *    starts one sceMc request and syncs it (blocking or polled), keeping
 *    the last function and its result;
 *  - SaveFile, one save as it lies on the card (0xc00 bytes: the system
 *    data, a stamp, a magic and a byte sum) plus the directory listing the
 *    manager reads for it;
 *  - Memcard, the save manager the menus drive: start a request, call
 *    update() each frame until it reports done, read the outcome.
 *  Four saves, /BESCES-5385100 .. /BESCES-5385103, each a directory with
 *  the data file of the same name, icon.sys and icon1.ico.
 * ======================================================================== */
#ifndef _GAME_MEMCARD_H
#define _GAME_MEMCARD_H

#include <stddef.h>

#include "game/sysdata.h"
#include "sdk/libmc.h"
#include "sdk/mcerror.h"
#include "sdk/scetypes.h"

// ──────────────────────────────────────────────────────────────────────
// McDevice

// The libmc command each McDevice call records in m_lastFunc and hands
// to mcErrorCode (sdk/mcerror.h McCmd). Read records sceMcReadFast's
// command 0x16 although it calls sceMcRead.
enum McFunc
{
    McFunc_GetInfo = McCmd_GetInfo,
    McFunc_Open    = McCmd_Open,
    McFunc_Close   = McCmd_Close,
    McFunc_Write   = McCmd_Write,
    McFunc_Flush   = McCmd_Flush,
    McFunc_Mkdir   = McCmd_Mkdir,
    McFunc_GetDir  = McCmd_GetDir,
    McFunc_Format  = McCmd_Format,
    McFunc_Read    = McCmd_ReadFast,
};

// McDevice::m_result: 0 when the request finished well, -1 while it runs,
// else the low 16 bits of mcErrorCode(function, result).
enum McResult
{
    McResult_Done = 0,
    McResult_Busy = -1,
};

// The error codes the manager tests (mcErrorCode's low 16 bits).
enum McError
{
    McError_NoEntry     = McErrno_NoEnt,        // GetDir: sceMcResNoEntry (-4), no such directory
    McError_DirDenied   = McErrno_Io,           // GetDir: sceMcResNotEmpty (-6)
    McError_NoFormat    = McErrno_NoFormat,     // GetInfo: sceMcResNoFormat (-2)
    McError_ChangedCard = McErrno_ChangedCard,  // GetInfo: sceMcResChangedCard (-1)
};

// The three values sceMcGetInfo fills (Memcard +0x3c).
struct McCardInfo
{
    int type;                   // FS2 +0x0: sceMcTypePS2 for a usable card
    int free;                   // FS2 +0x4: free clusters
    int format;                 // FS2 +0x8: nonzero when formatted (the ROM stores and tests its low byte)
};
static_assert(sizeof(McCardInfo) == 0xc, "McCardInfo: EE 0xc");

// McDevice::getInfo's mask: which of McCardInfo to ask for.
enum McInfoMask
{
    McInfo_Type   = 1 << 0,
    McInfo_Free   = 1 << 1,
    McInfo_Format = 1 << 2,
    McInfo_All    = McInfo_Type | McInfo_Free | McInfo_Format,
};

// One port / slot. `wait` true blocks in sceMcSync until the request ends;
// false polls once (the result is then McResult_Busy until sync() sees it
// finish).
class McDevice
{
public:
    McDevice(int port, int slot);                                        // FS2 0x002acf90
    ~McDevice();                                                         // FS2 0x002acf40

    int read(int fd, void *buf, int size, int wait);                     // FS2 0x002ac630
    int write(int fd, const void *buf, int size, int wait);              // FS2 0x002ac710
    int open(const char *path, int mode, int wait);                      // FS2 0x002ac7f0
    int mkdir(const char *path, int wait);                               // FS2 0x002ac8d0
    int getInfo(McCardInfo *info, u_int mask, int wait);                 // FS2 0x002ac9b0
    int getDir(const char *path, u_int mode, int maxEntries,
               sceMcTblGetDir *table, int wait);                         // FS2 0x002acac0
    int format(int wait);                                                // FS2 0x002acbb0
    int flush(int fd, int wait);                                         // FS2 0x002acc90
    int close(int fd, int wait);                                         // FS2 0x002acd60
    int sync(int wait);                                                  // FS2 0x002ace30

    int m_port;                 // FS2 +0x00
    int m_slot;                 // FS2 +0x04
    int m_result;               // FS2 +0x08: McResult, or an McError
    int m_syncResult;           // FS2 +0x0c: sceMcSync's *result (fd, bytes, entries, or < 0)
    int m_lastFunc;             // FS2 +0x10: McFunc
};
static_assert(offsetof(McDevice, m_syncResult) == 0x0c, "McDevice::m_syncResult");
static_assert(sizeof(McDevice) == 0x14, "McDevice: EE 0x14");

// libmc once for the whole module (the byte 0x31ae90).
void memcardLibInit();                                                   // FS2 0x002acf00
void memcardLibEnd();                                                    // FS2 0x002acec0

// ──────────────────────────────────────────────────────────────────────
// SaveFile

constexpr int kSaveFiles = 4;               // BESCES-5385100 .. 03
constexpr int kSaveDirEntries = 8;          // McDevice::getDir's maxEntries

// The three files a save directory holds, by size.
constexpr u_int kSaveDataSize = 0xc00;      // BESCES-53851xx: SaveFile up to +0xc00
constexpr u_int kIconSysSize = 0x3c4;       // icon.sys (sceMcIconSys)
constexpr u_int kIconSize = 0x10eb8;        // icon1.ico

// SaveFile::magic: a date, 2006-01-26.
constexpr u_int kSaveMagic = 0x20060126;
// SaveFile::sum covers the bytes before the stamp's end.
constexpr u_int kSaveSumSize = 0xa30;

// SaveFile::status.
enum SaveStatus
{
    SaveStatus_Unknown = -1,    // not looked at yet
    SaveStatus_Ok      = 0,     // all three files, magic and sum good
    SaveStatus_NoDir   = 1,     // GetDir: McError_NoEntry
    SaveStatus_Denied  = 2,     // GetDir: McError_DirDenied
    SaveStatus_Broken  = 3,     // some files, or a bad magic / sum
    SaveStatus_Empty   = 4,     // the directory holds none of the three
};

// SaveFile::found: which of the three files the listing showed.
enum SaveFound
{
    SaveFound_IconSys = 1 << 0,
    SaveFound_Icon    = 1 << 1,
    SaveFound_Data    = 1 << 2,
    SaveFound_All     = SaveFound_IconSys | SaveFound_Icon | SaveFound_Data,
};

// Which save and when: the file index, second..month of the save's local
// time and a random 16 bits. The module keeps the loaded save's stamp
// (memcardGetStamp); -1 is none.
union SaveStamp
{
    u_long value;               // FS2 +0x0: read and written whole
    struct
    {
        u_char  index;          // FS2 +0x0
        u_char  second;         // FS2 +0x1
        u_char  minute;         // FS2 +0x2
        u_char  hour;           // FS2 +0x3
        u_char  day;            // FS2 +0x4
        u_char  month;          // FS2 +0x5
        u_short random;         // FS2 +0x6: randNext()
    } f;
};
static_assert(sizeof(SaveStamp) == 0x8, "SaveStamp: EE 0x8");

// The block getSaveExtra returns (0x5995e0), saved beside the system data.
// layout: host -- contents unread (copied as one int and 0x20 doublewords;
// SaveFile::clear zeroes the int).
struct SaveExtra
{
    int    head;                // FS2 +0x000
    u_char body[0x100];         // FS2 +0x004
};
static_assert(sizeof(SaveExtra) == 0x104, "SaveExtra: EE 0x104");

// One save. The first kSaveDataSize bytes are the data file; the rest is the
// manager's view of the save's directory. Allocated 0xe40 from the global
// heap.
struct SaveFile
{
    void init(int index);                                                // FS2 0x002aa720
    void clear();                                                        // FS2 0x002abc20
    bool isValid();                                                      // FS2 0x002abe40
    bool hasAllFiles();                                                  // FS2 0x002abf00
    void apply() const;                                                  // FS2 0x002ac070
    void exportTo(SystemData *dst) const;                                // FS2 0x002ac110
    void capture(int index);                                             // FS2 0x002ac3f0
    void importFrom(const SystemData *src);                              // FS2 0x002ac540
    void judge();               // inline in the ROM (stepLoad, stepCheckAll): sets status

    SaveData    save;           // FS2 +0x000
    StageSelect select;         // FS2 +0x4bc
    GameOptions options;        // FS2 +0x4d0
    FlagTable   flags;          // FS2 +0x520
    SaveExtra   extra;          // FS2 +0x920
    u_char      _a24[0x4];      // FS2 +0xa24
    SaveStamp   stamp;          // FS2 +0xa28
    u_char      reserved[0x1c8];    // FS2 +0xa30: zeroed, outside the sum
    u_int       magic;          // FS2 +0xbf8: kSaveMagic
    u_int       sum;            // FS2 +0xbfc: byte sum of +0..kSaveSumSize
    sceMcTblGetDir dir[kSaveDirEntries];    // FS2 +0xc00: the directory listing
    int         status;         // FS2 +0xe00: SaveStatus
    u_int       found;          // FS2 +0xe04: SaveFound bits
    int         index;          // FS2 +0xe08: 0..3, -1 for the wildcard name
};
static_assert(offsetof(SaveFile, select) == 0x4bc, "SaveFile::select");
static_assert(offsetof(SaveFile, options) == 0x4d0, "SaveFile::options");
static_assert(offsetof(SaveFile, flags) == 0x520, "SaveFile::flags");
static_assert(offsetof(SaveFile, extra) == 0x920, "SaveFile::extra");
static_assert(offsetof(SaveFile, stamp) == 0xa28, "SaveFile::stamp");
static_assert(offsetof(SaveFile, reserved) == kSaveSumSize, "SaveFile::reserved");
static_assert(offsetof(SaveFile, magic) == 0xbf8, "SaveFile::magic");
static_assert(offsetof(SaveFile, dir) == kSaveDataSize, "SaveFile::dir");
static_assert(offsetof(SaveFile, status) == 0xe00, "SaveFile::status");
static_assert(offsetof(SaveFile, index) == 0xe08, "SaveFile::index");
// sceMcTblGetDir's 64-byte alignment pads it to what Heap::alloc reserves.
static_assert(sizeof(SaveFile) == 0xe40, "SaveFile: EE 0xe40");

// The free space a save needs, in sceMcGetInfo's 1 KB clusters
// (memcardRequiredKB, stepScan).
constexpr int kSaveRequiredKB = 80;

// Heap::alloc's alignment for the manager's buffers.
constexpr u_int kMemcardAlign = 0x40;

// ──────────────────────────────────────────────────────────────────────
// Memcard

// Memcard::m_request: which step function update() runs.
enum MemcardRequest
{
    MemcardRequest_None     = 0,
    MemcardRequest_CardInfo = 1,    // stepCardInfo
    MemcardRequest_ReadDirs = 2,    // stepReadDirs
    MemcardRequest_Scan     = 3,    // stepScan: room to save, the first game's saves
    MemcardRequest_CheckAll = 4,    // stepCheckAll
    MemcardRequest_Load     = 5,    // stepLoad
    MemcardRequest_Save     = 6,    // stepSave
    MemcardRequest_Save7    = 7,    // stepSave as well; no starter in this TU
    MemcardRequest_Idle8    = 8,    // ends at once
    MemcardRequest_Format   = 9,    // stepFormat
};

// Memcard::m_outcome, set when a request ends.
enum MemcardOutcome
{
    MemcardOutcome_None        = 0,
    MemcardOutcome_Ok          = 1,
    MemcardOutcome_Unusable    = 2,     // no save to load; for Format: already formatted
    MemcardOutcome_NoCard      = 3,     // no PS2 card in the slot
    MemcardOutcome_Failed      = 4,
    MemcardOutcome_Unformatted = 5,     // Save on an unformatted card
};

// A step function's step value: < 0 ends the request (Memcard::update
// returns true), 0 starts it, 1 / 2 are the card-info sub-steps, 10, 20,
// 30, 40 start the per-file sequences (file (step - 10) / 10), 50 follows
// the last one.
constexpr int kStepDone = -1;
constexpr int kStepFirstFile = 10;
constexpr int kStepPerFile = 10;
constexpr int kStepFilesEnd = kStepFirstFile + kSaveFiles * kStepPerFile;   // 50: past the last file

// The save manager (0x90 bytes; the menus 0x2b5770 / 0x2b6810 / 0x2b85b0
// own one). Port 0, slot 0.
class Memcard : public McDevice
{
public:
    Memcard();                                                           // FS2 0x002ab740
    ~Memcard();                                                          // FS2 0x002ab660

    bool update();                                                       // FS2 0x002ab500

    bool startFormat();                                                  // FS2 0x002ab9f0
    bool startCardInfo();                                                // FS2 0x002aba30
    bool startLoad(int index);                                           // FS2 0x002aba70
    bool startCheckAll();                                                // FS2 0x002abb50
    bool startSave(int index);                                           // FS2 0x002abca0
    bool startScan();                                                    // FS2 0x002abd70

    int  stepFormat(int step, int result);                               // FS2 0x002a9cc0
    int  stepSave(int step, int result);                                 // FS2 0x002a9dd0
    int  stepLoad(int step, int result);                                 // FS2 0x002aa2f0
    int  stepCheckAll(int step, int result);                             // FS2 0x002aa7e0
    int  stepScan(int step, int result);                                 // FS2 0x002aaee0
    int  stepReadDirs(int step, int result);                             // FS2 0x002ab1f0
    int  stepCardInfo(int step, int result);                             // FS2 0x002ab460

    void buildIconSys(int index);                                        // FS2 0x002ab7c0

    u_char      _14[0x28];      // FS2 +0x14: not touched by the code read so far
    McCardInfo  m_info;         // FS2 +0x3c
    int         m_request;      // FS2 +0x48: MemcardRequest
    int         m_step;         // FS2 +0x4c: the request's step, < 0 idle
    int         m_dirStep;      // FS2 +0x50: stepReadDirs as a sub-step
    int         m_infoStep;     // FS2 +0x54: stepCardInfo as a sub-step
    int         m_fileIndex;    // FS2 +0x58: the save loaded / written
    int         m_lastSync;     // FS2 +0x5c: update's sync() result
    u_char      m_cardNotice;   // FS2 +0x60: getInfo ended with changed card / no format
    int         m_fd;           // FS2 +0x64: the open file
    int         m_outcome;      // FS2 +0x68: MemcardOutcome
    float       m_progress;     // FS2 +0x6c: 0..1 for the save / check bars
    sceMcIconSys *m_iconSys;    // FS2 +0x70: kIconSysSize, built by buildIconSys
    void       *m_iconData;     // FS2 +0x74: icon1.ico, MenuPackItem_SaveIcon
    int         m_iconSize;     // FS2 +0x78: its size in the pack; stepSave writes kIconSize
    SaveFile   *m_image;        // FS2 +0x7c: the save being written (kSaveDataSize used)
    SaveFile   *m_files[kSaveFiles];    // FS2 +0x80

private:
    void resetFiles();          // inline in the ROM
    bool begin(int request);    // inline in the ROM
    void clearInfo();           // inline in the ROM
    void advanceProgress();     // inline in the ROM: m_progress += 0.05, up to 1
    int  failSave();            // inline in the ROM
    int  failRead();            // inline in the ROM
    void setCheckOutcome();     // inline in the ROM
};
static_assert(offsetof(Memcard, m_info) == 0x3c, "Memcard::m_info");
static_assert(offsetof(Memcard, m_request) == 0x48, "Memcard::m_request");
static_assert(offsetof(Memcard, m_cardNotice) == 0x60, "Memcard::m_cardNotice");
static_assert(offsetof(Memcard, m_fd) == 0x64, "Memcard::m_fd");
static_assert(offsetof(Memcard, m_progress) == 0x6c, "Memcard::m_progress");
// layout: host from +0x70 on -- the pointers grow on a 64-bit host; the EE
// offsets are in the comments, the EE size is 0x90.

// ──────────────────────────────────────────────────────────────────────
// The module's own state

void memcardInit();                                                      // FS2 0x002a9c80
void memcardEnd();                                                       // FS2 0x002a9c70
int  memcardRequiredKB();                                                // FS2 0x002a9cb0

// Set by stepScan when the card holds a save of the first game (any of its
// ten releases' directories lists an entry); SaveData::setDefaults seeds
// SaveData::unlock468 from it: the first game's save bonus (the byte
// 0x31ae80).
u_char memcardGetState();                                                // FS2 0x002ac610

void memcardClearStamp();                                                // FS2 0x002ac5a0
void memcardSetStamp(u_long stamp);                                      // FS2 0x002ac5b0
void memcardGetStamp(u_long *out);                                       // FS2 0x002ac5c0

// The byte 0x31ae84: set = no save to offer. memcardInit and a started game
// set it, menuSaveDialog clears it and sets it again when the dialog
// answers 0, and menuPlayLoop offers a save only while it is clear.
u_char memcardGetFlag();                                                 // FS2 0x002ac5d0
void   memcardSetFlag();                                                 // FS2 0x002ac5e0
void   memcardClearFlag();                                               // FS2 0x002ac5f0
void   memcardSetFlagAgain();                                            // FS2 0x002ac600

void clearDirEntry(sceMcTblGetDir *entry);                               // FS2 0x002ac620

// Outside this TU.
SaveExtra *getSaveExtra();                                               // FS2 0x0029efd0

#endif /* _GAME_MEMCARD_H */
