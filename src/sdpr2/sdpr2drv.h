/* ==========================================================================
 *  sdpr2/sdpr2drv.h
 *
 *  SDPR2's driver layer (sdpr2/sdpr2drv.c 0x170088..0x171300): one driver
 *  registers a function table (Sdpr2Driver) and the types (sdpr2pair.c,
 *  sdpr2single.c) reach their voices only through the sdpr2Drv* calls, each
 *  checking the table and its function first. The only driver is the SE
 *  driver over rxSndSe (sdpr2SeDrvInstall, table 0x2f7a68), installed by
 *  sdpr2Init through sdpr2DrvInit. architecture.md "SDPR2".
 * ======================================================================== */
#ifndef _SDPR2_SDPR2DRV_H
#define _SDPR2_SDPR2DRV_H

#ifdef __cplusplus
extern "C" {
#endif

/* The config: 8 words (sdpr2Init's, kept by the driver at bss 0x537d38
 * and read back by the types through sdpr2DrvGetConfig). The SE driver's
 * meaning of them: Sdpr2SeDrvConfig in sdpr2drv.c. */
#define SDPR2_DRV_CONFIG_WORDS  8

/* sdpr2DrvPlay / PlayEx note: bit 16 is a flag, not part of the note. The
 * SE driver strips it and sets the new voice's attribute to 1. */
#define SDPR2_NOTE_FLAG_ATTR    0x10000

/* A driver's function table (FS2 0x2f7a68 the SE driver's, in data; the
 * active one at bss 0x537d30). layout: host -- function pointers, words on
 * the EE: offsets in the comments. A null entry makes its sdpr2Drv* call
 * an error (-1). */
typedef struct Sdpr2Driver
{
    int  (*play)(int xvab, int prog, int note, int volL, int volR,
                 int signL, int signR);                         /* +0x00 */
    int  (*playEx)(int xvab, int prog, int note, int volL, int volR,
                   int signL, int signR, int preset);           /* +0x04 */
    void (*stop)(int voice, int mode);                          /* +0x08 */
    void (*pause)(int voice, int on);                           /* +0x0c */
    void (*setPitch)(int voice, int pitch);                     /* +0x10 */
    void (*setPitchF)(int voice, float pitch);                  /* +0x14 */
    void (*setVolume)(int voice, int volL, int volR, int signL,
                      int signR);                               /* +0x18 */
    void (*setAttr)(int voice, int attr);                       /* +0x1c */
    void (*setParam)(int voice, int preset);                    /* +0x20 */
    int  (*getStatus)(int voice);                               /* +0x24 */
    int  (*ext28)(int a);                                       /* +0x28: null in the SE driver */
    int  (*ext2C)(int a, int b);                                /* +0x2c: null in the SE driver */
    int  (*ext30)(int a, int b, int c);                         /* +0x30: null in the SE driver */
} Sdpr2Driver;                                                  /* 0x34 on the EE */

/* config: sdpr2Init's 8 words, or null for the defaults. 0, or -1 (the
 * install is called a second time for the answer, SDPR2_TRY). */
int  sdpr2DrvInit(const void *config);                                  /* FS2 0x00170088 */
/* 0, or -1 when a driver is already registered / none is. */
int  sdpr2DrvRegister(const Sdpr2Driver *driver);                       /* FS2 0x001700d0 */
int  sdpr2DrvUnregister(void);                                          /* FS2 0x001700f8 */

/* A voice: the SE driver's id, or -1. volL / volR 0..0x80 (the pan's
 * gains, sdpr2CalcPanVolumes), signL / signR the phase signs (+1 / -1).
 * note may carry SDPR2_NOTE_FLAG_ATTR. */
int  sdpr2DrvPlay(int xvab, int prog, int note, int volL, int volR,
                  int signL, int signR);                                /* FS2 0x00170120 */
/* preset: 0..SDPR2_PRESET_MAX, the SE parameter set. */
int  sdpr2DrvPlayEx(int xvab, int prog, int note, int volL, int volR,
                    int signL, int signR, int preset);                  /* FS2 0x00170280 */
/* The rest answer 0 once the driver ran, -1 when it has no such function. */
int  sdpr2DrvStop(int voice, int mode);                                 /* FS2 0x001703f0 */
int  sdpr2DrvPause(int voice, int on);                                  /* FS2 0x00170508 */
/* pitch: SDPR2_PITCH_ONE = 1.0. */
int  sdpr2DrvSetPitch(int voice, int pitch);                            /* FS2 0x00170620 */
int  sdpr2DrvSetPitchF(int voice, float pitch);                         /* FS2 0x00170738 */
int  sdpr2DrvSetVolume(int voice, int volL, int volR, int signL,
                       int signR);                                      /* FS2 0x00170850 */
int  sdpr2DrvSetAttr(int voice, int attr);                              /* FS2 0x00170998 */
int  sdpr2DrvSetParam(int voice, int preset);                           /* FS2 0x00170ab0 */
/* SDPR2_STATUS_*, 0 ended; -1 (without a message) when there is no driver
 * or no getStatus. */
int  sdpr2DrvGetStatus(int voice);                                      /* FS2 0x00170bc8 */
/* The three spare slots; no caller in the ROM. */
int  sdpr2DrvExt28(int a);                                              /* FS2 0x00170c10 */
int  sdpr2DrvExt2C(int a, int b);                                       /* FS2 0x00170d10 */
int  sdpr2DrvExt30(int a, int b, int c);                                /* FS2 0x00170e20 */
/* The driver's 8 config words, kept / handed back. GetConfig: -1 when out
 * is null. */
void sdpr2DrvSetConfig(const int *config);                              /* FS2 0x00170f40 */
int  sdpr2DrvGetConfig(int *out);                                       /* FS2 0x00170f90 */

/* The SE driver: its config (null: the defaults) then its table. 0, or -1
 * when a driver is already registered. */
int  sdpr2SeDrvInstall(const int *config);                              /* FS2 0x00171230 */
int  sdpr2SeDrvUninstall(void);                                         /* FS2 0x001712e0 */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2DRV_H */
