/* ==========================================================================
 *  sdpr2/sdpr2.h
 *
 *  SDPR2 (FS2-only middleware, sdpr2/sdpr2*.c 0x1681c0..0x171300): panned
 *  sound effects over rxSndSe. This is sdpr2/sdpr2.c's header, the core
 *  and public API (0x1681c0..0x16c870):
 *
 *  - Up to 8 sound types register a function table (Sdpr2Type) in the
 *    registry; two exist: SDPR2_TYPE_PAIR, a two-voice sound (voices at the
 *    angle and the angle + 180, the second detuned by the pair mode:
 *    sdpr2pair.c), and SDPR2_TYPE_SINGLE, one voice (sdpr2single.c).
 *  - A sound is a handle, an index into 64 slots (Sdpr2Slot); the type's
 *    play function picks the slot (sdpr2FindFreeSlot) and fills it, the API
 *    then stores the type in it so the handle calls reach the same table.
 *  - Every API call checks and limits its arguments (sdpr2check.h) and
 *    calls the type's function; a missing function is an error (-1).
 *
 *  Argument names read from the game's callers (sound/sound.cpp).
 *  architecture.md "SDPR2".
 * ======================================================================== */
#ifndef _SDPR2_SDPR2_H
#define _SDPR2_SDPR2_H

#ifdef __cplusplus
extern "C" {
#endif

#define SDPR2_TYPE_MAX      8       /* registry entries */
#define SDPR2_SLOT_MAX      64      /* handles          */

/* The types the game plays (sdpr2PlayPan's type). */
#define SDPR2_TYPE_PAIR     0
#define SDPR2_TYPE_SINGLE   1

/* sdpr2GetStatus: 1 / 2 while it sounds. */
#define SDPR2_STATUS_PLAYING    1
#define SDPR2_STATUS_PAUSED     2

#define SDPR2_PITCH_ONE     0x1000  /* Sdpr2Slot.pitch of 1.0       */
#define SDPR2_PITCH_MAX     0x2000  /* sdpr2SetPitch's limit (2.0)  */
#define SDPR2_VOLUME_MAX    0x80
#define SDPR2_SPREAD_MAX    0x80
#define SDPR2_XVAB_MAX      0xffff
#define SDPR2_PROG_MAX      0x7f
#define SDPR2_NOTE_MAX      0x1ffff
#define SDPR2_PRESET_MAX    3       /* the Ex calls' parameter set  */
#define SDPR2_PAIR_MODE_MAX 5
#define SDPR2_ANGLE_RANGE   360     /* degrees                      */

#define SDPR2_VOICE_FREE    (-1)    /* Sdpr2Slot.voice[] of a free slot */

/* A type's function table (FS2 0x537c50 pair, 0x537cc0 single; filled in
 * code by their Init). layout: host -- function pointers, words on the
 * EE: offsets in the comments. */
typedef struct Sdpr2Type
{
    int (*playPan)(int xvab, int prog, int note, int volume, int deg,
                   int spread);                                 /* +0x00 */
    int (*play)(int xvab, int prog, int note);                  /* +0x04 */
    int (*playPanEx)(int xvab, int prog, int note, int volume, int deg,
                     int spread, int preset);                   /* +0x08 */
    int (*playEx)(int xvab, int prog, int note, int preset);    /* +0x0c */
    int (*stop)(int handle, int mode);                          /* +0x10 */
    int (*stopAll)(int mode);                                   /* +0x14 */
    int (*pause)(int handle, int on);                           /* +0x18 */
    int (*setPitch)(int handle, int pitch);                     /* +0x1c */
    int (*setPitchF)(int handle, float pitch);                  /* +0x20 */
    int (*setAngle)(int handle, int deg);                       /* +0x24 */
    int (*setVolume)(int handle, int volume);                   /* +0x28 */
    int (*setSpread)(int handle, int spread);                   /* +0x2c */
    int (*setAttr)(int handle, int attr);                       /* +0x30 */
    int (*getLevels)(int handle, int *levels);                  /* +0x34 */
    int (*getStatus)(int handle);                               /* +0x38 */
    int (*update)(void);                                        /* +0x3c */
    int (*ext40)(int a);                                        /* +0x40: null in both types */
    int (*ext44)(int a, int b);                                 /* +0x44: null in both types */
    int (*ext48)(int a, int b, int c);                          /* +0x48: null in both types */
} Sdpr2Type;                                                    /* 0x4c on the EE */

/* A handle (bss 0x537140, 0x2c bytes each on the EE; layout: host, the
 * type pointer is a host pointer). The type functions fill everything but
 * the type; the getters read angle, volume, pitch and spread. */
typedef struct Sdpr2Slot
{
    const Sdpr2Type *type;  /* +0x00: null while free                   */
    int voice[2];           /* +0x04: SE voice ids, SDPR2_VOICE_FREE    */
    int flip;               /* +0x0c                                    */
    int _10;                /* +0x10: an angle (unread here)            */
    int angle;              /* +0x14: degrees, sdpr2GetAngle            */
    int _18;                /* +0x18: an angle (unread here)            */
    int volume;             /* +0x1c                                    */
    int pitch;              /* +0x20: SDPR2_PITCH_ONE = 1.0             */
    int spread;             /* +0x24                                    */
    int state;              /* +0x28                                    */
} Sdpr2Slot;

/* config: 8 words handed to the driver (sdpr2DrvInit). 0, or -1 when an
 * init failed. */
int   sdpr2Init(const void *config);                                    /* FS2 0x001683a8 */

void  sdpr2SetSurround(int on);                                         /* FS2 0x001681c0 */
int   sdpr2GetSurround(void);                                           /* FS2 0x00168250 */
void  sdpr2SetPairMode(int mode);                                       /* FS2 0x00168260 */
int   sdpr2GetPairMode(void);                                           /* FS2 0x001682f0 */
void  sdpr2SetDebug(int on);                                            /* FS2 0x00168300 */
int   sdpr2GetDebug(void);                                              /* FS2 0x00168398 */

int   sdpr2RegisterType(int type, const Sdpr2Type *table);              /* FS2 0x001684a0 */
int   sdpr2UnregisterType(int type);                                    /* FS2 0x001685d8 */

/* A handle playing prog / note of the XVAB at angle deg (0..359) and
 * spread (0..0x80); < 0 failed. */
int   sdpr2PlayPan(int type, int xvab, int prog, int note, int volume,
                   int deg, int spread);                                /* FS2 0x00168708 */
int   sdpr2Play(int type, int xvab, int prog, int note);                /* FS2 0x00168d38 */
int   sdpr2PlayPanEx(int type, int xvab, int prog, int note, int volume,
                     int deg, int spread, int preset);                  /* FS2 0x00169198 */
int   sdpr2PlayEx(int type, int xvab, int prog, int note, int preset);  /* FS2 0x00169838 */

int   sdpr2Stop(int handle, int mode);                                  /* FS2 0x00169d20 */
int   sdpr2StopType(int type, int mode);                                /* FS2 0x00169f78 */
int   sdpr2StopAll(int mode);                                           /* FS2 0x0016a1c0 */
int   sdpr2Pause(int handle, int on);                                   /* FS2 0x0016a2b0 */
int   sdpr2SetPitch(int handle, int pitch);                             /* FS2 0x0016a508 */
int   sdpr2SetPitchF(int handle, float pitch);                          /* FS2 0x0016a770 */
int   sdpr2SetAngle(int handle, int deg);                               /* FS2 0x0016aa08 */
int   sdpr2SetVolume(int handle, int volume);                           /* FS2 0x0016acd0 */
int   sdpr2SetSpread(int handle, int spread);                           /* FS2 0x0016af38 */
int   sdpr2SetAttr(int handle, int attr);                               /* FS2 0x0016b1a0 */
int   sdpr2GetLevels(int handle, int *levels);                          /* FS2 0x0016b3f8 */
int   sdpr2GetStatus(int handle);                                       /* FS2 0x0016b5f0: SDPR2_STATUS_*, -1 */
int   sdpr2Update(void);                                                /* FS2 0x0016b698 */

int   sdpr2TypeExt40(int type, int a);                                  /* FS2 0x0016b718 */
int   sdpr2TypeExt44(int type, int a, int b);                           /* FS2 0x0016b900 */
int   sdpr2TypeExt48(int type, int a, int b, int c);                    /* FS2 0x0016baf8 */

int   sdpr2GetPitch(int handle);                                        /* FS2 0x0016bd00 */
float sdpr2GetPitchF(int handle);                                       /* FS2 0x0016bf10 */
int   sdpr2GetAngle(int handle);                                        /* FS2 0x0016c138 */
int   sdpr2GetVolume(int handle);                                       /* FS2 0x0016c348 */
int   sdpr2GetSpread(int handle);                                       /* FS2 0x0016c558 */

/* For the types: a free slot's index (both voices free), or -1. */
int        sdpr2FindFreeSlot(void);                                     /* FS2 0x0016c768 */
Sdpr2Slot *sdpr2GetSlot(int handle);                                    /* FS2 0x0016c7c0: null out of range */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2_H */
