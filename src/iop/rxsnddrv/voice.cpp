/* ==========================================================================
 *  voice.cpp  (RXSNDDRV.IRX 0x35c0..0x45b0: the 48 SPU2 voices)
 *
 *  Every sound the driver plays (BGM notes, SEs, NOTE commands) takes a voice
 *  here: RxSndDrvVoiceAlloc() hands one out by group (0 any core, 1 core 0,
 *  2 core 1), stealing the least important when none is free; the owner
 *  writes the voice's registers into its record (sample, loop, volume, pitch,
 *  mix switches) and keys it on and off, and RxSndDrvVoiceTick() applies all
 *  of it once per V-blank: the parameters first, the mix switches as one
 *  write per core, then KON / KOFF.
 *
 *  Voice v is SPU2 core v / 24, channel v % 24.  A voice the owner releases
 *  fades out over 8 ticks (0x80 - 0x10 a tick) and is then freed; an
 *  auto-free voice is also released once its envelope has been silent for 9
 *  ticks.  The owner's callback hears both {voice, reason: 0 ended, 1 taken}.
 *
 *  Commands 0x90..0x93 (rx/rxsnd.c rxSndGetVoiceTable*, rxSndSetVoiceEntry)
 *  read the table back to the EE and enable / disable voices for allocation.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "iop_voice.h"
#include "libsd.h"

#include <string.h>

#define RXSNDDRV_VOICES 48

/* m_flags */
enum
{
    RXSNDDRV_VOICE_ACTIVE   = 0x01,     /* allocated: the tick runs it */
    RXSNDDRV_VOICE_OWNED    = 0x02,     /* an owner holds it (and its callback) */
    RXSNDDRV_VOICE_KEYED    = 0x04,     /* keyed on */
    RXSNDDRV_VOICE_RELEASED = 0x08,     /* fading out, freed at fade 0 */
    RXSNDDRV_VOICE_AUTOFREE = 0x10,     /* released when the envelope ends */
};

/* m_pending: what the next tick writes. */
enum
{
    RXSNDDRV_VOICE_SET_SAMPLE = 0x01,   /* SSA, ADSR1, ADSR2 */
    RXSNDDRV_VOICE_SET_LOOP   = 0x02,   /* LSAX */
    RXSNDDRV_VOICE_SET_SWITCH = 0x04,   /* m_switch into the per-core switches */
    RXSNDDRV_VOICE_SET_VOLUME = 0x08,   /* VOLL / VOLR */
    RXSNDDRV_VOICE_SET_PITCH  = 0x10,   /* PITCH */
    RXSNDDRV_VOICE_SET_NOISE  = 0x20,   /* FS2: the core's noise clock (RxSndDrvVoiceSetNoise) */
};

/* A voice (0x3c on the IOP in FS2, RoR's 0x38: m_noise at +0x24 moved the
 * rest up by 4).  layout: host -- m_done / m_arg are pointers; the IOP
 * offsets (RoR's) are in the comments and asserted up to m_envx. */
struct RxSndDrvVoice
{
    int     m_priority;                 // IOP +0x00: a steal takes the largest
    int     m_age;                      // IOP +0x04: ticks since the allocation
    u_char  m_no;                       // IOP +0x08: its own index
    u_char  m_flags;                    // IOP +0x09: RXSNDDRV_VOICE_*
    u_char  m_quiet;                    // IOP +0x0a: ticks with ENVX 0 (auto-free)
    u_char  m_fade;                     // IOP +0x0b: volume scale, 0x80 full
    u_char  m_pending;                  // IOP +0x0c: RXSNDDRV_VOICE_SET_*
    u_char  m_switch;                   // IOP +0x0d: RxSndDrvVoiceSetSwitch bits
    short   m_pitch;                    // IOP +0x0e
    u_int   m_ssa;                      // IOP +0x10: SPU address
    u_int   m_lsax;                     // IOP +0x14: SPU address
    u_short m_adsr1;                    // IOP +0x18
    u_short m_adsr2;                    // IOP +0x1a
    int     m_vol[2];                   // IOP +0x1c: L, R in register units << 7
    int     m_envx;                     // IOP +0x24: read every tick
    RxSndDrvVoiceDone m_done;           // IOP +0x28
    void   *m_arg;                      // IOP +0x2c
    int     m_enable;                   // IOP +0x30: allocatable (command 0x91)
    int     m_owner;                    // IOP +0x34: an allocation by the same owner takes it
    int     m_noise;                    // FS2 IOP +0x24: the noise clock RxSndDrvVoiceSetNoise sets
};
static_assert(offsetof(RxSndDrvVoice, m_no) == 0x08, "RxSndDrvVoice::m_no");
static_assert(offsetof(RxSndDrvVoice, m_pending) == 0x0c, "RxSndDrvVoice::m_pending");
static_assert(offsetof(RxSndDrvVoice, m_pitch) == 0x0e, "RxSndDrvVoice::m_pitch");
static_assert(offsetof(RxSndDrvVoice, m_ssa) == 0x10, "RxSndDrvVoice::m_ssa");
static_assert(offsetof(RxSndDrvVoice, m_adsr1) == 0x18, "RxSndDrvVoice::m_adsr1");
static_assert(offsetof(RxSndDrvVoice, m_vol) == 0x1c, "RxSndDrvVoice::m_vol");
static_assert(offsetof(RxSndDrvVoice, m_envx) == 0x24, "RxSndDrvVoice::m_envx");

/* The voice state (0xab8 at RoR IOP 0x1aa80, cleared as one block).
 * layout: host (holds RxSndDrvVoice); the IOP offsets are in the comments. */
struct RxSndDrvVoiceState
{
    int           m_stereo;             // IOP +0x000: output mode, 0 mono (RxSndDrvVoiceSetOutputMode)
    RxSndDrvVoice m_voice[RXSNDDRV_VOICES]; // IOP +0x004
    int           m_next[3];            // IOP +0xa84: where each group's search starts
    u_int         m_exclude[3][2];      // IOP +0xa90: per group and core, voices it may not take
    u_int         m_keyMask[2];         // IOP +0xaa8: per core, voices keyed on or off this tick
    u_int         m_keyOn[2];           // IOP +0xab0: of those, the ones keyed on
};

static RxSndDrvVoiceState s_voice;      // RoR IOP 0x0001aa80

/* PC: RxSndDrvVoiceSetMixGroup. */
static int s_mixGroup = YAMIPAN_VOICE_GROUP_MUSIC;

/* The voice defaults (RoR IOP 0xde30): no voice mixed, no noise, no pitch
 * modulation on either core; then volume, pitch and both addresses of voice
 * 31 on each core.  There is no voice 31 (24 a core): kept as the ROM has it,
 * the host libsd ignores voice numbers past 23. */
static sceSdBatch s_voiceDefaults[0x16] = {
    { SD_BATCH_SETSWITCH, SD_S_VMIXL | SD_CORE_0, 0 },
    { SD_BATCH_SETSWITCH, SD_S_VMIXR | SD_CORE_0, 0 },
    { SD_BATCH_SETSWITCH, SD_S_VMIXEL | SD_CORE_0, 0 },
    { SD_BATCH_SETSWITCH, SD_S_VMIXER | SD_CORE_0, 0 },
    { SD_BATCH_SETSWITCH, SD_S_NON | SD_CORE_0, 0 },
    { SD_BATCH_SETSWITCH, SD_S_PMON | SD_CORE_0, 0 },
    { SD_BATCH_SETSWITCH, SD_S_VMIXL | SD_CORE_1, 0 },
    { SD_BATCH_SETSWITCH, SD_S_VMIXR | SD_CORE_1, 0 },
    { SD_BATCH_SETSWITCH, SD_S_VMIXEL | SD_CORE_1, 0 },
    { SD_BATCH_SETSWITCH, SD_S_VMIXER | SD_CORE_1, 0 },
    { SD_BATCH_SETSWITCH, SD_S_NON | SD_CORE_1, 0 },
    { SD_BATCH_SETSWITCH, SD_S_PMON | SD_CORE_1, 0 },
    { SD_BATCH_SETPARAM, SD_VP_VOLL | 0x3e, 0 },
    { SD_BATCH_SETPARAM, SD_VP_VOLR | 0x3e, 0 },
    { SD_BATCH_SETPARAM, SD_VP_PITCH | 0x3e, 0 },
    { SD_BATCH_SETPARAM, SD_VP_VOLL | 0x3f, 0 },
    { SD_BATCH_SETPARAM, SD_VP_VOLR | 0x3f, 0 },
    { SD_BATCH_SETPARAM, SD_VP_PITCH | 0x3f, 0 },
    { SD_BATCH_SETADDR, SD_VA_SSA | 0x3e, 0 },
    { SD_BATCH_SETADDR, SD_VA_LSAX | 0x3e, 0 },
    { SD_BATCH_SETADDR, SD_VA_SSA | 0x3f, 0 },
    { SD_BATCH_SETADDR, SD_VA_LSAX | 0x3f, 0 },
};

static int VoiceCore(int voice)
{
    return voice >= 24;
}

static int VoiceChannel(int voice)
{
    return voice >= 24 ? voice - 24 : voice;
}

/* The libsd voice key: core in bit 0, channel in bits 1..5. */
static u_short VoiceEntry(int voice)
{
    return (u_short)(VoiceCore(voice) | VoiceChannel(voice) << 1);
}

static int VoiceClamp(int v)
{
    if (v > 0x3fff)
        return 0x3fff;
    if (v < -0x4000)
        return -0x4000;
    return v;
}

/* Group `group` may not take `voice` (group 0 takes any). */
static int VoiceExcluded(int group, int voice)
{
    return group > 0 && (s_voice.m_exclude[group][VoiceCore(voice)] & 1u << VoiceChannel(voice)) != 0;
}

// IOP 0x000037c0 (RoR 0x000035c0, 97% of it aligned: FS2 read, the noise clock)
void RxSndDrvVoiceTick(void)
{
    u_int sw[6][2];                     /* VMIXL, VMIXR, VMIXEL, VMIXER, NON, PMON per core */

    for (int core = 0; core < 2; core++)
    {
        sw[0][core] = sceSdGetSwitch((u_short)(core | SD_S_VMIXL));
        sw[1][core] = sceSdGetSwitch((u_short)(core | SD_S_VMIXR));
        sw[2][core] = sceSdGetSwitch((u_short)(core | SD_S_VMIXEL));
        sw[3][core] = sceSdGetSwitch((u_short)(core | SD_S_VMIXER));
        sw[4][core] = sceSdGetSwitch((u_short)(core | SD_S_NON));
        sw[5][core] = sceSdGetSwitch((u_short)(core | SD_S_PMON));
    }

    for (int v = 0; v < RXSNDDRV_VOICES; v++)
    {
        RxSndDrvVoice *p     = &s_voice.m_voice[v];
        u_short        entry = VoiceEntry(v);

        p->m_envx = sceSdGetParam((u_short)(entry | SD_VP_ENVX));
        if (!(p->m_flags & RXSNDDRV_VOICE_ACTIVE))
            continue;

        if (p->m_flags & RXSNDDRV_VOICE_RELEASED)
        {
            p->m_fade -= 0x10;
            p->m_pending |= RXSNDDRV_VOICE_SET_VOLUME;
        }

        u_char pending = p->m_pending;
        if (pending != 0)
        {
            if (pending & RXSNDDRV_VOICE_SET_SAMPLE)
            {
                sceSdSetAddr((u_short)(entry | SD_VA_SSA), p->m_ssa);
                sceSdSetParam((u_short)(entry | SD_VP_ADSR1), p->m_adsr1);
                sceSdSetParam((u_short)(entry | SD_VP_ADSR2), p->m_adsr2);
            }
            if (pending & RXSNDDRV_VOICE_SET_LOOP)
                sceSdSetAddr((u_short)(entry | SD_VA_LSAX), p->m_lsax);
            if (pending & RXSNDDRV_VOICE_SET_SWITCH)
            {
                int   core = VoiceCore(v);
                u_int bit  = 1u << VoiceChannel(v);
                u_int dry  = (p->m_switch & 1) ? bit : 0;
                u_int wet  = (p->m_switch & 2) ? bit : 0;
                u_int non  = (p->m_switch & 4) ? bit : 0;
                u_int pmon = (p->m_switch & 8) ? bit : 0;

                sw[0][core] = (sw[0][core] & ~bit) | dry;
                sw[1][core] = (sw[1][core] & ~bit) | dry;
                sw[2][core] = (sw[2][core] & ~bit) | wet;
                sw[3][core] = (sw[3][core] & ~bit) | wet;
                sw[4][core] = (sw[4][core] & ~bit) | non;
                sw[5][core] = (sw[5][core] & ~bit) | pmon;
            }
            if (pending & RXSNDDRV_VOICE_SET_VOLUME)
            {
                int l = p->m_vol[0];
                int r = p->m_vol[1];

                if (s_voice.m_stereo == 0)
                {
                    if (l < 0)
                        l = -l;
                    if (r < 0)
                        r = -r;
                    l = (l + r) >> 1;
                    r = l;
                }
                l = VoiceClamp((l * (int)p->m_fade) >> 7);
                r = VoiceClamp((r * (int)p->m_fade) >> 7);
                sceSdSetParam((u_short)(entry | SD_VP_VOLL), (u_short)(l & 0x7fff));
                sceSdSetParam((u_short)(entry | SD_VP_VOLR), (u_short)(r & 0x7fff));
            }
            if (pending & RXSNDDRV_VOICE_SET_PITCH)
                sceSdSetParam((u_short)(entry | SD_VP_PITCH), (u_short)p->m_pitch);
            /* FS2: the noise clock is the core's, so the last noise voice set wins. */
            if (pending & RXSNDDRV_VOICE_SET_NOISE)
                sceSdSetCoreAttr((u_short)(VoiceCore(v) | SD_C_NOISE_CLK), (u_short)p->m_noise);
            p->m_pending = 0;
        }

        if (p->m_flags & RXSNDDRV_VOICE_AUTOFREE)
        {
            if (p->m_envx != 0)
                p->m_quiet = 0;
            else if (++p->m_quiet >= 9)
            {
                RxSndDrvVoiceRelease(p->m_no, 0, 0);
                RxSndDrvVoiceFree(p->m_no);
                continue;
            }
        }

        if ((p->m_flags & RXSNDDRV_VOICE_RELEASED) && p->m_fade == 0)
            RxSndDrvVoiceFree(p->m_no);
        else
            p->m_age++;
    }

    for (int core = 0; core < 2; core++)
    {
        sceSdSetSwitch((u_short)(core | SD_S_VMIXL), sw[0][core]);
        sceSdSetSwitch((u_short)(core | SD_S_VMIXR), sw[1][core]);
        sceSdSetSwitch((u_short)(core | SD_S_VMIXEL), sw[2][core]);
        sceSdSetSwitch((u_short)(core | SD_S_VMIXER), sw[3][core]);
        sceSdSetSwitch((u_short)(core | SD_S_NON), sw[4][core]);
        sceSdSetSwitch((u_short)(core | SD_S_PMON), sw[5][core]);
    }

    for (int core = 0; core < 2; core++)
    {
        u_int on  = s_voice.m_keyMask[core] & s_voice.m_keyOn[core];
        u_int off = s_voice.m_keyMask[core] & ~s_voice.m_keyOn[core];

        if (on != 0)
            sceSdSetSwitch((u_short)(core | SD_S_KON), on);
        if (off != 0)
            sceSdSetSwitch((u_short)(core | SD_S_KOFF), off);
        s_voice.m_keyMask[core] = 0;
        s_voice.m_keyOn[core]   = 0;
    }
}

/* Takes voice `voice` (< 0: the first free one of `group`, else the one with
 * the largest priority, then the oldest) for `owner`; first every voice
 * `owner` (> 0) already holds is taken back.  Answers the voice, or -1 when
 * the group has none. */
// IOP 0x00003da0 (RoR 0x00003b80, 92% of it aligned: FS2 differs, unread)
int RxSndDrvVoiceAlloc(int voice, int priority, int autoFree, int group, int owner,
                       RxSndDrvVoiceDone done, void *arg)
{
    if (group > 2)
        group = 2;
    if (group < 0)
        group = 0;

    if (owner > 0)
    {
        for (int v = 0; v < RXSNDDRV_VOICES; v++)
        {
            RxSndDrvVoice *p = &s_voice.m_voice[v];
            if ((p->m_flags & RXSNDDRV_VOICE_OWNED) && p->m_owner == owner)
                RxSndDrvVoiceRelease(v, 1, 0);
        }
    }

    if (voice < 0)
    {
        int n;

        voice = s_voice.m_next[group];
        for (n = 0; n < RXSNDDRV_VOICES; n++)
        {
            RxSndDrvVoice *p = &s_voice.m_voice[voice];

            if (p->m_enable != 0 && !VoiceExcluded(group, voice) &&
                !(p->m_flags & RXSNDDRV_VOICE_OWNED))
            {
                s_voice.m_next[group] = voice + 1 < RXSNDDRV_VOICES ? voice + 1 : 0;
                break;
            }
            voice = voice + 1 < RXSNDDRV_VOICES ? voice + 1 : 0;
        }

        if (n == RXSNDDRV_VOICES)
        {
            int bestPriority = 0;
            int bestAge      = 0;

            voice = -1;
            for (int v = 0; v < RXSNDDRV_VOICES; v++)
            {
                RxSndDrvVoice *p = &s_voice.m_voice[v];

                if (p->m_enable == 0 || VoiceExcluded(group, v))
                    continue;
                if (voice == -1 || bestPriority < p->m_priority ||
                    (bestPriority == p->m_priority && bestAge < p->m_age))
                {
                    bestPriority = p->m_priority;
                    bestAge      = p->m_age;
                    voice        = v;
                }
            }
            if (voice == -1)
                return -1;
        }
    }

    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    if (p->m_flags & RXSNDDRV_VOICE_OWNED)
        RxSndDrvVoiceRelease(voice, 1, 0);
    p->m_priority = priority;
    p->m_age      = 0;
    p->m_flags    = autoFree == 0 ? RXSNDDRV_VOICE_ACTIVE | RXSNDDRV_VOICE_OWNED
                                  : RXSNDDRV_VOICE_ACTIVE | RXSNDDRV_VOICE_OWNED | RXSNDDRV_VOICE_AUTOFREE;
    p->m_quiet    = 0;
    p->m_fade     = 0x80;
    p->m_done     = done;
    p->m_owner    = owner;
    p->m_pending &= ~RXSNDDRV_VOICE_SET_LOOP;
    p->m_arg      = arg;
    /* PC: which of the player's volumes the host mixer applies to it. */
    YamiPan_VoiceSetGroup(VoiceCore(voice), VoiceChannel(voice), s_mixGroup);
    return voice;
}

// IOP 0x000040d8 (RoR 0x00003eb8, 95% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceInit(void)
{
    memset(&s_voice, 0, sizeof(s_voice));
    sceSdProcBatch(s_voiceDefaults, NULL, 0x16);
    s_voice.m_stereo = 1;
    for (int v = 0; v < RXSNDDRV_VOICES; v++)
    {
        s_voice.m_voice[v].m_no     = (u_char)v;
        s_voice.m_voice[v].m_enable = 1;
    }
    s_voice.m_next[1]       = 0;
    s_voice.m_next[2]       = 0;
    s_voice.m_exclude[1][0] = 0;            /* group 1: core 0 only */
    s_voice.m_exclude[1][1] = 0xffffff;
    s_voice.m_exclude[2][0] = 0xffffff;     /* group 2: core 1 only */
    s_voice.m_exclude[2][1] = 0;
}

/* System parameter 2 (sys.cpp): 0 mono, else stereo; every active voice's
 * volume is written again. */
// IOP 0x0000418c (RoR 0x00003f6c, 95% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceSetOutputMode(int mode)
{
    if (s_voice.m_stereo == mode)
        return;
    s_voice.m_stereo = mode;
    for (int v = 0; v < RXSNDDRV_VOICES; v++)
    {
        RxSndDrvVoice *p = &s_voice.m_voice[v];
        if (p->m_flags & RXSNDDRV_VOICE_ACTIVE)
            p->m_pending |= RXSNDDRV_VOICE_SET_VOLUME;
    }
}

/* The owner lets the voice go: keyed off, fading out unless `noFade`, and its
 * callback told {voice, reason}. */
// IOP 0x000041ec (RoR 0x00003fcc, 90% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceRelease(int voice, int reason, int noFade)
{
    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    if ((p->m_flags & (RXSNDDRV_VOICE_ACTIVE | RXSNDDRV_VOICE_OWNED)) !=
        (RXSNDDRV_VOICE_ACTIVE | RXSNDDRV_VOICE_OWNED))
        return;
    if (p->m_flags & RXSNDDRV_VOICE_KEYED)
        RxSndDrvVoiceKeyOff(voice);
    if (noFade == 0)
        p->m_flags |= RXSNDDRV_VOICE_RELEASED;
    if (p->m_done != NULL)
    {
        p->m_done(voice, reason, p->m_arg);
        p->m_done = NULL;
        p->m_arg  = NULL;
    }
    p->m_flags &= ~RXSNDDRV_VOICE_OWNED;
}

// IOP 0x000042bc (RoR 0x0000409c, 84% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceSetSample(int voice, u_int ssa, u_short adsr1, u_short adsr2)
{
    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    p->m_ssa      = ssa;
    p->m_adsr1    = adsr1;
    p->m_adsr2    = adsr2;
    p->m_pending |= RXSNDDRV_VOICE_SET_SAMPLE;
}

// IOP 0x000042f0 (RoR 0x000040d0, 81% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceSetLoop(int voice, u_int lsax)
{
    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    p->m_lsax     = lsax;
    p->m_pending |= RXSNDDRV_VOICE_SET_LOOP;
}

/* Bits: 1 dry (VMIXL / VMIXR), 2 reverb (VMIXEL / VMIXER), 4 noise, 8 pitch
 * modulation. */
// IOP 0x0000431c (RoR 0x000040fc, 81% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceSetSwitch(int voice, u_char bits)
{
    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    p->m_switch   = bits;
    p->m_pending |= RXSNDDRV_VOICE_SET_SWITCH;
}

/* L / R as 0..0x7f. */
// IOP 0x00004348 (RoR 0x00004128, 85% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceSetVolume(int voice, int l, int r)
{
    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    p->m_vol[0]   = l << 7;
    p->m_vol[1]   = r << 7;
    p->m_pending |= RXSNDDRV_VOICE_SET_VOLUME;
}

/* L / R in register units. */
// IOP 0x00004380 (RoR 0x00004160, 83% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceSetVolumeRaw(int voice, int l, int r)
{
    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    p->m_vol[0]   = l;
    p->m_vol[1]   = r;
    p->m_pending |= RXSNDDRV_VOICE_SET_VOLUME;
}

/* The pitch of `note` + `fine` for a sample recorded at `center` +
 * `centerFine`, times `scale` / 0x1000. */
// IOP 0x000043b0 (RoR 0x00004190, 93% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceSetPitch(int voice, u_short center, u_short centerFine, u_short note, short fine,
                           int scale)
{
    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    p->m_pending |= RXSNDDRV_VOICE_SET_PITCH;
    p->m_pitch    = (short)((int)(sceSdNote2Pitch(center, centerFine, note, fine) * scale) >> 12);
}

/* FS2 only: the noise clock (0..0x3f) for the core of `voice`, written by
 * the next tick (RxSndDrvNoteTick passes a noise region's centre note). */
// IOP 0x00004428 (FS2 only, read 2026-11-09)
void RxSndDrvVoiceSetNoise(int voice, int clock)
{
    s_voice.m_voice[voice].m_noise    = clock;
    s_voice.m_voice[voice].m_pending |= RXSNDDRV_VOICE_SET_NOISE;
}

// IOP 0x00004454 (RoR 0x00004208, the same; FS2 put RxSndDrvVoiceSetNoise before it)
void RxSndDrvVoiceKeyOn(int voice)
{
    int   core = VoiceCore(voice);
    u_int bit  = 1u << VoiceChannel(voice);

    s_voice.m_voice[voice].m_flags |= RXSNDDRV_VOICE_KEYED;
    s_voice.m_keyMask[core] |= bit;
    s_voice.m_keyOn[core]   |= bit;
}

// IOP 0x000044e8 (RoR 0x0000429c, 95% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceKeyOff(int voice)
{
    RxSndDrvVoice *p = &s_voice.m_voice[voice];

    if (p->m_flags & RXSNDDRV_VOICE_KEYED)
    {
        int   core = VoiceCore(voice);
        u_int bit  = 1u << VoiceChannel(voice);

        p->m_flags &= ~RXSNDDRV_VOICE_KEYED;
        s_voice.m_keyMask[core] |= bit;
        s_voice.m_keyOn[core]   &= ~bit;
    }
}

// IOP 0x0000458c (RoR 0x00004340, word for word)
u_short RxSndDrvVoiceGetEnvx(int voice)
{
    return sceSdGetParam((u_short)(VoiceEntry(voice) | SD_VP_ENVX));
}

/* Command 0x90: every voice's ENVX. */
// IOP 0x000045d0 (RoR 0x00004384, 83% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceGetEnvxAll(u_char *reply)
{
    u_int *out = (u_int *)reply;

    for (int v = 0; v < RXSNDDRV_VOICES; v++)
        out[v] = (u_int)s_voice.m_voice[v].m_envx;
}

/* Command 0x93: one field of every voice -- 0 ENVX, 1 volume {L, R} as
 * 0..0x7f halves, 2 pitch, 3 switch bits, 4 priority, 5 {ADSR1, ADSR2}. */
// IOP 0x00004600 (RoR 0x000043b4, 95% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceGetTable(u_int kind, u_char *reply)
{
    u_int *out = (u_int *)reply;

    for (int v = 0; v < RXSNDDRV_VOICES; v++)
    {
        RxSndDrvVoice *p = &s_voice.m_voice[v];

        switch (kind)
        {
        case 0:
            out[v] = (u_int)p->m_envx;
            break;
        case 1:
            out[v] = (u_int)(p->m_vol[0] >> 7) << 16 | ((u_int)(p->m_vol[1] >> 7) & 0xffff);
            break;
        case 2:
            out[v] = (u_int)(int)p->m_pitch;
            break;
        case 3:
            out[v] = p->m_switch;
            break;
        case 4:
            out[v] = (u_int)p->m_priority;
            break;
        case 5:
            out[v] = (u_int)p->m_adsr1 << 16 | p->m_adsr2;
            break;
        default:
            break;
        }
    }
}

/* The voice's play position: NAX read until two reads agree (4 tries). */
// IOP 0x000046b4 (RoR 0x00004468, word for word)
u_int RxSndDrvVoiceGetNax(int voice)
{
    u_short entry = (u_short)(VoiceEntry(voice) | SD_VA_NAX);
    u_int   a, b;
    int     n = 0;

    do
    {
        a = sceSdGetAddr(entry);
        b = sceSdGetAddr(entry);
        n++;
    } while (a != b && n < 4);
    return a;
}

/* Command 0x91 {voice, enable}. */
// IOP 0x00004768 (RoR 0x0000451c, 77% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceSetEnable(u_int voice, u_int enable)
{
    s_voice.m_voice[voice].m_enable = (int)enable;
}

/* Command 0x92: every voice's flags. */
// IOP 0x0000478c (RoR 0x00004540, 91% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceGetFlags(u_char *reply)
{
    u_int *out = (u_int *)reply;

    for (int v = 0; v < RXSNDDRV_VOICES; v++)
        out[v] = s_voice.m_voice[v].m_flags;
}

// IOP 0x000047bc (RoR 0x00004570, 87% of it aligned: FS2 differs, unread)
void RxSndDrvVoiceFree(int voice)
{
    RxSndDrvVoiceKeyOff(voice);
    s_voice.m_voice[voice].m_flags = 0;
}

/* PC: every allocation happens on the tick thread, in the BGM, SE or ADPCM
 * tick (RxSndDrvTickThread), so the tick in progress is the group. */
void RxSndDrvVoiceSetMixGroup(int group)
{
    s_mixGroup = group;
}
