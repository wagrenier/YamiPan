/* ==========================================================================
 *  rx/filehost.c
 *
 *  rx's host0: device driver (0x156060..0x1565c8). Every name is joined to
 *  a root prefix ("host0:" until rxFileHostSetRoot says otherwise) and
 *  opened with the plain rxSys file calls, one open / seek / read / close
 *  per request; bit 0 of s_hostFlags says a call is in progress.
 *
 *  FS2 links Rule of Rose's prebuilt rx: this is RosePan's host0: driver
 *  (../RosePan/src/rx/rxfile.cpp, ROR 0x246318..0x246830) with one
 *  difference: FS2's open-failure message carries the source file and line
 *  ("rxfile(%s,%d) : failed to open file '%s' %d", "filehost.c", lines
 *  125 / 157 / 188 / 234) and the fd. Data: s_hostRootDefault 0x2f5aa8
 *  (-> "host0:" 0x3018d0), s_hostFlags 0x2f5aac, s_hostRoot bss 0x4d5a20
 *  (0x100 bytes, as RoR's).
 *
 *  PC: rxSysOpen maps "host0:" onto the data root, so this is the driver
 *  the boot's file loads go through when a device is mapped onto HOST.
 * ======================================================================== */
#include "rx/filehost.h"

#include <string.h>

#include "rx/rxfile.h"
#include "rx/rxsys.h"
#include "sdk/eekernel.h"
#include "sdk/fileio.h"

/* ---- constants -------------------------------------------------------- */

/* s_hostFlags bits */
#define RX_FILE_HOST_BUSY        0x1    /* inside a file call (rxFileHostGetStatus 1) */

/* rxFileHostGetStatus queries */
#define RX_FILE_HOST_STATUS_BUSY 1

/* Driver call results */
#define RX_FILE_HOST_ERR_OPEN    (-2)   /* the file did not open */
#define RX_FILE_HOST_ERR_IO      (-1)   /* read / write returned nothing */

#define RX_FILE_HOST_PATH_MAX    0x100

/* The open-failure message: the ROM's __FILE__ / __LINE__ (0x301908, and
 * the line numbers each call passes). */
#define RX_FILE_HOST_SRC         "filehost.c"
#define RX_FILE_HOST_OPEN_FAILED "rxfile(%s,%d) : failed to open file '%s' %d\n" /* 0x3018d8 */

/* ---- data ------------------------------------------------------------- */

static const char *s_hostRootDefault = "host0:";    /* data 0x2f5aa8 (-> 0x3018d0) */
static int s_hostFlags;                             /* data 0x2f5aac: RX_FILE_HOST_BUSY */
static char s_hostRoot[RX_FILE_HOST_PATH_MAX];      /* bss 0x4d5a20 */

// ──────────────────────────────────────────────────────────────────────
// Fill the host0: entry of the driver table; the root back to "host0:".
// FS2 0x00156060

void rxFileHostInit(RxFileDriver *drv)
{
    drv->makePath  = rxFileHostMakePath;
    drv->setRoot   = rxFileHostSetRoot;
    drv->control   = rxFileHostControl;
    drv->getStatus = rxFileHostGetStatus;
    drv->getSize   = rxFileHostGetSize;
    drv->create    = rxFileHostCreate;
    drv->read      = rxFileHostRead;
    drv->write     = rxFileHostWrite;
    drv->getRoot   = rxFileHostGetRoot;
    rxFileHostSetRoot(0);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001560e8

void rxFileHostTerm(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001560f0

void rxFileHostMakePath(char *out, const char *name)
{
    rxFileHostJoinRoot(out, name);
}

// ──────────────────────────────────────────────────────────────────────
// No controls.
// FS2 0x00156108

int rxFileHostControl(int cmd, int arg)
{
    (void)cmd;
    (void)arg;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00156110

int rxFileHostGetStatus(int what)
{
    if (what == RX_FILE_HOST_STATUS_BUSY)
        return s_hostFlags & RX_FILE_HOST_BUSY;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The file's size (a seek to its end), or -2 if it does not open.
// FS2 0x00156138

int rxFileHostGetSize(const char *name)
{
    char path[RX_FILE_HOST_PATH_MAX];
    int size;

    s_hostFlags |= RX_FILE_HOST_BUSY;
    rxFileHostJoinRoot(path, name);
    int fd = rxSysOpen(path, SCE_RDONLY);
    if (fd < 0)
    {
        if (rxFileVerbose != 0)
            rxSysPrintf(RX_FILE_HOST_OPEN_FAILED, RX_FILE_HOST_SRC, 125, path, fd);
        size = RX_FILE_HOST_ERR_OPEN;
    }
    else
    {
        size = rxSysLseek(fd, 0, SCE_SEEK_END);
        rxSysClose(fd);
    }
    s_hostFlags &= ~RX_FILE_HOST_BUSY;
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// Create the file (emptied when truncate is set): 1, or -2.
// FS2 0x00156208

int rxFileHostCreate(const char *name, int truncate)
{
    char path[RX_FILE_HOST_PATH_MAX];
    int ret;

    s_hostFlags |= RX_FILE_HOST_BUSY;
    rxFileHostJoinRoot(path, name);
    int mode = SCE_WRONLY | SCE_CREAT | SCE_TRUNC;
    if (truncate == 0)
        mode = SCE_WRONLY | SCE_CREAT;
    int fd = rxSysOpen(path, mode);
    if (fd < 0)
    {
        /* The source passes no fd here, but the trailing %d reads t0, which
         * still holds the fd (0x156250 move t0,v0): the same output. */
        if (rxFileVerbose != 0)
            rxSysPrintf(RX_FILE_HOST_OPEN_FAILED, RX_FILE_HOST_SRC, 157, path, fd);
        ret = RX_FILE_HOST_ERR_OPEN;
    }
    else
    {
        rxSysClose(fd);
        ret = 1;
    }
    s_hostFlags &= ~RX_FILE_HOST_BUSY;
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// Read size bytes at offset: size, -2 (no file) or -1 (nothing read).
// FS2 0x001562c8

int rxFileHostRead(const char *name, void *buf, int offset, int size)
{
    char path[RX_FILE_HOST_PATH_MAX];

    s_hostFlags |= RX_FILE_HOST_BUSY;
    rxFileHostJoinRoot(path, name);
    FlushCache(0);
    int fd = rxSysOpen(path, SCE_RDONLY);
    if (fd < 0)
    {
        if (rxFileVerbose != 0)
            rxSysPrintf(RX_FILE_HOST_OPEN_FAILED, RX_FILE_HOST_SRC, 188, path, fd);
        size = RX_FILE_HOST_ERR_OPEN;
    }
    else
    {
        rxSysLseek(fd, offset, SCE_SEEK_SET);
        if (rxSysRead(fd, buf, size) < 1)
        {
            rxSysClose(fd);
            size = RX_FILE_HOST_ERR_IO;
        }
        else
        {
            rxSysClose(fd);
            FlushCache(0);
        }
    }
    s_hostFlags &= ~RX_FILE_HOST_BUSY;
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// Write size bytes at offset into an existing file: size, -2 or -1.
// FS2 0x001563f8

int rxFileHostWrite(const char *name, int offset, const void *buf, int size)
{
    char path[RX_FILE_HOST_PATH_MAX];

    s_hostFlags |= RX_FILE_HOST_BUSY;
    rxFileHostJoinRoot(path, name);
    FlushCache(0);
    int fd = rxSysOpen(path, SCE_WRONLY);
    if (fd < 0)
    {
        if (rxFileVerbose != 0)
            rxSysPrintf(RX_FILE_HOST_OPEN_FAILED, RX_FILE_HOST_SRC, 234, path, fd);
        size = RX_FILE_HOST_ERR_OPEN;
    }
    else
    {
        rxSysLseek(fd, offset, SCE_SEEK_SET);
        if (rxSysWrite(fd, buf, size) < 1)
        {
            rxSysClose(fd);
            size = RX_FILE_HOST_ERR_IO;
        }
        else
        {
            rxSysClose(fd);
            FlushCache(0);
        }
    }
    s_hostFlags &= ~RX_FILE_HOST_BUSY;
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// out = root + name.
// FS2 0x00156528

void rxFileHostJoinRoot(char *out, const char *name)
{
    strcpy(out, s_hostRoot);
    strcpy(out + strlen(out), name);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00156570

char *rxFileHostGetRoot(void)
{
    return s_hostRoot;
}

// ──────────────────────────────────────────────────────────────────────
// null = "host0:".
// FS2 0x00156580

void rxFileHostSetRoot(const char *root)
{
    if (root == 0)
    {
        strcpy(s_hostRoot, s_hostRootDefault);
        return;
    }
    strcpy(s_hostRoot, root);
}
