/* ==========================================================================
 *  sdk/mcerror.h
 *
 *  libmc's error encoder (sdk/mcerror.cpp, 0x121b80..0x121ef0): one
 *  function, linked after libmc's read-fast tail and before libc, that
 *  turns a libmc command number and its negative result into a
 *  0x8108xxxx code (SCE error prefix 0x80000000, the memory-card facility
 *  0x01080000, an errno in the EE's newlib numbering or a Sony code
 *  0x8000.. / 0x9000..). The game keeps the low 16 bits (game/memcard.h
 *  McError).
 *
 *  The values here are the EE's: host <errno.h> numbers differ (MinGW's
 *  ENOTEMPTY is 41, newlib's 90), so the codes are spelled out.
 * ======================================================================== */
#ifndef _SDK_MCERROR_H
#define _SDK_MCERROR_H

// libmc command numbers, as sceMcSync reports them in *cmd (SDK libmc.h
// sceMcFuncNo*; the two fast ones are FS2's read-fast rpcs).
// Bootstrap 6: move these into sdk/libmc.h when it comes over from RosePan.
enum McCmd
{
    McCmd_GetInfo   = 0x01,     // sceMcFuncNoCardInfo
    McCmd_Open      = 0x02,     // sceMcFuncNoOpen
    McCmd_Close     = 0x03,     // sceMcFuncNoClose
    McCmd_Seek      = 0x04,     // sceMcFuncNoSeek
    McCmd_Read      = 0x05,     // sceMcFuncNoRead
    McCmd_Write     = 0x06,     // sceMcFuncNoWrite
    McCmd_Flush     = 0x0a,     // sceMcFuncNoFlush
    McCmd_Mkdir     = 0x0b,     // sceMcFuncNoMkdir
    McCmd_Chdir     = 0x0c,     // sceMcFuncNoChDir
    McCmd_GetDir    = 0x0d,     // sceMcFuncNoGetDir
    McCmd_Delete    = 0x0f,     // sceMcFuncNoDelete
    McCmd_Format    = 0x10,     // sceMcFuncNoFormat
    McCmd_EntSpace  = 0x12,     // sceMcFuncNoEntSpace
    McCmd_Rename    = 0x13,     // sceMcFuncNoRename
    McCmd_ReadFast  = 0x16,     // sceMcReadFast 0x121970's rpc (McDevice::read records it)
    McCmd_WriteFast = 0x1b,     // sceMcWriteFast 0x121a80's rpc
    McCmd_NoError   = 0x20,     // unknown; every negative result of it maps to 0
};

// The low 16 bits of the codes (EE newlib errno values, then Sony's).
enum McErrno
{
    McErrno_NoEnt       = 2,        // ENOENT
    McErrno_Io          = 5,        // EIO
    McErrno_BadF        = 9,        // EBADF
    McErrno_Acces       = 13,       // EACCES
    McErrno_Busy        = 16,       // EBUSY
    McErrno_Exist       = 17,       // EEXIST
    McErrno_NoDev       = 19,       // ENODEV
    McErrno_Inval       = 22,       // EINVAL
    McErrno_MFile       = 24,       // EMFILE
    McErrno_NoSpc       = 28,       // ENOSPC
    McErrno_NoFormat    = 0x002f,   // sceMcResNoFormat (not a newlib errno)
    McErrno_NotEmpty    = 90,       // ENOTEMPTY
    McErrno_Refused     = 111,      // newlib ECONNREFUSED: DeniedPermit outside the file calls
    McErrno_NotInit     = 0x8001,   // sceMcErrNotInit (-100)
    McErrno_Sony8003    = 0x8003,   // result -101
    McErrno_Sony8009    = 0x8009,   // results -120, -121
    McErrno_Sony800a    = 0x800a,   // result -91
    McErrno_ChangedCard = 0x9003,   // sceMcResChangedCard
};

// SCE_ERROR_ENCODE(memory card, e).
constexpr int kMcErrorPrefix = (int)0x81080000;
constexpr int mcErrorEncode(int e) { return kMcErrorPrefix | e; }

// (libmc command, libmc result) -> the result itself when >= 0, else its
// 0x8108xxxx code (one case answers a bare McErrno_Io: mcerror.cpp).
int mcErrorCode(int cmd, int result);                                    // FS2 0x00121b80

#endif // _SDK_MCERROR_H
