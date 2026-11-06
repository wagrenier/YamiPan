/* ==========================================================================
 *  game/rumblelist.h
 *
 *  The pad's rumble: tracks named in the init pack's item 0x14 (ALERT,
 *  PETITJACK, DAMAGE1, BITE, CARCLASH1.., 23 of them) played side by side.
 *  The stage scene's +0x30. Each frame StageScene::updateRumble takes the
 *  strongest of every track as the two motors' values, and a pulse of a
 *  track's third channel as a sound. Time is in 1/30 s frames.
 *
 *  0x2bc100..0x2bcd20, one class with its nodes. The file name is the
 *  port's; the class was KeepList until the data was read.
 * ======================================================================== */
#ifndef _GAME_RUMBLELIST_H
#define _GAME_RUMBLELIST_H

#include "sdk/scetypes.h"

class StageObject;

enum RumbleFlag
{
    RumbleFlag_Loop   = 0x1,    // a channel starts over; survives a scene change
    RumbleFlag_Always = 0x2,    // heard whoever the frame's rumble source is
};

// What a channel's step values mean (the file's channel kind).
enum RumbleChannelKind
{
    RumbleChannel_Level = 0,    // held until the next step
    RumbleChannel_Pulse = 1,    // for one frame, as a step starts
};

enum
{
    RumbleChannel_Large = 0,    // PadMotor_Large: strength 0..255
    RumbleChannel_Small = 1,    // PadMotor_Small: 0 or 1
    RumbleChannel_Event = 2,    // pulses: 1 sounds StageScene::updateRumble's alert
    kRumbleChannels     = 3,
    kRumblePulses       = 4,    // a frame's pulses of one channel
};

// One channel on disc: kind, and where its steps are from the entry's start
// (0 none). A step is a varint length (7 bits a byte, low first, bit 7 more)
// then a value byte; length 0 ends the list.
struct RumbleFileChannel
{
    int kind;                   // FS2 +0x0
    int offset;                 // FS2 +0x4
};
static_assert(sizeof(RumbleFileChannel) == 0x8, "a track's channel header");

// One named track on disc, its steps behind it.
struct RumbleFileEntry
{
    u_int             magic;    // FS2 +0x0: "fa2S"
    u_int             pad;      // FS2 +0x4
    char              name[0x10];               // FS2 +0x8
    u_int             flags;    // FS2 +0x18: RumbleFlag
    RumbleFileChannel channels[kRumbleChannels];    // FS2 +0x1c
};
static_assert(sizeof(RumbleFileEntry) == 0x34, "the track header's size");

// The init pack's item: count offsets of RumbleFileEntry from its start.
struct RumbleFile
{
    u_int magic;                // FS2 +0x0: "sv2S"
    u_int pad;                  // FS2 +0x4
    u_int count;                // FS2 +0x8
    int   offsets[1];           // FS2 +0xc
};
static_assert(sizeof(RumbleFile) == 0x10, "the file's header size");

// layout: host -- EE 0x18, offsets in the comments.
struct RumbleChannel
{
    u_int          flags;       // FS2 +0x0: the track's RumbleFlag
    int            kind;        // FS2 +0x4: RumbleChannelKind
    const u_char  *steps;       // FS2 +0x8: where a loop starts over
    const u_char  *cursor;      // FS2 +0xc: the next step
    float          timer;       // FS2 +0x10: frames left of this step
    u_char         values[kRumblePulses];       // FS2 +0x14

    // Counts down frames and takes the steps that come due.
    void advance(float frames);                                          // FS2 0x002bcb20
};

// A playing track, a node of the list. EE 0x5c from operator new.
// layout: host -- EE offsets in the comments.
struct RumbleTrack
{
    RumbleTrack(const RumbleFileEntry *entry, StageObject *source);      // FS2 0x002bc9a0
    // Writes this frame's output (the two motors, and up to maxEvents bytes
    // of the event channel), then advances the channels.
    void play(float frames, u_char *motorLarge, u_char *motorSmall, char *events,
              int maxEvents);                                            // FS2 0x002bc790

    RumbleTrack  *next;         // FS2 +0x0
    u_int         flags;        // FS2 +0x4: RumbleFlag
    const char   *name;         // FS2 +0x8: in the file's entry
    StageObject  *source;       // FS2 +0xc: who it is felt through
    RumbleChannel channels[kRumbleChannels];    // FS2 +0x10
    u_char        finished;     // FS2 +0x58
};

// layout: host -- EE 0xc.
struct RumbleList
{
    RumbleList();                                                        // FS2 0x002bc770: three words zero
    ~RumbleList();                                                       // FS2 0x002bc6f0: deleting; deletes every track

    // Starts the track of that name, none without one. source null: felt
    // whoever the frame's source is.
    void addNamed(const char *name, StageObject *source);                // FS2 0x002bc3c0
    // Stops every track of that name; how many there were.
    int  removeNamed(const char *name);                                  // FS2 0x002bc4f0
    // Stops every track that does not loop.
    void removeUnlooped();                                               // FS2 0x002bc5d0
    // Whether a track of that name plays.
    int  isPlaying(const char *name);                                    // FS2 0x002bc690
    // Advances every track by frames. The tracks of source (or all, source
    // null) and the RumbleFlag_Always ones give the strongest of each motor
    // and up to max event bytes; sources gets each event's track source.
    void update(float frames, u_char *motorLarge, u_char *motorSmall, char *events,
                StageObject **sources, int max, StageObject *source);    // FS2 0x002bc100

    const RumbleFile *data;     // FS2 +0x0: the init pack's item (StageFiles::loadList)
    RumbleTrack      *head;     // FS2 +0x4
    RumbleTrack      *tail;     // FS2 +0x8

private:
    // Unlinks and deletes track, prev before it; the track after it.
    RumbleTrack *remove(RumbleTrack *prev, RumbleTrack *track);
};

#endif
