// game/memcard.cpp -- the memory card: libmc device wrapper, save files and
// the save manager the menus drive (game/memcard.h).

#include "game/memcard.h"

#include <string.h>

#include "game/datetime.h"
#include "game/heap.h"
#include "game/sysdata.h"
#include "sdk/libmc.h"
#include "sdk/scetypes.h"

// McDevice keeps the low half of mcErrorCode's 0x8108xxxx codes.
constexpr int kMcErrorMask = 0xffff;

// The module's own bytes (sbss, gp-0x79f0..gp-0x79e0).
static u_char sMcSaveFound;     /* sbss 0x31ae80: memcardGetState */
static u_char sMcFlag;          /* sbss 0x31ae84: memcardGetFlag */
static u_long sMcLoadedStamp;   /* sbss 0x31ae88: the loaded save's SaveStamp, -1 none */
static u_char sMcLibReady;      /* sbss 0x31ae90: sceMcInit done */

// The four saves' names and paths (data 0x2fb790..0x2fb7d0, strings
// 0x30a788..0x30a9b0).
static const char *const sSaveNames[kSaveFiles] = {         /* data 0x2fb790 */
    "BESCES-5385100", "BESCES-5385101", "BESCES-5385102", "BESCES-5385103",
};
static const char *const sSaveDirGlobs[kSaveFiles] = {      /* data 0x2fb7a0 */
    "/BESCES-5385100/*", "/BESCES-5385101/*", "/BESCES-5385102/*", "/BESCES-5385103/*",
};
static const char *const sSaveDataPaths[kSaveFiles] = {     /* data 0x2fb7b0 */
    "/BESCES-5385100/BESCES-5385100", "/BESCES-5385101/BESCES-5385101",
    "/BESCES-5385102/BESCES-5385102", "/BESCES-5385103/BESCES-5385103",
};
static const char *const sIconSysPaths[kSaveFiles] = {      /* data 0x2fb7c0 */
    "/BESCES-5385100/icon.sys", "/BESCES-5385101/icon.sys",
    "/BESCES-5385102/icon.sys", "/BESCES-5385103/icon.sys",
};
static const char *const sIconPaths[kSaveFiles] = {         /* data 0x2fb7d0 */
    "/BESCES-5385100/icon1.ico", "/BESCES-5385101/icon1.ico",
    "/BESCES-5385102/icon1.ico", "/BESCES-5385103/icon1.ico",
};
// A SaveFile's name when its index is -1 (string 0x30aa90).
static const char kSaveNameAny[] = "BESCES-53851*";
// The entry names SaveFile::hasAllFiles looks for (strings 0x30aad8 / 0x30aae8).
static const char kIconSysName[] = "icon.sys";
static const char kIconName[] = "icon1.ico";

// The data file's name for a save index.
static inline const char *saveName(int index)
{
    return index == -1 ? kSaveNameAny : sSaveNames[index];
}

// The byte sum SaveFile::sum holds (inline in the ROM at each use).
static inline u_int saveByteSum(const SaveFile *file)
{
    const u_char *p = reinterpret_cast<const u_char *>(file);
    u_int sum = 0;
    for (u_int i = 0; i < kSaveSumSize; i++)
        sum += p[i];
    return sum;
}

// A per-file step of the sequences: file 0..3 at phase 0..9.
static inline bool isFileStep(int step, int phase)
{
    return step >= kStepFirstFile && step < kStepFilesEnd
        && (step - kStepFirstFile) % kStepPerFile == phase;
}

static inline int fileOfStep(int step)
{
    return (step - kStepFirstFile) / kStepPerFile;
}

// ──────────────────────────────────────────────────────────────────────
// The module's exported end: a tail jump to memcardLibEnd.
// FS2 0x002a9c70

void memcardEnd()
{
    memcardLibEnd();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002a9c80

void memcardInit()
{
    memcardLibInit();
    sMcLoadedStamp = (u_long)-1;
    sMcFlag = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The free space the save menu asks for (the save is ~73 KB; unit a guess).
// FS2 0x002a9cb0

int memcardRequiredKB()
{
    return 80;
}

// ──────────────────────────────────────────────────────────────────────
// Request 9: card info, then format an unformatted PS2 card.
// FS2 0x002a9cc0

int Memcard::stepFormat(int step, int result)
{
    if (step == kStepFirstFile + 1) {
        if (result == McResult_Done)
            m_outcome = MemcardOutcome_Ok;
        else
            m_outcome = MemcardOutcome_Failed;
        return kStepDone;
    }
    if (step != kStepFirstFile) {
        if (step != 2) {
            if (step != 1) {
                if (step != 0)
                    return step;
                m_infoStep = 0;
            }
            m_infoStep = stepCardInfo(m_infoStep, result);
            if (m_infoStep >= 0)
                return 1;
        }
        if (m_info.type != sceMcTypePS2) {
            m_outcome = MemcardOutcome_NoCard;
            return kStepDone;
        }
        if ((u_char)m_info.format != 0) {
            m_outcome = MemcardOutcome_Unusable;
            return kStepDone;
        }
        step = kStepFirstFile;
    }
    format(0);
    return step + 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002aa720
// callcheck: memset, setDefaults, init, clear (SaveFile::clear inline in the ROM)

void SaveFile::init(int fileIndex)
{
    clear();
    index = fileIndex;
    status = SaveStatus_Unknown;
    found = 0;
    for (int i = 0; i < kSaveDirEntries; i++)
        clearDirEntry(&dir[i]);
}

// ──────────────────────────────────────────────────────────────────────
// Request 2, and the sub-step m_dirStep of load / check / scan: card info,
// then each save directory's listing into its SaveFile.
// FS2 0x002ab1f0
// callcheck: alloc, init (resetFiles inline in the ROM)

int Memcard::stepReadDirs(int step, int result)
{
    if (step == kStepFilesEnd) {
        step = kStepDone;
    } else if (isFileStep(step, 1)) {
        int file = fileOfStep(step);
        if (result == McError_NoEntry) {
            m_files[file]->status = SaveStatus_NoDir;
        } else if (result == McError_DirDenied) {
            m_files[file]->status = SaveStatus_Denied;
        } else if (result != McResult_Done) {
            resetFiles();
            clearInfo();
            return kStepDone;
        }
        m_files[file]->hasAllFiles();
        step += kStepPerFile - 1;
    } else {
        if (!isFileStep(step, 0)) {
            if (step != 2) {
                if (step != 1) {
                    if (step != 0)
                        return step;
                    m_infoStep = 0;
                }
                m_infoStep = stepCardInfo(m_infoStep, result);
                if (m_infoStep >= 0)
                    return 1;
            }
            if (m_info.type != sceMcTypePS2)
                return kStepDone;
            if ((u_char)m_info.format == 0)
                return kStepDone;
            step = kStepFirstFile;
        }
        int file = fileOfStep(step);
        getDir(sSaveDirGlobs[file], 0, kSaveDirEntries, m_files[file]->dir, 0);
        step++;
    }
    return step;
}

// ──────────────────────────────────────────────────────────────────────
// Request 1, and the sub-step m_infoStep of the others: the card's type,
// free space and format. A changed card or an unformatted one keeps what
// sceMcGetInfo filled and raises m_cardNotice; any other error clears it.
// FS2 0x002ab460

int Memcard::stepCardInfo(int step, int result)
{
    if (step == kStepFirstFile) {
        if (result == McError_ChangedCard)
            m_cardNotice = 1;
        else if (result == McError_NoFormat)
            m_cardNotice = 1;
        else if (result != McResult_Done)
            clearInfo();
        step = kStepDone;
    } else if (step == 0) {
        getInfo(&m_info, McInfo_All, 0);
        step = kStepFirstFile;
    }
    return step;
}

// ──────────────────────────────────────────────────────────────────────
// Polls the running request and advances the current step function; true
// once the request has ended.
// FS2 0x002ab500

bool Memcard::update()
{
    int result = sync(0);
    m_lastSync = result;
    if (result == McResult_Busy)
        return false;

    switch (m_request) {
    case MemcardRequest_CardInfo:
        m_step = stepCardInfo(m_step, result);
        break;
    case MemcardRequest_ReadDirs:
        m_step = stepReadDirs(m_step, result);
        break;
    case MemcardRequest_Scan:
        m_step = stepScan(m_step, result);
        break;
    case MemcardRequest_CheckAll:
        m_step = stepCheckAll(m_step, result);
        break;
    case MemcardRequest_Load:
        m_step = stepLoad(m_step, result);
        break;
    case MemcardRequest_Save:
        m_step = stepSave(m_step, result);
        break;
    case MemcardRequest_Save7:
        m_step = stepSave(m_step, result);
        break;
    case MemcardRequest_Idle8:
        m_step = kStepDone;
        break;
    case MemcardRequest_Format:
        m_step = stepFormat(m_step, result);
        break;
    default:
        m_step = kStepDone;
        break;
    }
    return m_step < 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ab660
// callcheck: ~McDevice, operator_delete (the implicit base destructor; the deleting destructor's delete)

Memcard::~Memcard()
{
    if (m_iconSys != 0) {
        gGlobalHeap.free(m_iconSys);
        m_iconSys = 0;
    }
    m_iconData = 0;
    m_iconAux = 0;
    if (m_image != 0) {
        gGlobalHeap.free(m_image);
        m_image = 0;
    }
    for (int i = 0; i < kSaveFiles; i++) {
        if (m_files[i] != 0) {
            gGlobalHeap.free(m_files[i]);
            m_files[i] = 0;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Port 0, slot 0. m_outcome and m_fd are left as they are.
// FS2 0x002ab740
// callcheck: McDevice (the base constructor in the initialiser list)

Memcard::Memcard()
    : McDevice(0, 0)
{
    m_iconSys = 0;
    m_iconData = 0;
    m_iconAux = 0;
    m_image = 0;
    m_request = MemcardRequest_None;
    m_fileIndex = 0;
    m_lastSync = 0;
    m_info.type = 0;
    m_info.free = 0;
    m_info.format = 0;
    m_cardNotice = 0;
    m_step = kStepDone;
    m_progress = 0.0f;
    for (int i = 0; i < kSaveFiles; i++)
        m_files[i] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Every request starter's tail (inline in the ROM): start when idle.

inline bool Memcard::begin(int request)
{
    bool idle = m_step < 0;
    if (idle) {
        m_request = request;
        m_step = 0;
        m_dirStep = 0;
        m_infoStep = 0;
        m_outcome = MemcardOutcome_None;
        m_progress = 0.0f;
    }
    return idle;
}

// The four SaveFiles allocated once and re-initialised (inline in the ROM).
inline void Memcard::resetFiles()
{
    for (int i = 0; i < kSaveFiles; i++) {
        if (m_files[i] == 0) {
            m_files[i] = static_cast<SaveFile *>(gGlobalHeap.alloc(sizeof(SaveFile), kMemcardAlign));
            m_files[i]->init(i);
        } else {
            m_files[i]->init(i);
        }
    }
}

// Forget the card info (inline in the ROM).
inline void Memcard::clearInfo()
{
    m_info.type = 0;
    m_info.free = 0;
    m_info.format = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ab9f0

bool Memcard::startFormat()
{
    return begin(MemcardRequest_Format);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002aba30

bool Memcard::startCardInfo()
{
    return begin(MemcardRequest_CardInfo);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002aba70
// callcheck: alloc, init (resetFiles inline in the ROM)

bool Memcard::startLoad(int index)
{
    resetFiles();
    m_fileIndex = index;
    return begin(MemcardRequest_Load);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002abb50
// callcheck: alloc, init (resetFiles inline in the ROM)

bool Memcard::startCheckAll()
{
    resetFiles();
    return begin(MemcardRequest_CheckAll);
}

// ──────────────────────────────────────────────────────────────────────
// The data file part of a save to its defaults.
// FS2 0x002abc20

void SaveFile::clear()
{
    memset(reserved, 0, sizeof(reserved));
    save.setDefaults();
    select.setDefaults();
    options.init();
    flags.clear();
    extra.head = 0;
    memset(&stamp, 0, sizeof(stamp));
    stamp.value = (u_long)-1;
    magic = kSaveMagic;
    sum = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Builds the image of save `index` from the game's state and starts
// writing it.
// FS2 0x002abca0

bool Memcard::startSave(int index)
{
    if (m_image == 0)
        m_image = static_cast<SaveFile *>(gGlobalHeap.alloc(kSaveDataSize, kMemcardAlign));
    m_image->clear();
    m_image->capture(index);
    buildIconSys(index);
    void *aux = getSaveResourceAux(0);
    m_iconData = getSaveResource(0);
    m_iconAux = aux;
    m_fileIndex = index;
    return begin(MemcardRequest_Save);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002abd70
// callcheck: alloc, init (resetFiles inline in the ROM)

bool Memcard::startScan()
{
    resetFiles();
    return begin(MemcardRequest_Scan);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002abe40

bool SaveFile::isValid()
{
    if (!hasAllFiles())
        return false;
    if (magic != kSaveMagic)
        return false;
    return sum == saveByteSum(this);
}

// ──────────────────────────────────────────────────────────────────────
// Which of the three files the listing shows, each by name and size;
// only for a save not judged yet or judged good.
// FS2 0x002abf00

bool SaveFile::hasAllFiles()
{
    if (status != SaveStatus_Unknown && status != SaveStatus_Ok)
        return false;

    found = 0;
    int fileIndex = index;
    for (int i = 0; i < kSaveDirEntries; i++) {
        u_int size = dir[i].FileSizeByte;
        const char *name = reinterpret_cast<const char *>(dir[i].EntryName);
        if (name[0] == '\0' || size == 0)
            continue;
        if (strcmp(kIconSysName, name) == 0) {
            if (size == kIconSysSize)
                found |= SaveFound_IconSys;
        } else if (strcmp(kIconName, name) == 0) {
            if (size == kIconSize)
                found |= SaveFound_Icon;
        } else if (strcmp(saveName(fileIndex), name) == 0 && size == kSaveDataSize) {
            found |= SaveFound_Data;
        }
    }
    return found == SaveFound_All;
}

// ──────────────────────────────────────────────────────────────────────
// A loaded save into the game. The loaded stamp is kept only when the
// options' byte +0x4d is set.
// FS2 0x002ac070

void SaveFile::apply() const
{
    // getSaveData and getSystemData both answer the block 0x598b30.
    exportTo(reinterpret_cast<SystemData *>(getSaveData()));
    SaveExtra *ext = getSaveExtra();
    if (ext != 0)
        *ext = extra;
    if (options._4d != 0)
        sMcLoadedStamp = stamp.value;
    else
        sMcLoadedStamp = (u_long)-1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac110
// callcheck: operator= (the implicit copy assignments)

void SaveFile::exportTo(SystemData *dst) const
{
    if (dst != 0) {
        dst->save = save;
        dst->select = select;
        dst->options = options;
        dst->flags = flags;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The game's state into a save image, stamped with `fileIndex`, the local
// time and a random value, then summed.
// FS2 0x002ac3f0

void SaveFile::capture(int fileIndex)
{
    importFrom(reinterpret_cast<const SystemData *>(getSaveData()));
    SaveExtra *ext = getSaveExtra();
    if (ext != 0)
        extra = *ext;

    DateTime now;
    getLocalDateTime(&now, DateFormat_Binary);
    stamp.f.index = (u_char)fileIndex;
    stamp.f.second = now.second;
    stamp.f.minute = now.minute;
    stamp.f.hour = now.hour;
    stamp.f.day = now.day;
    stamp.f.month = now.month;
    stamp.f.random = (u_short)randNext();

    sum = saveByteSum(this);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac540
// callcheck: operator= (the implicit copy assignments)

void SaveFile::importFrom(const SystemData *src)
{
    if (src != 0) {
        save = src->save;
        select = src->select;
        options = src->options;
        flags = src->flags;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac5a0

void memcardClearStamp()
{
    sMcLoadedStamp = (u_long)-1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac5b0

void memcardSetStamp(u_long stamp)
{
    sMcLoadedStamp = stamp;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac5c0

void memcardGetStamp(u_long *out)
{
    *out = sMcLoadedStamp;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac5d0

u_char memcardGetFlag()
{
    return sMcFlag;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac5e0

void memcardSetFlag()
{
    sMcFlag = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac5f0

void memcardClearFlag()
{
    sMcFlag = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac600

void memcardSetFlagAgain()
{
    sMcFlag = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac610

u_char memcardGetState()
{
    return sMcSaveFound;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac620

void clearDirEntry(sceMcTblGetDir *entry)
{
    memset(entry, 0, sizeof(sceMcTblGetDir));
}

// ──────────────────────────────────────────────────────────────────────
// McDevice: each call records its function, then either keeps the
// rejected request's error or syncs (the sync() tail, inline in the ROM).
// FS2 0x002ac630
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::read(int fd, void *buf, int size, int wait)
{
    int ret = sceMcRead(fd, buf, size);
    m_lastFunc = McFunc_Read;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_Read, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac710
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::write(int fd, const void *buf, int size, int wait)
{
    int ret = sceMcWrite(fd, buf, size);
    m_lastFunc = McFunc_Write;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_Write, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac7f0
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::open(const char *path, int mode, int wait)
{
    int ret = sceMcOpen(m_port, m_slot, path, mode);
    m_lastFunc = McFunc_Open;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_Open, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac8d0
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::mkdir(const char *path, int wait)
{
    int ret = sceMcMkdir(m_port, m_slot, path);
    m_lastFunc = McFunc_Mkdir;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_Mkdir, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ac9b0
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::getInfo(McCardInfo *info, u_int mask, int wait)
{
    int *type = (mask & McInfo_Type) ? &info->type : 0;
    int *freeOut = (mask & McInfo_Free) ? &info->free : 0;
    int *fmt = (mask & McInfo_Format) ? &info->format : 0;
    int ret = sceMcGetInfo(m_port, m_slot, type, freeOut, fmt);
    m_lastFunc = McFunc_GetInfo;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_GetInfo, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acac0
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::getDir(const char *path, u_int mode, int maxEntries,
                     sceMcTblGetDir *table, int wait)
{
    int ret = sceMcGetDir(m_port, m_slot, path, mode, maxEntries, table);
    m_lastFunc = McFunc_GetDir;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_GetDir, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acbb0
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::format(int wait)
{
    int ret = sceMcFormat(m_port, m_slot);
    m_lastFunc = McFunc_Format;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_Format, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acc90
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::flush(int fd, int wait)
{
    int ret = sceMcFlush(fd);
    m_lastFunc = McFunc_Flush;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_Flush, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acd60
// callcheck: sceMcSync (the sync() tail inline in the ROM)

int McDevice::close(int fd, int wait)
{
    int ret = sceMcClose(fd);
    m_lastFunc = McFunc_Close;
    m_syncResult = 0;
    if (ret < 0)
        m_result = mcErrorCode(McFunc_Close, ret) & kMcErrorMask;
    else {
        m_result = McResult_Busy;
        sync(wait);
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// Waits for (wait != 0) or polls the running request. Finished: its result
// in m_syncResult, m_result 0 or its error; still running: McResult_Busy;
// nothing running: 0.
// FS2 0x002ace30

int McDevice::sync(int wait)
{
    int cmd;
    int result = 0;
    int state = sceMcSync(wait == 0, &cmd, &result);
    if (state == sceMcExecFinish) {
        if (result < 0) {
            m_syncResult = result;
            m_result = mcErrorCode(cmd, result) & kMcErrorMask;
        } else {
            m_syncResult = result;
            m_result = McResult_Done;
        }
    } else if (state == sceMcExecRun) {
        m_result = McResult_Busy;
    } else {
        m_result = McResult_Done;
    }
    return m_result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acec0

void memcardLibEnd()
{
    if (sMcLibReady) {
        sceMcEnd();
        sMcLibReady = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acf00

void memcardLibInit()
{
    if (!sMcLibReady && sceMcInit() == 0)
        sMcLibReady = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acf40
// callcheck: operator_delete (the deleting destructor's delete)

McDevice::~McDevice()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acf90
// callcheck: sceMcInit (memcardLibInit inline in the ROM)

McDevice::McDevice(int port, int slot)
{
    m_port = port;
    m_slot = slot;
    m_lastFunc = 0;
    m_result = McResult_Done;
    memcardLibInit();
}
