/* ==========================================================================
 *  sdpr2/sdpr2drv.c
 *
 *  SDPR2's driver layer and its one driver (FS2 0x170088..0x171300, data
 *  0x2f7a58..0x2f7a9c, bss 0x537d30..0x537d78). See sdpr2drv.h.
 *
 *  - The driver layer: the registered table (sdpr2DrvRegister) and one
 *    call per table slot. Each checks the table and the slot with
 *    sdpr2check.h's sdpr2CheckFunc under SDPR2_TRY, whose second
 *    evaluation prints "SDPR2 ERR Call Undefine Function !!" twice when a
 *    check fails (this file's message copy is 0x302bf0..; kept as the ROM
 *    does it). GetStatus alone checks quietly.
 *  - The SE driver: rxSndSe voices. Gains times phase signs make the
 *    volumes, the note's bit 16 sets the attribute, a preset picks
 *    parameter 0xb, the int pitch is 1/4096 steps, and the status bits come
 *    back as SDPR2_STATUS_*.
 *
 *  FS2-only middleware, named by reading (batches 011, 203).
 * ======================================================================== */
#include "sdpr2/sdpr2drv.h"

#include <stddef.h>

#include "rx/rxsnd.h"
#include "sdpr2/sdpr2.h"
#include "sdpr2/sdpr2check.h"

/* rxSndSeGetStatus's bits (rxsnd.h). */
#define SDPR2_SE_STATUS_OPEN    0x1
#define SDPR2_SE_STATUS_ENDED   0x4
#define SDPR2_SE_STATUS_PAUSED  0x8

/* sdpr2SeDrvSetPitch: SDPR2_PITCH_ONE (0x1000) is 1.0 (the ROM's 0x39800000). */
#define SDPR2_SE_PITCH_SCALE    (1.0f / 4096.0f)

/* The SE driver's config, the 8 words it was installed with (or its
 * defaults) and hands to sdpr2DrvSetConfig. The game's (sound.cpp
 * kSdpr2Config): {0, 0, -1, -1, 0, 0, 0, 0}. */
typedef struct Sdpr2SeDrvConfig
{
    int keep;       /* +0x00: rxSndSePlay's keep; sdpr2SingleUpdate stops ended voices when 1. Default 1 */
    int pauseArg;   /* +0x04: rxSndSePause's third argument. Default 1 */
    int word2;      /* +0x08: not read here; default -1 */
    int word3;      /* +0x0c: not read here; default -1 */
    int word4[4];   /* +0x10: all four the config's word 4 (the ROM copies it four times); default 0 */
} Sdpr2SeDrvConfig;
static_assert(sizeof(Sdpr2SeDrvConfig) == SDPR2_DRV_CONFIG_WORDS * 4, "Sdpr2SeDrvConfig is the 8 config words");

/* ---- the SE driver's table and presets (data) -------------------------- */

static int  sdpr2SeDrvPlay(int xvab, int prog, int note, int volL, int volR,
                           int signL, int signR);
static int  sdpr2SeDrvPlayEx(int xvab, int prog, int note, int volL, int volR,
                             int signL, int signR, int preset);
static void sdpr2SeDrvStop(int voice, int mode);
static void sdpr2SeDrvPause(int voice, int on);
static void sdpr2SeDrvSetPitch(int voice, int pitch);
static void sdpr2SeDrvSetPitchF(int voice, float pitch);
static void sdpr2SeDrvSetVolume(int voice, int volL, int volR, int signL,
                                int signR);
static void sdpr2SeDrvSetAttr(int voice, int attr);
static void sdpr2SeDrvSetParam(int voice, int preset);
static int  sdpr2SeDrvGetStatus(int voice);

/* data 0x2f7a58: rxSndSeSetParamB's value per preset (0..SDPR2_PRESET_MAX). */
static const int s_sePresetParamB[SDPR2_PRESET_MAX + 1] = { 1, 2, 0, 0 };

/* data 0x2f7a68 */
static const Sdpr2Driver s_seDriver =
{
    sdpr2SeDrvPlay,         /* +0x00 */
    sdpr2SeDrvPlayEx,       /* +0x04 */
    sdpr2SeDrvStop,         /* +0x08 */
    sdpr2SeDrvPause,        /* +0x0c */
    sdpr2SeDrvSetPitch,     /* +0x10 */
    sdpr2SeDrvSetPitchF,    /* +0x14 */
    sdpr2SeDrvSetVolume,    /* +0x18 */
    sdpr2SeDrvSetAttr,      /* +0x1c */
    sdpr2SeDrvSetParam,     /* +0x20 */
    sdpr2SeDrvGetStatus,    /* +0x24 */
    NULL,                   /* +0x28 */
    NULL,                   /* +0x2c */
    NULL,                   /* +0x30 */
};

/* ---- bss --------------------------------------------------------------- */

static const Sdpr2Driver *s_driver;                 /* bss 0x537d30 */
static int s_config[SDPR2_DRV_CONFIG_WORDS];        /* bss 0x537d38: sdpr2DrvSetConfig's copy */
static Sdpr2SeDrvConfig s_seConfig;                 /* bss 0x537d58 */

// ──────────────────────────────────────────────────────────────────────
// A failed install is called a second time for the answer (SDPR2_TRY).
// FS2 0x00170088

int sdpr2DrvInit(const void *config)
{
    SDPR2_TRY(sdpr2SeDrvInstall((const int *)config));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001700d0

int sdpr2DrvRegister(const Sdpr2Driver *driver)
{
    if (s_driver != NULL)
        return -1;
    s_driver = driver;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001700f8

int sdpr2DrvUnregister(void)
{
    if (s_driver == NULL)
        return -1;
    s_driver = NULL;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170120

int sdpr2DrvPlay(int xvab, int prog, int note, int volL, int volR,
                 int signL, int signR)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->play));
    return s_driver->play(xvab, prog, note, volL, volR, signL, signR);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170280

int sdpr2DrvPlayEx(int xvab, int prog, int note, int volL, int volR,
                   int signL, int signR, int preset)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->playEx));
    return s_driver->playEx(xvab, prog, note, volL, volR, signL, signR, preset);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001703f0

int sdpr2DrvStop(int voice, int mode)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->stop));
    s_driver->stop(voice, mode);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170508

int sdpr2DrvPause(int voice, int on)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->pause));
    s_driver->pause(voice, on);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170620

int sdpr2DrvSetPitch(int voice, int pitch)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->setPitch));
    s_driver->setPitch(voice, pitch);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170738

int sdpr2DrvSetPitchF(int voice, float pitch)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->setPitchF));
    s_driver->setPitchF(voice, pitch);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170850

int sdpr2DrvSetVolume(int voice, int volL, int volR, int signL, int signR)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->setVolume));
    s_driver->setVolume(voice, volL, volR, signL, signR);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170998

int sdpr2DrvSetAttr(int voice, int attr)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->setAttr));
    s_driver->setAttr(voice, attr);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// No caller in the ROM.
// FS2 0x00170ab0

int sdpr2DrvSetParam(int voice, int preset)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->setParam));
    s_driver->setParam(voice, preset);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The quiet one: no message, -1 as the answer.
// FS2 0x00170bc8

int sdpr2DrvGetStatus(int voice)
{
    if (s_driver == NULL || s_driver->getStatus == NULL)
        return -1;
    return s_driver->getStatus(voice);
}

// ──────────────────────────────────────────────────────────────────────
// No caller in the ROM; null in the SE driver.
// FS2 0x00170c10

int sdpr2DrvExt28(int a)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->ext28));
    return s_driver->ext28(a);
}

// ──────────────────────────────────────────────────────────────────────
// No caller in the ROM; null in the SE driver.
// FS2 0x00170d10

int sdpr2DrvExt2C(int a, int b)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->ext2C));
    return s_driver->ext2C(a, b);
}

// ──────────────────────────────────────────────────────────────────────
// No caller in the ROM; null in the SE driver.
// FS2 0x00170e20

int sdpr2DrvExt30(int a, int b, int c)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckFunc, inline)
    SDPR2_TRY(sdpr2CheckFunc(s_driver));
    SDPR2_TRY(sdpr2CheckFunc(s_driver->ext30));
    return s_driver->ext30(a, b, c);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170f40

void sdpr2DrvSetConfig(const int *config)
{
    int i;

    for (i = 0; i < SDPR2_DRV_CONFIG_WORDS; i++)
        s_config[i] = config[i];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00170f90

int sdpr2DrvGetConfig(int *out)
{
    int i;

    if (out == NULL)
        return -1;
    for (i = 0; i < SDPR2_DRV_CONFIG_WORDS; i++)
        out[i] = s_config[i];
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The gains times the phase signs make the voice's volumes.
// FS2 0x00170ff0

static int sdpr2SeDrvPlay(int xvab, int prog, int note, int volL, int volR,
                          int signL, int signR)
{
    int voice;

    voice = rxSndSePlay(xvab, prog, note & ~SDPR2_NOTE_FLAG_ATTR,
                        volL * signL, volR * signR, s_seConfig.keep);
    if (note & SDPR2_NOTE_FLAG_ATTR)
        rxSndSeSetAttr(voice, 1);
    return voice;
}

// ──────────────────────────────────────────────────────────────────────
// sdpr2SeDrvPlay, then the preset's parameter 0xb.
// FS2 0x00171068

static int sdpr2SeDrvPlayEx(int xvab, int prog, int note, int volL, int volR,
                            int signL, int signR, int preset)
{
    int voice;

    voice = rxSndSePlay(xvab, prog, note & ~SDPR2_NOTE_FLAG_ATTR,
                        volL * signL, volR * signR, s_seConfig.keep);
    if (note & SDPR2_NOTE_FLAG_ATTR)
        rxSndSeSetAttr(voice, 1);
    rxSndSeSetParamB(voice, s_sePresetParamB[preset]);
    return voice;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00171100

static void sdpr2SeDrvStop(int voice, int mode)
{
    rxSndSeStop(voice, mode);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00171118

static void sdpr2SeDrvPause(int voice, int on)
{
    rxSndSePause(voice, on, s_seConfig.pauseArg);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00171138

static void sdpr2SeDrvSetPitch(int voice, int pitch)
{
    rxSndSeSetPitch(voice, (float)pitch * SDPR2_SE_PITCH_SCALE);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00171160

static void sdpr2SeDrvSetPitchF(int voice, float pitch)
{
    rxSndSeSetPitch(voice, pitch);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00171178

static void sdpr2SeDrvSetVolume(int voice, int volL, int volR, int signL,
                                int signR)
{
    rxSndSeSetVolume(voice, signL * volL, signR * volR);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001711a0

static void sdpr2SeDrvSetAttr(int voice, int attr)
{
    rxSndSeSetAttr(voice, attr);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001711b8

static void sdpr2SeDrvSetParam(int voice, int preset)
{
    rxSndSeSetParamB(voice, s_sePresetParamB[preset]);
}

// ──────────────────────────────────────────────────────────────────────
// Closed: 0 (ended); open: paused, ended, or playing.
// FS2 0x001711e0

static int sdpr2SeDrvGetStatus(int voice)
{
    u_int status;

    status = rxSndSeGetStatus(voice);
    if (!(status & SDPR2_SE_STATUS_OPEN))
        return 0;
    if (status & SDPR2_SE_STATUS_PAUSED)
        return SDPR2_STATUS_PAUSED;
    return (status & SDPR2_SE_STATUS_ENDED) == 0 ? SDPR2_STATUS_PLAYING : 0;
}

// ──────────────────────────────────────────────────────────────────────
// The config (null: the defaults) is kept, handed to the driver layer,
// and the table registered. Word 4 fills words 4..7 (kept as the ROM does
// it).
// FS2 0x00171230

int sdpr2SeDrvInstall(const int *config)
{
    if (config == NULL)
    {
        s_seConfig.keep = 1;
        s_seConfig.pauseArg = 1;
        s_seConfig.word2 = -1;
        s_seConfig.word3 = -1;
        s_seConfig.word4[0] = 0;
        s_seConfig.word4[1] = 0;
        s_seConfig.word4[2] = 0;
        s_seConfig.word4[3] = 0;
    }
    else
    {
        s_seConfig.keep = config[0];
        s_seConfig.pauseArg = config[1];
        s_seConfig.word2 = config[2];
        s_seConfig.word3 = config[3];
        s_seConfig.word4[0] = config[4];
        s_seConfig.word4[1] = config[4];
        s_seConfig.word4[2] = config[4];
        s_seConfig.word4[3] = config[4];
    }
    sdpr2DrvSetConfig((const int *)&s_seConfig);
    return sdpr2DrvRegister(&s_seDriver);
}

// ──────────────────────────────────────────────────────────────────────
// No caller in the ROM.
// FS2 0x001712e0

int sdpr2SeDrvUninstall(void)
{
    return sdpr2DrvUnregister();
}
