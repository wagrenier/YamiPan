/* ==========================================================================
 *  rx/filehdd.c
 *
 *  rx's HDD device driver (0x157db8..0x158400). Every name is joined to a
 *  root prefix ("host0:" until rxFileHddSetRoot says otherwise) and opened
 *  with the plain rxSys file calls, one open / seek / read / close per
 *  request; bit 0 of s_hddFlags says a call is in progress. A drive error
 *  (EIO, ENXIO, ENODEV or the HDD's own -65531) moves RX_SYS_DEVICE_HDD
 *  onto its fallback device and calls the game's error hook.
 *
 *  FS2 links Rule of Rose's prebuilt rx: this is RosePan's HDD driver
 *  (../RosePan/src/rx/rxfile.cpp, ROR 0x248018..0x248660), every function
 *  the same in FS2's decompilation. Data: s_hddRootDefault 0x2f5b10,
 *  s_hddFlags 0x2f5b14, s_hddRoot bss 0x4e7d80 (0x100 bytes, as RoR's).
 *
 *  PC: the paths go through rxSysOpen like host0:'s; nothing in FS2 maps a
 *  device onto HDD on the smoke path, so this runs only when a save or a
 *  debug option asks for it.
 * ======================================================================== */
#include "rx/filehdd.h"

#include <string.h>

#include "rx/rxfile.h"
#include "rx/rxsys.h"
#include "sdk/eekernel.h"
#include "sdk/fileio.h"

/* ---- constants -------------------------------------------------------- */

/* s_hddFlags bits */
#define RX_FILE_HDD_BUSY        0x1     /* inside a file call (rxFileHddGetStatus 1) */

/* rxFileHddGetStatus queries */
#define RX_FILE_HDD_STATUS_BUSY 1

/* Errors rxFileHddOnError treats as a lost drive (the IOP's negated errno
 * values; -65531 = -0xfffb comes from the HDD driver itself). */
#define RX_FILE_HDD_EIO         (-5)
#define RX_FILE_HDD_ENXIO       (-6)
#define RX_FILE_HDD_ENODEV      (-19)
#define RX_FILE_HDD_EDRIVE      (-0xfffb)

/* Driver call results */
#define RX_FILE_HDD_ERR_OPEN    (-2)    /* the file did not open */
#define RX_FILE_HDD_ERR_IO      (-1)    /* read / write returned nothing */

#define RX_FILE_HDD_PATH_MAX    0x100

/* ---- data ------------------------------------------------------------- */

static const char *s_hddRootDefault = "host0:"; /* data 0x2f5b10 (-> 0x3019b8) */
static int s_hddFlags;                          /* data 0x2f5b14: RX_FILE_HDD_BUSY */
static char s_hddRoot[RX_FILE_HDD_PATH_MAX];    /* bss 0x4e7d80 */

// ──────────────────────────────────────────────────────────────────────
// Fill the HDD entry of the driver table; the root back to "host0:".
// FS2 0x00157db8

void rxFileHddInit(RxFileDriver *drv)
{
    drv->makePath  = rxFileHddMakePath;
    drv->setRoot   = rxFileHddSetRoot;
    drv->control   = rxFileHddControl;
    drv->getStatus = rxFileHddGetStatus;
    drv->getSize   = rxFileHddGetSize;
    drv->create    = rxFileHddCreate;
    drv->read      = rxFileHddRead;
    drv->write     = rxFileHddWrite;
    drv->getRoot   = rxFileHddGetRoot;
    rxFileHddSetRoot(0);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00157e40

void rxFileHddTerm(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// On a drive error send HDD accesses to its fallback device and tell the
// error hook.
// FS2 0x00157e48

void rxFileHddOnError(const char *name, int error)
{
    /* the ROM: error < -4 && (error > -7 || error == -0xfffb || error == -19) */
    if ((error <= RX_FILE_HDD_EIO && error >= RX_FILE_HDD_ENXIO)
        || error == RX_FILE_HDD_EDRIVE || error == RX_FILE_HDD_ENODEV)
    {
        if (rxFileVerbose != 0)
            rxSysPrintf("rxFile: occured I/O error (%d), RX_FILE_DEVICE_HDD.\n", error);
        rxFileSetDeviceMap(RX_SYS_DEVICE_HDD, rxFileGetFallbackDevice(RX_SYS_DEVICE_HDD));
        if (rxFileGetErrorHook() != 0)
            rxFileGetErrorHook()(name, error);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00157ef8

void rxFileHddMakePath(char *out, const char *name)
{
    rxFileHddJoinRoot(out, name);
}

// ──────────────────────────────────────────────────────────────────────
// No controls.
// FS2 0x00157f10

int rxFileHddControl(int cmd, int arg)
{
    (void)cmd;
    (void)arg;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00157f18

int rxFileHddGetStatus(int what)
{
    if (what == RX_FILE_HDD_STATUS_BUSY)
        return s_hddFlags & RX_FILE_HDD_BUSY;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The file's size (a seek to its end), or -2 if it does not open.
// FS2 0x00157f40

int rxFileHddGetSize(const char *name)
{
    char path[RX_FILE_HDD_PATH_MAX];
    int size;

    s_hddFlags |= RX_FILE_HDD_BUSY;
    rxFileHddJoinRoot(path, name);
    int fd = rxSysOpen(path, SCE_RDONLY);
    if (fd < 0)
    {
        rxFileHddOnError(name, fd);
        if (rxFileVerbose != 0)
            rxSysPrintf("rxfile : failed to open file '%s'\n", path);
        size = RX_FILE_HDD_ERR_OPEN;
    }
    else
    {
        size = rxSysLseek(fd, 0, SCE_SEEK_END);
        rxSysClose(fd);
    }
    s_hddFlags &= ~RX_FILE_HDD_BUSY;
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// Create the file (emptied when truncate is set): 1, or -2.
// FS2 0x00158010

int rxFileHddCreate(const char *name, int truncate)
{
    char path[RX_FILE_HDD_PATH_MAX];
    int ret;

    s_hddFlags |= RX_FILE_HDD_BUSY;
    rxFileHddJoinRoot(path, name);
    int mode = SCE_WRONLY | SCE_CREAT | SCE_TRUNC;
    if (truncate == 0)
        mode = SCE_WRONLY | SCE_CREAT;
    int fd = rxSysOpen(path, mode);
    if (fd < 0)
    {
        rxFileHddOnError(name, fd);
        if (rxFileVerbose != 0)
            rxSysPrintf("rxfile : failed to open file '%s'\n", path);
        ret = RX_FILE_HDD_ERR_OPEN;
    }
    else
    {
        rxSysClose(fd);
        ret = 1;
    }
    s_hddFlags &= ~RX_FILE_HDD_BUSY;
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// Read size bytes at offset: size, -2 (no file) or -1 (nothing read).
// FS2 0x001580e0

int rxFileHddRead(const char *name, void *buf, int offset, int size)
{
    char path[RX_FILE_HDD_PATH_MAX];

    s_hddFlags |= RX_FILE_HDD_BUSY;
    rxFileHddJoinRoot(path, name);
    FlushCache(0);
    int fd = rxSysOpen(path, SCE_RDONLY);
    if (fd < 0)
    {
        rxFileHddOnError(name, fd);
        if (rxFileVerbose != 0)
            rxSysPrintf("rxfile : failed to open file '%s'\n", path);
        size = RX_FILE_HDD_ERR_OPEN;
    }
    else
    {
        rxSysLseek(fd, offset, SCE_SEEK_SET);
        int got = rxSysRead(fd, buf, size);
        if (got < 1)
        {
            rxSysClose(fd);
            rxFileHddOnError(name, got);
            size = RX_FILE_HDD_ERR_IO;
        }
        else
        {
            rxSysClose(fd);
            FlushCache(0);
        }
    }
    s_hddFlags &= ~RX_FILE_HDD_BUSY;
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// Write size bytes at offset into an existing file: size, -2 or -1.
// FS2 0x00158220

int rxFileHddWrite(const char *name, int offset, const void *buf, int size)
{
    char path[RX_FILE_HDD_PATH_MAX];

    s_hddFlags |= RX_FILE_HDD_BUSY;
    rxFileHddJoinRoot(path, name);
    FlushCache(0);
    int fd = rxSysOpen(path, SCE_WRONLY);
    if (fd < 0)
    {
        rxFileHddOnError(name, fd);
        if (rxFileVerbose != 0)
            rxSysPrintf("rxfile : failed to open file '%s'\n", path);
        size = RX_FILE_HDD_ERR_OPEN;
    }
    else
    {
        rxSysLseek(fd, offset, SCE_SEEK_SET);
        int put = rxSysWrite(fd, buf, size);
        if (put < 1)
        {
            rxSysClose(fd);
            rxFileHddOnError(name, put);
            size = RX_FILE_HDD_ERR_IO;
        }
        else
        {
            rxSysClose(fd);
            FlushCache(0);
        }
    }
    s_hddFlags &= ~RX_FILE_HDD_BUSY;
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// out = root + name.
// FS2 0x00158360

void rxFileHddJoinRoot(char *out, const char *name)
{
    strcpy(out, s_hddRoot);
    strcpy(out + strlen(out), name);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001583a8

char *rxFileHddGetRoot(void)
{
    return s_hddRoot;
}

// ──────────────────────────────────────────────────────────────────────
// null = "host0:".
// FS2 0x001583b8

void rxFileHddSetRoot(const char *root)
{
    if (root == 0)
    {
        strcpy(s_hddRoot, s_hddRootDefault);
        return;
    }
    strcpy(s_hddRoot, root);
}
