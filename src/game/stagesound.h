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
 *  The update (once a frame, from StageScene) places every positional
 *  sound around the listener (the active character): pan from its facing,
 *  volume from the distance over the entry's range; a cue stream behind a
 *  wall counts as out of range, behind a thin one at twice the distance.
 *
 *  The stage objects start the cue streams (playCue / setCue: the owner's
 *  head bone, a variant of the entry; a channel goes to the cue that
 *  matters most, and only when it can be heard), the music through
 *  playMusic / stopMusic, the positional sources through addSource.
 *
 *  The file name is the port's (no source string).
 * ======================================================================== */
#ifndef _GAME_STAGESOUND_H
#define _GAME_STAGESOUND_H

#include "game/stagenode.h"   /* StageNode */
#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"

class SePlayer;                 // game/gamehooks.h
struct SeEntry;                 // game/gamehooks.h
struct SeTable;                 // game/gamehooks.h
class SoundTrack;               // game/gamehooks.h
class StageObject;              // game/gamehooks.h
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

// What StageSound::pickCueChannel answers instead of a channel.
enum StageCueReject
{
    StageCueReject_NoTable    = -0x8000,    // the streams have no StDT table
    StageCueReject_OutOfRange = -0x7ffd,    // farther than the entry's range
    StageCueReject_NoChannel  = -0x7ffc,    // both channels busy with cues that matter more
    StageCueReject_Occluded   = -0x7ffb,    // behind a wall (a thin one: past half the range)
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

// A positional sound source (slot kind 1): a scene node (vtable 0x315a20
// over StageNode's 0x315a80, game/stagenode.h), new'd by
// StageSound::addSource (0x140 bytes) and deleted through its virtual slot
// 2. Its slots 0 / 1 / 3..6 (0x186a00 / 0x186a10 / 0x186a20 / 0x186a30 /
// 0x1d7ce0 / 0x1d8420) are StageNode's stand-ins. Without a parent node
// the update rebuilds its matrix (updateMatrix); under one, the parent's
// does.
// layout: host -- EE: StageNode, +0x100 handle, +0x108 entry, +0x10c
// 1.0f, +0x110 variant, +0x114 bySceneFlag, +0x115 stopped.
class StageSoundSource : public StageNode
{
public:
    // Inline in the ROM (StageSound::addSource).
    StageSoundSource(const SeEntry *entry, int variant);
    ~StageSoundSource() override;                                        // FS2 0x001d3e90: slot 2 (+0x10), deleting

    SeHandle       m_handle;    // FS2 +0x100: -1 not playing (yet)
    const SeEntry *m_entry;     // FS2 +0x108: the first of its variants
    float          _10c;        // FS2 +0x10c: 1.0f, unread here
    int            m_variant;   // FS2 +0x110: the entry played (m_entry[m_variant])
    u_char         m_bySceneFlag; // FS2 +0x114: its alternate follows the scene flag, else the player flag
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
// layout: host -- EE six words.
struct StageStreamCue
{
    StageNode  *source;         // FS2 +0x00: the owner's head bone (BoneNode::find(owner model, "bn_head"))
    SeEntry    *def;            // FS2 +0x04: the first of its variants (range +0x10)
    StageObject *owner;         // FS2 +0x08
    int         variant;        // FS2 +0x0c: def[variant] plays (cycled or random)
    int         resumeAt;       // FS2 +0x10: the start position (a stream stopped for a restart)
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

    // Once a frame (StageScene::update / drawPaused, 0x287f20): frees the
    // ended effects, settles the turn-down, places the sources and the cue
    // streams around listener (null: the sources are not placed),
    // starts / pauses / resumes the channels. playerFlag (StagePlayer
    // 0x214f60) and sceneFlag (StageScene 0x19b900) pick the alternate
    // entries; a change restarts what follows it.
    void update(StageObject *listener, u_char playerFlag, u_char sceneFlag); // FS2 0x001d5580

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
    // Takes the stage's SEDT file (0x19d9e0): every effect released, the
    // master volume full; an entry called X_J / X_D becomes the alternate
    // of X (SeEntry::altOffset both ways).
    void setSeTable(SeTable *table);                                     // FS2 0x001d70e0

    // Positional sources: the entry placed at pos (x, y, z) under parent
    // (null: none), a variant picked; null none (no table, no entry, no
    // free slot).
    StageSoundSource *addSource(const SeEntry *entry, const float *pos,
                                StageNodeBase *parent);                  // FS2 0x001d6d10
    StageSoundSource *addSourceNamed(const char *name, const float *pos,
                                     StageNodeBase *parent);             // FS2 0x001d6bd0
    // source when a slot holds it, else null.
    StageSoundSource *findSource(StageSoundSource *source);              // FS2 0x001d5510
    // Sets a playing source's pitch: true done.
    bool setSourcePitch(StageSoundSource *source, float pitch);          // FS2 0x001d73b0

    // The cue streams (channels 2 / 3). The entry called name in their
    // stream table; null none.
    SeEntry *findCue(const char *name);                                  // FS2 0x001d5140
    // The cue channel (0 / 1) a cue of owner would get, heard by listener,
    // or a StageCueReject (< 0).
    int  pickCueChannel(SeEntry *def, StageObject *owner, StageObject *listener); // FS2 0x001d4e70
    // Queues def for owner on the channel pickCueChannel picks (fading out
    // what plays there): false none.
    bool playCue(SeEntry *def, StageObject *owner, StageObject *listener); // FS2 0x001d4d30
    // Queues def for owner on cue channel 0 / 1 (fading out what plays).
    bool setCue(SeEntry *def, StageObject *owner, int channel);          // FS2 0x001d4c10
    // Fades out (10 frames) owner's cue def, queued or playing.
    void stopCue(StageObject *owner, const SeEntry *def);                // FS2 0x001d47e0
    // Fades out (4 frames) every cue of owner.
    void stopCuesOf(StageObject *owner);                                 // FS2 0x001d4920
    // owner has a cue queued or playing.
    bool hasCue(StageObject *owner);                                     // FS2 0x001d4a30
    // owner's cue called name is queued or playing.
    bool isCuePlaying(const char *name, StageObject *owner);             // FS2 0x001d4aa0
    // The subtitle of the cue playing with subtitles (channel 3 before 2);
    // null none.
    const u_short *getCueSubtitle();                                     // FS2 0x001d46e0

    // The music (channels 0 / 1): queues the entry called name on channel,
    // fading in over fadeFrames (30 fps) once what plays has faded out.
    void playMusic(int channel, const char *name, int fadeFrames);       // FS2 0x001d5390
    // Fades channel's music out over fadeFrames (30 fps) to stop.
    void stopMusic(int channel, int fadeFrames);                         // FS2 0x001d52d0
    // Channel 0's volume in percent (its stream and its fade scale).
    void setMusicVolume(int percent);                                    // FS2 0x001d5210

private:
    StageEffect *playEffect(float left, float right, const char *name, bool always); // FS2 0x001d66e0
    // The update's halves.
    void updateSlots(StageObject *listener, u_char playerFlag, u_char sceneFlag, u_char attr); // FS2 0x001d5e40
    void updateChannels(StageObject *listener, u_char playerFlag, u_char attr);                // FS2 0x001d56a0

    // Inline in the ROM.
    bool isTurnedDown() const
    {
        return m_state == StageSoundState_TurningDown || m_state == StageSoundState_Down;
    }
    int  findFreeSlot();
    // The variant of a run to play: random (variants < 0) or the next in
    // turn. Inline in the ROM (setCue, playCue).
    static int pickCueVariant(SeEntry *def);
    // Queues a cue on ch and fades out what plays. Inline in the ROM
    // (setCue, playCue).
    void queueCue(StageStreamChannel &ch, SeEntry *def, StageObject *owner, int variant);
    // Fades a channel out to stop (StageStreamChannel_Stopping) unless idle
    // or stopping already. Inline in the ROM (the cue / music setters).
    static void fadeOutChannel(StageStreamChannel &ch, int frames);
    void releaseSlots();
    void stopEffect(StageEffect *effect);

public:
    SeTable           *m_seTable = nullptr; // FS2 +0x000: the stage's SEDT file (setSeTable; the player takes it too)
    int                _04 = 0;         // FS2 +0x004: unread
    SePlayer          *m_player;        // FS2 +0x008: new / delete (0x1298)
    u_char             m_playerFlag;    // FS2 +0x00c: the update's playerFlag, last frame
    u_char             m_sceneFlag;     // FS2 +0x00d: the update's sceneFlag, last frame
    u_char             m_effectAttr;    // FS2 +0x00e: the sound attr at the listener (StageScene::getSoundAttr), last frame
    StageStreamChannel m_channels[kStageSoundChannels];     // FS2 +0x010
    StageSoundSlot     m_slots[kStageSoundSlots];           // FS2 +0x100
    int                m_freeHint;      // FS2 +0x300: where the free-slot search starts
    int                m_lastSlot;      // FS2 +0x304: the highest slot used, -1 none
    int                _308;            // FS2 +0x308: 0 in the ctor, unread
    int                m_state;         // FS2 +0x30c: StageSoundState
    int                m_downReason;    // FS2 +0x310: StageSoundDownReason
};

// The distance from listener to pos (its matrix's position), and in pan
// the angle to pos from the listener's facing (radians, the sign
// flipped). A Kind11 listener whose m_linked record muffles hears a sound
// to its side closer, ahead farther (the distance over
// clamp(1 - (pi/4) / |pan|, 0.4, 1) * 2.5).
float stageSoundLocateFacing(float facing, float *pan, const sceVu0FVECTOR pos,
                             StageObject *listener);                     // FS2 0x001d7a30
// stageSoundLocateFacing with the listener's own facing.
float stageSoundLocate(float *pan, const sceVu0FVECTOR pos, StageObject *listener); // FS2 0x001d79c0

#endif /* _GAME_STAGESOUND_H */
