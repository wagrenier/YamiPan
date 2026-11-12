/* ==========================================================================
 *  game/seplayer.h
 *
 *  The sound players (game/seplayer.cpp, 0x2908d0..0x2937e0):
 *
 *  - SoundTrack: a SoundStream that plays the requests of an StDT stream
 *    table (the music and the streamed cues), with an fsm subtitle file
 *    timed by the stream's position. StageSound owns four.
 *  - SePlayer: the sound-effect player over an SEDT effect table: up to 16
 *    banks, each an SPU voice bank (SoundSeBank) or a stream bank
 *    (SoundStream), a master volume with a fade, and a record of every
 *    playing voice so the fade can re-level them. StageSound owns one,
 *    getSePlayer (game/gamehooks.h) is the one outside a stage.
 *
 *  Both tables are files relocated in place once (S2dRef, game/s2ddata.h),
 *  so they keep their EE layout on the host (asserted).
 *
 *  The file name is the port's (no source string).
 * ======================================================================== */
#ifndef _GAME_SEPLAYER_H
#define _GAME_SEPLAYER_H

#include <stddef.h>
#include <string.h>

#include "game/s2ddata.h"   /* S2dRef */
#include "sdk/scetypes.h"
#include "sound/sound.h"

// ──────────────────────────────────────────────────────────────────────
// Handles

// What SePlayer::play answers: -1 none, -2 / -3 the failures each method
// lists. A handle packs where the sound plays:
//   bits  0..31  the bank voice, the pan handle or the stream request
//   bits 32..47  the entry (SeEntry::id, its index in the table)
//   bits 48..55  the bank (signed)
//   bits 56..59  placed: a pan handle (playPlaced)
//   bits 60..63  paused (pause; resume plays the entry again)
typedef long long SeHandle;

constexpr SeHandle kSeHandleNone   = -1;
constexpr SeHandle kSeHandlePaused = 0x1000000000000000LL;
constexpr SeHandle kSeHandleMask   = 0x0fffffffffffffffLL;

inline int seHandleVoice(SeHandle h) { return (int)h; }
inline int seHandleEntry(SeHandle h) { return (u_short)(h >> 32); }
inline int seHandleBank(SeHandle h)  { return (signed char)(h >> 48); }
// The ROM tests the top byte's nibbles sign-extended; only zero / non-zero
// is ever asked.
inline bool seHandlePlaced(SeHandle h) { return ((h >> 56) & 0xf) != 0; }
inline bool seHandlePaused(SeHandle h) { return ((h >> 60) & 0xf) != 0; }
inline SeHandle seMakeHandle(int voice, int entry, int bank, bool placed)
{
    return ((SeHandle)(placed ? 1 : 0) << 56)
         | ((SeHandle)(u_char)bank << 48)
         | ((SeHandle)(u_short)entry << 32)
         | (SeHandle)(u_int)voice;
}

// ──────────────────────────────────────────────────────────────────────
// The effect / stream entries

// SeEntry::falloff: the distance curve of a placed sound
// (seDistanceVolume, game/areagrid.h).
enum SeFalloff
{
    SeFalloff_Linear = 0,       // volume * (1 - ratio)
    SeFalloff_Cos    = 1,       // volume * cos(ratio * pi/2)
    SeFalloff_Sin    = 2,       // volume * (1 - sin(ratio * pi/2)); others linear
};

// An effect of an SEDT table or a request of an StDT table (0x30 bytes):
// SePlayer::play reads its bank (+4), program / note (+0x14 / +0x18; a
// stream bank's request is the note), pitch spread (+0x20), id (+0x26),
// volume (+0x28).
// +0x1d: resumable (StageSound pauses it while turned down and plays it
// then; the others do not play while turned down); a placed one also
// keeps playing at the range's edge beyond it (StageSound::updateSlots).
// An entry can head a run of variants (the next entries): a stream cue
// picks one by variants (+0x24), and altOffset (+0x2e) leads a variant to
// its alternate (StageSound's player / scene flags).
struct SeEntry
{
    S2dRef<const char> name;    // FS2 +0x00: StageSound::stopNamed compares it
    int         bank;           // FS2 +0x04: SeTable::banks / StreamTable::banks
    S2dRef<const char> subtitleKey; // FS2 +0x08: StDT: its fsm track (SoundTrack::hasSubtitles); a string index before relocate, < 0 none
    float       loudness;       // FS2 +0x0c: what can hear a played sound by (StageObject::playEntryHeard, SceneNoiseList::addSourceNoise); 0 unheard
    float       range;          // FS2 +0x10: a placed sound's reach; 0 everywhere
    int         program;        // FS2 +0x14
    int         note;           // FS2 +0x18: a stream bank's / an StDT entry's request
    u_char      placed;         // FS2 +0x1c: positional (pan and distance volume)
    u_char      resumable;      // FS2 +0x1d
    u_char      _1e[2];         // FS2 +0x1e
    float       pitchSpread;    // FS2 +0x20: the random pitch is 1 +- spread
    signed char variants;       // FS2 +0x24: a cue's variant count; < 0 picked at random
    u_char      _25;            // FS2 +0x25
    short       id;             // FS2 +0x26: its index (the handle's entry)
    u_char      volume;         // FS2 +0x28: 0..0x80, scales left / right
    u_char      falloff;        // FS2 +0x29: SeFalloff
    u_char      _2a[2];         // FS2 +0x2a
    short       priority;       // FS2 +0x2c: a cue's; an equal or lower one takes a busy channel (StageSound::pickCueChannel)
    signed char altOffset;      // FS2 +0x2e: entries to the alternate, 0 none
    mutable u_char cycle;       // FS2 +0x2f: the variant turn (cues, sources: run-time state)
};
static_assert(sizeof(SeEntry) == 0x30, "SeEntry: EE 0x30");
static_assert(offsetof(SeEntry, bank) == 0x04, "SeEntry::bank");
static_assert(offsetof(SeEntry, subtitleKey) == 0x08, "SeEntry::subtitleKey");
static_assert(offsetof(SeEntry, range) == 0x10, "SeEntry::range");
static_assert(offsetof(SeEntry, program) == 0x14, "SeEntry::program");
static_assert(offsetof(SeEntry, note) == 0x18, "SeEntry::note");
static_assert(offsetof(SeEntry, placed) == 0x1c, "SeEntry::placed");
static_assert(offsetof(SeEntry, resumable) == 0x1d, "SeEntry::resumable");
static_assert(offsetof(SeEntry, pitchSpread) == 0x20, "SeEntry::pitchSpread");
static_assert(offsetof(SeEntry, variants) == 0x24, "SeEntry::variants");
static_assert(offsetof(SeEntry, id) == 0x26, "SeEntry::id");
static_assert(offsetof(SeEntry, volume) == 0x28, "SeEntry::volume");
static_assert(offsetof(SeEntry, falloff) == 0x29, "SeEntry::falloff");
static_assert(offsetof(SeEntry, priority) == 0x2c, "SeEntry::priority");
static_assert(offsetof(SeEntry, altOffset) == 0x2e, "SeEntry::altOffset");
static_assert(offsetof(SeEntry, cycle) == 0x2f, "SeEntry::cycle");

// A table's name index, sorted by name (8 bytes).
struct SeName
{
    int                index;   // FS2 +0x0: the entries index
    S2dRef<const char> name;    // FS2 +0x4
};
static_assert(sizeof(SeName) == 8, "SeName: EE 8");
static_assert(offsetof(SeName, name) == 4, "SeName::name");

// ──────────────────────────────────────────────────────────────────────
// SEDT: the effect table (SePlayer::setTable, the player's +0)

constexpr int   kSeBanks         = 16;
constexpr float kSeTableVersion  = 1.2f;   // the oldest one setTable takes

// A bank's record (0x40). Banks are counted by their non-empty names
// (StageFiles::parseSndseh).
struct SeBankInfo
{
    char   file[0x18];          // FS2 +0x00: the bank's file (StageFiles::parseSndseh loads it)
    char   streamFile[0x18];    // FS2 +0x18: a stream bank's second file
    u_char isStream;            // FS2 +0x30: a stream bank (SePlayer::m_streams), else a voice bank
    u_char _31[0xf];
};
static_assert(sizeof(SeBankInfo) == 0x40, "SeBankInfo: EE 0x40");
static_assert(offsetof(SeBankInfo, isStream) == 0x30, "SeBankInfo::isStream");

// A reverb core's setting (SoundConfig::setEffect).
struct SeReverb
{
    int   mode;                 // FS2 +0x0
    float depth;                // FS2 +0x4: both sides, truncated
};
static_assert(sizeof(SeReverb) == 8, "SeReverb: EE 8");

struct SeTable
{
    char       magic[4];        // FS2 +0x000: "SEDT"
    float      version;         // FS2 +0x004: >= kSeTableVersion
    u_char     _008[4];         // FS2 +0x008
    int        count;           // FS2 +0x00c: entries
    SeBankInfo banks[kSeBanks]; // FS2 +0x010
    SeReverb   reverb[2];       // FS2 +0x410: SoundEffectCore_0 / _1
    u_char     relocated;       // FS2 +0x420
    u_char     _421[3];
    int        stringBase;      // FS2 +0x424: the names' offsets count from it
    int        nameCount;       // FS2 +0x428
    S2dRef<SeName> names;       // FS2 +0x42c
    SeEntry    entries[1];      // FS2 +0x430: count

    // Banks in use: those with a file name. Inline in the ROM
    // (StageFiles::parseSndseh, createMenuSePlayer).
    int bankCount() const
    {
        int used = 0;
        for (int i = 0; i < kSeBanks; i++) {
            if (banks[i].file[0] != '\0')
                used++;
        }
        return used;
    }

    // The effect called name; null none. A binary search over names,
    // inline in the ROM (UiLayout::playSound, StageSound's play 0x1d66e0);
    // like the ROM it reads names[0] even when there are none.
    const SeEntry *find(const char *name)
    {
        int hi = nameCount;
        int lo = 0;
        int index = -1;
        do {
            int mid = (hi + lo) >> 1;
            int cmp = strcmp(names[mid].name, name);
            if (cmp == 0) {
                index = names[mid].index;
                break;
            }
            if (cmp < 1)
                lo = mid + 1;
            else
                hi = mid;
        } while (hi != lo);
        if (index < 0)
            return nullptr;
        if (index < count)
            return &entries[index];
        return nullptr;
    }
};
static_assert(offsetof(SeTable, version) == 0x004, "SeTable::version");
static_assert(offsetof(SeTable, count) == 0x00c, "SeTable::count");
static_assert(offsetof(SeTable, banks) == 0x010, "SeTable::banks");
static_assert(offsetof(SeTable, reverb) == 0x410, "SeTable::reverb");
static_assert(offsetof(SeTable, relocated) == 0x420, "SeTable::relocated");
static_assert(offsetof(SeTable, stringBase) == 0x424, "SeTable::stringBase");
static_assert(offsetof(SeTable, nameCount) == 0x428, "SeTable::nameCount");
static_assert(offsetof(SeTable, names) == 0x42c, "SeTable::names");
static_assert(offsetof(SeTable, entries) == 0x430, "SeTable::entries");

// ──────────────────────────────────────────────────────────────────────
// StDT: a SoundTrack's stream table (SoundTrack::setTable)

// A stream bank (0x10): the XWS header and the body file a request plays
// from (the body path is the track's name + file).
struct StBank
{
    S2dRef<const char> name;    // FS2 +0x0
    S2dRef<const char> file;    // FS2 +0x4: the body file, after SoundTrack::m_name
    u_char             _08[4];  // FS2 +0x8
    // Not relocated by setTable: StageFiles::applySoundTarget points it at
    // the file called name once the stage's loader has it. Null: no play.
    S2dRef<const void> header;  // FS2 +0xc
};
static_assert(sizeof(StBank) == 0x10, "StBank: EE 0x10");
static_assert(offsetof(StBank, file) == 0x4, "StBank::file");
static_assert(offsetof(StBank, header) == 0xc, "StBank::header");

struct StreamTable
{
    char    magic[4];           // FS2 +0x00: "StDT"
    u_char  relocated;          // FS2 +0x04: stdtIsRelocated
    u_char  _05[0xb];
    int     count;              // FS2 +0x10: entries (stdtEntryCount)
    int     bankCount;          // FS2 +0x14
    S2dRef<StBank> banks;       // FS2 +0x18
    int     bankStringBase;     // FS2 +0x1c: the banks' name / file offsets count from it
    int     stringCount;        // FS2 +0x20
    S2dRef<S2dRef<const char> > strings; // FS2 +0x24: the entries' subtitle keys by index
    int     stringBase;         // FS2 +0x28
    int     nameBase;           // FS2 +0x2c: the entries' and names' name offsets count from it
    int     nameCount;          // FS2 +0x30
    S2dRef<SeName> names;       // FS2 +0x34
    u_char  _38[8];
    SeEntry entries[1];         // FS2 +0x40: count

    // The file's offsets become pointers, once (inline in the ROM:
    // SoundTrack::setTable, StageFiles::parseSndsth).
    void relocate();

    // The entry called name; null none. A linear search, inline in the ROM
    // (StageSound::isCuePlaying / findCue / playMusic).
    SeEntry *find(const char *name)
    {
        for (int i = 0; i < count; i++) {
            if (strcmp(entries[i].name, name) == 0)
                return &entries[i];
        }
        return nullptr;
    }
};
static_assert(offsetof(StreamTable, relocated) == 0x04, "StreamTable::relocated");
static_assert(offsetof(StreamTable, count) == 0x10, "StreamTable::count");
static_assert(offsetof(StreamTable, bankCount) == 0x14, "StreamTable::bankCount");
static_assert(offsetof(StreamTable, banks) == 0x18, "StreamTable::banks");
static_assert(offsetof(StreamTable, bankStringBase) == 0x1c, "StreamTable::bankStringBase");
static_assert(offsetof(StreamTable, stringCount) == 0x20, "StreamTable::stringCount");
static_assert(offsetof(StreamTable, strings) == 0x24, "StreamTable::strings");
static_assert(offsetof(StreamTable, stringBase) == 0x28, "StreamTable::stringBase");
static_assert(offsetof(StreamTable, nameBase) == 0x2c, "StreamTable::nameBase");
static_assert(offsetof(StreamTable, nameCount) == 0x30, "StreamTable::nameCount");
static_assert(offsetof(StreamTable, names) == 0x34, "StreamTable::names");
static_assert(offsetof(StreamTable, entries) == 0x40, "StreamTable::entries");

// ──────────────────────────────────────────────────────────────────────
// SePlayer

constexpr int kSeVoices = 0x40;     // records per kind

// SeVoice::scale.
enum SeVoiceScale
{
    SeVoice_Free     = -1,
    SeVoice_Unscaled = 0,           // playUnscaled: the master volume does not apply
    SeVoice_Scaled   = 1,           // under the master volume
};

// A playing voice as the master fade re-levels it (0x18 bytes).
struct SeVoice
{
    int    scale;               // FS2 +0x00: SeVoiceScale
    int    volume;              // FS2 +0x04: the level before the master volume; -1 free
    float  pan;                 // FS2 +0x08: placed ones
    float  ratio;               // FS2 +0x0c: placed ones
    int    bank;                // FS2 +0x10
    u_char placed;              // FS2 +0x14
    u_char _15[3];

    // Inline in the ROM (every method that ends a voice).
    void clear()
    {
        scale  = SeVoice_Free;
        volume = -1;
        bank   = 0;
        pan    = 0.0f;
        ratio  = 0.0f;
        placed = 0;
    }
    // Inline in the ROM: a level below 1 frees the record.
    void set(int scaleKind, int level, int bankIndex, bool isPlaced, float panAngle, float distRatio)
    {
        if (level < 1) {
            clear();
            return;
        }
        volume = level;
        bank   = bankIndex;
        pan    = panAngle;
        ratio  = distRatio;
        scale  = scaleKind;
        placed = isPlaced ? 1 : 0;
    }
};
static_assert(sizeof(SeVoice) == 0x18, "SeVoice: EE 0x18");
static_assert(offsetof(SeVoice, pan) == 0x08, "SeVoice::pan");
static_assert(offsetof(SeVoice, bank) == 0x10, "SeVoice::bank");
static_assert(offsetof(SeVoice, placed) == 0x14, "SeVoice::placed");

// The effect player (0x1298 bytes). A handle names a bank voice, a pan
// handle or a stream (SeHandle).
// layout: host -- EE: +0 the effect table, +4 16 SoundSeBank, +0x44 16
// SoundStream, +0x84 the master volume, +0x88..+0x94 its fade, then the
// voice records (3 x 0x40 of 0x18) from +0x98.
class SePlayer
{
public:
    // A paused handle does nothing; 0 done, 1 not playing.
    int      setAttr(SeHandle handle, int attr);                         // FS2 0x00291ac0
    // Plays a paused handle's entry again (placed ones at pan 0, ratio 0,
    // at left): the new handle. An unpaused one answers itself while it
    // plays, else -2; -3 none. A stream bank's: 0 resumed, -2 playing.
    SeHandle resume(SeHandle handle, int left, int right);               // FS2 0x00291c00
    // Stops a voice and answers its paused handle; -2 failed.
    SeHandle pause(SeHandle handle);                                     // FS2 0x00291d80
    // 0 done, -1 placed, -2 not playing, -3 none.
    int      setVolume(SeHandle handle, int left, int right);            // FS2 0x00291e50
    // A stream bank from an XWS header and its body in EE memory (copied
    // to IOP memory): true made, false no table or there already.
    bool     openStreamBank(int bank, const void *header, const void *body, int bodySize); // FS2 0x00292090
    // A voice bank from an XVAB image (loaded, waited for).
    bool     loadBank(int bank, const void *xvab);                       // FS2 0x00292140
    // Takes an SEDT file (magic "SEDT", version >= 1.2) as its table,
    // relocated once, and sets the two reverb cores from it; false not one.
    bool     setTable(SeTable *table);                                   // FS2 0x002921d0
    // Fades the master volume from from to to over frames (0: at once).
    void     fadeMaster(int frames, int from, int to);                   // FS2 0x00292360
    ~SePlayer();                                                         // FS2 0x002923a0: deleting
    SePlayer();                                                          // FS2 0x00292450
    // Plays an entry placed at pan (radians from ahead) and ratio (the
    // distance over its range, 0..1); pair picks the two-voice pan type:
    // -2 beyond 1, -1 no entry / bank.
    SeHandle playPlaced(float pan, float ratio, const SeEntry *se, int volume, int pair); // FS2 0x00292520
    // play, not under the master volume.
    SeHandle playUnscaled(const SeEntry *se, int left, int right);      // FS2 0x00292830
    // Plays an effect at a left / right volume 0..0x80 (random pitch).
    SeHandle play(const SeEntry *se, int left, int right);              // FS2 0x00292ac0
    // 0 (also when not playing or -1).
    int      stop(SeHandle handle);                                      // FS2 0x00292db0
    // A paused handle (top nibble) counts as playing.
    bool     isPlaying(SeHandle handle);                                 // FS2 0x00292f60
    // 0 done, 1 paused, -1 not playing, -2 another request, -3 none.
    int      setPitch(float pitch, SeHandle handle);                     // FS2 0x00293070
    // Moves a placed handle to pan / ratio: 0 done (or paused / none), -1
    // not placed, 1 not playing; a voice bank's: SoundSeBank::setPan.
    int      setPlacement(float pan, float ratio, SeHandle handle, int volume); // FS2 0x002931b0
    // Once a frame: steps the master fade (re-levelling every voice while
    // it runs, freeing the ended ones), then updates the stream banks.
    void     update();                                                   // FS2 0x00293440

private:
    // Inline in the ROM.
    bool isStreamBank(int bank) const
    {
        return m_table != nullptr && m_table->banks[bank].isStream != 0;
    }
    const SeEntry *entryOf(SeHandle handle) const
    {
        int index = seHandleEntry(handle);
        if (m_table == nullptr || index >= m_table->count)
            return nullptr;
        return &m_table->entries[index];
    }
    int masterScaled(int level) const { return level * m_masterVolume / kSoundVolumeFull; }

public:
    SeTable     *m_table;                   // FS2 +0x000
    SoundSeBank *m_banks[kSeBanks];         // FS2 +0x004
    SoundStream *m_streams[kSeBanks];       // FS2 +0x044
    int      m_masterVolume;                // FS2 +0x084: 0..0x80
    int      m_masterFadeFrom;              // FS2 +0x088
    int      m_masterFadeTo;                // FS2 +0x08c
    int      m_masterFadeFrames;            // FS2 +0x090: -1 not fading
    int      m_masterFadeFrame;             // FS2 +0x094
    SeVoice  m_voices[kSeVoices];           // FS2 +0x098: voice banks' voices, by voice
    SeVoice  m_panVoices[kSeVoices];        // FS2 +0x698: placed voices, by pan handle
    SeVoice  m_streamVoices[kSeVoices];     // FS2 +0xc98: stream banks, by bank (16 used)
};

// ──────────────────────────────────────────────────────────────────────
// SoundTrack

constexpr int kSoundTrackBuffer = 0x10000;  // its stream buffer, from the heap

// A SoundStream that plays an StDT table's requests, with a name (the
// body files' path prefix), an fsm subtitle file and the position the
// subtitles follow.
// layout: host -- EE 0x90 (SoundStream's 0x60 first; +0x60 the StDT table).
class SoundTrack : public SoundStream
{
public:
    int  setAttr(int attr);                                              // FS2 0x002908d0: 0 done, 1 not now
    // The fsm file has text for se's subtitle key.
    bool hasSubtitles(const SeEntry *se);                                // FS2 0x00290930
    // Playing or paused: the subtitle text at the stream's position (16-bit
    // characters); null none.
    const u_short *getSubtitle();                                        // FS2 0x00290980
    // false: not an fsm file.
    bool setSubtitles(void *file);                                       // FS2 0x00290b10
    void setName(const char *name);                                      // FS2 0x00290bb0: a copy; null none
    ~SoundTrack();                                                       // FS2 0x00290c20: deleting
    SoundTrack();                                                        // FS2 0x00290ca0: name "snd", 0x10000 buffer
    // false: not an StDT file; relocates it once.
    bool setTable(void *file);                                           // FS2 0x00290d60
    // playEntry placed at pan / ratio (an unplaced entry: playEntry); -2
    // beyond ratio 1 or no bank.
    int  playPlaced(float pan, float ratio, const SeEntry *se, int volume, int start, int secondHalf); // FS2 0x00290fd0
    // Plays an entry's request (its bank in the StDT table) at left /
    // right from start; secondHalf plays the request's second half
    // (StageSound's player flag). Already playing: stops instead, -1;
    // -2 no bank / start beyond the end.
    int  playEntry(const SeEntry *se, int left, int right, int start, int secondHalf); // FS2 0x00291370
    // The SoundStream state Paused.
    bool isPaused();                                                     // FS2 0x002916b0
    // Playing or paused.
    bool isActive();                                                     // FS2 0x002916c0
    // Stops it; the position it stopped at (in its half), -1 not playing.
    int  stop();                                                         // FS2 0x002916f0
    // Moves a placed stream to pan / ratio: -1 not placed, -2 not playing.
    int  setPlacement(float pan, float ratio, int volume);               // FS2 0x00291770
    // Once a frame: notes the start, then SoundStream::update.
    void update();                                                       // FS2 0x002918c0
    int  pause(bool on);                                                 // FS2 0x00291a00: answers on
    // SoundStream::setVolume scaled by the entry's volume: 0 done, -1
    // placed, -2 not playing.
    int  setVolume(int left, int right);                                 // FS2 0x00291a30

private:
    // Inline in the ROM.
    void stopForPlay();
    int  openEntry(const SeEntry *se, int start, int secondHalf);
    float playedSeconds();

public:
    StreamTable   *m_table;             // FS2 +0x60: the StDT table (setTable)
    const SeEntry *m_entry;             // FS2 +0x64: what plays
    u_char  m_placed;                   // FS2 +0x68: started by playPlaced
    int     m_channels;                 // FS2 +0x6c: the request's
    int     m_rate;                     // FS2 +0x70: the request's
    u_char  m_secondHalf;               // FS2 +0x74
    int     m_size;                     // FS2 +0x78: the request's bytes (both halves)
    void   *m_subtitles;                // FS2 +0x7c: the fsm file
    int     m_subtitleTrack;            // FS2 +0x80: -1 none
    char   *m_name;                     // FS2 +0x84
    char   *m_path;                     // FS2 +0x88: the body file playing (m_name + file)
    u_char  m_started;                  // FS2 +0x8c: its position went past 0
};

#endif /* _GAME_SEPLAYER_H */
