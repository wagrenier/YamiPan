/* ==========================================================================
 *  game/stagesound.h
 *
 *  The stage's sound (game/stagesound.cpp, 0x1d3dc0..0x1d7c50): one
 *  StageSound per stage, built by stageThread (game/stageloop.cpp, sbss
 *  0x31ae04) and handed to every StageScene it builds (the scene's +0x24;
 *  a sub-scene gets none). It owns an effect player (SePlayer), 0x40 slots
 *  of playing effects -- named one-shots (StageEffect) and positional
 *  sources (StageSoundSource) -- and four stream channels (SoundTrack):
 *  0 / 1 the music, 2 / 3 the streamed cues the stage objects start.
 *
 *  Turning the sound down (the pause and map menus, the clear, game over)
 *  fades the player's master volume to 0 over 12 frames; once the fade is
 *  done the update pauses the resumable effects (StageSoundState_Down) and
 *  turning it back up resumes them. The music channels fade to half.
 *
 *  The file name is the port's (no source string). The update
 *  (0x1d5580 / 0x1d56a0 / 0x1d5e40), the stream cues 0x1d46e0..0x1d5510,
 *  the positional sources (0x1d6bd0 / 0x1d6d10, dtor 0x1d3e90) and
 *  0x1d70e0 / 0x1d73b0 / 0x1d79c0 / 0x1d7a30 are not written yet.
 * ======================================================================== */
#ifndef _GAME_STAGESOUND_H
#define _GAME_STAGESOUND_H

#include "sdk/scetypes.h"

class SePlayer;                 // game/gamehooks.h
struct SeEntry;                 // game/gamehooks.h
class SoundTrack;               // game/gamehooks.h
class StageSound;

// The handle SePlayer::play answers: -1 none; bit 60 set while paused
// (SePlayer::pause).
typedef long long SeHandle;

// StageSound::m_state: the master volume's turn-down.
enum StageSoundState
{
    StageSoundState_Normal      = 0,
    StageSoundState_TurningDown = 1,    // fading to 0 (12 frames)
    StageSoundState_Down        = 2,    // the fade is done, resumable effects paused (the update)
    StageSoundState_TurningUp   = 3,    // fading back to 0x80; the update then goes Normal
};

// StageSound::m_downReason: why it is turned down.
enum StageSoundDownReason
{
    StageSoundDown_None   = 0,
    StageSoundDown_Paused = 1,          // pause (the pause / map menus)
    StageSoundDown_Muted  = 2,          // setMuted(true) (the clear, game over)
};

// StageSoundSlot::kind.
enum StageSoundSlotKind
{
    StageSoundSlot_Free   = 0,
    StageSoundSlot_Source = 1,          // a StageSoundSource
    StageSoundSlot_Effect = 2,          // a StageEffect
};

// StageStreamChannel::state, as the update (0x1d56a0) moves it. Starting /
// Pausing are read from where the update sets them only.
enum StageStreamChannelState
{
    StageStreamChannel_Idle     = 0,
    StageStreamChannel_Starting = 1,    // played, fading in
    StageStreamChannel_Playing  = 2,
    StageStreamChannel_Pausing  = 3,
    StageStreamChannel_Paused   = 4,
    StageStreamChannel_Stopping = 5,    // fading out to stop (10 frames, SoundFadeEnd_Stop)
};

constexpr int kStageSoundSlots         = 0x40;
constexpr int kStageSoundChannels      = 4;
constexpr int kStageSoundMusicChannels = 2;     // channels 0 / 1; 2 / 3 are the cue streams
// The master volume's fade when turning down / up, and the music's.
constexpr int kStageSoundFadeFrames    = 12;
constexpr int kStageMusicFadeFrames    = 10;

// A named effect playing in a slot (playNamed). EE 0x18.
// layout: host -- EE +0x0 entry, +0x8 handle (64-bit), +0x10 stopped.
struct StageEffect
{
    const SeEntry *entry;       // FS2 +0x00
    SeHandle       handle;      // FS2 +0x08
    bool           stopped;     // FS2 +0x10: stopped by stopNamed / a failed pause
};

// A positional sound source (slot kind 1), built by 0x1d6d10 (vtable
// 0x315a20 over the base 0x315a80, whose dtor is 0x1865c0) and deleted
// through its virtual slot 2. Stand-in: only what is read here; its slots
// 0 / 1 / 3..6 (0x186a00 / 0x186a10 / 0x186a20 / 0x186a30 / 0x1d7ce0 /
// 0x1d8420) are not declared.
// layout: host -- EE: vptr +0, +0x100 handle, +0x108 entry, +0x115 stopped.
class StageSoundSource
{
public:
    virtual ~StageSoundSource();                                         // FS2 0x001d3e90: slot 2 (+0x10), deleting

    SeHandle       m_handle;    // FS2 +0x100
    const SeEntry *m_entry;     // FS2 +0x108
    bool           m_stopped;   // FS2 +0x115
};

// One of StageSound's 0x40 effect slots. EE 8 bytes.
// layout: host -- EE {int kind, pointer}.
struct StageSoundSlot
{
    int kind = StageSoundSlot_Free;     // FS2 +0x0: StageSoundSlotKind
    union {
        void             *ptr = nullptr;
        StageEffect      *effect;       // StageSoundSlot_Effect
        StageSoundSource *source;       // StageSoundSlot_Source
    };                                  // FS2 +0x4

    // Stops what the slot plays (on the sound's player) and frees it; the
    // slot is free after.
    void release(StageSound *sound);                                     // FS2 0x001d3dc0
};

// What a stream channel plays or will play next: set by the stage objects'
// cues (0x1d4c10 / 0x1d4d30), started by the update. EE 0x18.
// layout: host -- EE six words; what source / def / owner are is unread
// (def has a priority short at +0x2c and a range float at +0x10, owner a
// position through its virtual slot +0x14).
struct StageStreamCue
{
    void       *source;         // FS2 +0x00: from 0x177270(owner +0x30 +0x14, 0x303178)
    const void *def;            // FS2 +0x04
    void       *owner;          // FS2 +0x08
    int         variant;        // FS2 +0x0c: the def's request index (cycled or random)
    int         _10;            // FS2 +0x10
    int         fadeFrames;     // FS2 +0x14: fade-in, 30 fps frames (scaled by 25 / 30)

    void clear();                                                        // FS2 0x001d7690
};

// One of StageSound's four stream channels. EE 0x3c.
// layout: host -- EE: +0x0 stream, +0x4 cue, +0x1c next, +0x34 volume,
// +0x38 state.
struct StageStreamChannel
{
    SoundTrack     *stream = nullptr;   // FS2 +0x00: new / delete
    StageStreamCue  cue{};              // FS2 +0x04: the one playing
    StageStreamCue  next{};             // FS2 +0x1c: the one asked for
    float           volume = 1.0f;      // FS2 +0x34: the music's fade scale
    int             state = StageStreamChannel_Idle; // FS2 +0x38: StageStreamChannelState

    // Stops the stream when it plays, idle, both cues cleared (the volume
    // stays).
    void reset();                                                        // FS2 0x001d3f00
};

// layout: host -- EE 0x314.
class StageSound
{
public:
    StageSound();                                                        // FS2 0x001d7750
    ~StageSound();                                                       // FS2 0x001d76b0: deleting

    // Releases every effect, the master volume back to full at once, the
    // channels reset, turned up. The ctor, the dtor and
    // StageScene::~StageScene.
    void reset();                                                        // FS2 0x001d7510
    void resetChannels();                                                // FS2 0x001d7620

    // Turns the sound down and fades the playing music to half (the pause
    // and map menus); resume undoes it.
    void pause();                                                        // FS2 0x001d42e0
    void resume();                                                       // FS2 0x001d41d0
    // Turns the sound down (muted) or back up (the clear, game over and a
    // sub-scene's end).
    void setMuted(bool muted);                                           // FS2 0x001d43f0
    // Pauses or resumes every resumable effect still playing.
    void setEffectsPaused(bool paused);                                  // FS2 0x001d44c0
    // Stops every effect and stream (before a reload).
    void stopAll();                                                      // FS2 0x001d4020
    // No stream still playing or stopping.
    bool isStopped();                                                    // FS2 0x001d3f80
    // Not turning down: turned down, once no channel plays any more
    // (0x287f20 polls it).
    bool isSettled();                                                    // FS2 0x001d4130

    // Plays the effect called name at a volume 0..1; while the sound is
    // turned down only resumable effects play, started paused.
    void playNamed(const char *name, float volume);                      // FS2 0x001d6a80
    // playNamed, also while the sound is turned down and through the
    // player's unscaled entry (not under the master volume): the effect,
    // null when it did not play.
    StageEffect *playNamedAlways(const char *name, float volume);        // FS2 0x001d6a70
    // Stops every playing effect called name.
    void stopNamed(const char *name);                                    // FS2 0x001d6a90
    // Stops a positional source's sound (null: nothing).
    void stopSource(StageSoundSource *source);                           // FS2 0x001d74b0

    // Hands the four streams the stage's fsm subtitle file, stream name
    // and StDT stream table (0x19d9e0).
    void setSubtitles(void *file);                                       // FS2 0x001d6fc0
    void setStreamName(const char *name);                                // FS2 0x001d7020
    void setStreamTable(void *file);                                     // FS2 0x001d7080

private:
    StageEffect *playEffect(float left, float right, const char *name, bool always); // FS2 0x001d66e0

    // Inline in the ROM.
    bool isTurnedDown() const
    {
        return m_state == StageSoundState_TurningDown || m_state == StageSoundState_Down;
    }
    int  findFreeSlot();
    void releaseSlots();
    void stopEffect(StageEffect *effect);

public:
    int                _00 = 0;         // FS2 +0x000: unread
    int                _04 = 0;         // FS2 +0x004: unread
    SePlayer          *m_player;        // FS2 +0x008: new / delete (0x1298)
    u_char             m_b0c;           // FS2 +0x00c: set by the update 0x1d5580 (a1), unread
    u_char             m_b0d;           // FS2 +0x00d: set by the update 0x1d5580 (a2), unread
    u_char             m_effectAttr;    // FS2 +0x00e: the attr setEffectsPaused(true) gives a paused source
    StageStreamChannel m_channels[kStageSoundChannels];     // FS2 +0x010
    StageSoundSlot     m_slots[kStageSoundSlots];           // FS2 +0x100
    int                m_freeHint;      // FS2 +0x300: where the free-slot search starts
    int                m_lastSlot;      // FS2 +0x304: the highest slot used, -1 none
    int                _308;            // FS2 +0x308: 0 in the ctor, unread
    int                m_state;         // FS2 +0x30c: StageSoundState
    int                m_downReason;    // FS2 +0x310: StageSoundDownReason
};

#endif /* _GAME_STAGESOUND_H */
