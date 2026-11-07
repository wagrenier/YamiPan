/* ==========================================================================
 *  rx/rxsndstr.c
 *
 *  The rx stream core (FS2 0x1634b8..0x164398, bss 0x51ef80..0x5217d0):
 *  64 stream slots on a free and an active list, and 8 format slots whose
 *  function tables play them. rxSndStrPlayEx takes a free slot, fills it
 *  from its arguments and the request's format opens it; the setters only
 *  store a value and mark a dirty bit, which the format's update applies.
 *  See rxsndstr.h.
 *
 *  FS2-only (not in RoR, no symbols): every name was read from the code
 *  (batch 008); rxSndStrStop / rxSndStrStopAsync / rxSndStrStopAll are the
 *  names the file's own BG-thread checks print. The ROM saves the COP0
 *  Status around the callbacks' DIntr ... EIntr and restores it after, left
 *  out on the host as in rxfile.c.
 * ======================================================================== */
#include "rx/rxsndstr.h"

#include "rx/rxsndut.h"
#include "rx/rxsys.h"
#include "sdk/eekernel.h"

#include <stddef.h>

/* A format slot (bss 0x52168c, 8 x 0x28 on the EE; the registered ones
 * listed from s_formatList through next). layout: host -- code pointers. */
typedef struct RxSndStrFormatSlot
{
    struct RxSndStrFormatSlot *next;    // FS2 +0x00
    int                        registered; // FS2 +0x04: 1 registered
    RxSndStrFormat             funcs;   // FS2 +0x08
} RxSndStrFormatSlot;

/* ---- data ------------------------------------------------------------- */

static RxSndStrSlot        s_slots[RXSNDSTR_SLOT_MAX];     /* bss 0x51ef80 */
static RxSndStrSlot       *s_active;                       /* bss 0x521680: newest first */
static RxSndStrSlot       *s_free;                         /* bss 0x521684 */
static short               s_masterVolL;                   /* bss 0x521688 */
static short               s_masterVolR;                   /* bss 0x52168a */
static RxSndStrFormatSlot  s_formats[RXSNDSTR_FORMAT_MAX]; /* bss 0x52168c */
static RxSndStrFormatSlot *s_formatList;                   /* bss 0x5217cc: last registered first */

/* The slot of an active stream, null for a bad id or an idle slot (inline
 * in every per-stream call). */
static RxSndStrSlot *rxSndStrGetActiveSlot(int stream)
{
    if ((u_int)stream >= RXSNDSTR_SLOT_MAX)
        return NULL;
    RxSndStrSlot *slot = &s_slots[stream];
    if (!(slot->status & RXSNDSTR_STATUS_ACTIVE))
        return NULL;
    return slot;
}

// ──────────────────────────────────────────────────────────────────────
// The stream id, -1 when the request is unknown, its format unregistered,
// every slot is taken or the format fails to open it (each reported under
// RXSNDUT_DEBUG_ERROR).
// FS2 0x001634b8

int rxSndStrPlayEx(int bank, int request, u_int start, void *buf, int bufSize,
                   u_int iopAddr, int iopKB, u_int spuAddr, int spuKB, int keepOnEnd)
{
    const RxSndStrRequest *req = rxSndStrBankGetRequest(bank, request);
    /* PC: the format byte comes from the bank file; the ROM indexes with
     * it unchecked. */
    if (req == NULL || req->format >= RXSNDSTR_FORMAT_MAX || !s_formats[req->format].registered) {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : unknow format bank or request '%d, %d'\n", bank, request);
        return -1;
    }
    RxSndStrSlot *slot = s_free;
    if (slot == NULL) {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : empty stream slot\n");
        return -1;
    }
    slot->format = &s_formats[req->format].funcs;
    slot->dirty = RxSndStrDirty_Attr | RxSndStrDirty_Volume | RxSndStrDirty_TrackVolume;
    slot->keepOnEnd = (u_char)keepOnEnd;
    slot->volR = RXSNDSTR_VOLUME_UNITY;
    slot->floatParam = 1.0f;
    slot->start = start;
    slot->buf = buf;
    slot->bufSize = bufSize;
    slot->iopAddr = iopAddr;
    slot->iopKB = iopKB;
    slot->spuAddr = spuAddr;
    slot->handle = -1;
    slot->spuKB = spuKB;
    slot->bank = bank;
    slot->request = request;
    slot->_10 = 0;
    slot->_14 = 0;
    slot->_1a = 0;
    slot->attr = 0;
    slot->pause = 0;
    slot->dataWritten = 0;
    slot->volL = RXSNDSTR_VOLUME_UNITY;
    slot->pitch = 1.0f;
    slot->userData = NULL;
    slot->markCallback = NULL;
    slot->branchCallback = NULL;
    slot->filled = 0;
    for (int i = 0; i < RXSNDSTR_TRACK_MAX; i++) {
        slot->trackVolL[i] = RXSNDSTR_VOLUME_UNITY;
        slot->trackVolR[i] = RXSNDSTR_VOLUME_UNITY;
    }
    slot->handle = slot->format->open(slot);
    if (slot->handle < 0) {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : failed to open stream '%d, %d'\n", bank, request);
        return -1;
    }
    s_free = slot->next;
    slot->next = s_active;
    s_active = slot;
    slot->status = RXSNDSTR_STATUS_ACTIVE;
    return slot->id;
}

// ──────────────────────────────────────────────────────────────────────
// From the start.
// FS2 0x001636e8

int rxSndStrPlay(int bank, int request, void *buf, int bufSize, u_int iopAddr,
                 int iopKB, u_int spuAddr, int spuKB, int keepOnEnd)
{
    return rxSndStrPlayEx(bank, request, 0, buf, bufSize, iopAddr, iopKB, spuAddr, spuKB,
                          keepOnEnd);
}

// ──────────────────────────────────────────────────────────────────────
// rxSndStrPlayEx under its public name.
// FS2 0x00163738

int rxSndStrPlayAt(int bank, int request, u_int start, void *buf, int bufSize,
                   u_int iopAddr, int iopKB, u_int spuAddr, int spuKB, int keepOnEnd)
{
    return rxSndStrPlayEx(bank, request, start, buf, bufSize, iopAddr, iopKB, spuAddr, spuKB,
                          keepOnEnd);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163750

void rxSndStrPause(int stream, int pause, int arg)
{
    (void)arg;
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot != NULL) {
        slot->pause = (u_char)pause;
        slot->dirty |= RxSndStrDirty_Pause;
    }
}

// ──────────────────────────────────────────────────────────────────────
// rxSndStrStop and rxSndStrStopAsync's body: unlinks the stream and closes it in its
// format (async passed on). An unknown id does nothing.
// FS2 0x001637a8

static void rxSndStrStopImpl(int stream, int arg, int async)
{
    (void)arg;
    rxSndUtCheckBgCall("rxSndStrStop or rxSndStrStopAsync");
    RxSndStrSlot **link = &s_active;
    RxSndStrSlot *slot;
    for (;;) {
        slot = *link;
        if (slot == NULL)
            return;
        if (slot->id == (u_int)stream)
            break;
        link = &slot->next;
    }
    *link = slot->next;
    slot->next = s_free;
    s_free = slot;
    slot->format->close(slot->handle, async);
    slot->status = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163848

void rxSndStrStop(int stream, int arg)
{
    rxSndStrStopImpl(stream, arg, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163860

void rxSndStrStopAsync(int stream, int arg)
{
    rxSndStrStopImpl(stream, arg, 1);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163878

void rxSndStrStopAll(int arg)
{
    rxSndUtCheckBgCall("rxSndStrStopAll");
    while (s_active != NULL)
        rxSndStrStop(s_active->id, arg);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001638d8

void rxSndStrSuspend(int stream, int arg)
{
    rxSndStrPause(stream, 1, arg);
    rxSndStrPauseRead(stream, 1);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163910

u_int rxSndStrGetStatus(int stream)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return 0;
    return slot->status;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163960

void rxSndStrSetAttr(int stream, int attr)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot != NULL) {
        slot->attr = (u_char)attr;
        slot->dirty |= RxSndStrDirty_Attr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Marks every active stream's volume dirty.
// FS2 0x001639b8

void rxSndStrSetMasterVolume(int volL, int volR)
{
    s_masterVolL = (short)volL;
    s_masterVolR = (short)volR;
    for (RxSndStrSlot *slot = s_active; slot != NULL; slot = slot->next)
        slot->dirty |= RxSndStrDirty_Volume;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163a00

int rxSndStrGetMasterVolumeL(void)
{
    return s_masterVolL;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163a10

int rxSndStrGetMasterVolumeR(void)
{
    return s_masterVolR;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163a20

void rxSndStrSetVolume(int stream, int volL, int volR)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot != NULL) {
        slot->volL = (short)volL;
        slot->volR = (short)volR;
        slot->dirty |= RxSndStrDirty_Volume;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163a78

void rxSndStrSetTrackVolume(int stream, int track, int volL, int volR)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot != NULL && (u_int)track < RXSNDSTR_TRACK_MAX) {
        slot->trackVolL[track] = (short)volL;
        slot->trackVolR[track] = (short)volR;
        slot->dirty |= RxSndStrDirty_TrackVolume;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163ae8

void rxSndStrSetPitch(int stream, float pitch)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot != NULL) {
        slot->pitch = pitch;
        slot->dirty |= RxSndStrDirty_Pitch;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163b40

void rxSndStrSetFloatParam(int stream, float value)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot != NULL) {
        slot->floatParam = value;
        slot->dirty |= RxSndStrDirty_FloatParam;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163b98

int rxSndStrGetFilled(int stream)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return 0;
    return slot->filled;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163be8

void *rxSndStrSetUserData(int stream, void *userData)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return NULL;
    void *old = slot->userData;
    slot->userData = userData;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163c38

void *rxSndStrGetUserData(int stream)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return NULL;
    return slot->userData;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163c88

RxSndStrMarkCallback rxSndStrSetMarkCallback(int stream, RxSndStrMarkCallback cb)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return NULL;
    RxSndStrMarkCallback old = slot->markCallback;
    slot->markCallback = cb;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163cd8

RxSndStrBranchCallback rxSndStrSetBranchCallback(int stream, RxSndStrBranchCallback cb)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return NULL;
    RxSndStrBranchCallback old = slot->branchCallback;
    slot->branchCallback = cb;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// 0 for an idle stream (the ROM's, though 0 is a valid bank).
// FS2 0x00163d28

int rxSndStrGetBank(int stream)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return 0;
    return slot->bank;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163d78

int rxSndStrGetRequest(int stream)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return 0;
    return slot->request;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163dc8

int rxSndStrGetHandle(int stream)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL || slot->format->getHandle == NULL)
        return -1;
    return slot->format->getHandle(slot->handle);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163e40

void rxSndStrPauseRead(int stream, int pause)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot != NULL && slot->format->pauseRead != NULL)
        slot->format->pauseRead(slot->handle, pause);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163ea8

int rxSndStrSyncRead(int stream, int arg)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL || slot->format->syncRead == NULL)
        return 0;
    return slot->format->syncRead(slot->handle, arg);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163f20

int rxSndStrGetDataWritten(int stream)
{
    RxSndStrSlot *slot = rxSndStrGetActiveSlot(stream);
    if (slot == NULL)
        return 0;
    return slot->dataWritten;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163f70

int rxSndStrGetActiveIds(u_int *ids)
{
    int n = 0;
    for (RxSndStrSlot *slot = s_active; slot != NULL; slot = slot->next) {
        if (ids != NULL)
            ids[n] = slot->id;
        n++;
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// Every slot free (pushed in order, so slot 63 is taken first), no
// format registered, the master volume at unity.
// FS2 0x00163fb8

void rxSndStrInit(void)
{
    s_active = NULL;
    s_free = NULL;
    for (int i = 0; i < RXSNDSTR_SLOT_MAX; i++) {
        s_slots[i].next = s_free;
        s_free = &s_slots[i];
        s_slots[i].id = (u_char)i;
    }
    s_formatList = NULL;
    s_masterVolR = RXSNDSTR_VOLUME_UNITY;
    s_masterVolL = RXSNDSTR_VOLUME_UNITY;
    for (int i = RXSNDSTR_FORMAT_MAX - 1; i >= 0; i--)
        s_formats[i].registered = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00164040

void rxSndStrExit(void)
{
    for (RxSndStrFormatSlot *f = s_formatList; f != NULL; f = f->next)
        f->funcs.exit();
    rxSndStrStopAll(0);
}

// ──────────────────────────────────────────────────────────────────────
// The formats' updates, then a played-out stream is stopped unless it
// was played with keepOnEnd.
// FS2 0x00164088

void rxSndStrUpdate(void)
{
    for (RxSndStrFormatSlot *f = s_formatList; f != NULL; f = f->next)
        f->funcs.update();
    RxSndStrSlot *next;
    for (RxSndStrSlot *slot = s_active; slot != NULL; slot = next) {
        next = slot->next;
        if ((slot->status & RXSNDSTR_STATUS_END) && slot->keepOnEnd == 0)
            rxSndStrStop(slot->id, 0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Copies the table, runs its init and lists it.
// FS2 0x00164128

int rxSndStrRegisterFormat(int id, const char *name, const RxSndStrFormat *format)
{
    if ((u_int)id >= RXSNDSTR_FORMAT_MAX || s_formats[id].registered == 1)
        return 0;
    RxSndStrFormatSlot *f = &s_formats[id];
    f->registered = 1;
    f->funcs = *format;
    f->funcs.init();
    f->next = s_formatList;
    s_formatList = f;
    if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_REGISTER)
        rxSysPrintf("rxsndstr : registed format '%s' : %d\n", name, id);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00164228

int rxSndStrFmtGetMasterVolumeL(void)
{
    return s_masterVolL;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00164238

int rxSndStrFmtGetMasterVolumeR(void)
{
    return s_masterVolR;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00164248

void rxSndStrFmtCallMarkCallback(RxSndStrSlot *slot, int mark)
{
    if (slot->markCallback != NULL) {
        DIntr();
        slot->markCallback(slot->id, mark);
        EIntr();
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 (take the branch) without a callback.
// FS2 0x001642e8

int rxSndStrFmtCallBranchCallback(RxSndStrSlot *slot, int branch)
{
    int result;
    if (slot->branchCallback == NULL) {
        result = 1;
    } else {
        DIntr();
        result = slot->branchCallback(slot->id, branch);
        EIntr();
    }
    return result;
}
