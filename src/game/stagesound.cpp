// game/stagesound.cpp -- the stage's sound: its effect player, effect
// slots and stream channels (game/stagesound.h).

#include "game/stagesound.h"

#include <string.h>

#include <algorithm>

#include "game/gamehooks.h"
#include "sound/sound.h"

// isStopped also counts a stream state 5 as stopped; SoundStream never
// sets it (sound/sound.h's SoundStreamState ends at 4). Kept as the ROM.
constexpr int kSoundStreamState5 = 5;

// ──────────────────────────────────────────────────────────────────────
// Slots, cues and channels

// callcheck: operator_delete (delete e / delete s)
// FS2 0x001d3dc0
void StageSoundSlot::release(StageSound *sound)
{
    if (kind == StageSoundSlot_Effect) {
        StageEffect *e = effect;
        if (e->handle >= 0)
            sound->m_player->stop(e->handle);
        delete e;
    } else if (kind == StageSoundSlot_Source) {
        StageSoundSource *s = source;
        if (s->m_handle >= 0)
            sound->m_player->stop(s->m_handle);
        if (s != nullptr)
            delete s;           // virtual slot 2, deleting
    }
    kind = StageSoundSlot_Free;
    ptr  = nullptr;
}

// FS2 0x001d7690
void StageStreamCue::clear()
{
    source     = nullptr;
    def        = nullptr;
    owner      = nullptr;
    variant    = 0;
    _10        = 0;
    fadeFrames = 0;
}

// FS2 0x001d3f00
void StageStreamChannel::reset()
{
    if (stream->isActive())
        stream->stop();
    state = StageStreamChannel_Idle;
    cue.clear();
    next.clear();
}

// ──────────────────────────────────────────────────────────────────────
// StageSound: life

// callcheck: operator_new (new SePlayer / new SoundTrack)
// FS2 0x001d7750
StageSound::StageSound()
{
    m_player     = new SePlayer();
    m_b0c        = 0;
    m_effectAttr = 0;
    // m_channels / m_slots: their initialisers (the ROM's array loops).
    m_state      = StageSoundState_Normal;
    m_downReason = StageSoundDown_None;
    for (int i = 0; i < kStageSoundChannels; i++) {
        StageStreamChannel &ch = m_channels[i];
        ch.stream = new SoundTrack();
        ch.reset();
        ch.cue.clear();
        ch.next.clear();
        ch.volume = 1.0f;
    }
    _308 = 0;
    reset();
}

// callcheck: operator_delete, ~SePlayer, ~SoundTrack (the delete-expressions' deleting destructors)
// FS2 0x001d76b0
StageSound::~StageSound()
{
    reset();
    delete m_player;
    for (int i = 0; i < kStageSoundChannels; i++) {
        delete m_channels[i].stream;
        m_channels[i].stream = nullptr;
    }
}

// Every busy slot released, the search from the start, none used. Inline
// in the ROM (stopAll, reset).
void StageSound::releaseSlots()
{
    for (int i = 0; i < kStageSoundSlots; i++) {
        if (m_slots[i].kind != StageSoundSlot_Free)
            m_slots[i].release(this);
    }
    m_freeHint = 0;
    m_lastSlot = -1;
}

// callcheck: release, setEffectsPaused (the inline releaseSlots / setMuted)
// FS2 0x001d7510
void StageSound::reset()
{
    releaseSlots();
    m_player->fadeMaster(0, m_player->m_masterVolume, kSoundVolumeFull);
    m_state = StageSoundState_Normal;
    resetChannels();
    // setMuted(false), inline in the ROM: the state is Normal by now, so
    // only m_downReason changes.
    setMuted(false);
    m_b0c        = 0;
    m_b0d        = 0;
    m_effectAttr = 0;
}

// FS2 0x001d7620
void StageSound::resetChannels()
{
    for (int i = 0; i < kStageSoundChannels; i++) {
        StageStreamChannel &ch = m_channels[i];
        if (ch.stream != nullptr) {
            ch.reset();
            ch.cue.clear();
            ch.next.clear();
            ch.volume = 1.0f;
        }
    }
}

// callcheck: isActive, release, stop (the inline releaseSlots and StageStreamChannel::reset)
// FS2 0x001d4020
void StageSound::stopAll()
{
    releaseSlots();
    m_player->fadeMaster(0, m_player->m_masterVolume, kSoundVolumeFull);
    m_state = StageSoundState_Normal;
    for (int i = 0; i < kStageSoundChannels; i++) {
        if (m_channels[i].stream != nullptr)
            m_channels[i].reset();
    }
}

// FS2 0x001d3f80
bool StageSound::isStopped()
{
    switch (m_state) {
    case StageSoundState_TurningDown:
    case StageSoundState_Down:
    case StageSoundState_TurningUp:
    case StageSoundState_Normal:
        for (int i = 0; i < kStageSoundChannels; i++) {
            int state = m_channels[i].stream->m_state;
            if (state != kSoundStreamState5 && state != SoundStream_Ended &&
                state != SoundStream_Stopped)
                return false;
        }
        break;
    }
    return true;
}

// FS2 0x001d4130
bool StageSound::isSettled()
{
    if (m_state == StageSoundState_Down) {
        for (int i = 0; i < kStageSoundChannels; i++) {
            int state = m_channels[i].state;
            if (state != StageStreamChannel_Paused && state != StageStreamChannel_Idle)
                return false;
        }
        return true;
    }
    return m_state != StageSoundState_TurningDown;
}

// ──────────────────────────────────────────────────────────────────────
// Turning down and up

// FS2 0x001d42e0
void StageSound::pause()
{
    if (m_state == StageSoundState_TurningUp || m_state == StageSoundState_Normal) {
        m_player->fadeMaster(kStageSoundFadeFrames, m_player->m_masterVolume, 0);
        m_state = StageSoundState_TurningDown;
    }
    m_downReason = StageSoundDown_Paused;
    for (int i = 0; i < kStageSoundMusicChannels; i++) {
        StageStreamChannel &ch = m_channels[i];
        if (ch.state == StageStreamChannel_Playing)
            ch.stream->startFade(kStageMusicFadeFrames, (int)(ch.volume * 128.0f),
                                 (int)(ch.volume * 64.0f), SoundFadeEnd_None);
    }
}

// FS2 0x001d41d0
void StageSound::resume()
{
    if (isTurnedDown()) {
        m_player->fadeMaster(kStageSoundFadeFrames, m_player->m_masterVolume, kSoundVolumeFull);
        m_state = StageSoundState_TurningUp;
        setEffectsPaused(false);
    }
    m_downReason = StageSoundDown_None;
    for (int i = 0; i < kStageSoundMusicChannels; i++) {
        StageStreamChannel &ch = m_channels[i];
        if (ch.state == StageStreamChannel_Playing)
            ch.stream->startFade(kStageMusicFadeFrames, (int)(ch.volume * 64.0f),
                                 (int)(ch.volume * 128.0f), SoundFadeEnd_None);
    }
}

// FS2 0x001d43f0
void StageSound::setMuted(bool muted)
{
    if (isTurnedDown()) {
        if (!muted) {
            m_player->fadeMaster(kStageSoundFadeFrames, m_player->m_masterVolume, kSoundVolumeFull);
            m_state = StageSoundState_TurningUp;
            setEffectsPaused(false);
        }
    } else if ((m_state == StageSoundState_TurningUp || m_state == StageSoundState_Normal) && muted) {
        m_player->fadeMaster(kStageSoundFadeFrames, m_player->m_masterVolume, 0);
        m_state = StageSoundState_TurningDown;
    }
    m_downReason = muted ? StageSoundDown_Muted : StageSoundDown_None;
}

// callcheck: stop (the inline stopEffect / stopSource)
// FS2 0x001d44c0
void StageSound::setEffectsPaused(bool paused)
{
    for (int i = 0; i < kStageSoundSlots; i++) {
        StageSoundSlot &slot = m_slots[i];
        if (slot.kind == StageSoundSlot_Source) {
            StageSoundSource *s = slot.source;
            if (!s->m_entry->resumable || s->m_handle < 0 || s->m_stopped)
                continue;
            bool playing = s != nullptr && s->m_handle >= 0;
            if (playing)
                playing = m_player->isPlaying(s->m_handle);
            if (!playing)
                continue;
            SeHandle handle = paused ? m_player->pause(s->m_handle)
                                     : m_player->resume(s->m_handle, kSoundVolumeFull, kSoundVolumeFull);
            if (handle < 0) {
                stopSource(s);
            } else {
                s->m_handle = handle;
                // A paused handle ignores setAttr (SePlayer): kept as the ROM.
                if (paused && m_effectAttr != 0)
                    m_player->setAttr(handle, m_effectAttr);
            }
        } else if (slot.kind == StageSoundSlot_Effect) {
            StageEffect *e = slot.effect;
            if (!e->entry->resumable || e->handle < 0 || e->stopped)
                continue;
            SeHandle handle = paused ? m_player->pause(e->handle)
                                     : m_player->resume(e->handle, kSoundVolumeFull, kSoundVolumeFull);
            if (handle < 0) {
                stopEffect(e);
            } else {
                e->handle = handle;
                if (!paused)
                    m_player->setVolume(e->handle, kSoundVolumeFull, kSoundVolumeFull);
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Effects

// The first free slot from m_freeHint, which moves there; -1 none (the
// search does not wrap: the update 0x1d5e40 moves m_freeHint back). Inline
// in the ROM (playEffect).
int StageSound::findFreeSlot()
{
    for (int i = m_freeHint; i < kStageSoundSlots; i++) {
        if (m_slots[i].kind == StageSoundSlot_Free) {
            m_freeHint = i;
            return i;
        }
    }
    return -1;
}

// Stops an effect once. Inline in the ROM (playEffect, setEffectsPaused,
// stopNamed).
void StageSound::stopEffect(StageEffect *effect)
{
    if (effect->stopped)
        return;
    if (effect->handle >= 0)
        m_player->stop(effect->handle);
    effect->stopped = true;
}

// callcheck: operator_new, stop, strcmp (new StageEffect, the inline stopEffect and SeTable::find)
// FS2 0x001d66e0
StageEffect *StageSound::playEffect(float left, float right, const char *name, bool always)
{
    if (m_player->m_table == nullptr)
        return nullptr;
    const SeEntry *entry = m_player->m_table->find(name);
    if (entry == nullptr)
        return nullptr;
    if (!always && isTurnedDown() && !entry->resumable)
        return nullptr;

    int slot = findFreeSlot();
    if (slot < 0)
        return nullptr;

    // The ROM clamps through two constants, 0x80 (0x304de8) and 0 (sdata
    // 0x31a9b4 / 0x31a9b8).
    int volL = std::max(std::min((int)(left * 128.0f), kSoundVolumeFull), 0);
    int volR = std::max(std::min((int)(right * 128.0f), kSoundVolumeFull), 0);
    SeHandle handle = always ? m_player->playUnscaled(entry, volL, volR)
                             : m_player->play(entry, volL, volR);
    if (handle < 0)
        return nullptr;

    // PC: the ROM still fills the slot when operator new fails.
    StageEffect *effect = new StageEffect;
    effect->entry   = entry;
    effect->handle  = handle;
    effect->stopped = false;
    m_slots[slot].kind   = StageSoundSlot_Effect;
    m_slots[slot].effect = effect;
    if (m_lastSlot < slot)
        m_lastSlot = slot;

    // Turned down: a resumable effect starts paused.
    if (!always && isTurnedDown()) {
        SeHandle pausedHandle = m_player->pause(effect->handle);
        if (pausedHandle < 0)
            stopEffect(effect);
        else
            effect->handle = pausedHandle;
    }
    return effect;
}

// FS2 0x001d6a70
StageEffect *StageSound::playNamedAlways(const char *name, float volume)
{
    return playEffect(volume, volume, name, true);
}

// FS2 0x001d6a80
void StageSound::playNamed(const char *name, float volume)
{
    playEffect(volume, volume, name, false);
}

// callcheck: stop (the inline stopEffect / stopSource)
// FS2 0x001d6a90
void StageSound::stopNamed(const char *name)
{
    for (int i = 0; i < kStageSoundSlots; i++) {
        StageSoundSlot &slot = m_slots[i];
        if (slot.kind == StageSoundSlot_Effect) {
            StageEffect *e = slot.effect;
            if (strcmp(e->entry->name, name) == 0 && !e->stopped)
                stopEffect(e);
        } else if (slot.kind == StageSoundSlot_Source) {
            // The ROM reads the source's entry before its null test: kept.
            StageSoundSource *s = slot.source;
            if (strcmp(s->m_entry->name, name) == 0)
                stopSource(s);
        }
    }
}

// FS2 0x001d74b0
void StageSound::stopSource(StageSoundSource *source)
{
    if (source == nullptr || source->m_stopped)
        return;
    if (source->m_handle >= 0)
        m_player->stop(source->m_handle);
    source->m_stopped = true;
}

// ──────────────────────────────────────────────────────────────────────
// The streams' stage files

// FS2 0x001d6fc0
void StageSound::setSubtitles(void *file)
{
    for (int i = 0; i < kStageSoundChannels; i++)
        m_channels[i].stream->setSubtitles(file);
}

// FS2 0x001d7020
void StageSound::setStreamName(const char *name)
{
    for (int i = 0; i < kStageSoundChannels; i++)
        m_channels[i].stream->setName(name);
}

// FS2 0x001d7080
void StageSound::setStreamTable(void *file)
{
    resetChannels();
    for (int i = 0; i < kStageSoundChannels; i++)
        m_channels[i].stream->setTable(file);
}
