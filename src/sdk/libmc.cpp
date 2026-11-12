// sdk/libmc.cpp -- no memory card yet: every request finishes at once with "no entry", so the game runs
// as with an empty slot. The real card, files under memcard/ in the user directory, follows.
#include "libmc.h"

namespace
{

int s_pendingCommand = 0;
int s_pendingResult = 0;

int finish(int result)
{
    s_pendingResult = result;
    return sceMcResSucceed;
}

} // namespace

int sceMcInit()
{
    return sceMcResSucceed;
}

int sceMcEnd()
{
    return sceMcResSucceed;
}

int sceMcSync(int, int *cmd, int *result)
{
    if (cmd != nullptr)
        *cmd = s_pendingCommand;
    if (result != nullptr)
        *result = s_pendingResult;
    return sceMcExecFinish;
}

int sceMcGetInfo(int, int, int *type, int *free, int *format)
{
    if (type != nullptr)
        *type = 0;
    if (free != nullptr)
        *free = 0;
    if (format != nullptr)
        *format = 0;
    return finish(sceMcResNoEntry);
}

int sceMcGetDir(int, int, const char *, unsigned int, int, sceMcTblGetDir *)
{
    return finish(sceMcResNoEntry);
}

int sceMcOpen(int, int, const char *, int)
{
    return finish(sceMcResNoEntry);
}

int sceMcClose(int)
{
    return finish(sceMcResSucceed);
}

int sceMcRead(int, void *, int)
{
    return finish(sceMcResNoEntry);
}

int sceMcWrite(int, const void *, int)
{
    return finish(sceMcResNoEntry);
}

int sceMcMkdir(int, int, const char *)
{
    return finish(sceMcResNoEntry);
}

int sceMcFormat(int, int)
{
    return finish(sceMcResSucceed);
}

int sceMcFlush(int)
{
    return finish(sceMcResSucceed);
}
