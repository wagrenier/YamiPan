/* ==========================================================================
 *  game/rumblelist.cpp (FS2 0x2bc100..0x2bcd20): the rumble tracks.
 * ======================================================================== */
#include "game/rumblelist.h"

#include <string.h>

// timer closer to 0 than this is 0.
constexpr float kRumbleTimerEpsilon = 1e-09f;

// The step at cursor (none, length 0, when cursor is null): the bytes it
// takes. Written out twice in the ROM.
static int readStep(const u_char *cursor, int *length, u_char *value)
{
    *length = 0;
    *value = 0;
    if (cursor == nullptr)
        return 0;
    int      used = 0;
    u_int    shift = 0;
    u_char   byte;
    do {
        byte = cursor[used++];
        *length += (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    *value = cursor[used++];
    return used;
}

// ──────────────────────────────────────────────────────────────────────

// FS2 0x002bcb20
// callcheck: memset, memset (the step reader is written out in the ROM)
void RumbleChannel::advance(float frames)
{
    if (kind == RumbleChannel_Pulse) {
        memset(values, 0, sizeof(values));
        if (timer == 0.0f)
            return;
    } else if (timer == 0.0f) {
        memset(values, 0, sizeof(values));
        return;
    }

    timer -= frames;
    if (timer < kRumbleTimerEpsilon && -kRumbleTimerEpsilon < timer)
        timer = 0.0f;
    if (timer > 0.0f)
        return;

    int pulses = 0;
    do {
        int    length;
        u_char value;
        int    used = readStep(cursor, &length, &value);
        if (length == 0) {
            if (!(flags & RumbleFlag_Loop)) {
                timer = 0.0f;
                return;
            }
            cursor = steps;
        } else {
            cursor += used;
            timer += (float)length;
            if (kind == RumbleChannel_Pulse) {
                if (pulses < kRumblePulses)
                    values[pulses++] = value;
            } else {
                values[0] = value;
            }
        }
    } while (timer <= 0.0f);
}

// ──────────────────────────────────────────────────────────────────────

// FS2 0x002bc9a0
// callcheck: memset, memcpy, memset (the header is read in place; the step reader is written out in the ROM)
RumbleTrack::RumbleTrack(const RumbleFileEntry *entry, StageObject *source)
{
    next = nullptr;
    this->source = source;
    memset(channels, 0, sizeof(channels));
    finished = 0;
    name = entry->name;
    flags = entry->flags;

    for (int i = 0; i < kRumbleChannels; i++) {
        RumbleChannel &channel = channels[i];
        const RumbleFileChannel &file = entry->channels[i];
        const u_char *steps = file.offset == 0 ? nullptr
                                               : (const u_char *)entry + file.offset;
        channel.flags = entry->flags;
        channel.kind = file.kind;
        channel.steps = steps;
        channel.cursor = steps;

        int    length;
        u_char value;
        int    used = readStep(channel.cursor, &length, &value);
        channel.timer = (float)length;
        if (length > 0) {
            channel.cursor += used;
            channel.values[0] = value;
        }
    }
}

// FS2 0x002bc790
void RumbleTrack::play(float frames, u_char *motorLarge, u_char *motorSmall, char *events, int maxEvents)
{
    if (finished) {
        *motorSmall = 0;
        *motorLarge = 0;
        return;
    }

    // PC: the ROM tests the channel's kind here and takes values[0] either way.
    if (motorLarge != nullptr)
        *motorLarge = channels[RumbleChannel_Large].values[0];
    if (motorSmall != nullptr)
        *motorSmall = channels[RumbleChannel_Small].values[0];
    if (events != nullptr && maxEvents != 0) {
        const RumbleChannel &channel = channels[RumbleChannel_Event];
        if (channel.kind == RumbleChannel_Pulse) {
            if (maxEvents > kRumblePulses)
                maxEvents = kRumblePulses;
            for (int i = 0; i < maxEvents; i++)
                events[i] = (char)channel.values[i];
        } else {
            *events = (char)channel.values[0];
        }
    }

    for (int i = 0; i < kRumbleChannels; i++)
        channels[i].advance(frames);
}

// ──────────────────────────────────────────────────────────────────────

// FS2 0x002bc770
RumbleList::RumbleList()
{
    data = nullptr;
    head = nullptr;
    tail = nullptr;
}

// FS2 0x002bc6f0
// callcheck: operator_delete, operator_delete (the delete expressions)
RumbleList::~RumbleList()
{
    while (head != nullptr) {
        RumbleTrack *track = head;
        head = track->next;
        delete track;
    }
    tail = nullptr;
}

RumbleTrack *RumbleList::remove(RumbleTrack *prev, RumbleTrack *track)
{
    if (prev == nullptr)
        head = track->next;
    else
        prev->next = track->next;
    if (track == tail)
        tail = prev;
    delete track;
    return prev == nullptr ? head : prev->next;
}

// FS2 0x002bc3c0
// callcheck: strcmp, operator_new, RumbleTrack
void RumbleList::addNamed(const char *name, StageObject *source)
{
    if (data == nullptr)
        return;

    RumbleTrack *track = nullptr;
    for (u_int i = 0; i < data->count; i++) {
        const RumbleFileEntry *entry =
            (const RumbleFileEntry *)((const char *)data + data->offsets[i]);
        if (strcmp(entry->name, name) == 0) {
            track = new RumbleTrack(entry, source);
            break;
        }
    }
    if (track == nullptr)
        return;

    if (head == nullptr)
        head = track;
    else
        tail->next = track;
    tail = track;
}

// FS2 0x002bc4f0
// callcheck: strcmp, operator_delete (remove is written out in the ROM)
int RumbleList::removeNamed(const char *name)
{
    int          removed = 0;
    RumbleTrack *prev = nullptr;
    RumbleTrack *track = head;
    while (track != nullptr) {
        if (strcmp(track->name, name) == 0) {
            track = remove(prev, track);
            removed++;
        } else {
            prev = track;
            track = track->next;
        }
    }
    return removed;
}

// FS2 0x002bc5d0
// callcheck: operator_delete (remove is written out in the ROM)
void RumbleList::removeUnlooped()
{
    RumbleTrack *prev = nullptr;
    RumbleTrack *track = head;
    while (track != nullptr) {
        if (!(track->flags & RumbleFlag_Loop)) {
            track = remove(prev, track);
        } else {
            prev = track;
            track = track->next;
        }
    }
}

// FS2 0x002bc690
int RumbleList::isPlaying(const char *name)
{
    for (RumbleTrack *track = head; track != nullptr; track = track->next) {
        if (strcmp(track->name, name) == 0)
            return 1;
    }
    return 0;
}

// FS2 0x002bc100
// callcheck: memset, memset, play, operator_delete
void RumbleList::update(float frames, u_char *motorLarge, u_char *motorSmall, char *events,
                        StageObject **sources, int max, StageObject *source)
{
    u_char trackLarge = 0;
    u_char trackSmall = 0;

    *motorSmall = 0;
    *motorLarge = 0;
    if (events != nullptr)
        memset(events, 0, max);
    if (sources != nullptr)
        memset(sources, 0, max * sizeof(*sources));

    RumbleTrack *prev = nullptr;
    RumbleTrack *track = head;
    while (track != nullptr) {
        track->play(frames, &trackLarge, &trackSmall, events, max);

        if (source == nullptr || source == track->source || (track->flags & RumbleFlag_Always)) {
            if (*motorLarge < trackLarge)
                *motorLarge = trackLarge;
            if (*motorSmall < trackSmall)
                *motorSmall = trackSmall;
            if (events != nullptr) {
                // Keeps what fired: the next track writes behind it.
                int fired = 0;
                while (fired < max && events[fired] != 0) {
                    if (sources != nullptr)
                        sources[fired] = track->source;
                    fired++;
                }
                if (sources != nullptr)
                    sources += fired;
                events += fired;
                max -= fired;
            }
        }

        if (!track->finished) {
            bool done = true;
            for (int i = 0; i < kRumbleChannels; i++) {
                if (track->channels[i].timer != 0.0f) {
                    done = false;
                    break;
                }
            }
            if (done)
                track->finished = 1;
        }

        if (track->finished) {
            track = remove(prev, track);
        } else {
            prev = track;
            track = track->next;
        }
    }
}
