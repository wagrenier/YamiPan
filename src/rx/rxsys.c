/* ==========================================================================
 *  rx/rxsys.c
 *
 *  The rx middleware's system layer: the console bring-up FS2's rxInit runs
 *  before the game (rxSysInit), the IOP reboot and module list, the boot
 *  path, system.ini, and rxSysPrintf. See rxsys.h.
 *
 *  FS2 links Rule of Rose's prebuilt rx: this file is RosePan's
 *  src/rx/rxsys.cpp (every function written there, checked against RoR)
 *  carried over to C. All 126 functions were compared word for word in the
 *  two ELFs (rxcmp.py: jump targets, lui and gp offsets masked) and are
 *  identical; the initialised data and the console font are RoR's words,
 *  moved by +0x14eb0 (bss: 0x31b100.. here, RoR 0x7cc340..). FS2 has no
 *  rxsys difference to mark.
 *
 *  The System Monitor (0x102fe0..0x1037a8), the text console the exception
 *  handlers print on, is log-only on the PC: its GS image uploads and
 *  display env are dropped, the text goes to the host log.
 * ======================================================================== */
#include "rx/rxsys.h"

#include <ctype.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rx/rxfile.h"
#include "rx/rxthread.h"
#include "yamipan/yamipan_memory.h"
#include "sdk/eekernel.h"
#include "sdk/libcdvd.h"
#include "sdk/libdma.h"
#include "sdk/libgraph.h"
#include "sdk/sifdev.h"
#include "sdk/sifrpc.h"
#include "sdk/fileio.h"

/* ---- data ------------------------------------------------------------- */

/* "0300" followed by "$RX_SCE_LIBRARY_VERSION:..." in the ROM: the SDK
 * version the middleware was built against. rxSysInit rewrites its first
 * character with the same '0'. */
static char s_libVersion[] = "0300";            /* data 0x2f0888 */

static RxSysPrintHook s_printHook;              /* data 0x2f0880 */

static char s_productNumber[16] = "SCPS-00000"; /* data 0x2f08b0, system.ini NUMBER   */
static int  s_version = 100;                    /* data 0x2f08c0, system.ini VERSION  */
static int  s_country;                          /* data 0x2f08c4, system.ini COUNTRY  */
static int  s_language;                         /* data 0x2f08c8, system.ini LANGUAGE */
static int  s_vmode;                            /* data 0x2f08cc, system.ini VMODE    */
static int  s_warning;                          /* data 0x2f08d0, system.ini WARNING  */
static int  s_bootDevice = -1;                  /* data 0x2f08d4, RX_SYS_DEVICE       */
static int  s_iniLoaded;                        /* data 0x2f08d8 */
static int  s_iopRebooted;                      /* data 0x2f08dc */
static int  s_romDate;                          /* data 0x2f08e0, 0xYYYYMMDD in BCD   */
static int  s_ttyFd = -1;                       /* data 0x2f08e4, host tty file       */
static int  s_ttyRpcBound;                      /* data 0x2f08e8 */
static char s_bootDev[0x80];                    /* data 0x2f08f0, "host0:" -> ""     */
static char s_bootDir[0x80];                    /* data 0x2f0970 */
static char s_bootName[0x80];                   /* data 0x2f09f0 */

/* Save-data prefix per COUNTRY (JAPAN, USA, EUROPE, ASIA). */
static const char *const s_regionPrefix[4] = { "BI", "BA", "BE", "BI" };  /* data 0x2f0a70 */
static int s_moduleCount;                       /* data 0x2f0a80 */

static const char *const s_vmodeNames[] = { "NTSC", "PAL", 0 };          /* data 0x2f0a88 */
static const char *const s_countryNames[] = { "JAPAN", "USA", "EUROPE", "ASIA", 0 };  /* data 0x2f0a98 */
static const char *const s_languageNames[] = {                           /* data 0x2f0ab0 */
    "JAPANESE", "ENGLISH", "GERMANY", "FRENCH", "ITALIAN", "SPANISH", "KOREAN", "CHINESE", 0,
};
static const char *const s_warningNames[] = { "NO", "YES", 0 };          /* data 0x2f0ad8 */
static const char *const s_deviceNames[RX_SYS_DEVICE_NUM] = {            /* data 0x2f0ae8 */
    "HOST ", "CDROM", "MCARD", "HDD  ",
};

static int  s_ttyBufLen;                        /* data 0x2f0af8 */
static char s_iopImage[0x100] = "ioprp300.img"; /* data 0x2f0b00 */

static const char *const s_moduleListBase[] = {                          /* data 0x2f0c08 */
    "sifcmd.irx", "fileio.irx", "cdvdman.irx", "cdvdfsv.irx", 0,
};
static const char *const s_moduleListPad[] = {                           /* data 0x2f0c20 */
    "sio2man.irx", "padman.irx", "mcman.irx", "mcserv.irx", 0,
};
static const char *const s_moduleListUsb[] = { "usbd.irx", 0 };          /* data 0x2f0c38 */

static int s_hardwareReset;                     /* data 0x2f0c40 */
static int s_sema1 = -1;                        /* data 0x2f0c44 */
static int s_printSema = -1;                    /* data 0x2f0c48 */

/* The exception handler's register rows: a label, then count registers
 * from first.
 * layout: host -- a pointer table, never read from files. */
typedef struct RxSysRegRow
{
    const char *name;
    int         first;
    int         count;
    int         pad;
} RxSysRegRow;
static const RxSysRegRow s_regRows[] = {                                  /* data 0x2f0c50 */
    { "\nat/v0-1    ",  1, 3, 0 },
    { "\na0-3       ",  4, 4, 0 },
    { "\nt0-3       ",  8, 4, 0 },
    { "\nt4-9       ", 12, 4, 0 },
    { "",              24, 2, 0 },
    { "\ns0-8       ", 16, 4, 0 },
    { "\ns4-7/k0-1  ", 20, 4, 0 },
    { "",              26, 2, 0 },
    { "\ngp/sp/fp/ra", 28, 4, 0 },
    { NULL,             0, 0, 0 },
};
static int s_exceptionOpen;                     /* data 0x2f0cf0, set by the handler, read by nothing found */
static int s_inException;                       /* data 0x2f0cf4 */
static YamiPanDebugHandler s_oldHandlersLow[7]; /* data 0x2f0cfc, causes 1..7   */
static YamiPanDebugHandler s_oldHandlersHigh[4];/* data 0x2f0d20, causes 10..13 */
static int s_debugHandlersSet;                  /* data 0x2f0d38 */

RxSysMemFunc g_rxSysMemFunc[4];                 /* data 0x2f0d3c..0x2f0d48 */

static char s_saveName[0x40];                   /* bss 0x31b100: region prefix + NUMBER  */
static char s_modules[32][32];                  /* bss 0x31b140: loaded IOP modules      */
static int  s_deviceReady[RX_SYS_DEVICE_NUM];   /* bss 0x31b540 */
static char s_confBuf[0x1000];                  /* bss 0x31b580: iopconf.cnf, system.ini */
static char s_ttyBuf[0x1000];                   /* bss 0x31c580 */
static sceSifClientData s_ttyClient;            /* bss 0x31d580 */

static inline int rxSysIsPathSep(char c)
{
    return c == '/' || c == '\\' || c == ':' || c == ',';
}

/* ---- memory hooks ----------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// What every unset memory hook points at.
// FS2 0x00100328

int rxSysNoMemFunc(void)
{
    rxSysPrintf("rxSys:No Memory Allocate Func !\n");
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Install memory hook 0 (null = rxSysNoMemFunc), returning the old one.
// FS2 0x00100350

RxSysMemFunc rxSysSetMemFunc0(RxSysMemFunc func)
{
    RxSysMemFunc old = g_rxSysMemFunc[0];
    g_rxSysMemFunc[0] = func;
    if (func == 0)
        g_rxSysMemFunc[0] = (RxSysMemFunc)rxSysNoMemFunc;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00100380

RxSysMemFunc rxSysSetMemFunc1(RxSysMemFunc func)
{
    RxSysMemFunc old = g_rxSysMemFunc[1];
    g_rxSysMemFunc[1] = func;
    if (func == 0)
        g_rxSysMemFunc[1] = (RxSysMemFunc)rxSysNoMemFunc;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001003b0

RxSysMemFunc rxSysSetMemFunc2(RxSysMemFunc func)
{
    RxSysMemFunc old = g_rxSysMemFunc[2];
    g_rxSysMemFunc[2] = func;
    if (func == 0)
        g_rxSysMemFunc[2] = (RxSysMemFunc)rxSysNoMemFunc;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001003e0

RxSysMemFunc rxSysSetMemFunc3(RxSysMemFunc func)
{
    RxSysMemFunc old = g_rxSysMemFunc[3];
    g_rxSysMemFunc[3] = func;
    if (func == 0)
        g_rxSysMemFunc[3] = (RxSysMemFunc)rxSysNoMemFunc;
    return old;
}

/* ---- string and path helpers ----------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Length up to the end of the line.
// FS2 0x00100410

int rxSysStrLenToEol(const char *s)
{
    int n = 0;
    for (char c; (c = *s++) != '\0'; n++)
    {
        if (c == '\r' || c == '\n')
            break;
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// Length up to the next blank.
// FS2 0x00100450

int rxSysStrLenToSpace(const char *s)
{
    int n = 0;
    for (char c; (c = *s++) != '\0'; n++)
    {
        if (c == ' ' || c == '\t')
            break;
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// Skip line ends.
// FS2 0x00100490

char *rxSysSkipEol(char *s)
{
    while (*s != '\0' && (*s == '\r' || *s == '\n'))
        s++;
    return s;
}

// ──────────────────────────────────────────────────────────────────────
// Skip blanks.
// FS2 0x001004d0

char *rxSysSkipSpace(char *s)
{
    while (*s != '\0' && (*s == ' ' || *s == '\t'))
        s++;
    return s;
}

// ──────────────────────────────────────────────────────────────────────
// Copy at most n characters and terminate.
// FS2 0x00100510

void rxSysStrCopyN(char *dst, const char *src, int n)
{
    int i = 0;
    if (n > 0)
    {
        for (char c = *src++; c != '\0'; c = *src++)
        {
            i++;
            *dst++ = c;
            if (i >= n)
                break;
        }
    }
    *dst = '\0';
}

// ──────────────────────────────────────────────────────────────────────
// Copy at most n characters of a path, '\' to '/' and lower case: rx's
// paths are case-folded the way the disc's ISO names are matched.
// FS2 0x00100550

void rxSysPathCopy(char *dst, const char *src, int n)
{
    int i = 0;
    if (n > 0)
    {
        for (int c = *src; c != 0; c = *src)
        {
            src++;
            i++;
            if (c == '\\')
                c = '/';
            if (isupper((u_char)c))  // newlib's ctype table 0x3000c1, bit _U
                c += 0x20;
            *dst++ = (char)c;
            if (i >= n)
                break;
        }
    }
    *dst = '\0';
}

// ──────────────────────────────────────────────────────────────────────
// Is this module name in the loaded-module table?
// FS2 0x001005b8

int rxSysIsModuleLoaded(const char *name)
{
    if (name != 0)
    {
        for (int i = 0; i < s_moduleCount; i++)
        {
            if (strcmp(name, s_modules[i]) == 0)
                return 1;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The file name: what follows the last '/', '\', ':' or ','.
// FS2 0x00100650

char *rxSysPathFileName(char *path)
{
    int i = (int)strlen(path) - 1;
    for (; i >= 0; i--)
    {
        if (rxSysIsPathSep(path[i]))
            break;
    }
    return path + i + 1;
}

// ──────────────────────────────────────────────────────────────────────
// The file name's extension (after its last '.'), or its end.
// FS2 0x00100708

char *rxSysPathExt(char *path)
{
    char *name = rxSysPathFileName(path);
    int len = (int)strlen(name);
    for (int i = len - 1; i >= 0; i--)
    {
        if (name[i] == '.')
            return name + i + 1;
    }
    return name + len;
}

// ──────────────────────────────────────────────────────────────────────
// Empty lines and lines starting with '-' or '#' are skipped.
// FS2 0x00100790

int rxSysIsCommentLine(const char *line)
{
    char c = *line;
    return c == '\0' || c == '-' || c == '#';
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001007c0

int rxSysSkipModule(char *path)
{
    if (rxSysIsCommentLine(path))
        return 1;
    return rxSysIsModuleLoaded(rxSysPathFileName(path)) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Record a module as loaded (32 at most).
// FS2 0x00100810

void rxSysAddModule(const char *name)
{
    if (s_moduleCount < 32)
    {
        int i = s_moduleCount++;
        rxSysPathCopy(s_modules[i], name, 31);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00100860

void rxSysAddModuleList(const char *const *names)
{
    for (; *names != 0; names++)
        rxSysAddModule(*names);
}

/* ---- IOP ---------------------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Media mode for the disc in the drive: DVD for a PS2 DVD (or DVD video),
// CD otherwise.
// FS2 0x001008a8

void rxSysSetCdMode(void)
{
    int type = sceCdGetDiskType();
    if (type != SCECdPS2DVD && type != SCECdDVDV)
    {
        sceCdMmode(SCECdCD);
        return;
    }
    sceCdMmode(SCECdDVD);
}

// ──────────────────────────────────────────────────────────────────────
// Reboot the IOP with an image (booted from the disc only), then bring
// SIF RPC, the IOP heap, the CD and the file system back up.
// FS2 0x001008f0

void rxSysRebootIop(const char *img)
{
    if (s_bootDevice == RX_SYS_DEVICE_CDROM)
    {
        rxSysPrintf("Iop Rebooting : %s\n", img);
        rxSysSetCdMode();
        while (rxFileRebootIop(img) == 0)
            ;
        sceSifInitRpc(0);
        sceSifInitIopHeap();
        while (sceCdInit(SCECdINIT) == 0)
            ;
        rxSysSetCdMode();
        sceFsReset();
        s_iopRebooted = 1;
        return;
    }
    rxSysPrintf("Skip Iop Reboot: %s\n", img);
}

// ──────────────────────────────────────────────────────────────────────
// Load the modules an iopconf.cnf lists, one per line: a path then its
// arguments, '#' / '-' lines skipped, modules already loaded skipped.
// ".img" lines are the reboot image and are ignored here.
// FS2 0x001009a8

int rxSysLoadModuleList(char *conf)
{
    char name[256];
    char args[1024];
    char line[512];

    while (*conf != '\0')
    {
        char *p = rxSysSkipSpace(conf);
        int n = rxSysStrLenToEol(p);
        rxSysStrCopyN(line, p, n);
        conf = rxSysSkipEol(p + n);
        if (rxSysIsCommentLine(line))
            continue;

        name[0] = '\0';
        args[0] = '\0';
        int len = rxSysStrLenToSpace(line);
        int arglen = 0;
        rxSysPathCopy(name, line, len);
        char *a = rxSysSkipSpace(line + len);
        while (*a != '\0')
        {
            int k = rxSysStrLenToSpace(a);
            rxSysStrCopyN(args + arglen, a, k);
            arglen += k + 1;
            a = rxSysSkipSpace(a + k);
        }
        const char *argp = 0;
        if (arglen > 0)
            argp = args;

        if (rxSysSkipModule(name))
            continue;
        char *ext = rxSysPathExt(name);
        if (strcmp("irx", ext) == 0)
        {
            rxSysAddModule(rxSysPathFileName(name));
            while (rxFileLoadModule(name, arglen, argp) == 0)
                ;
        }
        else if (strcmp("img", ext) != 0)
        {
            rxSysPrintf("Illigal Module Name:%s\n", name);
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Reboot the IOP with <dir><image> once, then read iopconf.cnf into buf
// (through the file layer's search directory).
// FS2 0x00100b80

int rxSysReadIopConf(char *buf, const char *dir)
{
    char path[256];

    if (s_iopRebooted == 0)
    {
        strcpy(path, dir);
        strcpy(path + strlen(path), s_iopImage);
        rxSysRebootIop(path);
    }
    int size = rxFileLoad("iopconf.cnf", buf);
    if (size > 0)
        buf[size] = '\0';
    return size > 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00100c08

void rxSysCdInit(int mode)
{
    rxSysPrintf("initializing CD (%d) ...\n", mode != 0);
    while (sceCdInit(mode != 0) == 0)
        ;
    rxSysPrintf("complete\n");
    s_deviceReady[RX_SYS_DEVICE_CDROM] = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The IOP bring-up: the default module table, SIF, the CD, then the IOP
// reboot and iopconf.cnf from modules/ (a devkit falls back to the SDK's
// /usr/local/sce/iop/modules/ and gives up with exit(1)).
// FS2 0x00100c60

void rxSysLoadIopModules(void)
{
    s_iopRebooted = 0;
    s_moduleCount = 0;
    rxSysAddModuleList(s_moduleListBase);
    if (s_bootDev[0] != '\0')
    {
        rxSysAddModuleList(s_moduleListPad);
        if (s_bootDev[0] == 'a')
            rxSysAddModuleList(s_moduleListUsb);
    }
    sceSifInitRpc(0);
    sceSifInitIopHeap();
    if (s_bootDevice == RX_SYS_DEVICE_CDROM)
        rxSysCdInit(0);
    rxSysPrintf("IOP Modules Load Start\n");
    rxSysSetFileSearchDir(0);
    if (s_bootDevice == RX_SYS_DEVICE_CDROM)
    {
        while (rxSysReadIopConf(s_confBuf, "modules/") == 0)
            ;
        while (rxSysLoadModuleList(s_confBuf) == 0)
            ;
    }
    else if (rxSysReadIopConf(s_confBuf, "modules/") == 0 || rxSysLoadModuleList(s_confBuf) == 0)
    {
        rxSysSetFileSearchDir("/usr/local/sce/iop/modules/");
        if (rxSysReadIopConf(s_confBuf, "") == 0 || rxSysLoadModuleList(s_confBuf) == 0)
        {
            rxSysSetFileSearchDir(0);
            rxSysPrintf("IOP Modules Load Failed\n");
            exit(1);
        }
    }
    rxSysSetFileSearchDir(0);
    if (s_bootDevice != RX_SYS_DEVICE_CDROM && rxSysIsModuleLoaded("cdvdman.irx"))
        rxSysCdInit(1);
    if (rxSysIsModuleLoaded("mcman.irx"))
        s_deviceReady[RX_SYS_DEVICE_MCARD] = 1;
    rxSysPrintf("IOP Modules Load End\n");
}

/* ---- devices ------------------------------------------------------------ */

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00100e58

int rxSysIsDeviceReady(int device)
{
    int i = s_bootDevice;
    if (device >= 0)
        i = device;
    return s_deviceReady[i];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00100e80

int rxSysGetBootDevice(void)
{
    return s_bootDevice;
}

// ──────────────────────────────────────────────────────────────────────
// No boot path given: a devkit's memory (more than 32 MB) means HOST, a
// retail console's means CDROM. The boot device is marked ready.
// FS2 0x00100e90

void rxSysDetectDevice(void)
{
    if (s_bootDevice < 0)
    {
        int size = GetMemorySize();
        if (size > 0x2000000)
            s_deviceReady[RX_SYS_DEVICE_HOST] = 1;
        else
            s_deviceReady[RX_SYS_DEVICE_CDROM] = 1;
        s_bootDevice = size <= 0x2000000;
        return;
    }
    s_deviceReady[s_bootDevice] = 1;
}

/* ---- system.ini ----------------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Index of s in a null-terminated table, or -1.
// FS2 0x00100f10

int rxSysFindString(const char *s, const char *const *table)
{
    for (int i = 0; table[i] != 0; i++)
    {
        if (strcmp(s, table[i]) == 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Read system.ini's [SYSTEM] section, when the file exists.
// FS2 0x00100f80

void rxSysReadSystemIni(void)
{
    char value[32];

    s_iniLoaded = 0;
    int size = rxFileLoad("system.ini", s_confBuf);
    if (size <= 0)
        return;
    s_iniLoaded = 1;
    s_confBuf[size] = '\0';

    if (rxSysIniGetString(s_productNumber, "SYSTEM", "NUMBER"))
        rxSysPrintf(" Number  :%s\n", s_productNumber);
    if (rxSysIniGetString(value, "SYSTEM", "VERSION"))
    {
        int version = atoi(value);
        s_version = version;
        rxSysPrintf(" Version :%d\n", version);
    }
    if (rxSysIniGetString(value, "SYSTEM", "VMODE"))
    {
        int i = rxSysFindString(value, s_vmodeNames);
        if (i >= 0)
        {
            s_vmode = i;
            rxSysPrintf(" VMode   :%s\n", s_vmodeNames[i]);
        }
    }
    if (rxSysIniGetString(value, "SYSTEM", "COUNTRY"))
    {
        int i = rxSysFindString(value, s_countryNames);
        if (i >= 0)
        {
            s_country = i;
            rxSysPrintf(" Country :%s\n", s_countryNames[i]);
        }
    }
    if (rxSysIniGetString(value, "SYSTEM", "LANGUAGE"))
    {
        int i = rxSysFindString(value, s_languageNames);
        if (i >= 0)
        {
            s_language = i;
            rxSysPrintf(" Language:%s\n", s_languageNames[i]);
        }
    }
    if (rxSysIniGetString(value, "SYSTEM", "WARNING"))
    {
        int i = rxSysFindString(value, s_warningNames);
        if (i >= 0)
        {
            s_warning = i;
            rxSysPrintf(" Warning :%s\n", s_warningNames[i]);
        }
    }
}

/* ---- init ----------------------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Once: Sony's standard reset of VIF0 / VU0 / VIF1 / VU1, the GS path and
// DMA, then the rx thread layer and the print semaphores.
// FS2 0x001011c0

void rxSysResetHardware(void)
{
    if (s_hardwareReset != 0)
        return;
    s_hardwareReset = 1;
    sceDevVif0Reset();
    sceDevVu0Reset();
    sceDevVif1Reset();
    sceDevVu1Reset();
    sceGsResetPath();
    sceDmaReset(1);
    rxThInitOnce();
    rxSysInitSema();
}

// ──────────────────────────────────────────────────────────────────────
// The BIOS date word (0x1fc00100) when the ROM carries Sony's signature.
// FS2 0x00101230

void rxSysReadRomDate(void)
{
    // PC: there is no BIOS image on the host; an all-zero ROM stands in for
    // 0x1fc00100, so the signature check fails and the date stays 0.
    static const char s_hostRom[0x30] = { 0 };
    const char *signature = "Sony Computer Entertainment Inc.";

    int len = (int)strlen(signature);
    if (strncmp(s_hostRom + 8, signature, len) == 0)
    {
        int date;
        memcpy(&date, s_hostRom, sizeof(date));
        s_romDate = date;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The console bring-up rxInit runs before the game: hardware, memory hooks,
// the boot device, the rx subsystems, the IOP modules, system.ini, and the
// save-data name (region prefix + product number, "BESCES-..." style;
// without a system.ini, FS2's disc has none, it stays "BISCPS-00000").
// FS2 0x00101288

void rxSysInit(void)
{
    s_libVersion[0] = '0';
    rxSysResetHardware();
    s_romDate = 0;
    rxSysReadRomDate();
    rxSysSetMemFunc0(0);
    rxSysSetMemFunc1(0);
    rxSysSetMemFunc2(0);
    rxSysSetMemFunc3(0);
    for (int i = RX_SYS_DEVICE_NUM - 1; i >= 0; i--)
        s_deviceReady[i] = 0;
    rxSysDetectDevice();
    rxSysPrintf("BootDevice = %d\n", s_bootDevice);
    rxSysPrintf("Rom Date = %04X/%02X/%02X\n", s_romDate >> 16, (s_romDate >> 8) & 0xff,
                s_romDate & 0xff);
    rxSysInitUser();
    rxSysInitModules();
    if (s_bootDev[0] != '\0')
        rxFileCallDevice(s_bootDevice, s_bootDev);
    rxSysSetFileDevice(0, s_bootDevice);
    rxSysSetFileDevice(1, s_bootDevice);
    rxSysSetFileDir(s_bootDir);
    rxSysLoadIopModules();
    for (int i = 0; i < RX_SYS_DEVICE_NUM; i++)
        rxSysPrintf("Device[%s] %s\n", s_deviceNames[i], s_deviceReady[i] != 0 ? "OK" : "--");
    rxSysReadSystemIni();
    strcpy(s_saveName, s_regionPrefix[rxSysGetCountry()]);
    const char *number = rxSysGetProductNumber();
    strcpy(s_saveName + strlen(s_saveName), number);
    rxSysInitEnd();
}

/* ---- ini parser ------------------------------------------------------------ */

static inline int rxSysIsEol(char c)
{
    return c == '\r' || c == '\n';
}

// ──────────────────────────────────────────────────────────────────────
// The line after "[section]"; NULL when there is none. A NULL section
// returns the text itself. Only a '[' in a line's first column counts.
// FS2 0x00101488

char *rxSysIniFindSection(char *text, const char *section)
{
    if (section == NULL)
        return text;
    int len = strlen(section);
    while (*text != '\0')
    {
        char *p = text;
        if (*text == '[')
        {
            // The ROM's compare leaves p one past the first mismatch.
            p = text + 1;
            int i = 0;
            if (len > 0)
            {
                char want = section[0];
                char have = *p++;
                while (have == want)
                {
                    i++;
                    if (i >= len)
                        break;
                    want = section[i];
                    have = *p++;
                }
            }
            if (i == len && *p == ']')
                return rxSysSkipEol(p + rxSysStrLenToEol(p));
        }
        text = rxSysSkipEol(p + rxSysStrLenToEol(p));
    }
    return NULL;
}

// ──────────────────────────────────────────────────────────────────────
// Copy a line's key -- up to '=' or the end of the line -- into out, with
// the blanks before '=' trimmed; returns where it stopped ('=', EOL or 0).
// FS2 0x001015a0

char *rxSysIniReadKey(char *out, char *line)
{
    int n = 0;
    out[0] = '\0';
    for (;;)
    {
        char c = *line;
        if (c == '\0' || rxSysIsEol(c))
            return line;
        if (c == '=')
        {
            while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t'))
                n--;
            out[n] = '\0';
            return line;
        }
        out[n] = c;
        out[n + 1] = '\0';
        line++;
        n++;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The value of key in a section's body (after '=' and blanks); NULL at
// the next section or the end of the text.
// FS2 0x00101668

char *rxSysIniFindKey(char *body, const char *key)
{
    char name[0x100];
    int len = strlen(key);
    for (;;)
    {
        char *p = rxSysSkipSpace(body);
        char c = *p;
        if (c == '\0' || c == '[')
            return NULL;
        if (c != '#' && !rxSysIsEol(c))
        {
            p = rxSysIniReadKey(name, p);
            if ((int)strlen(name) == len)
            {
                int i = 0;
                while (i < len && name[i] == key[i])
                    i++;
                if (i == len)
                {
                    p = rxSysSkipSpace(p);
                    return rxSysSkipSpace(p + 1);
                }
            }
        }
        body = rxSysSkipEol(p + rxSysStrLenToEol(p));
    }
}

// ──────────────────────────────────────────────────────────────────────
// A key's value: the rest of its line, a '\' before the end of the line
// joining the next one (comment lines between are skipped; the next line
// must start with a blank, which is kept). Returns the length.
// FS2 0x001017a8

int rxSysIniFind(char *out, const char *text, const char *section, const char *key)
{
    *out = '\0';
    char *body = rxSysIniFindSection((char *)text, section);
    if (body == NULL)
        return 0;
    char *p = rxSysIniFindKey(body, key);
    if (p == NULL)
        return 0;
    int n = 0;
    for (char c = *p; c != '\0' && !rxSysIsEol(c); c = *++p)
    {
        if (c == '\\' && rxSysIsEol(p[1]))
        {
            for (;;)
            {
                p = rxSysSkipEol(p + 1);
                if (*p != '#')
                    break;
                while (*p != '\0' && !rxSysIsEol(*p))
                    p++;
                if (*p == '\0')
                {
                    *out = '\0';
                    return n;
                }
            }
            if (*p != ' ' && *p != '\t')
            {
                *out = '\0';
                return n;
            }
            *out = *p;
        }
        else
        {
            *out = c;
        }
        out++;
        n++;
    }
    *out = '\0';
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// The key lines of a section (comments and blank lines not counted).
// FS2 0x00101928

int rxSysIniCountKeys(char *body)
{
    int n = 0;
    for (;;)
    {
        char *p = rxSysSkipSpace(body);
        char c = *p;
        if (c == '\0' || c == '[')
            return n;
        if (c != '#' && !rxSysIsEol(c))
            n++;
        body = rxSysSkipEol(p + rxSysStrLenToEol(p));
    }
}

// ──────────────────────────────────────────────────────────────────────
// The index-th key of a section into out; returns its length, 0 past the
// last.
// FS2 0x001019f0

int rxSysIniKeyAt(char *out, char *body, int index)
{
    int n = 0;
    *out = '\0';
    for (;;)
    {
        char *p = rxSysSkipSpace(body);
        char c = *p;
        if (c == '\0' || c == '[')
            return 0;
        if (c != '#' && !rxSysIsEol(c))
        {
            if (n == index)
            {
                rxSysIniReadKey(out, p);
                return strlen(out);
            }
            n++;
        }
        body = rxSysSkipEol(p + rxSysStrLenToEol(p));
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101af0

int rxSysIniFindKeyCount(char *text, const char *section)
{
    char *body = rxSysIniFindSection(text, section);
    if (body == NULL)
        return 0;
    return rxSysIniCountKeys(body);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101b28

int rxSysIniFindKeyAt(char *out, char *text, const char *section, int index)
{
    char *body = rxSysIniFindSection(text, section);
    if (body == NULL)
        return 0;
    return rxSysIniKeyAt(out, body, index);
}

// ──────────────────────────────────────────────────────────────────────
// A system.ini value; 0 when no system.ini was read.
// FS2 0x00101b90

int rxSysIniGetString(char *out, const char *section, const char *key)
{
    if (s_iniLoaded != 0)
        return rxSysIniFind(out, s_confBuf, section, key);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The number of keys in a system.ini section; 0 when no system.ini was read.
// FS2 0x00101bd8

int rxSysIniGetKeyCount(const char *section)
{
    if (s_iniLoaded != 0)
        return rxSysIniFindKeyCount(s_confBuf, section);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101c18

int rxSysIniGetKeyAt(char *out, const char *section, int index)
{
    if (s_iniLoaded != 0)
        return rxSysIniFindKeyAt(out, s_confBuf, section, index);
    return 0;
}

/* ---- tty ------------------------------------------------------------------ */

// ──────────────────────────────────────────────────────────────────────
// Bind the tty client, up to ten tries, each waiting a moment for the IOP
// to serve it; 1 when bound.
// FS2 0x00101c60

int rxSysBindTtyRpc(sceSifClientData *client, unsigned int id)
{
    rxSysPrintf("rxsys: binding IOP module %08X ...\n", id);
    for (int i = 0; i < 10; i++)
    {
        client->serve = 0;
        if (sceSifBindRpc(client, id, 0) < 0)
            break;
        // (a short busy-wait loop in the ROM)
        if (client->serve != 0)
            break;
    }
    FlushCache(0);
    int bound = client->serve != 0;
    if (bound)
        rxSysPrintf("rxsys : binding completed\n");
    else
        rxSysPrintf("rxsys : bind error\n");
    return bound;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101d38

void rxSysBindTty(void)
{
    sceSifInitRpc(0);
    if (rxSysBindTtyRpc(&s_ttyClient, 0x80000220))
        s_ttyRpcBound = 1;
}

// ──────────────────────────────────────────────────────────────────────
// With a boot device, send rxSysPrintf's output to the IOP tty service;
// when it does not bind, to the device's tty file ("host0:>"). The PC
// boot path leaves s_bootDev empty, so nothing opens.
// FS2 0x00101dd0

void rxSysOpenTty(void)
{
    char path[0x80];
    if (s_bootDev[0] == '\0')
        return;
    rxSysBindTty();
    if (s_ttyRpcBound != 0)
        return;
    strcpy(path, s_bootDev);
    strcpy(path + strlen(path), ">");
    s_ttyFd = rxSysOpen(path, SCE_WRONLY);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101e50

void rxSysCloseTty(void)
{
    if (s_ttyFd >= 0)
    {
        rxSysClose(s_ttyFd);
        s_ttyFd = -1;
    }
    s_ttyRpcBound = 0;
}

// ──────────────────────────────────────────────────────────────────────
// rxSysInit's teardown, in reverse.
// FS2 0x00101e98

void rxSysTerm(void)
{
    rxSysCloseTty();
    rxSysTermEnd();
    rxSysTermModules();
    rxSysTermUser();
    rxSysTermSema();
    rxThExitOnce();
    rxSysRestoreDebugHandlers();
}

// ──────────────────────────────────────────────────────────────────────
// Send buffered output to the IOP tty service and wait for it.
// FS2 0x00101d78

void rxSysTtyWrite(const char *buf, int size)
{
    sceSifCallRpc(&s_ttyClient, 0x10, SIF_RPC_M_NOWAIT, (void *)buf, size, 0, 0, 0, 0);
    while (sceSifCheckStatRpc(&s_ttyClient.rpcd) != 0)
        ;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101ef0

void rxSysSetIopImage(const char *img)
{
    strcpy(s_iopImage, img);
}

// ──────────────────────────────────────────────────────────────────────
// system.ini VMODE (0 NTSC, 1 PAL).
// FS2 0x00101f20

int rxSysGetVMode(void)
{
    return s_vmode;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101f30

int rxSysGetFrameRate(void)
{
    return rxSysGetVMode() != 0 ? 50 : 60;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101f60

const char *rxSysGetProductNumber(void)
{
    return s_productNumber;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101f80

int rxSysGetCountry(void)
{
    return s_country;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00101fd0

int rxSysGetMemorySize(void)
{
    return GetMemorySize();
}

// ──────────────────────────────────────────────────────────────────────
// A disc path: "cdrom0:..." or an ISO name ending ";1".
// FS2 0x00101fe8

int rxSysIsCdromPath(const char *path)
{
    int len = (int)strlen(path);
    if (strncmp(path, "cdrom0:", 7) == 0)
        return 1;
    return len > 2 && path[len - 1] == '1' && path[len - 2] == ';';
}

// ──────────────────────────────────────────────────────────────────────
// Skip a path's device: a drive letter ("x:"), a host's "...," prefix, or
// everything up to the first ':'.
// FS2 0x00102070

char *rxSysPathSkipDevice(char *path)
{
    if (path[1] == ':')
        return rxSysPathFileName(path);
    if (s_bootDevice == RX_SYS_DEVICE_HOST)
    {
        for (char *p = path; *p != '\0'; p++)
        {
            if (*p == ',')
                return p + 1;
        }
    }
    for (char *p = path; *p != '\0'; p++)
    {
        if (*p == ':')
            return p + 1;
    }
    return path;
}

// ──────────────────────────────────────────────────────────────────────
// Split the boot path (argv[0] style) into device, directory and name; a
// boot from a host device also opens the host tty.
// FS2 0x00102138

void rxSysSetBootPath(char *path)
{
    if (path == 0)
        return;
    rxSysResetHardware();
    s_bootDevice = rxSysIsCdromPath(path) != 0;
    char *rest = rxSysPathSkipDevice(path);
    int n = (int)(rest - path);
    if (path[1] == ':')
        n = 0;
    if (s_bootDevice != RX_SYS_DEVICE_CDROM)
    {
        rxSysPathCopy(s_bootDev, path, n);
        if (strcmp(s_bootDev, "host0:") == 0)
            s_bootDev[0] = '\0';
        rxSysOpenTty();
    }
    rxSysPrintf("BootDev = %s\n", s_bootDev);
    char *file = rxSysPathFileName(rest);
    rxSysPathCopy(s_bootDir, rest, (int)(file - rest));
    rxSysPrintf("BootDir = %s\n", s_bootDir);
    file = rxSysPathFileName(rest);
    int len = (int)strlen(file);
    if (len > 2 && file[len - 1] == '1' && file[len - 2] == ';')
        len -= 2;
    rxSysPathCopy(s_bootName, file, len);
    rxSysPrintf("BootName = %s\n", s_bootName);
}

/* ---- printing ------------------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001022d8

RxSysPrintHook rxSysSetPrintHook(RxSysPrintHook hook)
{
    RxSysPrintHook old = s_printHook;
    s_printHook = hook;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// The request rxSysLockPrint hands the rx thread while it waits.
// FS2 0x001022f0

int rxSysPrintWait(intptr_t *args)
{
    (void)args;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001022f8

int rxSysIsTtyOpen(void)
{
    return s_ttyRpcBound != 0 || s_ttyFd >= 0;
}

// ──────────────────────────────────────────────────────────────────────
// The middleware's printf: format into 1 KB, ask the print hook, then
// scePrintf, or buffer for the host tty (a file, or the IOP tty RPC).
// FS2 0x00102328

int rxSysPrintf(const char *fmt, ...)
{
    char buf[1024];
    int busy = 0;

    rxSysResetHardware();
    if (s_ttyRpcBound == 0 && s_ttyFd >= 0)
    {
        busy = rxThIsDispatchDisabled();
        if (s_ttyFd >= 0 && busy == 0)
            rxSysLockPrint();
    }
    rxThDisableDispatch();
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    int print = 1;
    if (s_printHook != 0)
        print = s_printHook(buf);
    int len = 0;
    if (print != 0)
    {
        len = (int)strlen(buf);
        if (rxSysIsTtyOpen() == 0)
        {
            // PC: the ROM passes buf as scePrintf's format; "%s" keeps a '%'
            // in a formatted message from being read as a conversion.
            scePrintf("%s", buf);
        }
        else
        {
            if (s_ttyBufLen + len < 0x1000)
            {
                strcpy(s_ttyBuf + s_ttyBufLen, buf);
                s_ttyBufLen += len;
            }
            if (s_ttyBufLen > 0)
            {
                FlushCache(0);
                if (s_ttyRpcBound == 0)
                {
                    if (busy == 0)
                    {
                        sceWrite(s_ttyFd, s_ttyBuf, s_ttyBufLen + 1);
                        s_ttyBufLen = 0;
                    }
                }
                else
                {
                    rxSysTtyWrite(s_ttyBuf, s_ttyBufLen + 1);
                    s_ttyBufLen = 0;
                }
            }
        }
    }
    rxThEnableDispatch();
    if (s_ttyRpcBound == 0 && s_ttyFd >= 0 && busy == 0)
        rxSysUnlockPrint();
    return len;
}

// ──────────────────────────────────────────────────────────────────────
// Empty in the retail build.
// FS2 0x00102520

void rxSysInitEnd(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Empty in the retail build; rxSysTerm's step after rxSysCloseTty.
// FS2 0x00102528

void rxSysTermEnd(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// The EE SemaParam takes max_count at +4 and init_count at +8, in that
// order of the arguments.
// FS2 0x00102530

int rxSysCreateSema(int max, int init)
{
    struct SemaParam param;
    memset(&param, 0, sizeof(param));
    param.maxCount = max;
    param.initCount = init;
    return CreateSema(&param);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102760

void rxSysInitSema(void)
{
    s_printSema = rxSysCreateSema(1, 1);
    s_sema1 = rxSysCreateSema(1, 1);
}

/* ---- IOP RPC clients ------------------------------------------------------ */

// ──────────────────────────────────────────────────────────────────────
// Create the client's call semaphore and bind it to an IOP service,
// retrying until it is served; 0 on a bind error. No caller in the retail
// ELF.
// FS2 0x00102558

int rxSysBindRpc(RxSysRpc *rpc, unsigned int id)
{
    rpc->sema = rxSysCreateSema(1, 1);
    rxSysPrintf("rxsys : binding IOP module %08X ...\n", id);
    do
    {
        if (sceSifBindRpc(&rpc->client, id, 0) < 0)
        {
            rxSysPrintf("rxsys : bind error '%08X'\n", id);
            return 0;
        }
        // (a short busy-wait loop in the ROM)
    } while (rpc->client.serve == 0);
    FlushCache(0);
    rxSysPrintf("rxsys : binding completed\n");
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102638

void rxSysUnbindRpc(RxSysRpc *rpc)
{
    if (rpc->sema >= 0)
    {
        DeleteSema(rpc->sema);
        rpc->sema = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// One call at a time per client: take its semaphore (rxSysRpcDone gives
// it back when the IOP answers), post the call, wait for the answer.
// FS2 0x00102678

void rxSysCallRpc(RxSysRpc *rpc, int fno, void *send, int sendSize, void *recv, int recvSize)
{
    rxThDisableDispatch();
    WaitSema(rpc->sema);
    FlushCache(0);
    sceSifCallRpc(&rpc->client, fno, SIF_RPC_M_NOWAIT, send, sendSize, recv, recvSize,
                  (sceSifEndFunc)rxSysRpcDone, rpc);
    rxThEnableDispatch();
    while (sceSifCheckStatRpc(&rpc->client.rpcd) == 1)
        ;
    FlushCache(0);
}

// ──────────────────────────────────────────────────────────────────────
// The call's end function (interrupt context).
// FS2 0x00102738

void rxSysRpcDone(void *rpc)
{
    iSignalSema(((RxSysRpc *)rpc)->sema);
}

// ──────────────────────────────────────────────────────────────────────
// rxSysInitSema's twin.
// FS2 0x001027a0

void rxSysTermSema(void)
{
    DeleteSema(s_sema1);
    DeleteSema(s_printSema);
}

// ──────────────────────────────────────────────────────────────────────
// Take the print semaphore, letting the rx thread run while it is held.
// FS2 0x001027d0

void rxSysLockPrint(void)
{
    while (PollSema(s_printSema) == -1)
    {
        rxThRequest(rxSysPrintWait, 0, 0, 0);
        rxThYield();
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102830

void rxSysUnlockPrint(void)
{
    SignalSema(s_printSema);
}

// ──────────────────────────────────────────────────────────────────────
// rxSysLockPrint's twin for the second semaphore.
// FS2 0x00102850

void rxSysLockSema1(void)
{
    while (PollSema(s_sema1) == -1)
    {
        rxThRequest(rxSysPrintWait, 0, 0, 0);
        rxThYield();
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001028b0

void rxSysUnlockSema1(void)
{
    SignalSema(s_sema1);
}

/* ---- file wrappers ------------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// A cdrom0: path switches the drive to CD mode first.
// FS2 0x001028d0

void rxSysCheckCdPath(const char *path)
{
    if (rxSysIsCdromPath(path) != 0)
        rxSysSetCdMode();
}

// ──────────────────────────────────────────────────────────────────────
// The file calls run under the print lock (the host tty shares fileio).
// FS2 0x001028f8

int rxSysOpen(const char *path, int mode)
{
    rxSysLockPrint();
    rxSysCheckCdPath(path);
    int fd = sceOpen(path, mode);
    rxSysUnlockPrint();
    return fd;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102948

int rxSysClose(int fd)
{
    rxSysLockPrint();
    int ret = sceClose(fd);
    rxSysUnlockPrint();
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102980

int rxSysRead(int fd, void *buf, int size)
{
    rxSysLockPrint();
    int ret = sceRead(fd, buf, size);
    rxSysUnlockPrint();
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001029d8

int rxSysWrite(int fd, const void *buf, int size)
{
    rxSysLockPrint();
    int ret = sceWrite(fd, buf, size);
    rxSysUnlockPrint();
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102a30

int rxSysLseek(int fd, int offset, int whence)
{
    rxSysLockPrint();
    int ret = sceLseek(fd, offset, whence);
    rxSysUnlockPrint();
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// sceIoctl under the print lock.
// FS2 0x00102a88

int rxSysIoctl(int fd, int req, void *arg)
{
    rxSysLockPrint();
    int ret = sceIoctl(fd, req, arg);
    rxSysUnlockPrint();
    return ret;
}

/* ---- exception handler ------------------------------------------------------ */

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102ae0

void rxSysLockConsole(void)
{
    rxSysLockPrint();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102af8

void rxSysUnlockConsole(void)
{
    rxSysUnlockPrint();
}

// ──────────────────────────────────────────────────────────────────────
// The debug exception handler rxSysSetDebugHandlers installs: once, stop
// the vblank interrupt, reset the VIFs, VUs, GS path and DMA, open the
// exception screen, print epc / status / cause, the bad addresses, the
// registers by row and 14 lines of the stack, then hang.
// The host kernel never
// raises a debug exception, so it does not run on the PC.
// FS2 0x00102b10

void rxSysExceptionHandler(u_int stat, u_int cause, u_int epc, u_int bva, u_int bpa,
                           u_long128 *gpr)
{
    if (s_inException != 0)
        return;
    s_inException = 1;
    s_exceptionOpen = 1;
    iDisableIntc(2);        // INTC_VBLANK_S
    sceDevVif0Reset();
    sceDevVu0Reset();
    sceDevVif1Reset();
    sceDevVu1Reset();
    sceGsResetPath();
    sceDmaReset(1);
    rxSysInitExceptionScreen();
    rxSysPrintf("epc     = %08x  status  = %08x  cause   = %08x\n", epc, stat, cause);
    rxSysPrintf("badvadr = %08x  badpadr = %08x\n", bva, bpa);
    for (const RxSysRegRow *row = s_regRows; row->name != NULL; row++)
    {
        rxSysPrintf("%s", row->name);
        for (int i = 0; i < row->count; i++)
            rxSysPrintf(" %08x", *(u_int *)&gpr[row->first + i]);
    }
    rxSysPrintf("\n\n");
    // PC: the stack pointer is an EE address; read it through emulated RAM.
    u_int sp = *(u_int *)&gpr[29];
    for (int line = 13; line >= 0; line--, sp += 0x10)
    {
        if (sp > 0xfffff && (int)sp <= rxSysGetMemorySize() - 0x11)
        {
            u_int *p = (u_int *)YamiPan_GetHostPointer(sp);
            rxSysPrintf("%08x: %08x %08x %08x %08x\n", sp, p[0], p[1], p[2], p[3]);
        }
    }
    for (;;)
    {
    }
}

// ──────────────────────────────────────────────────────────────────────
// Once (no caller in FS2; RoR's main calls it at boot): rxSysExceptionHandler
// for causes 1-7 and 10-13, keeping the old handlers for
// rxSysRestoreDebugHandlers.
// FS2 0x00102d30

void rxSysSetDebugHandlers(void)
{
    if (s_debugHandlersSet != 0)
        return;
    s_debugHandlersSet = 1;
    for (int cause = 1; cause < 8; cause++)
        s_oldHandlersLow[cause - 1] = (YamiPanDebugHandler)(intptr_t)SetDebugHandler(cause, rxSysExceptionHandler);
    for (int cause = 10; cause < 14; cause++)
        s_oldHandlersHigh[cause - 10] = (YamiPanDebugHandler)(intptr_t)SetDebugHandler(cause, rxSysExceptionHandler);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102de0

void rxSysRestoreDebugHandlers(void)
{
    if (s_debugHandlersSet == 0)
        return;
    s_debugHandlersSet = 0;
    for (int cause = 1; cause < 8; cause++)
        SetDebugHandler(cause, s_oldHandlersLow[cause - 1]);
    for (int cause = 10; cause < 14; cause++)
        SetDebugHandler(cause, s_oldHandlersHigh[cause - 10]);
}

// ──────────────────────────────────────────────────────────────────────
// Print memory 16 bytes a line: the address, the bytes in hex, then the
// printable ones (letters, digits, punctuation, space; '.' for others).
// No caller in the retail ELF.
// FS2 0x00102e78

void rxSysHexDump(const char *p, int size)
{
    while (size > 0)
    {
        int n = size < 0x10 ? size : 0x10;
        rxSysPrintf("%08X: ", (u_int)(uintptr_t)p);  // PC: the host address's low bits
        size -= n;
        int i;
        for (i = 0; i < n; i++)
            rxSysPrintf("%02X ", p[i]);
        for (; i < 0x10; i++)
            rxSysPrintf("   ");
        for (i = 0; i < n; i++)
        {
            unsigned char c = (unsigned char)p[i];
            if (!(isalnum(c) || ispunct(c) || c == ' '))
                c = '.';
            rxSysPrintf("%c", c);
        }
        for (; i < 0x10; i++)
            rxSysPrintf(" ");
        rxSysPrintf("\n");
        p += n;
    }
}

/* ---- System Monitor (the exception screen) -------------------------------- */

/* An 80 x 28 cell text console the exception handlers print on: each cell
 * is char << 24 | colour (0xRRGGBB, the GS's ABGR byte order). The cursor
 * runs over rows 1..26 and columns 3..77; row 0, row 27 and the side
 * columns are margins. A row is drawn as 8 lines of 640 PSMCT32 pixels
 * (16 in field mode) and uploaded with LoadImage at frame buffer 0.
 * PC: the upload and the display env never reach a GS (drawing does not go
 * through the emulated GS; open-items "Decisions"): the screen is log-only.
 * The text still reaches the host log, since the print hook returns 1 and
 * rxSysPrintf goes on to scePrintf. The cells and the expanded image are
 * kept as the ROM builds them, for a host debug-text draw to pick up. */
enum
{
    RX_CONSOLE_COLS     = 80,
    RX_CONSOLE_ROWS     = 28,
    RX_CONSOLE_LINE     = 75,   /* putc wraps at column 75 (0x4b) */
    RX_CONSOLE_LAST     = 25,   /* the last cursor row (0x19)     */
    RX_CONSOLE_WIDTH    = 640,
};

static RxSysPrintHook s_consoleOldHook;         /* data 0x2f0d50 */
static int s_consoleNewlineWait;                /* data 0x2f0d54, VSyncs after each newline */
static int s_consoleClosePause = 120;           /* data 0x2f0d58, VSyncs before closing     */
static int s_consoleOpen;                       /* data 0x2f0d5c */

static const u_char s_consoleFont[96][8] = {                         /* data 0x2f0d60 */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x002f0d60
    {0x0c, 0x0c, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00},  // 0x002f0d68
    {0x36, 0x36, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x002f0d70
    {0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00},  // 0x002f0d78
    {0x08, 0x7e, 0x0b, 0x3e, 0x68, 0x3f, 0x08, 0x00},  // 0x002f0d80
    {0x00, 0x62, 0x35, 0x1a, 0x2c, 0x56, 0x23, 0x00},  // 0x002f0d88
    {0x1c, 0x36, 0x1c, 0x6e, 0x7b, 0x33, 0x6e, 0x00},  // 0x002f0d90
    {0x0c, 0x0c, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x002f0d98
    {0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00},  // 0x002f0da0
    {0x06, 0x0c, 0x18, 0x18, 0x18, 0x0c, 0x06, 0x00},  // 0x002f0da8
    {0x18, 0x5a, 0x3c, 0x7e, 0x3c, 0x5a, 0x18, 0x00},  // 0x002f0db0
    {0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00},  // 0x002f0db8
    {0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x002f0dc0
    {0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00},  // 0x002f0dc8
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},  // 0x002f0dd0
    {0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x00},  // 0x002f0dd8
    {0x3e, 0x63, 0x73, 0x6b, 0x67, 0x63, 0x3e, 0x00},  // 0x002f0de0
    {0x18, 0x1c, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00},  // 0x002f0de8
    {0x3e, 0x63, 0x63, 0x38, 0x0e, 0x03, 0x7f, 0x00},  // 0x002f0df0
    {0x3e, 0x63, 0x60, 0x3c, 0x60, 0x63, 0x3e, 0x00},  // 0x002f0df8
    {0x30, 0x38, 0x3c, 0x36, 0x33, 0x7f, 0x30, 0x00},  // 0x002f0e00
    {0x7f, 0x03, 0x3f, 0x63, 0x60, 0x63, 0x3e, 0x00},  // 0x002f0e08
    {0x3e, 0x63, 0x03, 0x3f, 0x63, 0x63, 0x3e, 0x00},  // 0x002f0e10
    {0x7f, 0x63, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x00},  // 0x002f0e18
    {0x3e, 0x63, 0x63, 0x3e, 0x63, 0x63, 0x3e, 0x00},  // 0x002f0e20
    {0x3e, 0x63, 0x63, 0x7e, 0x60, 0x63, 0x3e, 0x00},  // 0x002f0e28
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},  // 0x002f0e30
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x10, 0x08, 0x00},  // 0x002f0e38
    {0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00},  // 0x002f0e40
    {0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e, 0x00, 0x00},  // 0x002f0e48
    {0x06, 0x0c, 0x18, 0x30, 0x18, 0x0c, 0x06, 0x00},  // 0x002f0e50
    {0x3c, 0x66, 0x60, 0x38, 0x18, 0x00, 0x18, 0x00},  // 0x002f0e58
    {0x3e, 0x41, 0x5d, 0x53, 0x6d, 0x01, 0x3e, 0x00},  // 0x002f0e60
    {0x1c, 0x3e, 0x36, 0x63, 0x7f, 0x63, 0x63, 0x00},  // 0x002f0e68
    {0x1f, 0x33, 0x33, 0x1f, 0x63, 0x63, 0x3f, 0x00},  // 0x002f0e70
    {0x3e, 0x63, 0x03, 0x03, 0x03, 0x63, 0x3e, 0x00},  // 0x002f0e78
    {0x1f, 0x33, 0x63, 0x63, 0x63, 0x33, 0x1f, 0x00},  // 0x002f0e80
    {0x7f, 0x03, 0x03, 0x3f, 0x03, 0x03, 0x7f, 0x00},  // 0x002f0e88
    {0x7f, 0x03, 0x03, 0x3f, 0x03, 0x03, 0x03, 0x00},  // 0x002f0e90
    {0x3e, 0x63, 0x03, 0x73, 0x63, 0x63, 0x3e, 0x00},  // 0x002f0e98
    {0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x00},  // 0x002f0ea0
    {0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00},  // 0x002f0ea8
    {0x60, 0x60, 0x60, 0x60, 0x63, 0x63, 0x3e, 0x00},  // 0x002f0eb0
    {0x63, 0x33, 0x1b, 0x0f, 0x1b, 0x33, 0x63, 0x00},  // 0x002f0eb8
    {0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x00},  // 0x002f0ec0
    {0x63, 0x77, 0x7f, 0x6b, 0x63, 0x63, 0x63, 0x00},  // 0x002f0ec8
    {0x63, 0x67, 0x6f, 0x6b, 0x7b, 0x73, 0x63, 0x00},  // 0x002f0ed0
    {0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00},  // 0x002f0ed8
    {0x3f, 0x63, 0x63, 0x3f, 0x03, 0x03, 0x03, 0x00},  // 0x002f0ee0
    {0x3e, 0x63, 0x63, 0x63, 0x6b, 0x33, 0x5e, 0x00},  // 0x002f0ee8
    {0x3f, 0x63, 0x63, 0x3f, 0x1b, 0x33, 0x63, 0x00},  // 0x002f0ef0
    {0x3e, 0x63, 0x03, 0x3e, 0x60, 0x63, 0x3e, 0x00},  // 0x002f0ef8
    {0x3f, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x00},  // 0x002f0f00
    {0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00},  // 0x002f0f08
    {0x63, 0x63, 0x36, 0x36, 0x1c, 0x1c, 0x08, 0x00},  // 0x002f0f10
    {0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00},  // 0x002f0f18
    {0x63, 0x36, 0x1c, 0x08, 0x1c, 0x36, 0x63, 0x00},  // 0x002f0f20
    {0x63, 0x36, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0x00},  // 0x002f0f28
    {0x7f, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x7f, 0x00},  // 0x002f0f30
    {0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00},  // 0x002f0f38
    {0x63, 0x36, 0x1c, 0x3f, 0x0c, 0x3f, 0x0c, 0x00},  // 0x002f0f40
    {0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00},  // 0x002f0f48
    {0x18, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x002f0f50
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00},  // 0x002f0f58
    {0x04, 0x0c, 0x18, 0x10, 0x00, 0x00, 0x00, 0x00},  // 0x002f0f60
    {0x00, 0x00, 0x1e, 0x30, 0x3e, 0x33, 0x7e, 0x00},  // 0x002f0f68
    {0x03, 0x03, 0x3f, 0x63, 0x63, 0x63, 0x3f, 0x00},  // 0x002f0f70
    {0x00, 0x00, 0x3e, 0x63, 0x03, 0x63, 0x3e, 0x00},  // 0x002f0f78
    {0x60, 0x60, 0x7e, 0x63, 0x63, 0x63, 0x7e, 0x00},  // 0x002f0f80
    {0x00, 0x00, 0x3e, 0x63, 0x7f, 0x03, 0x7e, 0x00},  // 0x002f0f88
    {0x70, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x00},  // 0x002f0f90
    {0x00, 0x00, 0x3e, 0x63, 0x63, 0x7e, 0x60, 0x3e},  // 0x002f0f98
    {0x03, 0x03, 0x3f, 0x63, 0x63, 0x63, 0x63, 0x00},  // 0x002f0fa0
    {0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00},  // 0x002f0fa8
    {0x60, 0x60, 0x00, 0x60, 0x60, 0x66, 0x3c, 0x00},  // 0x002f0fb0
    {0x03, 0x03, 0x33, 0x1b, 0x0f, 0x1b, 0x73, 0x00},  // 0x002f0fb8
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},  // 0x002f0fc0
    {0x00, 0x00, 0x36, 0x7f, 0x7f, 0x6b, 0x6b, 0x00},  // 0x002f0fc8
    {0x00, 0x00, 0x3b, 0x67, 0x63, 0x63, 0x63, 0x00},  // 0x002f0fd0
    {0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x3e, 0x00},  // 0x002f0fd8
    {0x00, 0x00, 0x3f, 0x63, 0x63, 0x3f, 0x03, 0x03},  // 0x002f0fe0
    {0x00, 0x00, 0x7e, 0x63, 0x63, 0x7e, 0x60, 0x60},  // 0x002f0fe8
    {0x00, 0x00, 0x76, 0x0e, 0x06, 0x06, 0x06, 0x00},  // 0x002f0ff0
    {0x00, 0x00, 0x7e, 0x07, 0x3e, 0x70, 0x3f, 0x00},  // 0x002f0ff8
    {0x0c, 0x0c, 0x3f, 0x0c, 0x0c, 0x0c, 0x38, 0x00},  // 0x002f1000
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x00},  // 0x002f1008
    {0x00, 0x00, 0x63, 0x63, 0x36, 0x3e, 0x1c, 0x00},  // 0x002f1010
    {0x00, 0x00, 0x6b, 0x6b, 0x6b, 0x7f, 0x36, 0x00},  // 0x002f1018
    {0x00, 0x00, 0x63, 0x36, 0x1c, 0x36, 0x63, 0x00},  // 0x002f1020
    {0x00, 0x00, 0x63, 0x66, 0x2c, 0x38, 0x18, 0x0e},  // 0x002f1028
    {0x00, 0x00, 0x7f, 0x30, 0x18, 0x0c, 0x7f, 0x00},  // 0x002f1030
    {0x18, 0x0c, 0x0c, 0x06, 0x0c, 0x0c, 0x18, 0x00},  // 0x002f1038
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},  // 0x002f1040
    {0x0c, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0c, 0x00},  // 0x002f1048
    {0x00, 0x00, 0x46, 0x6b, 0x31, 0x00, 0x00, 0x00},  // 0x002f1050
    {0x3e, 0x41, 0x5d, 0x45, 0x5d, 0x41, 0x3e, 0x00},  // 0x002f1058
};

static u_int        s_consoleImage[16][RX_CONSOLE_WIDTH];         /* bss 0x31d620: one row, expanded */
static sceGsDispEnv s_consoleDisp;              /* bss 0x327630 */
static int          s_consoleX;                 /* bss 0x327658 */
static int          s_consoleY;                 /* bss 0x32765c */
static u_int        s_consoleColor;             /* bss 0x327660 */
static u_int        s_consoleText[RX_CONSOLE_ROWS][RX_CONSOLE_COLS];  /* bss 0x327668 */
/* bss 0x329968 / 0x32996c: the two instructions of _exit (0x100218) the
 * screen patches; the PC does not patch the host's exit, so they are not kept. */

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00102fe0

static void rxSysConsoleWait(void)
{
    VSync();
}

// ──────────────────────────────────────────────────────────────────────
// Clear cursor row y (text row y + 1).
// FS2 0x00102ff8

static void rxSysConsoleClearRow(int y)
{
    for (int x = RX_CONSOLE_COLS - 1; x >= 0; x--)
        s_consoleText[y + 1][x] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00103040

static void rxSysConsoleScroll(void)
{
    for (int row = 0; row < RX_CONSOLE_ROWS - 1; row++)
        for (int x = 0; x < RX_CONSOLE_COLS; x++)
            s_consoleText[row][x] = s_consoleText[row + 1][x];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00103098 // callcheck: sceGsSetDefLoadImage, FlushCache, sceGsExecLoadImage (PC: log-only screen, no GS upload)

static void rxSysConsoleLoadImage(short x, short y, short w, short h, const u_int *image)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)image;
}

// ──────────────────────────────────────────────────────────────────────
// Expand text row `row` through the 8x8 font into the row image (bit 0 of
// a glyph line is its left pixel; set pixels take the cell's colour with
// full alpha), repeat each line in field mode, and upload it.
// FS2 0x00103110

static void rxSysConsoleDrawRow(int row)
{
    int shift = 1 - sceGsGetGParam()->sceGsFFMode;
    int lines = 1 << shift;
    for (int x = 0; x < RX_CONSOLE_COLS; x++)
    {
        u_int cell = s_consoleText[row][x];
        u_int glyph = (cell >> 24) - 0x20;
        if (glyph > 0x5f)
            glyph = 0;
        const u_char *font = s_consoleFont[glyph];
        int line = 0;
        for (int gy = 0; gy < 8; gy++)
        {
            u_int *dst = &s_consoleImage[line][x * 8];
            for (int gx = 0; gx < 8; gx++)
                dst[gx] = (font[gy] >> gx & 1) != 0 ? (cell & 0xffffff) | 0x80000000 : 0;
            line++;
            for (int i = 0; i < lines - 1; i++, line++)
                for (int gx = 0; gx < 8; gx++)
                    s_consoleImage[line][x * 8 + gx] = s_consoleImage[line - 1][x * 8 + gx];
        }
    }
    rxSysConsoleLoadImage(0, (short)((row * 8) << shift), RX_CONSOLE_WIDTH, (short)(8 << shift),
                          &s_consoleImage[0][0]);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001032a8 // callcheck: sceGsPutDispEnv (PC: log-only screen, no GS display)

static void rxSysConsoleRedraw(void)
{
    for (int row = 0; row < RX_CONSOLE_ROWS; row++)
        rxSysConsoleDrawRow(row);
    rxSysConsoleWait();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001032f8

static void rxSysConsoleNewline(void)
{
    s_consoleX = 0;
    s_consoleY++;
    if (s_consoleY > RX_CONSOLE_LAST)
    {
        rxSysConsoleScroll();
        s_consoleY = RX_CONSOLE_LAST;
    }
    rxSysConsoleClearRow(s_consoleY);
    rxSysConsoleRedraw();
    for (int i = 0; i < s_consoleNewlineWait; i++)
        rxSysConsoleWait();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00103390

static void rxSysConsolePutc(u_char c)
{
    if (c != '\n')
    {
        s_consoleText[s_consoleY + 1][s_consoleX + 3] = (u_int)c << 24 | (s_consoleColor & 0xffffff);
        s_consoleX++;
        if (s_consoleX < RX_CONSOLE_LINE)
            return;
    }
    rxSysConsoleNewline();
}

// ──────────────────────────────────────────────────────────────────────
// The print hook while the screen is open: every character to the
// console, and 1 so rxSysPrintf still prints the text.
// FS2 0x00103420

static int rxSysConsolePrintHook(const char *text)
{
    while (*text != '\0')
        rxSysConsolePutc((u_char)*text++);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Unreferenced in the ROM.
// FS2 0x00103470 // callcheck: sceGsPutDispEnv (PC: log-only screen, no GS display)

void rxSysConsoleSetDisplay(int on)
{
    s_consoleDisp.pmode.EN2 = on != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001034b0

static void rxSysConsoleClear(void)
{
    for (int row = 0; row < RX_CONSOLE_ROWS; row++)
        for (int x = RX_CONSOLE_COLS - 1; x >= 0; x--)
            s_consoleText[row][x] = 0;
    s_consoleX = 0;
    s_consoleY = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Unreferenced in the ROM.
// FS2 0x00103518

void rxSysConsoleSetWaits(int newlineWait, int closePause)
{
    s_consoleNewlineWait = newlineWait;
    s_consoleClosePause = closePause;
}

// ──────────────────────────────────────────────────────────────────────
// Unreferenced in the ROM.
// FS2 0x00103530

u_int rxSysConsoleSetColor(u_int color)
{
    u_int old = s_consoleColor;
    s_consoleColor = color;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// Where the patched _exit jumps while the screen is open: the exit code
// in orange, then spin. Not reached on the PC (the host's exit is not
// patched).
// FS2 0x00103548

void rxSysExitHook(int retval)
{
    s_consoleColor = 0x80004080;
    rxSysPrintf("\nExit System (retval=%d) !!!\n", retval);
    for (;;)
    {
    }
}

// ──────────────────────────────────────────────────────────────────────
// Open the System Monitor (once): patch _exit to rxSysExitHook, reset
// the hardware, hook rxSysPrintf, set up a 640 x 224 display (448 in
// field mode) on frame buffer 0 with only circuit 2 on, clear, draw, and
// print the banner in blue (cells are ABGR).
// FS2 0x00103590 // callcheck: FlushCache, sceGsGetGParam, sceGsSetDefDispEnv (PC: the host's exit is not patched; log-only screen, no GS display)

void rxSysInitExceptionScreen(void)
{
    if (s_consoleOpen != 0)
        return;
    s_consoleOpen = 1;
    // PC: the ROM saves _exit's two instructions, writes "j rxSysExitHook;
    // nop" over them and flushes the caches; the host's exit is native code.
    rxSysResetHardware();
    s_consoleOldHook = rxSysSetPrintHook(rxSysConsolePrintHook);
    memset(&s_consoleDisp, 0, sizeof(s_consoleDisp));
    s_consoleDisp.pmode.EN2 = 1;            // PMODE 0x66
    s_consoleDisp.pmode.CRTMD = 1;
    s_consoleDisp.pmode.MMOD = 1;
    s_consoleDisp.pmode.AMOD = 1;
    rxSysConsoleClear();
    rxSysConsoleRedraw();
    s_consoleColor = 0x80804040;
    rxSysPrintf("System Monitor Open\n\n");
    s_consoleColor = 0x807f7f7f;
}

// ──────────────────────────────────────────────────────────────────────
// Close the System Monitor: the closing line, the pause, the old print
// hook back and _exit restored. Unreferenced in the ROM.
// FS2 0x001036a8 // callcheck: FlushCache (PC: the host's exit is not patched)

void rxSysTermExceptionScreen(void)
{
    if (s_consoleOpen == 0)
        return;
    s_consoleOpen = 0;
    s_consoleColor = 0x80804040;
    rxSysPrintf("\nSystem Monitor Close...Pause %d\n", s_consoleClosePause);
    s_consoleColor = 0x807f7f7f;
    for (int i = 0; i < s_consoleClosePause; i++)
        rxSysConsoleWait();
    rxSysSetPrintHook(s_consoleOldHook);
    s_consoleOldHook = 0;
}

/* ---- subsystems ----------------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Empty in the retail build.
// FS2 0x001037a8

void rxSysInitUser(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Empty in the retail build.
// FS2 0x001037b0

void rxSysTermUser(void)
{
}

/* The four pools it threads into free lists (bss 0x4d0b80 .. 0x4d5988) are
 * rxfile's: its file slots, markers, ring segments and tags (rxfile.h). */

// ──────────────────────────────────────────────────────────────────────
// Reset the rx pools, start the file layer and its thread, and register
// the per-tick callback with the thread layer.
// FS2 0x00153120

void rxSysInitModules(void)
{
    for (int i = 0; i < 64; i++)
        rxFileSlots[i].used = 0;
    rxFileMarkerFree = 0;
    for (int i = 0; i < 64; i++)
    {
        rxFileMarkerPool[i].next = rxFileMarkerFree;
        rxFileMarkerFree = &rxFileMarkerPool[i];
    }
    rxFileSegmentFree = 0;
    for (int i = 0; i < 256; i++)
    {
        rxFileSegmentPool[i].next = rxFileSegmentFree;
        rxFileSegmentFree = &rxFileSegmentPool[i];
    }
    rxFileTagFree = 0;
    for (int i = 0; i < 64; i++)
    {
        rxFileTagPool[i].next = rxFileTagFree;
        rxFileTagFree = &rxFileTagPool[i];
    }
    rxFileInit();
    rxFileStartThread();
    rxFileInitSlots();
    rxThAddCallback(rxSysThreadCallback);
}

// ──────────────────────────────────────────────────────────────────────
// rxSysInitModules' twin: close every rx file, drop the per-tick
// callback, stop rxfile's thread and shut rxfile down.
// FS2 0x00153210

void rxSysTermModules(void)
{
    for (int i = 0; i < 64; i++)
        rxFileClose(i);
    rxThRemoveCallback(rxSysThreadCallback);
    rxFileUpdate();
    rxFileStopThread();
    rxFileTerm();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153108

void rxSysThreadCallback(int yielding)
{
    rxSysSetFileContext(yielding);
}

// ──────────────────────────────────────────────────────────────────────
// rxfile's context follows the running thread: 1 while the main thread
// yields to the rx thread (rxThYield), 0 when it runs again.
// FS2 0x00153280

int rxSysSetFileContext(int index)
{
    return rxFileSetContext(index);
}

// ──────────────────────────────────────────────────────────────────────
// A device driver's control call. No caller in the retail ELF.
// FS2 0x00153298

int rxSysDeviceControl(int device, int cmd, int arg)
{
    RxFileDriver *driver = rxFileGetDriver(device);
    if (driver != NULL)
        return driver->control(cmd, arg);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// A device driver's status. No caller in the retail ELF.
// FS2 0x001532e8

int rxSysDeviceStatus(int device, int what)
{
    RxFileDriver *driver = rxFileGetDriver(device);
    if (driver != NULL)
        return driver->getStatus(what);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The current file context's device (rxInit passes 1, CDROM); returns the
// previous one.
// FS2 0x00153328

int rxSysInitFileDevice(int device)
{
    return rxFileSetCurrent(device);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153340

int rxSysGetFileDevice(void)
{
    return rxFileGetCurrent();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153358

int rxSysSetFileDevice(int slot, int device)
{
    return rxFileSetDevice(slot, device);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153370

void rxSysSetFileDir(const char *dir)
{
    rxFileSetDir(rxFileGetCurrent(), dir);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001533a0

char *rxSysGetFileDir(void)
{
    return rxFileGetDir(rxFileGetCurrent());
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001533c0

void rxSysSetFileSearchDir(const char *dir)
{
    rxFileSetSearchDir(rxFileGetCurrent(), dir);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001533f0

char *rxSysGetFileSearchDir(void)
{
    return rxFileGetSearchDir(rxFileGetCurrent());
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153410

void rxSysPushSearchDir(void)
{
    rxFilePushSearchDir(rxFileGetCurrent());
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153430

void rxSysPopSearchDir(void)
{
    rxFilePopSearchDir(rxFileGetCurrent());
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153450

char *rxSysGetLastName(void)
{
    return rxFileGetLastName(rxFileGetCurrent());
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153470

int rxSysSetDeviceParam(int value)
{
    return rxFileSetDeviceParam(rxFileGetCurrent(), value);
}

// ──────────────────────────────────────────────────────────────────────
// Open an rxfile slot on the current device (returns the fd, -1).
// FS2 0x001534a8

int rxSysStreamOpen(const char *name, int flags)
{
    return rxFileOpenEx(name, flags, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Open a slot that streams into a ring buffer, refilled while it holds
// threshold bytes or fewer.
// FS2 0x001534c8

int rxSysStreamOpenRing(const char *name, void *ring, int ringSize, int threshold)
{
    return rxFileOpenEx(name, SCE_RDONLY | RX_FILE_NOWAIT | RX_FILE_RING, ring, ringSize, threshold);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001534f8

int rxSysStreamOpenPath(int device, const char *path, int flags, void *ring, int ringSize,
                        int threshold)
{
    return rxFileOpenSlot(device, path, flags, ring, ringSize, threshold);
}
