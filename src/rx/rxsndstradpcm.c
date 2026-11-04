// rx/rxsndstradpcm.c -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// The stubs and bridges below go when it is reconstructed; see tools/reverse/stage.py.

#include "common/yamipan_stub.h"
#include "rx/rxsndstradpcm.h"
#include "rx/rxsndstrbank.h"

#include <time.h>

// ---- bridges: the format with no sound ----
//
// PC: not FS2's code (rxSndAdpcm* and the stream reader are unwritten and the
// host has no mixer yet). A stream here is silent, but it lasts as long as its
// request: an event waits for its voice (StageScene::update), so a stream that
// never ended held the event on its black last frame for ever. A looping
// request never ends, as in the game. Wall time, as a real voice would run.

enum
{
    kAdpcmBlockBytes   = 16,    // PS-ADPCM: a block of 16 bytes per channel ...
    kAdpcmBlockSamples = 28,    // ... holds 28 samples
    kAdpcmHostStreams  = RXSNDSTR_SLOT_MAX,
};

// layout: host
typedef struct AdpcmHostStream
{
    RxSndStrSlot *slot;         // null: free
    double        secondsLeft;  // below 0: loops
    double        secondsPlayed;
    clock_t       lastTick;
} AdpcmHostStream;

static AdpcmHostStream s_hostStreams[kAdpcmHostStreams];

// What is left of request after start (bytes), in seconds at pitch 1.
static double adpcmRequestSeconds(const RxSndStrRequest *request, u_int start)
{
    int channels = request->channels != 0 ? request->channels : 1;
    u_int bytes = start < request->size ? request->size - start : 0;
    double samples = (double)bytes / (kAdpcmBlockBytes * channels) * kAdpcmBlockSamples;
    return request->rate != 0 ? samples / request->rate : 0.0;
}

// bridge of FS2 0x001650f8
int rxSndStrAdpcmOpen(RxSndStrSlot *slot)
{
    YAMIPAN_BRIDGE("rxSndStrAdpcmOpen", 0x001650f8);
    const RxSndStrRequest *request = rxSndStrBankGetRequest(slot->bank, slot->request);
    if (request == NULL)
        return -1;
    for (int i = 0; i < kAdpcmHostStreams; i++) {
        AdpcmHostStream *stream = &s_hostStreams[i];
        if (stream->slot == NULL) {
            stream->slot = slot;
            stream->secondsLeft = (request->flags & 1) ? -1.0
                                                       : adpcmRequestSeconds(request, slot->start);
            stream->secondsPlayed = 0.0;
            stream->lastTick = clock();
            return i;
        }
    }
    return -1;
}

// bridge of FS2 0x001655f8
void rxSndStrAdpcmClose(int handle, int async)
{
    YAMIPAN_BRIDGE("rxSndStrAdpcmClose", 0x001655f8);
    (void)async;
    if (handle >= 0 && handle < kAdpcmHostStreams)
        s_hostStreams[handle].slot = NULL;
}

// bridge of FS2 0x00164d18
void rxSndStrAdpcmUpdate(void)
{
    YAMIPAN_BRIDGE("rxSndStrAdpcmUpdate", 0x00164d18);
    clock_t now = clock();
    for (int i = 0; i < kAdpcmHostStreams; i++) {
        AdpcmHostStream *stream = &s_hostStreams[i];
        if (stream->slot == NULL)
            continue;
        double elapsed = (double)(now - stream->lastTick) / CLOCKS_PER_SEC;
        stream->lastTick = now;
        if (stream->slot->pause)
            continue;
        stream->secondsPlayed += elapsed * stream->slot->pitch;
        if (stream->secondsLeft < 0.0)
            continue;
        stream->secondsLeft -= elapsed * stream->slot->pitch;
        if (stream->secondsLeft <= 0.0) {
            stream->secondsLeft = 0.0;
            stream->slot->status |= RXSNDSTR_STATUS_END;
        }
    }
}

// The bytes played of a stream's request, as rxSndAdpcmGetPosition would answer
// them: the subtitles run on it.
u_long rxSndStrAdpcmBridgePosition(int handle)
{
    if (handle < 0 || handle >= kAdpcmHostStreams || s_hostStreams[handle].slot == NULL)
        return 0;
    const AdpcmHostStream *stream = &s_hostStreams[handle];
    const RxSndStrRequest *request = rxSndStrBankGetRequest(stream->slot->bank, stream->slot->request);
    if (request == NULL)
        return 0;
    double samples = stream->secondsPlayed * request->rate;
    return (u_long)(samples / kAdpcmBlockSamples * kAdpcmBlockBytes * request->channels);
}

// bridge of FS2 0x00165680: the handle is the stream, nothing maps it.
int rxSndStrAdpcmGetHandle(int handle)
{
    YAMIPAN_BRIDGE("rxSndStrAdpcmGetHandle", 0x00165680);
    return handle;
}

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x00164c80
void rxSndStrAdpcmExit(void)
{
    YAMIPAN_STUB("rxSndStrAdpcmExit", 0x00164c80);
}

// stub of FS2 0x00164c38
void rxSndStrAdpcmInit(void)
{
    YAMIPAN_STUB("rxSndStrAdpcmInit", 0x00164c38);
}

// stub of FS2 0x00165698
void rxSndStrAdpcmPauseRead(int handle, int pause)
{
    YAMIPAN_STUB("rxSndStrAdpcmPauseRead", 0x00165698);
}

// stub of FS2 0x001656c0
int rxSndStrAdpcmSyncRead(int handle, int arg)
{
    YAMIPAN_STUB("rxSndStrAdpcmSyncRead", 0x001656c0);
    return {};
}

