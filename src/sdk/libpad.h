/* ==========================================================================
 *  sdk/libpad.h  (SCE PS2 EE <libpad.h> -- pad library surface)
 *
 *  Minimal PC-port declaration header for modules that include the PS2 SDK pad
 *  library.  The current reconstruction only needs the header to resolve; the
 *  concrete scePad* declarations can be filled in as pad.c is reconstructed.
 * ======================================================================== */

#pragma once
#include "scetypes.h"

#define scePadDmaBufferMax      16

#define scePadStateDiscon       0
#define scePadStateFindPad      1
#define scePadStateFindCTP1     2
#define scePadStateExecCmd      5
#define scePadStateStable       6
#define scePadStateError        7
#define scePadStateClosed       99

#define scePadReqStateComplete  0
#define scePadReqStateFaild     1
#define scePadReqStateFailed    1
#define scePadReqStateBusy      2

#define InfoModeCurID           1
#define InfoModeCurExID         2
#define InfoModeCurExOffs       3
#define InfoModeIdTable         4

/* The buttons, as bits of the report's byte-swapped, inverted word. */
#define SCE_PADLup      (1 << 12)
#define SCE_PADLdown    (1 << 14)
#define SCE_PADLleft    (1 << 15)
#define SCE_PADLright   (1 << 13)
#define SCE_PADRup      (1 << 4)
#define SCE_PADRdown    (1 << 6)
#define SCE_PADRleft    (1 << 7)
#define SCE_PADRright   (1 << 5)
#define SCE_PADi        (1 << 9)    /* L3 */
#define SCE_PADj        (1 << 10)   /* R3 */
#define SCE_PADk        (1 << 8)
#define SCE_PADl        (1 << 3)
#define SCE_PADm        (1 << 1)
#define SCE_PADn        (1 << 2)
#define SCE_PADo        (1 << 0)
#define SCE_PADh        (1 << 11)
#define SCE_PADL1       SCE_PADn
#define SCE_PADL2       SCE_PADo
#define SCE_PADR1       SCE_PADl
#define SCE_PADR2       SCE_PADm
#define SCE_PADstart    SCE_PADh
#define SCE_PADselect   SCE_PADk

int scePadInit(int mode);
int scePadEnd(void);
int scePadPortOpen(int port, int slot, void *addr);
int scePadPortClose(int port, int slot);
int scePadRead(int port, int slot, unsigned char *rdata);
int scePadGetState(int port, int slot);
int scePadGetReqState(int port, int slot);
int scePadInfoAct(int port, int slot, int actno, int term);
int scePadInfoComb(int port, int slot, int listno, int offs);
int scePadInfoMode(int port, int slot, int term, int offs);
int scePadSetMainMode(int port, int slot, int offs, int lock);
int scePadSetActDirect(int port, int slot, const unsigned char *data);
int scePadSetActAlign(int port, int slot, const unsigned char *data);
int scePadInfoPressMode(int port, int slot);
int scePadEnterPressMode(int port, int slot);
int scePadExitPressMode(int port, int slot);
int scePadGetSlotMax(int port);
int scePadSetWarningLevel(int level);

