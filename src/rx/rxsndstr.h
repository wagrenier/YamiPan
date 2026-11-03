/* ==========================================================================
 *  rx/rxsndstr.h
 *
 *  FS2's rx sound streams (rx/rxsndstr*.c, not written yet; not in RoR, so
 *  RosePan has no counterpart): XWS banks (rxSndStrBank*) and the streams
 *  played from them (rxSndStr*). Only what written TUs call so far; the
 *  argument names are read from the game's callers (sound/sound.cpp).
 *  architecture.md "Batch 008" has the layers.
 * ======================================================================== */
#ifndef _RX_RXSNDSTR_H
#define _RX_RXSNDSTR_H

#include "sdk/scetypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* rxSndStrGetStatus bits. */
#define RXSNDSTR_STATUS_END     0x4     /* the stream played out */

/* XWS banks: a bank from its RXWS header, its body attached from a file,
 * EE memory or IOP memory. -1 failed. */
int  rxSndStrBankOpen(int slot, const void *header);                    /* FS2 0x001622f8 */
void rxSndStrBankClose(int bank);                                       /* FS2 0x001624d8 */
void rxSndStrBankAttachBodyFile(int bank, const char *name);            /* FS2 0x00162530 */
void rxSndStrBankAttachBodyMemory(int bank, const void *body, int size); /* FS2 0x001625c8 */
void rxSndStrBankAttachBodyIop(int bank, u_int iopAddr, int size);      /* FS2 0x00162610 */
int  rxSndStrBankGetRequestRate(int bank, int request);                 /* FS2 0x00162820 */
int  rxSndStrBankGetRequestChannels(int bank, int request);             /* FS2 0x00162850 */
int  rxSndStrBankGetRequestSize(int bank, int request);                 /* FS2 0x001628b0 */
int  rxSndStrBankGetTrackCount(int bank, int request);                  /* FS2 0x00162c20 */

/* Streams: request of bank from start, read through buf (file bodies;
 * null for memory / IOP ones) into an SPU ring at spuAddr. The stream id,
 * -1 failed. a6 / a7 (the game passes 0, 4) are unread. */
int  rxSndStrPlayAt(int bank, int request, int start, void *buf, int bufSize,
                    int a6, int a7, u_int spuAddr);                     /* FS2 0x00163738 */
void rxSndStrPause(int stream, int pause, int arg);                     /* FS2 0x00163750 */
void rxSndStrStop(int stream, int arg);                                 /* FS2 0x00163848 */
void rxSndStrSuspend(int stream, int arg);                              /* FS2 0x001638d8 */
u_int rxSndStrGetStatus(int stream);                                    /* FS2 0x00163910: RXSNDSTR_STATUS_* */
void rxSndStrSetAttr(int stream, int attr);                             /* FS2 0x00163960 */
void rxSndStrSetVolume(int stream, int volL, int volR);                 /* FS2 0x00163a20 */
void rxSndStrSetPitch(int stream, float pitch);                         /* FS2 0x00163ae8 */
/* The rxSndAdpcm / rxSndPcm stream under it. */
int  rxSndStrGetHandle(int stream);                                     /* FS2 0x00163dc8 */
/* Non-zero while a read is still in flight. */
int  rxSndStrSyncRead(int stream, int arg);                             /* FS2 0x00163ea8 */
int  rxSndStrGetDataWritten(int stream);                                /* FS2 0x00163f20 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTR_H */
