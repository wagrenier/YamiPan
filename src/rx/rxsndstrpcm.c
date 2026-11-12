/* ==========================================================================
 *  rx/rxsndstrpcm.c
 *
 *  The PCM stream format (FS2 0x164398..0x164c38, table 0x2f5f18): feeds an
 *  rxSndPcm stream from a stream source. The twin of rxsndstradpcm.c, cut
 *  down: one or two channels, no tracks, no pitch, no attribute, and writes
 *  of any size (no block rounding, the start is not aligned). Open turns the
 *  request's marks into the source's tags, its branches and its end into the
 *  source's markers (a looping request ends in a jump back); update applies
 *  the slot's pause and volume, then writes whatever the source has and the
 *  IOP can take.
 *
 *  FS2-only (not in RoR, no symbols): every name was read from the code
 *  (batch 008); the messages say "rxsndstr". See rxsndstrpcm.h.
 *
 *  PC: RXSNDDRV answers rxSndPcmUpdate (sdk/sif.cpp boots it). Should it not
 *  run, nothing would: the ring would stay full and the stream would never end.
 *  The block "the driver's silent side" below stands for the driver then, as
 *  in rxsndstradpcm.c: it answers as RXSNDDRV would, from the request's length.
 * ======================================================================== */
#include "rx/rxsndstrpcm.h"
#include "sifcmd.h"     /* PC: YamiPan_IopSoundDriverRuns */

#include "rx/rxsnd.h"
#include "rx/rxsndstrbank.h"
#include "rx/rxsndstrstream.h"
#include "rx/rxsndut.h"
#include "rx/rxsys.h"

#include <malloc.h>     /* alloca: the ROM's stack arrays, as many as the request has */
#include <stddef.h>
#include <time.h>

/* rxSndPcmOpen's modes (RxSndPcm::m_mode). */
enum
{
    kPcmModeMono   = 0,                 /* blocks of 0x800 bytes */
    kPcmModeStereo = 2,                 /* blocks of 0x1000 bytes, interleaved */
};

static RxSndStrPcmStream  s_streams[RXSNDSTR_PCM_STREAM_MAX];   /* bss 0x5217d0 */
static RxSndStrPcmStream *s_active;                             /* bss 0x521ed0 */
static RxSndStrPcmStream *s_free;                               /* bss 0x521ed4 */

/* ---- PC: the driver's silent side ---------------------------------------
 *
 * Not FS2's code. RXSNDDRV reports each open stream every frame; this does,
 * through the same rxSndPcmUpdate. A stream is silent, but it lasts as long
 * as its request (a looping one never ends, as in the game) and its position
 * runs, in wall time, as a real voice would, so the position callbacks (the
 * request's marks) fire on time. Paused like the slot. The IOP's ring is
 * reported drained, so the source is read as fast as it comes.
 *
 * The samples are taken as 16-bit (the SPU2's PCM input): a second is
 * rate * 2 bytes per channel. A guess until the IOP side (RXSNDDRV's PCM
 * path) is read.
 */

enum
{
    kPcmSampleBytes    = 2,
    kPcmMonoBlock      = 0x800,         /* rxSndPcmOpen's block sizes */
    kPcmStereoBlock    = 0x1000,
};

// layout: host
typedef struct PcmSilentVoice
{
    double  secondsLeft;                /* below 0: loops */
    double  secondsPlayed;
    double  bytesPerSecond;
    int     stereo;
    clock_t lastTick;
} PcmSilentVoice;

static PcmSilentVoice s_silentVoices[RXSNDSTR_PCM_STREAM_MAX];

static void pcmSilentStart(const RxSndStrPcmStream *stream, const RxSndStrRequest *request)
{
    PcmSilentVoice *voice = &s_silentVoices[stream->id];
    u_int           start = stream->slot->start;
    u_int           bytes = start < request->size ? request->size - start : 0;

    voice->stereo         = request->channels == 2;
    voice->bytesPerSecond = (double)request->rate * kPcmSampleBytes * request->channels;
    voice->secondsPlayed  = 0.0;
    voice->lastTick       = clock();
    voice->secondsLeft    = voice->bytesPerSecond > 0.0 ? bytes / voice->bytesPerSecond : 0.0;
    if (request->flags & RXSNDSTR_REQUEST_LOOP)
        voice->secondsLeft = -1.0;
}

static void pcmSilentReply(void)
{
    RxSndPcmStatus reply;
    clock_t        now = clock();

    reply.m_count = 0;
    for (RxSndStrPcmStream *stream = s_active; stream != NULL; stream = stream->next)
    {
        PcmSilentVoice      *voice   = &s_silentVoices[stream->id];
        const RxSndStrSlot  *slot    = stream->slot;
        double               elapsed = (double)(now - voice->lastTick) / CLOCKS_PER_SEC;
        RxSndPcmStatusEntry *entry   = &reply.m_entry[reply.m_count++];

        voice->lastTick = now;
        if (!slot->pause)
        {
            voice->secondsPlayed += elapsed;
            if (voice->secondsLeft > 0.0)
            {
                voice->secondsLeft -= elapsed;
                if (voice->secondsLeft < 0.0)
                    voice->secondsLeft = 0.0;
            }
        }

        /* The position as rxSndPcmAddCallback counts it: (block, offset in
         * the block; stereo per channel). */
        u_int bytes     = (u_int)(voice->secondsPlayed * voice->bytesPerSecond);
        u_int blockSize = voice->stereo ? kPcmStereoBlock : kPcmMonoBlock;
        entry->m_id     = (u_char)stream->pcm;
        entry->m_status = voice->secondsLeft == 0.0 ? 4 : slot->pause ? 8 : 2;
        entry->m_2      = 0;
        entry->m_posHi  = (int)(bytes / blockSize);
        entry->m_posLo  = (int)(voice->stereo ? bytes % blockSize / 2 : bytes % blockSize);
    }
    rxSndPcmUpdate(&reply);
}

/* ---- the format ---------------------------------------------------------- */

// FS2 0x00164398
void rxSndStrPcmInit(void)
{
    s_active = NULL;
    s_free   = NULL;
    for (int i = 0; i < RXSNDSTR_PCM_STREAM_MAX; i++)
    {
        s_streams[i].next = s_free;
        s_free            = &s_streams[i];
        s_streams[i].id   = i;
    }
}

// FS2 0x001643e0
void rxSndStrPcmExit(void)
{
    while (s_active != NULL)
        rxSndStrPcmClose(s_active->id, 0);
}

// PC: the user data is the stream's id, not the ROM's slot pointer (rxSndPcm
// keeps an int).
// FS2 0x00164430
static void rxSndStrPcmMarkCallback(int pcm, int mark)
{
    rxSndStrFmtCallMarkCallback(s_streams[rxSndPcmGetUserData(pcm)].slot, mark);
}

// FS2 0x00164460
static int rxSndStrPcmBranchCallback(void *slot, RxFileMarkerDef *marker)
{
    return rxSndStrFmtCallBranchCallback((RxSndStrSlot *)slot, marker->user);
}

// What the source has that the IOP can take; each write's tags become
// position callbacks. (Inlined in rxSndStrPcmUpdate in the ROM; its tag lists
// stay on the stack until the update returns, here too.)
static void rxSndStrPcmFeed(RxSndStrPcmStream *stream, RxSndStrSlot *slot)
{
    for (;;)
    {
        int size = rxSndStrStreamGetReadable(stream->source, rxSndPcmGetFree(stream->pcm));
        if (size <= 0)
            break;

        int           tagCount = rxSndStrStreamGetRingTags(stream->source, NULL, 0, size);
        RxFileTagHit *tags     = (RxFileTagHit *)alloca(tagCount * sizeof(RxFileTagHit));
        rxSndStrStreamGetRingTags(stream->source, tags, tagCount, size);
        for (int i = 0; i < tagCount; i++)
            rxSndPcmAddCallback(stream->pcm, tags[i].offset, rxSndStrPcmMarkCallback, tags[i].value);

        rxSndPcmWrite(stream->pcm, rxSndStrStreamGetData(stream->source), NULL, size);
        rxSndStrStreamConsume(stream->source, size);
        if (rxSndStrStreamIsEnd(stream->source))
            rxSndPcmEnd(stream->pcm);
        slot->dataWritten = 1;
    }
    slot->filled = rxSndStrStreamGetFilled(stream->source);
}

// callcheck: rxSndPcmAddCallback, rxSndPcmEnd, rxSndPcmGetFree, rxSndPcmWrite, rxSndStrStreamConsume, rxSndStrStreamGetData, rxSndStrStreamGetFilled, rxSndStrStreamGetReadable, rxSndStrStreamGetRingTags, rxSndStrStreamIsEnd (rxSndStrPcmFeed, inline)
// FS2 0x00164478
void rxSndStrPcmUpdate(void)
{
    if (s_active == NULL)
        return;
    if (!YamiPan_IopSoundDriverRuns())
        pcmSilentReply();   /* PC: the driver's report */

    for (RxSndStrPcmStream *stream = s_active; stream != NULL; stream = stream->next)
    {
        RxSndStrSlot *slot = stream->slot;

        if (slot->dirty & RxSndStrDirty_Pause)
            rxSndPcmPause(stream->pcm, slot->pause);
        if (slot->dirty & RxSndStrDirty_Volume)
        {
            int volL    = slot->volL;
            int volR    = slot->volR;
            int masterL = rxSndStrFmtGetMasterVolumeL();
            int masterR = rxSndStrFmtGetMasterVolumeR();
            rxSndPcmSetVolume(stream->pcm, masterL * ((volL * stream->volL) >> 7) >> 7,
                              masterR * ((volR * stream->volR) >> 7) >> 7);
        }
        slot->dirty = 0;    /* the attribute, pitch and track settings are dropped */

        u_int status = rxSndPcmGetStatus(stream->pcm);
        slot->status = (status & RXSNDSTR_STATUS_PLAYING) ? slot->status | RXSNDSTR_STATUS_PLAYING
                                                          : slot->status & ~RXSNDSTR_STATUS_PLAYING;
        slot->status = (status & RXSNDSTR_STATUS_PAUSED) ? slot->status | RXSNDSTR_STATUS_PAUSED
                                                         : slot->status & ~RXSNDSTR_STATUS_PAUSED;
        if (status & RXSNDSTR_STATUS_END)
            slot->status |= RXSNDSTR_STATUS_END;
        else
            rxSndStrPcmFeed(stream, slot);
    }
}

// FS2 0x001646b8
// A failed open leaves the stream on the free list. Kept from the ROM: it then
// closes the stream's source and rxSndPcm stream, also when this open never
// set them, so they are what the stream's last user left.
int rxSndStrPcmOpen(RxSndStrSlot *slot)
{
    RxSndStrPcmStream     *stream = s_free;
    const RxSndStrRequest *request;
    RxFileTagDef          *tags;
    RxFileMarkerDef       *markers;
    u_int                  iopAddr;
    int                    mode;
    int                    markCount;
    int                    branchCount;

    if (stream == NULL)
    {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : empty PCM slot\n");
        goto fail;
    }

    request = rxSndStrBankGetRequest(slot->bank, slot->request);
    if (request == NULL)
    {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : unknow format bank or request '%d, %d'\n", slot->bank,
                        slot->request);
        goto fail;
    }

    if (request->channels == 1)
    {
        mode = kPcmModeMono;
    }
    else if (request->channels == 2)
    {
        mode = kPcmModeStereo;
    }
    else
    {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : unsupported channel number '%d'\n", request->channels);
        goto fail;
    }

    stream->source = rxSndStrBankOpenBody(slot->bank, slot->buf, slot->bufSize);
    if (stream->source < 0)
    {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : failed to open file (memory) stream\n");
        goto fail;
    }

    /* The marks: tags in the data. */
    markCount = rxSndStrBankGetMarkCount(slot->bank, slot->request);
    if (markCount > 0)
    {
        tags = (RxFileTagDef *)alloca(markCount * sizeof(RxFileTagDef));
        for (int i = 0; i < markCount; i++)
        {
            const RxSndStrMark *mark = rxSndStrBankGetMark(slot->bank, slot->request, i);
            tags[i].pos    = request->offset + mark->offset;
            tags[i].active = mark->active;
            tags[i].id     = mark->id;
            tags[i].value  = i;
        }
        rxSndStrStreamSetTags(stream->source, tags, markCount);
    }

    /* The branches, then the end: markers. A branch's id is the low byte of
     * its position, in the ROM too. */
    branchCount = rxSndStrBankGetBranchCount(slot->bank, slot->request);
    markers     = (RxFileMarkerDef *)alloca((branchCount + 1) * sizeof(RxFileMarkerDef));
    for (int i = 0; i < branchCount; i++)
    {
        const RxSndStrMark *branch = rxSndStrBankGetBranch(slot->bank, slot->request, i);
        markers[i].pos      = request->offset + branch->offset;
        markers[i].active   = branch->active;
        markers[i].id       = (u_char)branch->offset;
        markers[i].count    = branch->count;
        markers[i].max      = branch->max;
        markers[i].jumpPos  = branch->param != -1 ? (int)request->offset + branch->param : -1;
        markers[i].callback = rxSndStrPcmBranchCallback;
        markers[i].user     = i;
    }
    markers[branchCount].pos      = request->offset + request->size;
    markers[branchCount].active   = 1;
    markers[branchCount].id       = 0;
    markers[branchCount].count    = 0;
    markers[branchCount].max      = 0;
    markers[branchCount].jumpPos  = (request->flags & RXSNDSTR_REQUEST_LOOP)
                                        ? (int)(request->offset + request->loopOffset) : -1;
    markers[branchCount].callback = NULL;
    markers[branchCount].user     = 0;
    rxSndStrStreamSetMarkers(stream->source, markers, branchCount + 1, slot);

    /* Start inside the request (any byte: unlike ADPCM, not aligned). */
    if (slot->start > request->size)
        slot->start = request->size;
    rxSndStrStreamSeek(stream->source, request->offset + slot->start);

    iopAddr = slot->iopAddr;    /* 0: rxSndPcmOpen allocates it */
    stream->pcm = rxSndPcmOpen(mode, iopAddr, slot->iopKB);
    if (stream->pcm < 0)
    {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : failed to open stream (rxsnd)\n");
        goto fail;
    }

    rxSndPcmSetUserData(stream->pcm, stream->id);   /* PC: the ROM's slot */
    stream->slot = slot;
    s_free       = stream->next;
    stream->volL = request->volL;
    stream->volR = request->volR;
    stream->attr = request->attr != 0;
    stream->next = s_active;
    s_active     = stream;
    if (!YamiPan_IopSoundDriverRuns())
        pcmSilentStart(stream, request);            /* PC: the driver's side */
    return stream->id;

fail:
    if (stream != NULL)
    {
        if (stream->source >= 0)
            rxSndStrStreamClose(stream->source, 0);
        if (stream->pcm >= 0)
            rxSndPcmClose(stream->pcm);
    }
    return -1;
}

// FS2 0x00164b30
void rxSndStrPcmClose(int handle, int async)
{
    RxSndStrPcmStream **link = &s_active;
    RxSndStrPcmStream  *stream;

    for (;;)
    {
        stream = *link;
        if (stream == NULL)
            return;
        if (stream->id == handle)
            break;
        link = &stream->next;
    }

    *link        = stream->next;
    stream->next = s_free;
    s_free       = stream;
    if (stream->source >= 0)
        rxSndStrStreamClose(stream->source, async);
    if (stream->pcm >= 0)
        rxSndPcmClose(stream->pcm);
}

// FS2 0x00164bb8
int rxSndStrPcmGetHandle(int handle)
{
    return s_streams[handle].pcm;
}

// FS2 0x00164bd8
void rxSndStrPcmPauseRead(int handle, int pause)
{
    rxSndStrStreamPause(s_streams[handle].source, pause);
}

// FS2 0x00164c08
int rxSndStrPcmSyncRead(int handle, int arg)
{
    return rxSndStrStreamSync(s_streams[handle].source, arg);
}
