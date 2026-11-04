// game/seplayer.cpp -- the sound players: SoundTrack (an StDT table's
// streams with fsm subtitles) and SePlayer (the SEDT effect player)
// (game/seplayer.h).

#include "game/seplayer.h"

#include <stdio.h>
#include <string.h>

#include "game/areagrid.h"  /* seDistanceVolume */
#include "game/gamehooks.h" /* fsm*, stdt* */
#include "game/heap.h"      /* randUnit */
#include "sound/sound.h"

// The default name (data 0x309778): the body files' path prefix.
static const char kSoundTrackDefaultName[] = "snd";

// An ADPCM block: 16 bytes hold 28 samples (per channel).
constexpr int kAdpcmBlockBytes   = 0x10;
constexpr int kAdpcmBlockSamples = 28;

// The table files' magics.
static bool isMagic(const char *p, char a, char b, char c, char d)
{
    return p[0] == a && p[1] == b && p[2] == c && p[3] == d;
}

// The random pitch: 1 +- the entry's spread. Inline in the ROM.
static float randomPitch(const SeEntry *se)
{
    float r = randUnit();
    return (r - 0.5f) * se->pitchSpread * 2.0f + 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// SoundTrack

// FS2 0x002908d0
int SoundTrack::setAttr(int attr)
{
    if (m_entry == nullptr)
        return 1;
    if (!isActive())
        return 1;
    SoundStream::setAttr(attr);
    return 0;
}

// FS2 0x00290930
bool SoundTrack::hasSubtitles(const SeEntry *se)
{
    if (m_subtitles == nullptr)
        return false;
    int track = fsmFindTrack(m_subtitles, se->subtitleKey);
    if (track < 0)
        return false;
    return fsmTrackHasText(m_subtitles, track);
}

// The seconds played, -1 not playing (or at 0 again once started).
// Inline in the ROM (getSubtitle, update).
float SoundTrack::playedSeconds()
{
    if (!isActive())
        return -1.0f;
    int pos = getPosition();
    if (pos < 0)
        return -1.0f;
    if (m_secondHalf)
        pos -= m_size / 2;
    if (pos < 0)
        return -1.0f;
    if (pos == 0 && m_started)
        return -1.0f;
    return (float)pos * (float)kAdpcmBlockSamples / (float)kAdpcmBlockBytes
           / (float)m_channels / (float)m_rate;
}

// callcheck: getPosition (the inline playedSeconds)
// FS2 0x00290980
const u_short *SoundTrack::getSubtitle()
{
    if (m_subtitles == nullptr || !isActive() || m_subtitleTrack < 0)
        return nullptr;
    float seconds = playedSeconds();
    if (seconds < 0.0f)
        return nullptr;
    return fsmFindText(seconds, m_subtitles, m_subtitleTrack);
}

// FS2 0x00290b10
bool SoundTrack::setSubtitles(void *file)
{
    if (!isMagic((const char *)file, 'f', 's', 'm', '\0'))
        return false;
    fsmRelocate(file);
    m_subtitles = file;
    return true;
}

// callcheck: operator_delete[], operator_new[] (the delete[] / new[] expressions)
// FS2 0x00290bb0
void SoundTrack::setName(const char *name)
{
    if (m_name != nullptr)
        delete[] m_name;
    m_name = nullptr;
    if (name != nullptr) {
        m_name = new char[strlen(name) + 1];
        strcpy(m_name, name);
    }
}

// callcheck: operator_delete[], ~SoundStream, operator_delete (the delete[] expressions, the base destructor, the deleting wrapper)
// FS2 0x00290c20
SoundTrack::~SoundTrack()
{
    if (m_name != nullptr)
        delete[] m_name;
    if (m_path != nullptr)
        delete[] m_path;
}

// callcheck: SoundStream, operator_delete[], strlen, operator_new[], strcpy (the base constructor, the inline setName)
// FS2 0x00290ca0
SoundTrack::SoundTrack()
{
    m_table         = nullptr;
    m_entry         = nullptr;
    m_placed        = 0;
    m_channels      = 1;
    m_rate          = 1;
    m_secondHalf    = 0;
    m_size          = 0;
    m_subtitles     = nullptr;
    m_subtitleTrack = -1;
    m_started       = 0;
    m_name          = nullptr;
    m_path          = nullptr;
    setName(kSoundTrackDefaultName);
    setBuffer(kSoundTrackBuffer, nullptr);
}

// One body for the two inlined copies of the ROM.
void StreamTable::relocate()
{
    if (stdtIsRelocated(this))
        return;
    StreamTable *t = this;
    char *base = (char *)t;
    t->banks.set(base + t->banks.rel);
    for (int i = 0; i < t->bankCount; i++) {
        StBank &b = t->banks[i];
        b.name.set(base + t->bankStringBase + b.name.rel);
        b.file.set(base + t->bankStringBase + b.file.rel);
    }
    t->strings.set(base + t->strings.rel);
    for (int i = 0; i < t->stringCount; i++) {
        S2dRef<const char> &str = t->strings[i];
        str.set(base + t->stringBase + str.rel);
    }
    // An entry's subtitle key is a strings index, < 0 none.
    for (int i = 0; i < t->count; i++) {
        SeEntry &e = t->entries[i];
        if (e.subtitleKey.rel < 0)
            e.subtitleKey.set(nullptr);
        else
            e.subtitleKey.set(t->strings[e.subtitleKey.rel].get());
    }
    char *names = base + t->nameBase;
    t->names.set(base + t->names.rel);
    for (int i = 0; i < stdtEntryCount(t); i++) {
        SeEntry &e = t->entries[i];
        e.name.set(names + e.name.rel);
    }
    for (int i = 0; i < t->nameCount; i++) {
        SeName &n = t->names[i];
        n.name.set(names + n.name.rel);
    }
    t->relocated = 1;
}

// FS2 0x00290d60
bool SoundTrack::setTable(void *file)
{
    if (!isMagic((const char *)file, 'S', 't', 'D', 'T'))
        return false;
    m_table = (StreamTable *)file;
    m_table->relocate();
    return true;
}

// Already playing: stopped, nothing started. Inline in the ROM
// (playPlaced, playEntry).
void SoundTrack::stopForPlay()
{
    m_started = 0;
    int pos = getPosition();
    SoundStream::stop(false);
    if (pos >= 0)
        m_subtitleTrack = -1;
}

// Opens the entry's bank (its header, the body file m_name + file) and
// reads the request's size and format: the stream offset to start at, -2
// no bank or start beyond the (half) request. Inline in the ROM
// (playPlaced, playEntry).
int SoundTrack::openEntry(const SeEntry *se, int start, int secondHalf)
{
    // PC: the ROM reads the bank record at null + 4 when there is no
    // table; none plays nothing (-2).
    if (m_table == nullptr)
        return -2;
    StBank &bank = m_table->banks[se->bank];
    const char *file = bank.file;
    const void *header = bank.header;
    if (m_path != nullptr)
        delete[] m_path;
    // PC: the ROM leaves the freed pointer when there is no file (and
    // frees it again next time).
    m_path = nullptr;
    const char *path = nullptr;
    if (file != nullptr) {
        size_t nameLen = m_name != nullptr ? strlen(m_name) : 0;
        m_path = new char[nameLen + strlen(file) + 1];
        if (m_name == nullptr)
            sprintf(m_path, "%s", file);              /* data 0x309770 */
        else
            sprintf(m_path, "%s%s", m_name, file);    /* data 0x309768 */
        path = m_path;
    }
    if (path == nullptr || header == nullptr)
        return -2;

    openBankFile(header, path);
    m_size     = getRequestSize(se->note);
    m_channels = getRequestChannels(se->note);
    m_rate     = getRequestRate(se->note);
    m_secondHalf = (u_char)secondHalf;
    int block = m_channels * kAdpcmBlockBytes;
    if (!m_secondHalf) {
        int pos = start / block * block;
        if (m_size <= pos)
            return -2;
        return pos;
    }
    int half = m_size / 2 / block * block;
    if (half <= start)
        return -2;
    int pos = start + half;
    // The second half's very start skips two blocks.
    if (pos == half)
        pos += m_channels * kAdpcmBlockBytes * 2;
    return pos;
}

// callcheck: getPosition, stop, randUnit, operator_delete[], strlen, operator_new[], sprintf, openBankFile, getRequestSize, getRequestChannels, getRequestRate, fsmFindTrack (the inline stopForPlay, openEntry and randomPitch)
// FS2 0x00290fd0
int SoundTrack::playPlaced(float pan, float ratio, const SeEntry *se, int volume, int start,
                           int secondHalf)
{
    if (isActive()) {
        stopForPlay();
        return -1;
    }
    if (m_state == SoundStream_StopPending)
        return -1;
    // PC: the ROM reads se->placed before its null test.
    if (se == nullptr)
        return -1;
    if (!se->placed) {
        int id = playEntry(se, volume, volume, start, secondHalf);
        m_placed = 1;
        return id;
    }
    if (ratio > 1.0f)
        return -2;
    m_started = 0;
    int level = seDistanceVolume(ratio, se->falloff, se->volume) * volume;
    float pitch = randomPitch(se);
    int pos = openEntry(se, start, secondHalf);
    if (pos < 0)
        return -2;
    int id = playPan(se->note, pos, pan, level / kSoundVolumeFull);
    setPitch(pitch);
    m_entry  = se;
    m_placed = 1;
    if (m_subtitles == nullptr)
        m_subtitleTrack = -1;
    else
        m_subtitleTrack = fsmFindTrack(m_subtitles, se->subtitleKey);
    return id;
}

// callcheck: getPosition, stop, randUnit, operator_delete[], strlen, operator_new[], sprintf, openBankFile, getRequestSize, getRequestChannels, getRequestRate (the inline stopForPlay, openEntry and randomPitch)
// FS2 0x00291370
int SoundTrack::playEntry(const SeEntry *se, int left, int right, int start, int secondHalf)
{
    if (isActive()) {
        stopForPlay();
        return -1;
    }
    if (m_state == SoundStream_StopPending)
        return -1;
    if (se == nullptr)
        return -1;
    left  *= se->volume;
    right *= se->volume;
    m_started = 0;
    float pitch = randomPitch(se);
    int pos = openEntry(se, start, secondHalf);
    if (pos < 0)
        return -2;
    int id = playVolume(se->note, pos, left / kSoundVolumeFull, right / kSoundVolumeFull);
    setPitch(pitch);
    m_entry  = se;
    m_placed = 0;
    if (m_subtitles == nullptr)
        m_subtitleTrack = -1;
    else
        m_subtitleTrack = fsmFindTrack(m_subtitles, se->subtitleKey);
    return id;
}

// FS2 0x002916b0
bool SoundTrack::isPaused()
{
    return m_state == SoundStream_Paused;
}

// FS2 0x002916c0
bool SoundTrack::isActive()
{
    return m_state == SoundStream_Playing || m_state == SoundStream_Paused;
}

// FS2 0x002916f0
int SoundTrack::stop()
{
    m_started = 0;
    int pos = getPosition();
    SoundStream::stop(false);
    if (pos < 0)
        return -1;
    if (m_secondHalf)
        pos -= m_size / 2;
    m_subtitleTrack = -1;
    return pos;
}

// callcheck: setVolume (the inline setVolume for an unplaced entry)
// FS2 0x00291770
int SoundTrack::setPlacement(float pan, float ratio, int volume)
{
    if (!m_placed)
        return -1;
    if (m_entry == nullptr)
        return -2;
    if (!isActive())
        return -2;
    if (!m_entry->placed) {
        // An unplaced entry started through playPlaced: its volume only
        // (setVolume, inline, with the placed flag lowered for it).
        m_placed = 0;
        int result = setVolume(volume, volume);
        m_placed = 1;
        return result;
    }
    int level = seDistanceVolume(ratio, m_entry->falloff, m_entry->volume) * volume;
    return setPan(pan, level / kSoundVolumeFull);
}

// callcheck: getPosition (the inline playedSeconds)
// FS2 0x002918c0
void SoundTrack::update()
{
    if (playedSeconds() > 0.0f)
        m_started = 1;
    SoundStream::update();
}

// FS2 0x00291a00
int SoundTrack::pause(bool on)
{
    SoundStream::pause(on);
    return on;
}

// FS2 0x00291a30
int SoundTrack::setVolume(int left, int right)
{
    if (m_placed)
        return -1;
    if (m_entry == nullptr)
        return -2;
    if (!isActive())
        return -2;
    int volume = m_entry->volume;
    SoundStream::setVolume(volume * left / kSoundVolumeFull, volume * right / kSoundVolumeFull);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// SePlayer

// FS2 0x00291ac0
int SePlayer::setAttr(SeHandle handle, int attr)
{
    if (handle < 0)
        return 1;
    if (seHandlePaused(handle))
        return 0;
    int bank  = seHandleBank(handle);
    int voice = seHandleVoice(handle);
    if (!isStreamBank(bank)) {
        if (!seHandlePlaced(handle)) {
            if (!m_banks[bank]->isPlaying(voice))
                return 1;
            m_banks[bank]->setAttr(voice, attr);
        } else {
            if (!m_banks[bank]->isPanPlaying(voice))
                return 1;
            m_banks[bank]->setPanAttr(voice, attr != 0);
        }
        return 0;
    }
    SoundStream *stream = m_streams[bank];
    if (stream->m_request < 0)
        return 0;
    if (voice != stream->m_request)
        return 1;
    stream->setAttr(attr);
    return 0;
}

// FS2 0x00291c00
SeHandle SePlayer::resume(SeHandle handle, int left, int right)
{
    if (handle < 0)
        return -3;
    int bank = seHandleBank(handle);
    if (isStreamBank(bank)) {
        // Kept as the ROM: a stream that plays (or is paused) answers -2,
        // any other is unpaused.
        SoundStream *stream = m_streams[bank];
        if (stream->m_state == SoundStream_Playing || stream->m_state == SoundStream_Paused)
            return -2;
        stream->pause(false);
        return 0;
    }
    if (!seHandlePaused(handle)) {
        int voice = seHandleVoice(handle);
        if (!seHandlePlaced(handle))
            return m_banks[bank]->isPlaying(voice) ? handle : -2;
        return m_banks[bank]->isPanPlaying(voice) ? handle : -2;
    }
    const SeEntry *se = entryOf(handle);
    if (!seHandlePlaced(handle))
        return play(se, left, right);
    return playPlaced(0.0f, 0.0f, se, left, 0);
}

// FS2 0x00291d80
SeHandle SePlayer::pause(SeHandle handle)
{
    // PC: the ROM reads the bank of -1 as -1 (the table's magic as its
    // stream flag); none answers the failure.
    if (handle < 0)
        return -2;
    int bank = seHandleBank(handle);
    if (!isStreamBank(bank)) {
        if (stop(handle) != 0)
            return -2;
        return (handle & kSeHandleMask) | kSeHandlePaused;
    }
    // Kept as the ROM: a stream that plays (or is paused already) refuses.
    SoundStream *stream = m_streams[bank];
    if (stream->m_state == SoundStream_Playing || stream->m_state == SoundStream_Paused)
        return -2;
    stream->pause(true);
    return (handle & kSeHandleMask) | kSeHandlePaused;
}

// A voice bank's level change waits for the next master fade (update
// re-levels only then). Kept as the ROM.
// FS2 0x00291e50
int SePlayer::setVolume(SeHandle handle, int left, int right)
{
    if (handle < 0)
        return -3;
    if (seHandlePlaced(handle))
        return -1;
    if (seHandlePaused(handle))
        return 0;
    const SeEntry *se = entryOf(handle);
    left  *= se->volume;
    right *= se->volume;
    int bank  = seHandleBank(handle);
    int voice = seHandleVoice(handle);
    int level = left / kSoundVolumeFull;
    if (!isStreamBank(bank)) {
        if (!m_banks[bank]->isPlaying(voice))
            return -2;
        // PC: the ROM writes the record of whatever voice the handle names.
        if ((u_int)voice < (u_int)kSeVoices)
            m_voices[voice].set(SeVoice_Scaled, level, bank, false, 0.0f, 0.0f);
        return 0;
    }
    SoundStream *stream = m_streams[bank];
    if (stream->m_request < 0)
        return 0;
    if (voice != stream->m_request)
        return -2;
    stream->setVolume(level, right / kSoundVolumeFull);
    m_streamVoices[bank].set(SeVoice_Scaled, level, bank, false, 0.0f, 0.0f);
    return 0;
}

// callcheck: operator_new (new SoundStream)
// FS2 0x00292090
bool SePlayer::openStreamBank(int bank, const void *header, const void *body, int bodySize)
{
    if (m_table == nullptr)
        return false;
    if (m_streams[bank] != nullptr)
        return false;
    m_streams[bank] = new SoundStream();
    m_streams[bank]->openBankMemory(header, body, bodySize, true);
    return true;
}

// callcheck: operator_new (new SoundSeBank)
// FS2 0x00292140
bool SePlayer::loadBank(int bank, const void *xvab)
{
    if (m_table == nullptr)
        return false;
    if (m_banks[bank] != nullptr)
        return false;
    m_banks[bank] = new SoundSeBank(xvab, true, 0, 0);
    return true;
}

// FS2 0x002921d0
bool SePlayer::setTable(SeTable *table)
{
    bool ok = isMagic(table->magic, 'S', 'E', 'D', 'T');
    if (table->version < kSeTableVersion)
        ok = false;
    if (!ok)
        return false;
    if (!table->relocated) {
        char *base = (char *)table;
        char *strings = base + table->stringBase;
        table->names.set(base + table->names.rel);
        for (int i = 0; i < table->count; i++) {
            SeEntry &e = table->entries[i];
            e.name.set(strings + e.name.rel);
        }
        for (int i = 0; i < table->nameCount; i++) {
            SeName &n = table->names[i];
            n.name.set(strings + n.name.rel);
        }
        table->relocated = 1;
    }
    m_table = table;
    int depth = (int)m_table->reverb[0].depth;
    gSoundConfig->setEffect(SoundEffectCore_0, m_table->reverb[0].mode, depth, depth, 0, 0);
    depth = (int)m_table->reverb[1].depth;
    gSoundConfig->setEffect(SoundEffectCore_1, m_table->reverb[1].mode, depth, depth, 0, 0);
    return true;
}

// FS2 0x00292360
void SePlayer::fadeMaster(int frames, int from, int to)
{
    m_masterFadeFrom   = from;
    m_masterFadeTo     = to;
    m_masterFadeFrames = frames;
    m_masterFadeFrame  = 0;
    if (frames == 0) {
        m_masterVolume     = m_masterFadeTo;
        m_masterFadeFrames = -1;
    }
}

// callcheck: ~SoundSeBank, ~SoundStream, operator_delete (the delete-expressions, the deleting wrapper)
// FS2 0x002923a0
SePlayer::~SePlayer()
{
    for (int i = 0; i < kSeBanks; i++) {
        if (m_banks[i] != nullptr) {
            delete m_banks[i];
            m_banks[i] = nullptr;
        }
        if (m_streams[i] != nullptr) {
            delete m_streams[i];
            m_streams[i] = nullptr;
        }
    }
}

// FS2 0x00292450
SePlayer::SePlayer()
{
    m_table = nullptr;
    for (int i = 0; i < kSeBanks; i++) {
        m_banks[i]   = nullptr;
        m_streams[i] = nullptr;
    }
    for (int i = 0; i < kSeVoices; i++) {
        m_voices[i].clear();
        m_panVoices[i].clear();
        m_streamVoices[i].clear();
    }
    m_masterVolume     = kSoundVolumeFull;
    m_masterFadeFrom   = 0;
    m_masterFadeTo     = 0;
    m_masterFadeFrames = -1;
    m_masterFadeFrame  = 0;
}

// callcheck: randUnit (the inline randomPitch)
// FS2 0x00292520
SeHandle SePlayer::playPlaced(float pan, float ratio, const SeEntry *se, int volume, int pair)
{
    if (se == nullptr)
        return -1;
    if (ratio > 1.0f)
        return -2;
    int bank  = se->bank;
    int level = seDistanceVolume(ratio, se->falloff, se->volume) * volume / kSoundVolumeFull;
    float pitch = randomPitch(se);
    int voice;
    if (!isStreamBank(bank)) {
        if (m_banks[bank] == nullptr)
            return -1;
        voice = m_banks[bank]->playPan(pan, ratio, se->program, se->note, masterScaled(level),
                                       pair != 0);
        if (voice < 0)
            return -1;
        m_banks[bank]->setPanPitchF(voice, pitch);
        // PC: the ROM writes the record of whatever handle sdpr2 answers.
        if (voice < kSeVoices)
            m_panVoices[voice].set(SeVoice_Scaled, level, bank, true, pan, ratio);
    } else {
        if (m_streams[bank] == nullptr)
            return -1;
        m_streams[bank]->playPan(se->note, 0, pan, masterScaled(level));
        voice = se->note;
        m_streams[bank]->setPitch(pitch);
        m_streamVoices[bank].set(SeVoice_Scaled, level, bank, true, pan, ratio);
    }
    return seMakeHandle(voice, se->id, bank, true);
}

// callcheck: randUnit (the inline randomPitch)
// FS2 0x00292830
SeHandle SePlayer::playUnscaled(const SeEntry *se, int left, int right)
{
    if (se == nullptr)
        return -1;
    int bank   = se->bank;
    int volume = se->volume;
    left  *= volume;
    right *= volume;
    float pitch = randomPitch(se);
    int levelL = left / kSoundVolumeFull;
    int levelR = right / kSoundVolumeFull;
    int voice;
    if (!isStreamBank(bank)) {
        if (m_banks[bank] == nullptr)
            return -1;
        voice = m_banks[bank]->play(se->program, se->note, levelL, levelR);
        m_banks[bank]->setPitch(voice, pitch);
        // PC: the ROM writes the record of whatever voice rxSnd answers.
        if ((u_int)voice < (u_int)kSeVoices)
            m_voices[voice].set(SeVoice_Unscaled, levelL, bank, false, 0.0f, 0.0f);
    } else {
        if (m_streams[bank] == nullptr)
            return -1;
        m_streams[bank]->playVolume(se->note, 0, levelL, levelR);
        voice = se->note;
        m_streams[bank]->setPitch(pitch);
        // A stream bank's record keeps the entry's volume, not the level.
        m_streamVoices[bank].set(SeVoice_Unscaled, volume, bank, false, 0.0f, 0.0f);
    }
    return seMakeHandle(voice, se->id, bank, false);
}

// callcheck: randUnit (the inline randomPitch)
// FS2 0x00292ac0
SeHandle SePlayer::play(const SeEntry *se, int left, int right)
{
    if (se == nullptr)
        return -1;
    int bank   = se->bank;
    int volume = se->volume;
    left  *= volume;
    right *= volume;
    float pitch = randomPitch(se);
    int levelL = left / kSoundVolumeFull;
    int levelR = right / kSoundVolumeFull;
    int voice;
    if (!isStreamBank(bank)) {
        if (m_banks[bank] == nullptr)
            return -1;
        voice = m_banks[bank]->play(se->program, se->note, masterScaled(levelL),
                                    masterScaled(levelR));
        m_banks[bank]->setPitch(voice, pitch);
        // PC: the ROM writes the record of whatever voice rxSnd answers.
        if ((u_int)voice < (u_int)kSeVoices)
            m_voices[voice].set(SeVoice_Scaled, levelL, bank, false, 0.0f, 0.0f);
    } else {
        if (m_streams[bank] == nullptr)
            return -1;
        m_streams[bank]->playVolume(se->note, 0, masterScaled(levelL), masterScaled(levelR));
        voice = se->note;
        m_streams[bank]->setPitch(pitch);
        // A stream bank's record keeps the entry's volume, not the level.
        m_streamVoices[bank].set(SeVoice_Scaled, volume, bank, false, 0.0f, 0.0f);
    }
    return seMakeHandle(voice, se->id, bank, false);
}

// FS2 0x00292db0
int SePlayer::stop(SeHandle handle)
{
    if (handle < 0)
        return 0;
    int bank  = seHandleBank(handle);
    int voice = seHandleVoice(handle);
    if (!isStreamBank(bank)) {
        if (!seHandlePlaced(handle)) {
            if (m_banks[bank]->isPlaying(voice)) {
                m_banks[bank]->stop(voice);
                if ((u_int)voice < (u_int)kSeVoices)
                    m_voices[voice].clear();
            }
        } else if (m_banks[bank]->isPanPlaying(voice)) {
            m_banks[bank]->stopPan(voice);
            if ((u_int)voice < (u_int)kSeVoices)
                m_panVoices[voice].clear();
        }
        return 0;
    }
    SoundStream *stream = m_streams[bank];
    if (stream->m_request >= 0 && voice == stream->m_request) {
        stream->stop(false);
        m_streamVoices[bank].clear();
    }
    return 0;
}

// FS2 0x00292f60
bool SePlayer::isPlaying(SeHandle handle)
{
    if (handle < 0)
        return false;
    if (seHandlePaused(handle))
        return true;
    int bank  = seHandleBank(handle);
    int voice = seHandleVoice(handle);
    if (!isStreamBank(bank)) {
        if (!seHandlePlaced(handle))
            return m_banks[bank]->isPlaying(voice);
        return m_banks[bank]->isPanPlaying(voice);
    }
    SoundStream *stream = m_streams[bank];
    if (stream->m_request < 0 || voice != stream->m_request)
        return false;
    return stream->m_state == SoundStream_Playing || stream->m_state == SoundStream_Paused;
}

// FS2 0x00293070
int SePlayer::setPitch(float pitch, SeHandle handle)
{
    if (handle < 0)
        return -3;
    if (seHandlePaused(handle))
        return 1;
    int bank  = seHandleBank(handle);
    int voice = seHandleVoice(handle);
    if (!isStreamBank(bank)) {
        if (!seHandlePlaced(handle)) {
            if (!m_banks[bank]->isPlaying(voice))
                return -1;
            m_banks[bank]->setPitch(voice, pitch);
        } else {
            if (!m_banks[bank]->isPanPlaying(voice))
                return -1;
            m_banks[bank]->setPanPitchF(voice, pitch);
        }
        return 0;
    }
    SoundStream *stream = m_streams[bank];
    if (stream->m_request < 0)
        return -1;
    if (voice != stream->m_request)
        return -2;
    stream->setPitch(pitch);
    return 0;
}

// FS2 0x002931b0
int SePlayer::setPlacement(float pan, float ratio, SeHandle handle, int volume)
{
    if (handle < 0)
        return 0;
    if (!seHandlePlaced(handle))
        return -1;
    if (seHandlePaused(handle))
        return 0;
    // PC: the ROM reads the curve of a null entry (an index beyond the
    // table) at address 0x28; there is no such handle.
    const SeEntry *se = entryOf(handle);
    int level = seDistanceVolume(ratio, se->falloff, se->volume) * volume / kSoundVolumeFull;
    int bank  = seHandleBank(handle);
    int voice = seHandleVoice(handle);
    if (!isStreamBank(bank)) {
        if (!m_banks[bank]->isPanPlaying(voice))
            return 1;
        if ((u_int)voice < (u_int)kSeVoices)
            m_panVoices[voice].set(SeVoice_Scaled, level, bank, true, pan, ratio);
        return m_banks[bank]->setPan(pan, ratio, voice, masterScaled(level));
    }
    SoundStream *stream = m_streams[bank];
    if (stream->m_request < 0)
        return 0;
    if (voice != stream->m_request)
        return 1;
    m_streamVoices[bank].set(SeVoice_Scaled, level, bank, true, pan, ratio);
    // Kept as the ROM: a stream bank's pan is not under the master volume
    // here (update applies it while a fade runs).
    stream->setPan(pan, level);
    return 0;
}

// A record's level now: under the master volume or as it is; -1 free.
// Inline in the ROM (update).
static int voiceLevel(const SeVoice &v, int master)
{
    if (v.scale == SeVoice_Scaled)
        return master * v.volume / kSoundVolumeFull;
    if (v.scale == SeVoice_Unscaled)
        return v.volume;
    return -1;
}

// FS2 0x00293440
void SePlayer::update()
{
    bool fading = false;
    if (m_masterFadeFrames >= 0) {
        m_masterFadeFrame++;
        fading = true;
        if (m_masterFadeFrame < m_masterFadeFrames) {
            m_masterVolume = m_masterFadeFrom
                + m_masterFadeFrame * (m_masterFadeTo - m_masterFadeFrom) / m_masterFadeFrames;
        } else {
            m_masterFadeFrames = -1;
            m_masterFadeFrame  = 0;
            m_masterVolume     = m_masterFadeTo;
        }
    }
    if (fading) {
        for (int i = 0; i < kSeVoices; i++) {
            SeVoice &v = m_voices[i];
            if (v.scale != SeVoice_Free) {
                SoundSeBank *bank = m_banks[v.bank];
                if (!bank->isPlaying(i)) {
                    v.clear();
                } else {
                    int level = voiceLevel(v, m_masterVolume);
                    bank->setVolume(i, level, level);
                }
            }
            SeVoice &p = m_panVoices[i];
            if (p.scale != SeVoice_Free) {
                SoundSeBank *bank = m_banks[p.bank];
                if (!bank->isPanPlaying(i))
                    p.clear();
                else
                    bank->setPan(p.pan, p.ratio, i, voiceLevel(p, m_masterVolume));
            }
            SeVoice &s = m_streamVoices[i];
            if (s.scale != SeVoice_Free) {
                SoundStream *stream = m_streams[s.bank];
                if (stream == nullptr || stream->m_request < 0) {
                    s.clear();
                } else {
                    int level = voiceLevel(s, m_masterVolume);
                    if (!s.placed)
                        stream->setVolume(level, level);
                    else
                        stream->setPan(s.pan, level);
                }
            }
        }
    }
    for (int i = 0; i < kSeBanks; i++) {
        if (m_streams[i] != nullptr)
            m_streams[i]->update();
    }
}
