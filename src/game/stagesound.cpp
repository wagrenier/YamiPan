// game/stagesound.cpp -- the stage's sound: its effect player, effect
// slots and stream channels (game/stagesound.h).

#include "game/stagesound.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "game/gamehooks.h"
#include "game/heap.h"      /* randNext */
#include "game/mathutil.h"
#include "game/stageloop.h"
#include "game/sysdata.h"
#include "sound/sound.h"

// isStopped also counts a stream state 5 as stopped; SoundStream never
// sets it (sound/sound.h's SoundStreamState ends at 4). Kept as the ROM.
constexpr int kSoundStreamState5 = 5;

// The bone a character hears and is heard from (sdata 0x303178).
static const char kHeadBone[] = "bn_head";

// A cue's fade-in is given in 30 fps frames; the PAL game runs 25.
constexpr int kCueFadeFps = 30;
constexpr int kGameFps    = 25;

// StageScene::raycast's arguments for the cue streams' occlusion: the
// first ray, then one past a thin wall (meaning of the bits unread).
constexpr u_int kSoundRayMask       = 0x0e42600f;
constexpr u_int kSoundRayFlags      = 0x200;
constexpr u_int kSoundRayFlagsAgain = 0x600;

// A cue stream behind a wall: past its range (the update stops it).
constexpr float kSoundRatioBlocked = 1.1f;

// The muffled listener's distance scale (stageSoundLocateFacing; the ROM's
// 0x304df0 / 0x304df8 and an immediate 2.5).
constexpr float kMuffleMin   = 0.4f;
constexpr float kMuffleMax   = 1.0f;
constexpr float kMuffleScale = 2.5f;

// What the channel update finds between the listener and a cue stream.
enum SoundOcclusion
{
    SoundOcclusion_None    = 0,
    SoundOcclusion_Thin    = 1,     // one thin wall: twice the distance
    SoundOcclusion_Blocked = 2,     // out of range
};

// A cue stops faded out over 10 frames (stopCuesOf: 4).
constexpr int kCueStopFrames     = 10;
constexpr int kCueStopFastFrames = 4;

// pickCueChannel's scores: the cue channel taken is the best one, a tie
// the first. A busy channel scores only for a cue that matters at least as
// much (SeEntry::priority, lower first).
constexpr u_int kCueScoreNone      = 0;
constexpr u_int kCueScoreSameOwner = 200;   // replaces the owner's own cue
constexpr u_int kCueScoreFree      = 100;
constexpr u_int kCueScoreOther     = 50;    // replaces another owner's cue
// A cue behind a thin wall is heard to half its range.
constexpr float kCueThinWallRatio = 0.5f;

// getCueSubtitle skips a cue entry with this priority; a short never has
// it (kept as the ROM).
constexpr int kCuePriorityNoSubtitle = 0x7fffffff;

// The longest SE name setSeTable can pair (its stack buffer).
constexpr int kSeNameMax = 32;

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

// A source follows the scene flag when its entry's name starts "CAR_"
// (the ROM compares the four characters).
StageSoundSource::StageSoundSource(const SeEntry *entry, int variant)
    : m_handle(-1), m_entry(entry), _10c(1.0f), m_variant(variant), m_stopped(false)
{
    const char *name = entry->name;
    m_bySceneFlag = name[0] == 'C' && name[1] == 'A' && name[2] == 'R' && name[3] == '_';
}

// callcheck: ~StageNodeBase, operator_delete (the base dtor, deleting)
// FS2 0x001d3e90
StageSoundSource::~StageSoundSource()
{
}

// FS2 0x001d7690
void StageStreamCue::clear()
{
    source     = nullptr;
    def        = nullptr;
    owner      = nullptr;
    variant    = 0;
    resumeAt   = 0;
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
    m_playerFlag = 0;
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
    m_banksApplied = 0;
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
    m_playerFlag = 0;
    m_sceneFlag  = 0;
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
// Positional sources

// callcheck: strcmp (the inline SeTable::find)
// FS2 0x001d6bd0
StageSoundSource *StageSound::addSourceNamed(const char *name, const float *pos,
                                             StageNodeBase *parent)
{
    SeTable *table = m_player->m_table;
    if (table == nullptr)
        return nullptr;
    const SeEntry *entry = table->find(name);
    if (entry == nullptr)
        return nullptr;
    return addSource(entry, pos, parent);
}

// callcheck: operator_new, StageNodeBase, NodeTransform, erase, pushBack (new StageSoundSource with its bases, the inline setParent)
// FS2 0x001d6d10
StageSoundSource *StageSound::addSource(const SeEntry *entry, const float *pos,
                                        StageNodeBase *parent)
{
    if (m_player->m_table == nullptr || entry == nullptr)
        return nullptr;
    int slot = findFreeSlot();
    if (slot < 0)
        return nullptr;

    // Unlike a cue, the cycle keeps the variant played.
    int variant = 0;
    if (entry->variants < 0) {
        variant = (int)randNext() % abs(entry->variants);
    } else if (entry->variants > 0) {
        variant = (entry->cycle + 1) % entry->variants;
        entry->cycle = (u_char)variant;
    }

    // PC: the ROM goes on with a null source when operator new fails.
    StageSoundSource *source = new StageSoundSource(entry, variant);
    if (parent != nullptr)
        source->setParent(parent);
    source->m_xform.setPosition(pos);
    m_slots[slot].kind   = StageSoundSlot_Source;
    m_slots[slot].source = source;
    if (m_lastSlot < slot)
        m_lastSlot = slot;
    return source;
}

// FS2 0x001d5510
StageSoundSource *StageSound::findSource(StageSoundSource *source)
{
    if (source == nullptr)
        return nullptr;
    for (int i = 0; i <= m_lastSlot; i++) {
        if (m_slots[i].kind == StageSoundSlot_Source && m_slots[i].source == source)
            return source;
    }
    return nullptr;
}

// FS2 0x001d73b0
bool StageSound::setSourcePitch(StageSoundSource *source, float pitch)
{
    StageSoundSource *found = findSource(source);   // inline in the ROM
    if (found == nullptr || found->m_handle < 0 || !m_player->isPlaying(found->m_handle))
        return false;
    return m_player->setPitch(pitch, source->m_handle) == 0;
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

// callcheck: release, strcmp (the inline releaseSlots and SeTable::find)
// FS2 0x001d70e0
void StageSound::setSeTable(SeTable *table)
{
    releaseSlots();
    m_player->fadeMaster(0, m_player->m_masterVolume, kSoundVolumeFull);
    m_state   = StageSoundState_Normal;
    m_seTable = table;
    m_player->setTable(m_seTable);
    SeTable *se = m_player->m_table;
    if (se == nullptr)
        return;

    // An entry X_J / X_D is X's alternate: they point at each other. An
    // entry is reset when its turn comes, so a base after its alternate in
    // the table loses the link again (kept as the ROM).
    int count = m_seTable->count;
    for (int i = 0; i < count; i++) {
        SeEntry *entry = &se->entries[i];
        const char *name = entry->name;
        size_t len = strlen(name);
        entry->altOffset = 0;
        if (len < 2 || name[len - 2] != '_' || (name[len - 1] != 'J' && name[len - 1] != 'D'))
            continue;
        // PC: the ROM copies into its 32-byte buffer whatever the length.
        if (len >= (size_t)kSeNameMax)
            continue;
        char baseName[kSeNameMax];
        strcpy(baseName, name);
        baseName[len - 2] = '\0';
        SeEntry *base = const_cast<SeEntry *>(se->find(baseName));
        if (base != nullptr) {
            signed char offset = (signed char)(entry - base);
            base->altOffset  = offset;
            entry->altOffset = -offset;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Placing sounds around the listener

// The listener's facing: the angle of its matrix's row 1. Inline in the
// ROM (stageSoundLocate, the update's halves; they call getMatrix twice).
static float listenerFacing(StageObject *listener)
{
    const sceVu0FVECTOR *m = listener->getMatrix();
    return atan2Octant(m[1][0], m[1][1]);
}

// callcheck: getStageSelect, getKind, getMatrix (the listener's virtuals)
// FS2 0x001d7a30
float stageSoundLocateFacing(float facing, float *pan, const sceVu0FVECTOR pos,
                             StageObject *listener)
{
    const sceVu0FVECTOR *m = listener->getMatrix();
    sceVu0FVECTOR d;
    for (int i = 0; i < 4; i++)
        d[i] = pos[i] - m[3][i];        // vsub
    float dist = sqrtf(d[1] * d[1] + d[0] * d[0] + d[2] * d[2]);
    *pan = -addAngle(atan2Octant(d[0], d[1]), -facing);

    bool muffled = false;
    if (listener->getKind() == StageObject_Kind11) {
        StageObject *linked = listener->m_linked;
        if (linked != nullptr) {
            int set;
            switch (getStageSelect()->kind) {
            case StageKind_Hard:   set = 2; break;
            case StageKind_Normal: set = 1; break;
            case StageKind_Easy:   set = 0; break;
            default:               set = 1; break;
            }
            int index = linked->m_itemDef;
            // PC: the ROM reads the kind at address 0x10 for an index out
            // of range; none muffles here.
            if (index >= 0 && index < kStageObjRecordCount)
                muffled = gStageObjRecords[set][index].kind == kStageObjRecordMuffles;
        }
    }
    if (muffled) {
        float scale = 1.0f - (kMathPi / 4) / fabsf(*pan);
        scale = std::min(std::max(scale, kMuffleMin), kMuffleMax);
        dist /= scale * kMuffleScale;
    }
    return dist;
}

// callcheck: atan2Octant (the inline listenerFacing)
// FS2 0x001d79c0
float stageSoundLocate(float *pan, const sceVu0FVECTOR pos, StageObject *listener)
{
    return stageSoundLocateFacing(listenerFacing(listener), pan, pos, listener);
}

// ──────────────────────────────────────────────────────────────────────
// The update

// What lies between from and to for a sound: a first ray, and past a thin
// wall a second one. Inline in the ROM (updateChannels, pickCueChannel).
static int soundOcclusion(const sceVu0FVECTOR from, const sceVu0FVECTOR to)
{
    sceVu0FVECTOR hit;
    StageCollisionFace *face = gStageScene->raycast(hit, from, to, 0, kSoundRayMask,
                                                    kSoundRayFlags, nullptr);
    if (face == nullptr)
        return SoundOcclusion_None;
    if (!(face->flags & StageCollision_Thin))
        return SoundOcclusion_Blocked;
    sceVu0FVECTOR past;
    for (int k = 0; k < 4; k++)
        past[k] = hit[k];
    if (gStageScene->raycast(hit, past, to, 0, kSoundRayMask, kSoundRayFlagsAgain,
                             nullptr) != nullptr)
        return SoundOcclusion_Blocked;
    return SoundOcclusion_Thin;
}

// callcheck: getKind, getMatrix, find, getSoundAttr (the listener's virtuals, the bone's)
// FS2 0x001d5580
void StageSound::update(StageObject *listener, u_char playerFlag, u_char sceneFlag)
{
    u_char attr;
    if (listener == nullptr) {
        attr = 0;
    } else {
        const sceVu0FVECTOR *m;
        if (listener->getKind() == StageObject_Kind9 && listener->m_linked != nullptr) {
            // Hears from the linked object's head. The ROM does not test
            // the find for null.
            BoneMap::Node *head = listener->m_linked->m_model->boneMap.find(kHeadBone);
            m = head->value->getMatrix();
        } else {
            m = listener->getMatrix();
        }
        attr = gStageScene->getSoundAttr(m[3]);
    }
    updateSlots(listener, playerFlag, sceneFlag, attr);
    updateChannels(listener, playerFlag, attr);
    m_playerFlag = playerFlag;
    m_sceneFlag  = sceneFlag;
    m_effectAttr = attr;
}

// The variant a source plays, or its alternate when the flag it follows
// is set. Inline in the ROM (four copies in updateSlots).
static const SeEntry *sourceEntry(const StageSoundSource *s, u_char flag)
{
    const SeEntry *e = &s->m_entry[s->m_variant];
    if (flag != 0)
        e += e->altOffset;
    return e;
}

// callcheck: atan2Octant, update, isPlaying, operator_delete, setEffectsPaused, play, playPlaced, setPlacement, setVolume, setAttr, stop, getWorldMatrix, updateMatrix (the inline stopSource / listenerFacing)
// FS2 0x001d5e40
void StageSound::updateSlots(StageObject *listener, u_char playerFlag, u_char sceneFlag,
                             u_char attr)
{
    if (m_player->m_table == nullptr)
        return;
    m_player->update();

    // Free what ended; the free-slot search starts again at the first free
    // one.
    int firstFree = kStageSoundSlots;
    int last      = -1;
    for (int i = 0; i < kStageSoundSlots; i++) {
        StageSoundSlot &slot = m_slots[i];
        if (slot.kind != StageSoundSlot_Free) {
            if (slot.kind == StageSoundSlot_Effect) {
                // An effect without a handle stays (kept as the ROM).
                StageEffect *e = slot.effect;
                bool ended = e != nullptr && e->handle >= 0 && !m_player->isPlaying(e->handle);
                if (ended) {
                    delete e;
                    slot.kind = StageSoundSlot_Free;
                    slot.ptr  = nullptr;
                }
            } else {
                StageSoundSource *s = slot.source;
                bool ended = s->m_handle < 0 ? s->m_stopped : !m_player->isPlaying(s->m_handle);
                if (ended) {
                    if (s != nullptr)
                        delete s;       // virtual slot 2, deleting
                    slot.kind = StageSoundSlot_Free;
                    slot.ptr  = nullptr;
                }
            }
        }
        if (slot.kind == StageSoundSlot_Free)
            firstFree = std::min(firstFree, i);
        else
            last = std::max(last, i);
    }
    m_freeHint = firstFree;
    m_lastSlot = last;

    // The master fade is done: turned down (the resumable effects pause)
    // or back up.
    if (m_player->m_masterFadeFrames < 0) {
        if (m_player->m_masterVolume == 0) {
            if (m_state != StageSoundState_Down && m_state == StageSoundState_TurningDown) {
                m_state = StageSoundState_Down;
                setEffectsPaused(true);
            }
        } else if (m_state == StageSoundState_TurningUp) {
            m_state = StageSoundState_Normal;
        }
    }

    if (listener == nullptr)
        return;

    // The positional sources.
    bool sceneFlagChanged  = sceneFlag != m_sceneFlag;
    bool playerFlagChanged = playerFlag != m_playerFlag;
    float facing = listenerFacing(listener);
    int count = m_lastSlot + 1;
    for (int i = 0; i < count; i++) {
        if (m_slots[i].kind != StageSoundSlot_Source)
            continue;
        StageSoundSource *s = m_slots[i].source;
        if (s->m_stopped) {
            m_player->setAttr(s->m_handle, attr);
            continue;
        }
        if (s->m_parent == nullptr)
            s->updateMatrix();
        float pan;
        float dist = stageSoundLocateFacing(facing, &pan, s->getWorldMatrix()[3], listener);
        const SeEntry *entry = s->m_entry;
        float ratio = 0.0f;
        if (entry->range != 0.0f) {
            ratio = dist / entry->range;
            if (ratio > 1.0f) {
                // Out of range: a resumable one plays on at the edge.
                if (!entry->resumable) {
                    stopSource(s);
                    continue;
                }
                ratio = 1.0f;
            }
        }

        if (!entry->placed) {
            // Not positional: full volume wherever the listener is.
            if (s->m_handle < 0) {
                if (!isTurnedDown()) {
                    SeHandle handle = m_player->play(&entry[s->m_variant], kSoundVolumeFull,
                                                     kSoundVolumeFull);
                    if (handle < 0)
                        stopSource(s);
                    else
                        s->m_handle = handle;
                }
            } else {
                m_player->setVolume(s->m_handle, kSoundVolumeFull, kSoundVolumeFull);
            }
        } else if (s->m_handle < 0) {
            // Not started yet (turned down: waits).
            if (!isTurnedDown()) {
                u_char flag = s->m_bySceneFlag ? sceneFlag : playerFlag;
                SeHandle handle = m_player->playPlaced(pan, ratio, sourceEntry(s, flag),
                                                       kSoundVolumeFull, 0);
                if (handle < 0)
                    stopSource(s);
                else
                    s->m_handle = handle;
            }
        } else {
            // Playing: a change of the flag it follows restarts it on the
            // other entry (a resumable one with an alternate, not while
            // turned down); else it moves.
            bool restart = false;
            if ((playerFlagChanged || sceneFlagChanged) && entry->altOffset != 0 &&
                entry->resumable && !isTurnedDown())
                restart = true;
            bool changed = s->m_bySceneFlag ? sceneFlagChanged : playerFlagChanged;
            if (restart && changed) {
                m_player->stop(s->m_handle);
                u_char flag = s->m_bySceneFlag ? sceneFlag : playerFlag;
                SeHandle handle = m_player->playPlaced(pan, ratio, sourceEntry(s, flag),
                                                       kSoundVolumeFull, 0);
                if (handle < 0)
                    stopSource(s);      // stops the old handle again: kept as the ROM
                else
                    s->m_handle = handle;
            } else {
                m_player->setPlacement(pan, std::min(ratio, 1.0f), s->m_handle, kSoundVolumeFull);
            }
        }
        if (s->m_handle >= 0)
            m_player->setAttr(s->m_handle, attr);
    }
}

// callcheck: atan2Octant, update, isPaused, isActive, stop, playEntry, playPlaced, setPlacement, setAttr, pause, startFade, raycast, isFullInMode2, getMatrix, getWorldMatrix (the inline listenerFacing / soundOcclusion)
// FS2 0x001d56a0
void StageSound::updateChannels(StageObject *listener, u_char playerFlag, u_char attr)
{
    // PC: the ROM reads the listener's matrix without a null test (a
    // stage always has its active character here).
    if (listener == nullptr)
        return;
    u_char lastPlayerFlag = m_playerFlag;
    float facing = listenerFacing(listener);

    for (u_int i = 0; i < kStageSoundChannels; i++) {
        StageStreamChannel &ch = m_channels[i];
        SoundTrack *stream = ch.stream;
        if (stream == nullptr)
            continue;
        stream->update();
        bool isCue = i >= kStageSoundMusicChannels;

        // The channel follows its stream.
        int streamState = stream->m_state;
        bool busy = streamState == SoundStream_Playing || streamState == SoundStream_Paused ||
                    streamState == SoundStream_StopPending;
        if (!busy) {
            if (ch.state != StageStreamChannel_Idle)
                ch.state = StageStreamChannel_Idle;
        } else if (ch.state == StageStreamChannel_Idle) {
            // Playing without the channel having started it: a cue stream
            // only with its cue.
            bool adopt = true;
            if (isCue)
                adopt = ch.cue.source != nullptr && ch.cue.def != nullptr;
            if (adopt)
                ch.state = StageStreamChannel_Playing;
        } else if (!stream->isPaused()) {
            if (ch.state != StageStreamChannel_Playing &&
                (ch.state == StageStreamChannel_Paused || ch.state == StageStreamChannel_Starting))
                ch.state = StageStreamChannel_Playing;
        } else {
            switch (ch.state) {
            case StageStreamChannel_Paused:
                break;
            case StageStreamChannel_Stopping:
                // The fade-out paused it: stop it now.
                stream->stop();
                break;
            default:
                ch.state = StageStreamChannel_Paused;
                break;
            }
        }

        bool active    = ch.state != StageStreamChannel_Idle;
        bool startNext = m_downReason == StageSoundDown_None && !active && ch.next.def != nullptr;
        if (active || startNext) {
            if (!isCue) {
                // Music: the next cue starts, fading in.
                if (startNext) {
                    int id = stream->playEntry(&ch.next.def[ch.next.variant], kSoundVolumeFull,
                                               kSoundVolumeFull, 0, 0);
                    if (id >= 0) {
                        int fade = ch.next.fadeFrames < 1 ? 0
                                 : ch.next.fadeFrames * kGameFps / kCueFadeFps;
                        stream->startFade(fade, 0, kSoundVolumeFull, SoundFadeEnd_None);
                        ch.state = StageStreamChannel_Starting;
                        ch.cue   = ch.next;
                    }
                    ch.next.clear();
                }
            } else {
                // A cue stream: placed at its source's head.
                if (startNext)
                    ch.cue = ch.next;
                float pan;
                float dist = stageSoundLocateFacing(facing, &pan,
                                                    ch.cue.source->getWorldMatrix()[3], listener);
                float ratio = 0.0f;
                if (ch.cue.def->range != 0.0f) {
                    const sceVu0FVECTOR *from = listener->getMatrix();
                    const sceVu0FVECTOR *to   = ch.cue.source->getWorldMatrix();
                    int occlusion = soundOcclusion(from[3], to[3]);
                    if (occlusion == SoundOcclusion_Blocked)
                        ratio = kSoundRatioBlocked;
                    else if (occlusion == SoundOcclusion_Thin)
                        ratio = dist * 2.0f / ch.cue.def->range;
                    else if (occlusion == SoundOcclusion_None)
                        ratio = dist / ch.cue.def->range;
                }

                if (startNext) {
                    // In range: starts where it stopped (resumeAt).
                    if (ratio <= 1.0f) {
                        int id = stream->playPlaced(pan, ratio, &ch.next.def[ch.next.variant],
                                                    kSoundVolumeFull, ch.next.resumeAt,
                                                    playerFlag);
                        stream->startFade(0, 0, kSoundVolumeFull, SoundFadeEnd_None);
                        if (id >= 0) {
                            ch.state = StageStreamChannel_Starting;
                            ch.cue   = ch.next;
                        }
                    }
                    ch.next.clear();
                } else if (lastPlayerFlag == playerFlag || ch.state == StageStreamChannel_Idle ||
                           ch.state == StageStreamChannel_Stopping) {
                    if (ratio <= 1.0f) {
                        stream->setPlacement(pan, ratio, kSoundVolumeFull);
                    } else {
                        stream->stop();
                        ch.state = StageStreamChannel_Stopping;
                    }
                } else {
                    // The player flag changed: stop, and start it again
                    // from there next frame (playPlaced takes the flag).
                    int pos = stream->stop();
                    ch.state = StageStreamChannel_Stopping;
                    if (pos >= 0) {
                        ch.next          = ch.cue;
                        ch.next.resumeAt = pos;
                    }
                }
                if (stream->isActive())
                    stream->setAttr(attr);
            }
        }

        // Playing or paused: follow the turn-down. Music pauses only when
        // muted (pause fades it to half), the cue streams for any reason.
        int state = ch.state;
        if (state != StageStreamChannel_Idle && state != StageStreamChannel_Pausing &&
            state != StageStreamChannel_Stopping && state != StageStreamChannel_Starting) {
            bool wantDown = isCue ? m_downReason != StageSoundDown_None
                                  : m_downReason == StageSoundDown_Muted;
            if ((state == StageStreamChannel_Paused) != wantDown) {
                if (!wantDown) {
                    if (!gStageScene->fade->isFullInMode2()) {
                        stream->pause(false);
                        stream->startFade(kStageSoundFadeFrames, 0, kSoundVolumeFull,
                                          SoundFadeEnd_None);
                        ch.state = StageStreamChannel_Starting;
                    }
                } else {
                    stream->startFade(kStageSoundFadeFrames, kSoundVolumeFull, 0,
                                      SoundFadeEnd_Pause);
                    ch.state = StageStreamChannel_Pausing;
                }
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The cue streams and the music

// The cue that counts on a channel: the queued one, else the one playing;
// null none.
static const StageStreamCue *liveCue(const StageStreamChannel &ch)
{
    if (ch.next.def != nullptr)
        return &ch.next;
    if (ch.state != StageStreamChannel_Idle)
        return &ch.cue;
    return nullptr;
}

void StageSound::fadeOutChannel(StageStreamChannel &ch, int frames)
{
    if (ch.state == StageStreamChannel_Stopping || ch.state == StageStreamChannel_Idle)
        return;
    ch.stream->startFade(frames, kSoundVolumeFull, 0, SoundFadeEnd_Stop);
    ch.state = StageStreamChannel_Stopping;
}

int StageSound::pickCueVariant(SeEntry *def)
{
    if (def->variants == 0)
        return 0;
    if (def->variants < 0)
        return (int)randNext() % abs(def->variants);
    return def->cycle++ % def->variants;
}

// callcheck: find (the bone map's, the owner's head)
void StageSound::queueCue(StageStreamChannel &ch, SeEntry *def, StageObject *owner, int variant)
{
    ch.next.source     = owner->m_model->boneMap.find(kHeadBone)->value;
    ch.next.def        = def;
    ch.next.owner      = owner;
    ch.next.variant    = variant;
    ch.next.resumeAt   = 0;
    ch.next.fadeFrames = 0;
    fadeOutChannel(ch, kCueStopFrames);
}

// FS2 0x001d46e0
const u_short *StageSound::getCueSubtitle()
{
    int found = -1;
    for (int i = 0; i < kStageSoundChannels - kStageSoundMusicChannels; i++) {
        StageStreamChannel &ch = m_channels[kStageSoundMusicChannels + i];
        if (ch.state == StageStreamChannel_Idle)
            continue;
        if (!ch.stream->hasSubtitles(&ch.cue.def[ch.cue.variant]))
            continue;
        int priority = ch.cue.def->priority;
        if (priority != kCuePriorityNoSubtitle)
            found = i;
    }
    if (found < 0)
        return nullptr;
    return m_channels[kStageSoundMusicChannels + found].stream->getSubtitle();
}

// callcheck: startFade (the inline fadeOutChannel)
// FS2 0x001d47e0
void StageSound::stopCue(StageObject *owner, const SeEntry *def)
{
    for (int i = kStageSoundMusicChannels; i < kStageSoundChannels; i++) {
        StageStreamChannel &ch = m_channels[i];
        const StageStreamCue *cue = liveCue(ch);
        if (cue != nullptr && cue->owner == owner && cue->def == def)
            fadeOutChannel(ch, kCueStopFrames);
    }
}

// callcheck: startFade (the inline fadeOutChannel)
// FS2 0x001d4920
void StageSound::stopCuesOf(StageObject *owner)
{
    for (int i = kStageSoundMusicChannels; i < kStageSoundChannels; i++) {
        StageStreamChannel &ch = m_channels[i];
        const StageStreamCue *cue = liveCue(ch);
        if (cue != nullptr && cue->owner == owner)
            fadeOutChannel(ch, kCueStopFastFrames);
    }
}

// FS2 0x001d4a30
bool StageSound::hasCue(StageObject *owner)
{
    for (int i = kStageSoundMusicChannels; i < kStageSoundChannels; i++) {
        const StageStreamChannel &ch = m_channels[i];
        if (ch.next.def != nullptr && ch.next.owner == owner)
            return true;
        if (ch.state != StageStreamChannel_Idle && ch.cue.owner == owner)
            return true;
    }
    return false;
}

// callcheck: strcmp (the inline StreamTable::find)
// FS2 0x001d4aa0
bool StageSound::isCuePlaying(const char *name, StageObject *owner)
{
    if (m_channels[0].stream->m_table == nullptr)
        return false;
    StreamTable *table = m_channels[kStageSoundMusicChannels].stream->m_table;
    SeEntry *entry = table != nullptr ? table->find(name) : nullptr;
    if (entry == nullptr)
        return false;
    for (int i = kStageSoundMusicChannels; i < kStageSoundChannels; i++) {
        const StageStreamChannel &ch = m_channels[i];
        if (ch.next.def != nullptr && ch.next.def == entry && ch.next.owner == owner)
            return true;
        if (ch.state != StageStreamChannel_Idle && ch.cue.def == entry && ch.cue.owner == owner)
            return true;
    }
    return false;
}

// callcheck: abs, randNext, find, startFade (the inline pickCueVariant / queueCue)
// FS2 0x001d4c10
bool StageSound::setCue(SeEntry *def, StageObject *owner, int channel)
{
    StageStreamChannel &ch = m_channels[kStageSoundMusicChannels + channel];
    queueCue(ch, def, owner, pickCueVariant(def));
    return true;
}

// callcheck: abs, randNext, find, startFade (the inline pickCueVariant / queueCue)
// FS2 0x001d4d30
bool StageSound::playCue(SeEntry *def, StageObject *owner, StageObject *listener)
{
    if (m_channels[0].stream->m_table == nullptr)
        return false;
    int channel = pickCueChannel(def, owner, listener);
    if (channel < 0)
        return false;
    StageStreamChannel &ch = m_channels[kStageSoundMusicChannels + channel];
    queueCue(ch, def, owner, pickCueVariant(def));
    return true;
}

// callcheck: find, getMatrix, raycast (the owner's head bone and its virtual, the inline soundOcclusion)
// FS2 0x001d4e70
int StageSound::pickCueChannel(SeEntry *def, StageObject *owner, StageObject *listener)
{
    if (m_channels[0].stream->m_table == nullptr)
        return StageCueReject_NoTable;

    const u_int cueChannels = kStageSoundChannels - kStageSoundMusicChannels;
    u_int best      = cueChannels;
    u_int bestScore = kCueScoreNone;
    for (u_int i = 0; i < cueChannels; i++) {
        const StageStreamCue *cue = liveCue(m_channels[kStageSoundMusicChannels + i]);
        u_int score;
        if (cue == nullptr)
            score = kCueScoreFree;
        else if (def->priority <= cue->def->priority)
            score = cue->owner == owner ? kCueScoreSameOwner : kCueScoreOther;
        else
            score = kCueScoreNone;
        if (bestScore < score) {
            bestScore = score;
            best      = i;
        }
    }
    if (best >= cueChannels)
        return StageCueReject_NoChannel;

    // Heard from the owner's head: within range and not behind a wall.
    StageNodeBase *head = owner->m_model->boneMap.find(kHeadBone)->value;
    float pan;
    float dist  = stageSoundLocate(&pan, head->getMatrix()[3], listener);
    float ratio = 0.0f;
    if (def->range != 0.0f) {
        ratio = dist / def->range;
        if (ratio > 1.0f)
            return StageCueReject_OutOfRange;
    }
    const sceVu0FVECTOR *from = listener->getMatrix();
    const sceVu0FVECTOR *to   = head->getMatrix();
    int occlusion = soundOcclusion(from[3], to[3]);
    if (occlusion == SoundOcclusion_Blocked ||
        (occlusion == SoundOcclusion_Thin && ratio > kCueThinWallRatio))
        return StageCueReject_Occluded;
    return (int)best;
}

// callcheck: strcmp (the inline StreamTable::find)
// FS2 0x001d5140
SeEntry *StageSound::findCue(const char *name)
{
    StreamTable *table = m_channels[kStageSoundMusicChannels].stream->m_table;
    return table != nullptr ? table->find(name) : nullptr;
}

// FS2 0x001d5210
void StageSound::setMusicVolume(int percent)
{
    // The ROM clamps through 0 (sdata 0x31a9b4) and 0x80 (0x304de0).
    int volume = std::min(std::max((int)(percent / 100.0f * 128.0f), 0), kSoundVolumeFull);
    m_channels[0].stream->setVolume(volume, volume);
    m_channels[0].volume = percent / 100.0f;
}

// callcheck: startFade (the inline fadeOutChannel)
// FS2 0x001d52d0
void StageSound::stopMusic(int channel, int fadeFrames)
{
    if (m_channels[0].stream->m_table == nullptr)
        return;
    fadeOutChannel(m_channels[channel], fadeFrames * kGameFps / kCueFadeFps);
}

// callcheck: strcmp, startFade (the inline StreamTable::find / fadeOutChannel)
// FS2 0x001d5390
void StageSound::playMusic(int channel, const char *name, int fadeFrames)
{
    if (m_channels[0].stream->m_table == nullptr)
        return;
    StageStreamChannel &ch = m_channels[channel];
    StreamTable *table = ch.stream->m_table;
    SeEntry *entry = table != nullptr ? table->find(name) : nullptr;
    if (entry == nullptr)
        return;
    // Queued; the update starts it once what plays has faded out.
    ch.next.clear();
    ch.next.def = entry;
    fadeOutChannel(ch, kCueStopFrames);
    ch.next.fadeFrames = fadeFrames;
}
