// game/memcard.cpp -- the memory card: libmc device wrapper, save files and
// the save manager the menus drive (game/memcard.h).

#include "game/memcard.h"

#include <string.h>

#include "game/datetime.h"
#include "game/heap.h"
#include "game/menupack.h"
#include "game/sysdata.h"
#include "sdk/fileio.h"
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
// icon.sys titles, SJIS full-width "Forbidden" / "Siren 2 File01".."04",
// the line break after "Forbidden" (kIconSysTitleBreak) (data 0x2fb7e0,
// strings 0x30a9d0..0x30aa60).
#define FS2_ICON_TITLE(n)                                                   \
    "\x82\x65\x82\x8f\x82\x92\x82\x82\x82\x89\x82\x84\x82\x84\x82\x85\x82\x8e" \
    "\x82\x72\x82\x89\x82\x92\x82\x85\x82\x8e\x81\x40\x82\x51\x81\x40"        \
    "\x82\x65\x82\x89\x82\x8c\x82\x85\x82\x4f" n
static const char *const sIconSysTitles[kSaveFiles] = {     /* data 0x2fb7e0 */
    FS2_ICON_TITLE("\x82\x50"), FS2_ICON_TITLE("\x82\x51"),
    FS2_ICON_TITLE("\x82\x52"), FS2_ICON_TITLE("\x82\x53"),
};
#undef FS2_ICON_TITLE
constexpr u_short kIconSysTitleBreak = 0x12;    // bytes: the nine SJIS letters of "Forbidden"

// The first game's (Siren / Forbidden Siren) save directories in all its
// releases, which stepScan looks for (data 0x2fb760, strings
// 0x30a6e8..0x30a778).
constexpr int kFirstGameSaves = 10;
static const char *const sFirstGameSaveGlobs[kFirstGameSaves] = {   /* data 0x2fb760 */
    "BISCPS-15053*", "BASCUS-97355*", "BESCES-51920*", "BESCES-52328*", "BESCES-52327*",
    "BESCES-52330*", "BESCES-52329*", "BKSCKA-20019*", "BCSCCS-40013*", "BISCAJ-30003*",
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

// stepSave's steps: the directory, then each file's five phases.
enum SaveStep
{
    SaveStep_Mkdir   = kStepFirstFile,                      // 10, 11: mkdir, its result
    SaveStep_Data    = kStepFirstFile + kStepPerFile,       // 20: BESCES-53851xx
    SaveStep_Icon    = kStepFirstFile + 2 * kStepPerFile,   // 30: icon1.ico
    SaveStep_IconSys = kStepFirstFile + 3 * kStepPerFile,   // 40: icon.sys
    SaveStep_Done    = kStepFilesEnd,                       // 50
};
// What the step SaveStep_x + phase does with the last result (ints: the
// case labels add them to a SaveStep). Phase 0 is the file's open.
constexpr int kSavePhaseWrite = 1;
constexpr int kSavePhaseFlush = 2;
constexpr int kSavePhaseClose = 3;
constexpr int kSavePhaseNext  = 4;      // on to the next file's open

// stepScan lists sFirstGameSaveGlobs[(step - 20) / 10] at the phase-0
// steps 20..120 (120 ends the scan) and reads the count at 21..121.
constexpr int kScanStepFirst = kStepFirstFile + kStepPerFile;

static inline bool isScanStep(int step, int phase)
{
    return step >= kScanStepFirst && step <= kScanStepFirst + kFirstGameSaves * kStepPerFile + phase
        && (step - kScanStepFirst) % kStepPerFile == phase;
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
// The free space the save menu asks for (the save is ~73 KB; stepScan
// compares it with sceMcGetInfo's free clusters, 1 KB each).
// FS2 0x002a9cb0

int memcardRequiredKB()
{
    return kSaveRequiredKB;
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
// Requests 6 and 7: card info, the save's directory, then its three files,
// each opened, written, flushed and closed. m_progress drives the save bar.
// A failed write, flush or open ends the request; a failed close is noted
// and the next file still written (the last file's close decides).
// FS2 0x002a9dd0

int Memcard::stepSave(int step, int result)
{
    int fileIndex = m_fileIndex;

    switch (step) {
    case SaveStep_Done:
        m_outcome = MemcardOutcome_Ok;
        m_progress = 1.0f;
        return kStepDone;

    case SaveStep_Data + kSavePhaseNext:
    case SaveStep_Icon + kSavePhaseNext:
    case SaveStep_IconSys + kSavePhaseNext:
        step += kStepPerFile - kSavePhaseNext;
        if (result == McResult_Done) {
            advanceProgress();
        } else {
            m_outcome = MemcardOutcome_Failed;
            m_progress = 1.0f;
        }
        return step;

    case SaveStep_Data + kSavePhaseClose:
    case SaveStep_Icon + kSavePhaseClose:
    case SaveStep_IconSys + kSavePhaseClose:
        if (result != McResult_Done)
            return failSave();
        close(m_fd, 0);
        advanceProgress();
        return step + 1;

    case SaveStep_Data + kSavePhaseFlush:
    case SaveStep_Icon + kSavePhaseFlush:
    case SaveStep_IconSys + kSavePhaseFlush:
        if (result != McResult_Done)
            return failSave();
        flush(m_fd, 0);
        advanceProgress();
        return step + 1;

    case SaveStep_IconSys + kSavePhaseWrite:
        if (result != McResult_Done)
            return failSave();
        m_fd = m_syncResult;
        write(m_fd, m_iconSys, kIconSysSize, 0);
        m_progress = 0.85f;
        return step + 1;

    case SaveStep_Icon + kSavePhaseWrite:
        if (result != McResult_Done)
            return failSave();
        m_fd = m_syncResult;
        write(m_fd, m_iconData, kIconSize, 0);
        m_progress = 0.55f;
        return step + 1;

    case SaveStep_Data + kSavePhaseWrite:
        if (result != McResult_Done)
            return failSave();
        m_fd = m_syncResult;
        write(m_fd, m_image, kSaveDataSize, 0);
        m_progress = 0.25f;
        return step + 1;

    case SaveStep_IconSys:
        open(sIconSysPaths[fileIndex], SCE_CREAT | SCE_WRONLY, 0);
        m_progress = 0.8f;
        return step + 1;

    case SaveStep_Icon:
        open(sIconPaths[fileIndex], SCE_CREAT | SCE_WRONLY, 0);
        m_progress = 0.5f;
        return step + 1;

    case 0:
        m_infoStep = 0;
        m_progress = 0.0f;
        // fall through
    case 1:
        m_infoStep = stepCardInfo(m_infoStep, result);
        if (m_infoStep >= 0)
            return 1;
        m_progress = 0.1f;
        // fall through
    case 2:
        if (m_info.type != sceMcTypePS2) {
            m_outcome = MemcardOutcome_NoCard;
            m_progress = 1.0f;
            return kStepDone;
        }
        if ((u_char)m_info.format == 0) {
            m_outcome = MemcardOutcome_Unformatted;
            m_progress = 1.0f;
            return kStepDone;
        }
        step = SaveStep_Mkdir;
        // fall through
    case SaveStep_Mkdir:
        // Blocking: update()'s next sync finds nothing running and passes
        // McResult_Done, so an existing directory is no failure.
        mkdir(saveName(fileIndex), 1);
        return step + 1;

    case SaveStep_Mkdir + 1:
        if (result != McResult_Done) {
            clearInfo();
            m_progress = 1.0f;
            return kStepDone;
        }
        step = SaveStep_Data;
        // fall through
    case SaveStep_Data:
        open(sSaveDataPaths[fileIndex], SCE_CREAT | SCE_WRONLY, 0);
        m_progress = 0.2f;
        return step + 1;

    case kStepDone:
        return kStepDone;

    default:
        return step;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Request 5: the directories, then save m_fileIndex's data file read into
// its SaveFile and judged. Outcome Ok once read (whatever the judgement:
// the caller reads SaveFile::status); Unusable when the save lacks a file.
// FS2 0x002aa2f0
// callcheck: alloc, init, hasAllFiles (resetFiles and SaveFile::judge inline in the ROM)

int Memcard::stepLoad(int step, int result)
{
    int fileIndex = m_fileIndex;

    switch (step) {
    case kStepFilesEnd:
        break;

    case kStepFirstFile + 3:
        if (result != McResult_Done)
            return failRead();
        m_files[fileIndex]->judge();
        break;

    case kStepFirstFile + 2:
        if (result != McResult_Done)
            return failRead();
        close(m_fd, 0);
        return kStepFirstFile + 3;

    case kStepFirstFile + 1:
        if (result != McResult_Done)
            return failRead();
        m_fd = m_syncResult;
        read(m_fd, m_files[fileIndex], kSaveDataSize, 0);
        return kStepFirstFile + 2;

    case 0:
        m_dirStep = 0;
        // fall through
    case 1:
        m_dirStep = stepReadDirs(m_dirStep, result);
        if (m_dirStep >= 0)
            return 1;
        // fall through
    case 2:
        if (m_info.type != sceMcTypePS2) {
            m_outcome = MemcardOutcome_NoCard;
            return kStepDone;
        }
        if ((u_char)m_info.format == 0) {
            m_outcome = MemcardOutcome_Unusable;
            return kStepDone;
        }
        step = kStepFirstFile;
        // fall through
    case kStepFirstFile:
        if (m_files[fileIndex]->hasAllFiles() == true) {
            open(sSaveDataPaths[fileIndex], SCE_RDONLY, 0);
            return step + 1;
        }
        m_outcome = MemcardOutcome_Unusable;
        return kStepDone;

    default:
        return step;
    }

    m_outcome = MemcardOutcome_Ok;
    return kStepDone;
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
// Request 4: the directories, then every complete save's data file read
// and judged, for the load / save menus' file list. Outcome Ok when any
// save directory exists (SaveFile::status not Unknown / NoDir), else
// Unusable.
// FS2 0x002aa7e0
// callcheck: alloc, init, hasAllFiles, __floatsidf, __muldf3, __adddf3, __truncdfsf2 (resetFiles and SaveFile::judge inline in the ROM; the progress formula in double)

int Memcard::stepCheckAll(int step, int result)
{
    if (step == kStepFilesEnd) {
        setCheckOutcome();
        m_progress = 1.0f;
        return kStepDone;
    }

    if (isFileStep(step, 3)) {
        if (result != McResult_Done) {
            failRead();
            m_progress = 1.0f;
            return kStepDone;
        }
        m_files[fileOfStep(step)]->judge();
        return step + kStepPerFile - 3;
    }

    if (isFileStep(step, 2)) {
        if (result != McResult_Done)
            return failRead();
        close(m_fd, 0);
        return step + 1;
    }

    if (isFileStep(step, 1)) {
        if (result != McResult_Done) {
            failRead();
            m_progress = 1.0f;
            return kStepDone;
        }
        m_fd = m_syncResult;
        read(m_fd, m_files[fileOfStep(step)], kSaveDataSize, 0);
        return step + 1;
    }

    if (!isFileStep(step, 0)) {
        if (step != 2) {
            if (step != 1) {
                if (step != 0) {
                    if (step == kStepDone) {
                        m_progress = 1.0f;
                        return kStepDone;
                    }
                    return step;
                }
                m_dirStep = 0;
            }
            m_dirStep = stepReadDirs(m_dirStep, result);
            if (m_dirStep >= 0)
                return 1;
        }
        if (m_info.type != sceMcTypePS2) {
            m_outcome = MemcardOutcome_NoCard;
            m_progress = 1.0f;
            return kStepDone;
        }
        if ((u_char)m_info.format == 0) {
            m_outcome = MemcardOutcome_Unusable;
            m_progress = 1.0f;
            return kStepDone;
        }
        step = kStepFirstFile;
        m_progress = 0.1f;
    }

    // The next complete save from this step's file on. The step returned is
    // this file's read step even when a later file was opened, so a later
    // save's data lands in this file's SaveFile (the ROM's own slip; the
    // later file is then looked at again on its own turn).
    int first = fileOfStep(step);
    for (int i = first; i < kSaveFiles; i++) {
        if (m_files[i]->hasAllFiles() == true) {
            open(sSaveDataPaths[i], SCE_RDONLY, 0);
            m_progress = (float)(0.3f + 0.2 * i);   // in double, as the ROM
            return first * kStepPerFile + kStepFirstFile + 1;
        }
    }
    setCheckOutcome();
    m_progress = 1.0f;
    return kStepDone;
}

// ──────────────────────────────────────────────────────────────────────
// Request 3, at the title: is there room to save (kSaveRequiredKB free, or
// a save of this game to overwrite)? Then the first game's save
// directories (sFirstGameSaveGlobs) are listed one by one; any with an
// entry sets sMcSaveFound (memcardGetState), the first game's save bonus.
// An unformatted card counts as Ok.
// FS2 0x002aaee0

int Memcard::stepScan(int step, int result)
{
    if (isScanStep(step, 1)) {
        if (result == McError_NoEntry || result == McError_DirDenied)
            return step + kStepPerFile - 1;
        if (result != McResult_Done)
            return kStepDone;
        if (m_syncResult > 0) {
            sMcSaveFound = 1;
            return kStepDone;
        }
        return step + kStepPerFile - 1;
    }

    if (!isScanStep(step, 0)) {
        if (step != kStepFirstFile) {
            if (step != 2) {
                if (step != 1) {
                    if (step != 0) {
                        if (step == kStepDone)
                            return kStepDone;
                        m_outcome = MemcardOutcome_Ok;
                        return kStepDone;
                    }
                    m_dirStep = 0;
                }
                m_dirStep = stepReadDirs(m_dirStep, result);
                if (m_dirStep >= 0)
                    return 1;
            }
            if (m_info.type != sceMcTypePS2) {
                m_outcome = MemcardOutcome_NoCard;
                return kStepDone;
            }
            if ((u_char)m_info.format == 0) {
                m_outcome = MemcardOutcome_Ok;
                return kStepDone;
            }
        }
        if (m_info.free < kSaveRequiredKB) {
            bool any = false;
            for (int i = 0; i < kSaveFiles; i++) {
                if (m_files[i]->hasAllFiles()) {
                    any = true;
                    m_outcome = MemcardOutcome_Ok;
                    break;
                }
            }
            if (!any) {
                m_outcome = MemcardOutcome_Unusable;
                return kStepDone;
            }
        } else {
            m_outcome = MemcardOutcome_Ok;
        }
        step = kScanStepFirst;
    }

    int dir = (step - kScanStepFirst) / kStepPerFile;
    if (dir < kFirstGameSaves) {
        // Only the entry count is wanted: no table.
        getDir(sFirstGameSaveGlobs[dir], 0, -1, 0, 0);
        return step + 1;
    }
    return kStepDone;
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
    m_iconSize = 0;
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
    m_iconSize = 0;
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

// The save bar's step per finished call (inline in the ROM).
inline void Memcard::advanceProgress()
{
    m_progress += 0.05f;
    if (m_progress > 1.0f)
        m_progress = 1.0f;
}

// stepSave's failure (inline in the ROM).
inline int Memcard::failSave()
{
    clearInfo();
    m_outcome = MemcardOutcome_Failed;
    m_progress = 1.0f;
    return kStepDone;
}

// stepLoad's and stepCheckAll's failure: forget what was read (inline in
// the ROM).
inline int Memcard::failRead()
{
    resetFiles();
    clearInfo();
    m_outcome = MemcardOutcome_Failed;
    return kStepDone;
}

// stepCheckAll's outcome: Ok when any save directory is there (inline in
// the ROM).
inline void Memcard::setCheckOutcome()
{
    m_outcome = MemcardOutcome_Unusable;
    for (int i = 0; i < kSaveFiles; i++) {
        int status = m_files[i]->status;
        if (!(status == SaveStatus_Unknown || status == SaveStatus_NoDir)) {
            m_outcome = MemcardOutcome_Ok;
            break;
        }
    }
}

// A read data file's verdict (inline in the ROM, stepLoad / stepCheckAll):
// all three files with a good magic and sum, a broken save, or none of its
// files. A save already judged NoDir / Denied / Broken keeps its status.
inline void SaveFile::judge()
{
    if (hasAllFiles()) {
        if (magic == kSaveMagic && sum == saveByteSum(this))
            status = SaveStatus_Ok;
        else
            status = SaveStatus_Broken;
    } else if (status == SaveStatus_Unknown || status == SaveStatus_Ok) {
        status = found == 0 ? SaveStatus_Empty : SaveStatus_Broken;
    }
}

// ──────────────────────────────────────────────────────────────────────
// icon.sys for save `index` into m_iconSys: the lighting and background
// from the template MenuPackItem_IconSys, the title, icon1.ico for all
// three views.
// FS2 0x002ab7c0

void Memcard::buildIconSys(int index)
{
    if (m_iconSys == 0)
        m_iconSys = static_cast<sceMcIconSys *>(gGlobalHeap.alloc(kIconSysSize, kMemcardAlign));
    const sceMcIconSys *tmpl = static_cast<const sceMcIconSys *>(getMenuPackItem(MenuPackItem_IconSys));

    sceMcIconSys *sys = m_iconSys;
    memset(sys, 0, kIconSysSize);
    sys->Head[0] = 'P';
    sys->Head[1] = 'S';
    sys->Head[2] = '2';
    sys->Head[3] = 'D';
    strcpy(reinterpret_cast<char *>(sys->FnameView), kIconName);
    strcpy(reinterpret_cast<char *>(sys->FnameCopy), kIconName);
    strcpy(reinterpret_cast<char *>(sys->FnameDel), kIconName);
    if (tmpl != 0) {
        sys->TransRate = tmpl->TransRate;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                sys->BgColor[i][j] = tmpl->BgColor[i][j];
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
                sys->LightDir[i][j] = tmpl->LightDir[i][j];
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
                sys->LightColor[i][j] = tmpl->LightColor[i][j];
        for (int j = 0; j < 4; j++)
            sys->Ambient[j] = tmpl->Ambient[j];
    }

    sys = m_iconSys;
    const char *title = sIconSysTitles[index];
    memset(sys->TitleName, 0, sizeof(sys->TitleName));
    strncpy(reinterpret_cast<char *>(sys->TitleName), title, sizeof(sys->TitleName) - 2);
    sys->OffsLF = kIconSysTitleBreak;
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
    int iconSize = getMenuPackItemSize(MenuPackItem_SaveIcon);
    m_iconData = getMenuPackItem(MenuPackItem_SaveIcon);
    m_iconSize = iconSize;
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
// The implicit copy assignments of the system data's parts, which
// CodeWarrior emitted out of line here for exportTo / importFrom (0x2b9710
// calls them too). Each copies member by member, which bears out the
// layouts in sysdata.h (SaveData::_0ae is 0x27 bytes, then padding; the
// record table and FlagTable go as doublewords, the medals as byte
// pairs). Nothing to write: the C++ compiler makes them.
// FS2 0x002ac170 FlagTable::operator= (implicit)
// FS2 0x002ac1a0 GameOptions::operator= (implicit)
// FS2 0x002ac260 StageSelect::operator= (implicit)
// FS2 0x002ac290 SaveData::operator= (implicit)

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
