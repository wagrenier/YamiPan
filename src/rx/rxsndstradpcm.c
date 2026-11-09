/* ==========================================================================
 *  rx/rxsndstradpcm.c
 *
 *  The ADPCM stream format (FS2 0x164c38..0x1656e8, table 0x2f5f38): feeds an
 *  rxSndAdpcm stream from a stream source. Open turns the request's marks
 *  into the source's tags, its branches and its end into the source's markers
 *  (a looping request ends in a jump back), and update applies the slot's
 *  dirty settings, then writes whatever the source has and the IOP can take.
 *
 *  FS2-only (not in RoR, no symbols): every name was read from the code
 *  (batch 008); the messages say "rxsndstr". See rxsndstradpcm.h.
 *
 *  PC: RXSNDDRV answers rxSndAdpcmUpdate (sdk/sif.cpp boots it). Should it not
 *  run, nothing would, the stream would never end and an event would wait for
 *  its voice; the block "the driver's silent side" below stands for the
 *  driver then: it answers as RXSNDDRV would, from the request's length.
 * ======================================================================== */
#include "rx/rxsndstradpcm.h"

#include "rx/rxsnd.h"
#include "rx/rxsndstrbank.h"
#include "rx/rxsndstrstream.h"
#include "rx/rxsndut.h"
#include "rx/rxsys.h"
#include "sifcmd.h"     /* PC: YamiPan_IopSoundDriverRuns */

#include <malloc.h>     /* alloca: the ROM's stack arrays, as many as the request has */
#include <stddef.h>
#include <time.h>

/* rxSndAdpcmOpen's modes. */
enum
{
    kAdpcmModeMono        = 0,
    kAdpcmModeInterleaved = 2,          /* stereo */
    kAdpcmModeTracks      = 0x1000000,  /* n channels interleaved; ORed with n */
};

enum
{
    kAdpcmBlockBytes   = 16,            /* PS-ADPCM: a block of 16 bytes per channel ... */
    kAdpcmBlockSamples = 28,            /* ... holds 28 samples */
};

static RxSndStrAdpcmStream  s_streams[RXSNDSTR_ADPCM_STREAM_MAX];   /* bss 0x521ed8 */
static RxSndStrAdpcmStream *s_active;                               /* bss 0x5226d8 */
static RxSndStrAdpcmStream *s_free;                                 /* bss 0x5226dc */

/* ---- PC: the driver's silent side ---------------------------------------
 *
 * Not FS2's code. RXSNDDRV reports each open stream every frame; this does,
 * through the same rxSndAdpcmUpdate. A stream is silent, but it lasts as long
 * as its request (a looping one never ends, as in the game) and its position
 * runs, in wall time, as a real voice would: the subtitles read it. Paused
 * and pitched like the slot. The IOP's buffer is reported empty.
 */

// layout: host
typedef struct AdpcmSilentVoice
{
    double  secondsLeft;                /* below 0: loops */
    double  secondsPlayed;
    clock_t lastTick;
} AdpcmSilentVoice;

static AdpcmSilentVoice s_silentVoices[RXSNDSTR_ADPCM_STREAM_MAX];

static void adpcmSilentStart(const RxSndStrAdpcmStream *stream, const RxSndStrRequest *request)
{
    AdpcmSilentVoice *voice = &s_silentVoices[stream->id];
    u_int             start = stream->slot->start;
    u_int             bytes = start < request->size ? request->size - start : 0;
    double            samples = (double)bytes / stream->blockSize * kAdpcmBlockSamples;

    voice->secondsPlayed = 0.0;
    voice->lastTick      = clock();
    voice->secondsLeft   = request->rate != 0 ? samples / request->rate : 0.0;
    if (request->flags & RXSNDSTR_REQUEST_LOOP)
        voice->secondsLeft = -1.0;
}

static void adpcmSilentReply(void)
{
    RxSndAdpcmStatus reply;
    clock_t          now = clock();

    reply.m_count = 0;
    for (RxSndStrAdpcmStream *stream = s_active; stream != NULL; stream = stream->next)
    {
        AdpcmSilentVoice      *voice = &s_silentVoices[stream->id];
        const RxSndStrSlot    *slot  = stream->slot;
        const RxSndStrRequest *request = rxSndStrBankGetRequest(slot->bank, slot->request);
        double                 elapsed = (double)(now - voice->lastTick) / CLOCKS_PER_SEC;
        RxSndAdpcmStatusEntry *entry = &reply.m_entry[reply.m_count++];

        voice->lastTick = now;
        if (!slot->pause)
        {
            voice->secondsPlayed += elapsed * slot->pitch;
            if (voice->secondsLeft > 0.0)
            {
                voice->secondsLeft -= elapsed * slot->pitch;
                if (voice->secondsLeft < 0.0)
                    voice->secondsLeft = 0.0;
            }
        }

        double samples = voice->secondsPlayed * (request != NULL ? request->rate : 0);
        entry->m_id     = (u_char)stream->adpcm;
        entry->m_status = voice->secondsLeft == 0.0 ? 4 : slot->pause ? 8 : 2;
        entry->m_free   = slot->iopKB << 10;
        entry->m_posHi  = 0;
        entry->m_posLo  = (u_int)(samples / kAdpcmBlockSamples * kAdpcmBlockBytes);
    }
    rxSndAdpcmUpdate(&reply);
}

/* ---- the format ---------------------------------------------------------- */

// FS2 0x00164c38
void rxSndStrAdpcmInit(void)
{
    s_active = NULL;
    s_free   = NULL;
    for (int i = 0; i < RXSNDSTR_ADPCM_STREAM_MAX; i++)
    {
        s_streams[i].next = s_free;
        s_free            = &s_streams[i];
        s_streams[i].id   = i;
    }
}

// FS2 0x00164c80
void rxSndStrAdpcmExit(void)
{
    while (s_active != NULL)
        rxSndStrAdpcmClose(s_active->id, 0);
}

// PC: the user data is the stream's id, not the ROM's slot pointer (rxSndAdpcm
// keeps an int).
// FS2 0x00164cd0
static void rxSndStrAdpcmMarkCallback(int adpcm, int mark)
{
    rxSndStrFmtCallMarkCallback(s_streams[rxSndAdpcmGetUserData(adpcm)].slot, mark);
}

// FS2 0x00164d00
static int rxSndStrAdpcmBranchCallback(void *slot, RxFileMarkerDef *marker)
{
    return rxSndStrFmtCallBranchCallback((RxSndStrSlot *)slot, marker->user);
}

// What the source has, whole blocks, that the IOP can take; each write's tags
// become position callbacks. (Inlined in rxSndStrAdpcmUpdate in the ROM; its
// tag lists stay on the stack until the update returns, here too.)
static void rxSndStrAdpcmFeed(RxSndStrAdpcmStream *stream, RxSndStrSlot *slot)
{
    for (;;)
    {
        int free = rxSndAdpcmGetFree(stream->adpcm);
        int size = rxSndStrStreamGetReadable(stream->source, free);
        if (size <= 0)
            break;
        size = size / stream->blockSize * stream->blockSize;
        if (size <= 0)
            break;

        int           tagCount = rxSndStrStreamGetRingTags(stream->source, NULL, 0, size);
        RxFileTagHit *tags     = (RxFileTagHit *)alloca(tagCount * sizeof(RxFileTagHit));
        rxSndStrStreamGetRingTags(stream->source, tags, tagCount, size);
        for (int i = 0; i < tagCount; i++)
            rxSndAdpcmAddCallback(stream->adpcm, tags[i].offset, rxSndStrAdpcmMarkCallback,
                                  tags[i].value);

        rxSndAdpcmWrite(stream->adpcm, rxSndStrStreamGetData(stream->source), NULL, size);
        rxSndStrStreamConsume(stream->source, size);
        if (rxSndStrStreamIsEnd(stream->source))
            rxSndAdpcmEnd(stream->adpcm);
        slot->dataWritten = 1;
    }
    slot->filled = rxSndStrStreamGetFilled(stream->source);
}

// One channel volume per track: a stereo track's two sides are two channels.
// (Inlined in rxSndStrAdpcmUpdate in the ROM.)
static void rxSndStrAdpcmApplyTrackVolumes(RxSndStrAdpcmStream *stream, RxSndStrSlot *slot)
{
    int bank       = slot->bank;
    int request    = slot->request;
    int channel    = 0;
    int trackCount = rxSndStrBankGetTrackCount(bank, request);

    for (int track = 0; track < trackCount; track++)
    {
        int volL, volR;
        rxSndStrBankGetTrackVolume(bank, request, track, &volL, &volR);
        volL = volL * slot->trackVolL[track] / RXSNDSTR_VOLUME_UNITY;
        volR = volR * slot->trackVolR[track] / RXSNDSTR_VOLUME_UNITY;

        int channels = rxSndStrBankGetTrackChannels(bank, request, track);
        if (channels == 1)
        {
            rxSndAdpcmSetChannelVolume(stream->adpcm, channel, volL, volR);
            channel++;
        }
        else if (channels == 2)
        {
            rxSndAdpcmSetChannelVolume(stream->adpcm, channel, volL, 0);
            rxSndAdpcmSetChannelVolume(stream->adpcm, channel + 1, 0, volR);
            channel += 2;
        }
    }
}

// callcheck: rxSndAdpcmAddCallback, rxSndAdpcmEnd, rxSndAdpcmGetFree, rxSndAdpcmWrite, rxSndStrStreamConsume, rxSndStrStreamGetData, rxSndStrStreamGetFilled, rxSndStrStreamGetReadable, rxSndStrStreamGetRingTags, rxSndStrStreamIsEnd (rxSndStrAdpcmFeed, inline)
// callcheck: rxSndAdpcmSetChannelVolume, rxSndStrBankGetTrackChannels, rxSndStrBankGetTrackCount, rxSndStrBankGetTrackVolume (rxSndStrAdpcmApplyTrackVolumes, inline)
// FS2 0x00164d18
void rxSndStrAdpcmUpdate(void)
{
    if (s_active == NULL)
        return;
    if (!YamiPan_IopSoundDriverRuns())
        adpcmSilentReply(); /* PC: the driver's report */

    for (RxSndStrAdpcmStream *stream = s_active; stream != NULL; stream = stream->next)
    {
        RxSndStrSlot *slot = stream->slot;

        if (slot->dirty & RxSndStrDirty_Pause)
            rxSndAdpcmPause(stream->adpcm, slot->pause);
        if (slot->dirty & RxSndStrDirty_Attr)
            rxSndAdpcmSetAttr(stream->adpcm, stream->attr | slot->attr);
        if (slot->dirty & RxSndStrDirty_Volume)
        {
            int volL    = slot->volL;
            int volR    = slot->volR;
            int masterL = rxSndStrFmtGetMasterVolumeL();
            int masterR = rxSndStrFmtGetMasterVolumeR();
            rxSndAdpcmSetVolume(stream->adpcm, masterL * ((volL * stream->volL) >> 7) >> 7,
                                masterR * ((volR * stream->volR) >> 7) >> 7);
        }
        if (slot->dirty & RxSndStrDirty_Pitch)
            rxSndAdpcmSetPitch(stream->adpcm, slot->pitch);
        if (slot->dirty & RxSndStrDirty_TrackVolume)
            rxSndStrAdpcmApplyTrackVolumes(stream, slot);
        slot->dirty = 0;

        u_int status = rxSndAdpcmGetStatus(stream->adpcm);
        slot->status = (status & RXSNDSTR_STATUS_PLAYING) ? slot->status | RXSNDSTR_STATUS_PLAYING
                                                          : slot->status & ~RXSNDSTR_STATUS_PLAYING;
        slot->status = (status & RXSNDSTR_STATUS_PAUSED) ? slot->status | RXSNDSTR_STATUS_PAUSED
                                                         : slot->status & ~RXSNDSTR_STATUS_PAUSED;
        if (status & RXSNDSTR_STATUS_END)
            slot->status |= RXSNDSTR_STATUS_END;
        else
            rxSndStrAdpcmFeed(stream, slot);
    }
}

// FS2 0x001650f8
// A failed open leaves the stream on the free list. Kept from the ROM: it then
// closes the stream's source and rxSndAdpcm stream, also when this open never
// set them, so they are what the stream's last user left.
int rxSndStrAdpcmOpen(RxSndStrSlot *slot)
{
    RxSndStrAdpcmStream   *stream = s_free;
    const RxSndStrRequest *request;
    RxFileTagDef          *tags;
    RxFileMarkerDef       *markers;
    u_int                  iopAddr;
    u_int                  spuAddr;
    int                    channels;
    int                    mode;
    int                    blockSize;
    int                    markCount;
    int                    branchCount;

    if (stream == NULL)
    {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : empty ADPCM slot\n");
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

    channels = request->channels;
    if (rxSndStrBankGetTrackCount(slot->bank, slot->request) > 0)
    {
        mode = channels | kAdpcmModeTracks;
    }
    else if (channels == 1)
    {
        mode = kAdpcmModeMono;
    }
    else if (channels == 2)
    {
        mode = kAdpcmModeInterleaved;
    }
    else
    {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : unsupported channel number '%d'\n", request->channels);
        goto fail;
    }

    /* A read ring of whole pairs of channel blocks. */
    blockSize      = channels * kAdpcmBlockBytes;
    stream->source = rxSndStrBankOpenBody(slot->bank, slot->buf,
                                          slot->bufSize / (channels * 0x20) * blockSize * 2);
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
        markers[i].callback = rxSndStrAdpcmBranchCallback;
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

    /* Start on a block, inside the request. */
    slot->start = (int)slot->start / blockSize * blockSize;
    if (slot->start > request->size)
        slot->start = request->size;
    rxSndStrStreamSeek(stream->source, request->offset + slot->start);

    iopAddr = slot->iopAddr;    /* 0: rxSndAdpcmOpen allocates them */
    spuAddr = slot->spuAddr;
    stream->adpcm = rxSndAdpcmOpenEx(mode, request->rate, iopAddr, slot->iopKB, spuAddr, slot->spuKB,
                                     (request->flags & RXSNDSTR_REQUEST_NO_ARG) ? -1 : request->openArg);
    if (stream->adpcm < 0)
    {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : failed to open stream (rxsnd)\n");
        goto fail;
    }

    rxSndAdpcmSetUserData(stream->adpcm, stream->id);   /* PC: the ROM's slot */
    stream->slot      = slot;
    s_free            = stream->next;
    stream->volL      = request->volL;
    stream->volR      = request->volR;
    stream->blockSize = blockSize;
    stream->attr      = request->attr != 0;
    stream->next      = s_active;
    s_active          = stream;
    if (!YamiPan_IopSoundDriverRuns())
        adpcmSilentStart(stream, request);              /* PC: the driver's side */
    return stream->id;

fail:
    if (stream != NULL)
    {
        if (stream->source >= 0)
            rxSndStrStreamClose(stream->source, 0);
        if (stream->adpcm >= 0)
            rxSndAdpcmClose(stream->adpcm);
    }
    return -1;
}

// FS2 0x001655f8
void rxSndStrAdpcmClose(int handle, int async)
{
    RxSndStrAdpcmStream **link = &s_active;
    RxSndStrAdpcmStream  *stream;

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
    if (stream->adpcm >= 0)
        rxSndAdpcmClose(stream->adpcm);
}

// FS2 0x00165680
int rxSndStrAdpcmGetHandle(int handle)
{
    return s_streams[handle].adpcm;
}

// FS2 0x00165698
void rxSndStrAdpcmPauseRead(int handle, int pause)
{
    rxSndStrStreamPause(s_streams[handle].source, pause);
}

// FS2 0x001656c0
int rxSndStrAdpcmSyncRead(int handle, int arg)
{
    return rxSndStrStreamSync(s_streams[handle].source, arg);
}
