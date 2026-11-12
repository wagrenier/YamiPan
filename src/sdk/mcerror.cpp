// sdk/mcerror.cpp -- libmc's error encoder: a libmc command and its result
// to a 0x8108xxxx code (sdk/mcerror.h).

#include "sdk/mcerror.h"

#include "sdk/libmc.h"

// FS2 0x00121b80
// A compare chain in the ROM (CodeWarrior's switch); the command only
// matters for FullDevice, NoEntry, DeniedPermit and NotEmpty.

int mcErrorCode(int cmd, int result)
{
    if (result >= 0)
        return result;
    if (cmd == McCmd_NoError)
        return 0;

    switch (result) {
    case sceMcResChangedCard:
        return mcErrorEncode(McErrno_ChangedCard);
    case sceMcResNoFormat:
        return mcErrorEncode(McErrno_NoFormat);

    case sceMcResFullDevice:
        // A full card on a read answers a bare EIO, without the prefix (as
        // the ROM does; the game keeps only the low 16 bits anyway).
        if (cmd == McCmd_Read || cmd == McCmd_ReadFast)
            return McErrno_Io;
        return mcErrorEncode(McErrno_NoSpc);

    case sceMcResNoEntry:
        switch (cmd) {
        case McCmd_Delete:
        case McCmd_Chdir:
        case McCmd_GetDir:
        case McCmd_Open:
        case McCmd_Rename:
            return mcErrorEncode(McErrno_NoEnt);
        case McCmd_Mkdir:
            return mcErrorEncode(McErrno_Exist);    // mkdir's NoEntry: the name is taken
        case McCmd_Flush:
        case McCmd_Read:
        case McCmd_ReadFast:
        case McCmd_Close:
        case McCmd_Seek:
        case McCmd_Write:
        case McCmd_WriteFast:
            return mcErrorEncode(McErrno_BadF);     // the handle names no file
        default:
            return mcErrorEncode(McErrno_NoEnt);
        }

    case sceMcResDeniedPermit:
        switch (cmd) {
        case McCmd_GetDir:
        case McCmd_EntSpace:
        case McCmd_Open:
        case McCmd_Read:
        case McCmd_ReadFast:
        case McCmd_Write:
        case McCmd_WriteFast:
            return mcErrorEncode(McErrno_Acces);
        default:
            return mcErrorEncode(McErrno_Refused);
        }

    case sceMcResNotEmpty:
        if (cmd == McCmd_GetDir || cmd == McCmd_Chdir)
            return mcErrorEncode(McErrno_Io);
        return mcErrorEncode(McErrno_NotEmpty);

    case sceMcResUpLimitHandle:
        return mcErrorEncode(McErrno_MFile);

    case sceMcResFailReplace:
    case -9:
    case -40:
        return mcErrorEncode(McErrno_Io);

    case sceMcErrNotInit:
        return mcErrorEncode(McErrno_NotInit);
    case -101:
        return mcErrorEncode(McErrno_Sony8003);
    case -91:
        return mcErrorEncode(McErrno_Sony800a);
    case -120:
    case -121:
        return mcErrorEncode(McErrno_Sony8009);
    case sceMcErrBusy:
        return mcErrorEncode(McErrno_Busy);
    case sceMcErrEntryName:
    case -403:
        return mcErrorEncode(McErrno_Inval);

    // -10..-15 and -41 / -42 have their own compares in the ROM but share
    // the default's code.
    default:
        return mcErrorEncode(McErrno_NoDev);
    }
}
