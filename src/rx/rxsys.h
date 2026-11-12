/* ==========================================================================
 *  rx/rxsys.h
 *
 *  The rx middleware's system layer (0x100328..0x1037c0 + 0x153108..0x153518,
 *  pinned; no source-path string: the file name comes from its "rxSys:" /
 *  "rxsys :" messages). It brings the console up for the game: hardware
 *  reset, the boot device and path, the IOP reboot and module list
 *  (iopconf.cnf), system.ini, the memory hooks the game installs, and
 *  rxSysPrintf, the printf every rx and game module reports through. FS2's
 *  rxInit (0x1527a0) calls rxSysSetBootPath, rxSysInit, the
 *  rxSysSetMemFunc* setters, rxSysInitFileDevice and rxSysSetDeviceParam;
 *  rxTerm (0x152750) calls rxSysTerm.
 *
 *  FS2 links Rule of Rose's prebuilt rx: all 126 functions are RoR's word for
 *  word (rxcmp.py; jump targets, lui and gp offsets masked) and the data
 *  0x2f0880..0x2f1060 is RoR's 0x2db9d0.. moved by +0x14eb0 with the same
 *  words. FS2's link order splits the file in two: rxSysNoMemFunc ..
 *  rxSysTermUser at the start of .text, rxSysThreadCallback ..
 *  rxSysStreamOpenPath right before rxfile. RosePan's src/rx/rxsys.h is the
 *  same library (names from there).
 * ======================================================================== */
#ifndef _RX_RXSYS_H
#define _RX_RXSYS_H

#include <stdint.h>

#include "sdk/eekernel.h"
#include "sdk/scetypes.h"
#include "sdk/sifrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RxSysRpc RxSysRpc;

/* Boot devices: the index into the device-ready flags and the "Device[%s]"
 * names (data 0x2f0ae8). rxSysDetectDevice picks HOST on a devkit (more than
 * 32 MB), CDROM on a retail console. */
enum RX_SYS_DEVICE
{
    RX_SYS_DEVICE_HOST  = 0,
    RX_SYS_DEVICE_CDROM = 1,
    RX_SYS_DEVICE_MCARD = 2,
    RX_SYS_DEVICE_HDD   = 3,
    RX_SYS_DEVICE_NUM   = 4,
};

/* The four memory hooks (data 0x2f0d3c..0x2f0d48). Their signatures differ
 * (FS2's rxInit installs gameRxMemAlloc 0x1530b0 twice, gameRxMemRealloc
 * 0x153090 and gameRxMemFree 0x153080, over the game heap), so they are kept
 * as plain code pointers and cast at the call. Unset ones point at
 * rxSysNoMemFunc. In FS2 nothing but the setters was found touching the
 * table (RoR's allocators read it directly). */
typedef void (*RxSysMemFunc)(void);
extern RxSysMemFunc g_rxSysMemFunc[4];          /* data 0x2f0d3c */

/* An IOP RPC client with its call semaphore (rxSysBindRpc / rxSysCallRpc;
 * no caller in the retail ELF). EE: the semaphore at 0, the client data at
 * 0x40. layout: host -- sceSifClientData holds pointers. */
struct RxSysRpc
{
    int              sema;      // FS2 0x00
    sceSifClientData client;    // FS2 0x40
};

/* The print filter rxSysPrintf asks first: 0 drops the line. */
typedef int (*RxSysPrintHook)(const char *text);

int          rxSysNoMemFunc(void);                              // FS2 0x00100328
RxSysMemFunc rxSysSetMemFunc0(RxSysMemFunc func);               // FS2 0x00100350
RxSysMemFunc rxSysSetMemFunc1(RxSysMemFunc func);               // FS2 0x00100380
RxSysMemFunc rxSysSetMemFunc2(RxSysMemFunc func);               // FS2 0x001003b0
RxSysMemFunc rxSysSetMemFunc3(RxSysMemFunc func);               // FS2 0x001003e0

int         rxSysStrLenToEol(const char *s);                    // FS2 0x00100410
int         rxSysStrLenToSpace(const char *s);                  // FS2 0x00100450
char       *rxSysSkipEol(char *s);                              // FS2 0x00100490
char       *rxSysSkipSpace(char *s);                            // FS2 0x001004d0
void        rxSysStrCopyN(char *dst, const char *src, int n);   // FS2 0x00100510
void        rxSysPathCopy(char *dst, const char *src, int n);   // FS2 0x00100550
int         rxSysIsModuleLoaded(const char *name);              // FS2 0x001005b8
char       *rxSysPathFileName(char *path);                      // FS2 0x00100650
char       *rxSysPathExt(char *path);                           // FS2 0x00100708
int         rxSysIsCommentLine(const char *line);               // FS2 0x00100790
int         rxSysSkipModule(char *path);                        // FS2 0x001007c0
void        rxSysAddModule(const char *name);                   // FS2 0x00100810
void        rxSysAddModuleList(const char *const *names);       // FS2 0x00100860
void        rxSysSetCdMode(void);                               // FS2 0x001008a8
void        rxSysRebootIop(const char *img);                    // FS2 0x001008f0
int         rxSysLoadModuleList(char *conf);                    // FS2 0x001009a8
int         rxSysReadIopConf(char *buf, const char *dir);       // FS2 0x00100b80
void        rxSysCdInit(int mode);                              // FS2 0x00100c08
void        rxSysLoadIopModules(void);                          // FS2 0x00100c60
int         rxSysIsDeviceReady(int device);                     // FS2 0x00100e58
int         rxSysGetBootDevice(void);                           // FS2 0x00100e80
void        rxSysDetectDevice(void);                            // FS2 0x00100e90
int         rxSysFindString(const char *s, const char *const *table); // FS2 0x00100f10
void        rxSysReadSystemIni(void);                           // FS2 0x00100f80
void        rxSysResetHardware(void);                           // FS2 0x001011c0
void        rxSysReadRomDate(void);                             // FS2 0x00101230
void        rxSysInit(void);                                    // FS2 0x00101288
int         rxSysIniFind(char *out, const char *text, const char *section, const char *key); // FS2 0x001017a8
char       *rxSysIniFindSection(char *text, const char *section);  // FS2 0x00101488
char       *rxSysIniReadKey(char *out, char *line);             // FS2 0x001015a0
char       *rxSysIniFindKey(char *body, const char *key);       // FS2 0x00101668
int         rxSysIniCountKeys(char *body);                      // FS2 0x00101928
int         rxSysIniKeyAt(char *out, char *body, int index);    // FS2 0x001019f0
int         rxSysIniFindKeyCount(char *text, const char *section);  // FS2 0x00101af0
int         rxSysIniFindKeyAt(char *out, char *text, const char *section, int index); // FS2 0x00101b28
int         rxSysIniGetString(char *out, const char *section, const char *key); // FS2 0x00101b90
int         rxSysIniGetKeyCount(const char *section);           // FS2 0x00101bd8
int         rxSysIniGetKeyAt(char *out, const char *section, int index); // FS2 0x00101c18
void        rxSysTtyWrite(const char *buf, int size);           // FS2 0x00101d78
int         rxSysBindTtyRpc(sceSifClientData *client, unsigned int id); // FS2 0x00101c60
void        rxSysBindTty(void);                                 // FS2 0x00101d38
void        rxSysOpenTty(void);                                 // FS2 0x00101dd0
void        rxSysCloseTty(void);                                // FS2 0x00101e50
void        rxSysSetIopImage(const char *img);                  // FS2 0x00101ef0
const char *rxSysGetProductNumber(void);                        // FS2 0x00101f60
/* Closes the tty, the semaphore, the file devices, the IOP RPC and the
 * threads (FS2: rxTerm 0x152750). */
void        rxSysTerm(void);                                    // FS2 0x00101e98
int         rxSysGetVMode(void);                                // FS2 0x00101f20
int         rxSysGetFrameRate(void);                            // FS2 0x00101f30
int         rxSysGetCountry(void);                              // FS2 0x00101f80
int         rxSysGetMemorySize(void);                           // FS2 0x00101fd0
int         rxSysIsCdromPath(const char *path);                 // FS2 0x00101fe8
char       *rxSysPathSkipDevice(char *path);                    // FS2 0x00102070
void        rxSysSetBootPath(char *path);                       // FS2 0x00102138
RxSysPrintHook rxSysSetPrintHook(RxSysPrintHook hook);          // FS2 0x001022d8
int         rxSysPrintWait(intptr_t *args);                     // FS2 0x001022f0
int         rxSysIsTtyOpen(void);                               // FS2 0x001022f8
int         rxSysPrintf(const char *fmt, ...);                  // FS2 0x00102328
void        rxSysInitEnd(void);                                 // FS2 0x00102520
void        rxSysTermEnd(void);                                 // FS2 0x00102528
int         rxSysBindRpc(RxSysRpc *rpc, unsigned int id);       // FS2 0x00102558
void        rxSysUnbindRpc(RxSysRpc *rpc);                      // FS2 0x00102638
void        rxSysCallRpc(RxSysRpc *rpc, int fno, void *send, int sendSize,
                         void *recv, int recvSize);             // FS2 0x00102678
void        rxSysRpcDone(void *rpc);                            // FS2 0x00102738
void        rxSysTermSema(void);                                // FS2 0x001027a0
int         rxSysCreateSema(int max, int init);                 // FS2 0x00102530
void        rxSysInitSema(void);                                // FS2 0x00102760
void        rxSysLockPrint(void);                               // FS2 0x001027d0
void        rxSysUnlockPrint(void);                             // FS2 0x00102830
void        rxSysCheckCdPath(const char *path);                 // FS2 0x001028d0
int         rxSysOpen(const char *path, int mode);              // FS2 0x001028f8
int         rxSysClose(int fd);                                 // FS2 0x00102948
int         rxSysRead(int fd, void *buf, int size);             // FS2 0x00102980
int         rxSysWrite(int fd, const void *buf, int size);      // FS2 0x001029d8
int         rxSysLseek(int fd, int offset, int whence);         // FS2 0x00102a30
int         rxSysIoctl(int fd, int req, void *arg);             // FS2 0x00102a88
void        rxSysLockConsole(void);                             // FS2 0x00102ae0
void        rxSysUnlockConsole(void);                           // FS2 0x00102af8
void        rxSysExceptionHandler(u_int stat, u_int cause, u_int epc, u_int bva, u_int bpa,
                                  u_long128 *gpr);              // FS2 0x00102b10
void        rxSysSetDebugHandlers(void);                        // FS2 0x00102d30
void        rxSysRestoreDebugHandlers(void);                    // FS2 0x00102de0
void        rxSysHexDump(const char *p, int size);              // FS2 0x00102e78
void        rxSysConsoleSetDisplay(int on);                     // FS2 0x00103470
void        rxSysConsoleSetWaits(int newlineWait, int closePause); // FS2 0x00103518
u_int       rxSysConsoleSetColor(u_int color);                  // FS2 0x00103530
void        rxSysExitHook(int retval);                          // FS2 0x00103548
void        rxSysInitExceptionScreen(void);                     // FS2 0x00103590
void        rxSysTermExceptionScreen(void);                     // FS2 0x001036a8
void        rxSysLockSema1(void);                               // FS2 0x00102850
void        rxSysUnlockSema1(void);                             // FS2 0x001028b0
void        rxSysInitUser(void);                                // FS2 0x001037a8
void        rxSysTermUser(void);                                // FS2 0x001037b0
void        rxSysThreadCallback(int yielding);                  // FS2 0x00153108
void        rxSysInitModules(void);                             // FS2 0x00153120
void        rxSysTermModules(void);                             // FS2 0x00153210
/* rxSysThreadCallback's yielding flag picks rxfile's context: 1 while the
 * main thread yields to the rx thread, 0 when it runs again. */
int         rxSysSetFileContext(int index);                     // FS2 0x00153280
int         rxSysDeviceControl(int device, int cmd, int arg);   // FS2 0x00153298
int         rxSysDeviceStatus(int device, int what);            // FS2 0x001532e8
int         rxSysInitFileDevice(int device);                    // FS2 0x00153328
int         rxSysGetFileDevice(void);                           // FS2 0x00153340
int         rxSysSetFileDevice(int slot, int device);           // FS2 0x00153358
void        rxSysSetFileDir(const char *dir);                   // FS2 0x00153370
char       *rxSysGetFileDir(void);                              // FS2 0x001533a0
void        rxSysSetFileSearchDir(const char *dir);             // FS2 0x001533c0
char       *rxSysGetFileSearchDir(void);                        // FS2 0x001533f0
void        rxSysPushSearchDir(void);                           // FS2 0x00153410
void        rxSysPopSearchDir(void);                            // FS2 0x00153430
char       *rxSysGetLastName(void);                             // FS2 0x00153450
int         rxSysSetDeviceParam(int value);                     // FS2 0x00153470
int         rxSysStreamOpen(const char *name, int flags);       // FS2 0x001534a8
int         rxSysStreamOpenRing(const char *name, void *ring, int ringSize, int threshold); // FS2 0x001534c8
int         rxSysStreamOpenPath(int device, const char *path, int flags, void *ring,
                                int ringSize, int threshold);   // FS2 0x001534f8

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSYS_H */
