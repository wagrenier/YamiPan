/* ==========================================================================
 *  sound/sound.h
 *
 *  The game's sound layer over rx's sound modules and SDPR2, one object
 *  file 0x144080..0x148db0 (sound/sound.cpp): streamed banks (SoundStream),
 *  the sinks and feeder the movie player pumps its audio through, sound
 *  effect banks (SoundSeBank), the output / volume / reverb settings
 *  (SoundConfig) and the boot's soundInit / soundUpdate / soundShutdown with
 *  the IOP and SPU2 memory hooks rx allocates through. Every rx / SDPR2
 *  call runs under the sound semaphore (also taken by the rx thread
 *  callback). architecture.md "The game's sound layer".
 * ======================================================================== */
#ifndef _SOUND_SOUND_H
#define _SOUND_SOUND_H

#include "sdk/scetypes.h"

class StreamRing;               // game/gamehooks.h (its TU is not cut yet)

// The full volume of every level here (L / R, fades, master volumes).
constexpr int kSoundVolumeFull = 0x80;

void soundInit(int isPal);                                               // FS2 0x001486e0
void soundUpdate();                                                      // FS2 0x00148500
void soundShutdown();                                                    // FS2 0x00148540

// rx's thread callback: takes the sound semaphore while rx's thread runs
// (lock non-zero) and gives it back after.
void soundThreadLock(int lock);                                          // FS2 0x001488f0

// rx's IOP memory hooks (sceSifAllocSysMemory from the bottom / top).
u_int soundIopAllocLow(int size);                                        // FS2 0x00148d60
u_int soundIopAllocHigh(int size);                                       // FS2 0x00148d10
void  soundIopFree(u_int addr);                                          // FS2 0x00148cd0

// rx's SPU2 memory hooks over the game's own allocator: 64-byte units in
// the span 0x5040..0x1f9000, at most 32 blocks. 0 none.
u_int spuMemAllocLow(int size);                                          // FS2 0x00148a20
u_int spuMemAllocHigh(int size);                                         // FS2 0x001489d0
void  spuMemFree(u_int addr);                                            // FS2 0x00148920
// The first fit from the bottom or (fromTop) the top; the caller holds the
// SPU semaphore.
u_int spuMemAlloc(int size, bool fromTop);                               // FS2 0x00148b60
// The block [addr, addr + size) if it is free.
u_int spuMemReserve(int size, u_int addr);                               // FS2 0x00148a70

// ──────────────────────────────────────────────────────────────────────
// SoundStream: one rxSndStr stream over an XWS bank.

// SoundStream::m_state.
enum SoundStreamState
{
    SoundStream_Stopped     = 0,
    SoundStream_Playing     = 1,
    SoundStream_Paused      = 2,
    SoundStream_StopPending = 3,    // stop(false) found a read in flight; update finishes it
    SoundStream_Ended       = 4,
};

// SoundStream::m_bodyKind: where the bank's body is.
enum SoundBody
{
    SoundBody_None   = 0,
    SoundBody_File   = 1,           // a file, read through the stream buffer
    SoundBody_Memory = 2,           // EE memory
    SoundBody_Iop    = 3,           // copied to IOP memory
};

// SoundStream::m_fadeEnd: what a finished fade does.
enum SoundFadeEnd
{
    SoundFadeEnd_None  = 0,
    SoundFadeEnd_Stop  = 1,
    SoundFadeEnd_Pause = 2,
};

// The SPU2 ring each playing stream takes (spuMemAllocHigh).
constexpr int kSoundStreamSpuRing = 0x4000;

// A bank body copied to IOP memory (inline methods; its dtor runs after
// ~SoundStream's body).
struct SoundIopBody
{
    // Frees the IOP copy unless it is borrowed.
    void release();
    ~SoundIopBody() { release(); }

    u_int  m_addr;              // FS2 +0x0: IOP address
    int    m_size;              // FS2 +0x4
    u_char m_borrowed;          // FS2 +0x8: never set in this file
};
static_assert(sizeof(SoundIopBody) == 0xc, "SoundIopBody is 0xc bytes on the EE");

// layout: host -- EE 0x60; m_buffer (+0x18) is the one pointer, which
// widens on the host.
class SoundStream
{
public:
    SoundStream();                                                       // FS2 0x00145d70
    ~SoundStream();                                                      // FS2 0x00145be0: deleting (obj, 1)

    // The stream buffer a file body is read through; a null buffer
    // allocates size bytes from the global heap.
    void setBuffer(int size, void *buffer);                              // FS2 0x00145a50
    void freeBuffer();                                                   // FS2 0x001458f0
    // A bank from its header with the body in the file bodyName. The bank,
    // -1 failed.
    int  openBankFile(const void *header, const char *bodyName);         // FS2 0x00145750
    // A bank with its body in EE memory, or copied to IOP memory (toIop).
    int  openBankMemory(const void *header, const void *body, int bodySize, bool toIop); // FS2 0x00145530
    int  closeBank();                                                    // FS2 0x00145100

    // Plays request from start; the stream id, -1 failed.
    int  play(int request, int start);                                   // FS2 0x00145240
    int  playVolume(int request, int start, int volL, int volR);         // FS2 0x00145420
    int  playPan(int request, int start, float angle, int volume);       // FS2 0x00144400
    // Stops (wait: until its reads are done); the position it stopped at.
    int  stop(bool wait);                                                // FS2 0x00144d80
    void pause(bool on);                                                 // FS2 0x00144d10
    // Once a frame: ends the stream when rx says so, runs the fade.
    void update();                                                       // FS2 0x00144a10

    void setVolume(int volL, int volR);                                  // FS2 0x001447e0
    // Volume from an angle in radians (0 ahead, counter-clockwise).
    int  setPan(float angle, int volume);                                // FS2 0x00144250
    int  setPitch(float pitch);                                          // FS2 0x001441f0
    void setAttr(int attr);                                              // FS2 0x00144880
    // A fade over frames from from to to, then end.
    void startFade(int frames, int from, int to, int end);               // FS2 0x001447c0: SoundFadeEnd

    // The played position plus the start, -1 when not playing.
    int  getPosition();                                                  // FS2 0x00144990
    int  getDataWritten();                                               // FS2 0x001441a0
    // Of request (getRequestRate: -1 = the playing one).
    int  getRequestRate(int request);                                    // FS2 0x001445d0
    int  getRequestChannels(int request);                                // FS2 0x001448d0
    int  getRequestSize(int request);                                    // FS2 0x00144930

private:
    int  copyBodyToIop(const void *body, int size);                      // FS2 0x00144090
    void updateFade();                                                   // FS2 0x00144650
    // Inline in the ROM.
    void applyVolume();
    void freeSpuRing();

public:
    SoundIopBody m_body;        // FS2 +0x0
    int    m_bodyKind;          // FS2 +0xc: SoundBody
    int    m_state;             // FS2 +0x10: SoundStreamState
    int    m_bufSize;           // FS2 +0x14
    void  *m_buffer;            // FS2 +0x18
    u_char m_bufOwned;          // FS2 +0x1c: m_buffer from the global heap
    int    m_bank;              // FS2 +0x20: -1 none
    int    m_stream;            // FS2 +0x24: -1 none
    int    m_request;           // FS2 +0x28: -1 none
    int    m_trackCount;        // FS2 +0x2c
    int    m_startPos;          // FS2 +0x30
    u_int  m_spuRing;           // FS2 +0x34: kSoundStreamSpuRing bytes from the top, 0 none
    int    m_volL;              // FS2 +0x38
    int    m_volR;              // FS2 +0x3c
    int    m_fade;              // FS2 +0x40: kSoundVolumeFull when not fading
    int    m_fadeFrom;          // FS2 +0x44
    int    m_fadeTo;            // FS2 +0x48
    int    m_fadeFrames;        // FS2 +0x4c: 0 no fade
    int    m_fadeFrame;         // FS2 +0x50
    int    m_fadeEnd;           // FS2 +0x54: SoundFadeEnd
    int    m_channels;          // FS2 +0x58
    int    m_panState;          // FS2 +0x5c: sdpr2CalcPanDeg's state
};

// ──────────────────────────────────────────────────────────────────────
// The movie player's audio: a SoundFeeder moves a StreamRing's bytes into
// a SoundSink (an rx PCM or ADPCM stream) each frame.

// SoundSink::getStatus bits (rxSndPcmGetStatus / rxSndAdpcmGetStatus).
enum SoundSinkStatus
{
    SoundSinkStatus_Started = 0x2,  // playing: the feeder sets its volume
    SoundSinkStatus_Ended   = 0x4,
};

// SoundSink::open's modes.
enum SoundSinkMode
{
    SoundSinkMode_Mono        = 0,
    SoundSinkMode_Stereo      = 1,  // left and right blocks of m_interleave bytes each
    SoundSinkMode_Interleaved = 2,  // left / right interleaved by 16 bytes
};

// layout: host -- EE 0x10, vtable 0x311180 at +0xc (CodeWarrior: after the
// data).
class SoundSink
{
public:
    virtual ~SoundSink() {}                                              // FS2 0x001468e0
    virtual void setVolume(int volL, int volR) = 0;                      // slot +0xc
    // a3..a6 are rxSndPcmOpen's / rxSndAdpcmOpen's (the PCM sink passes
    // a3 / a4); the handle, < 0 failed.
    virtual int  open(int mode, int rate, int a3, int a4, int a5, int a6, int interleave) = 0; // slot +0x10
    virtual void close() = 0;                                            // slot +0x14
    virtual u_int getStatus() = 0;                                       // slot +0x18: SoundSinkStatus
    virtual int  getFree() = 0;                                          // slot +0x1c
    virtual int  write(const void *data, const void *dataR, int size) = 0; // slot +0x20
    virtual void end() = 0;                                              // slot +0x24
    virtual int  setUserData(int data) = 0;                              // slot +0x28
    virtual int  getUserData() = 0;                                      // slot +0x2c
    virtual int  addCallback(int offset, void (*func)(int, int), int arg) = 0; // slot +0x30
    virtual void setAttr(int attr) = 0;                                  // slot +0x34
    virtual int  muteIfEnded() = 0;                                      // slot +0x38

    int m_handle;               // FS2 +0x0: -1 closed
    int m_mode;                 // FS2 +0x4: SoundSinkMode
    int m_interleave;           // FS2 +0x8
};

// layout: host -- EE 0x10, vtable 0x311140.
class SoundPcmSink : public SoundSink
{
public:
    SoundPcmSink();                                                      // FS2 0x001468b0
    virtual ~SoundPcmSink();                                             // FS2 0x00146830
    virtual void setVolume(int volL, int volR);                          // FS2 0x001467d0
    virtual int  open(int mode, int rate, int a3, int a4, int a5, int a6, int interleave); // FS2 0x00146740
    virtual void close();                                                // FS2 0x001466d0
    virtual u_int getStatus();                                           // FS2 0x00146680
    virtual int  getFree();                                              // FS2 0x00146630
    virtual int  write(const void *data, const void *dataR, int size);   // FS2 0x001465b0
    virtual void end();                                                  // FS2 0x00146570
    virtual int  setUserData(int data);                                  // FS2 0x00146510
    virtual int  getUserData();                                          // FS2 0x001464c0
    virtual int  addCallback(int offset, void (*func)(int, int), int arg); // FS2 0x00146440
    virtual void setAttr(int attr);                                      // FS2 0x00144080: empty
    virtual int  muteIfEnded();                                          // FS2 0x001463d0
};

// layout: host -- EE 0x10, vtable 0x3111c0.
class SoundAdpcmSink : public SoundSink
{
public:
    SoundAdpcmSink();                                                    // FS2 0x00146eb0
    virtual ~SoundAdpcmSink();                                           // FS2 0x00146e30
    virtual void setVolume(int volL, int volR);                          // FS2 0x00146dd0
    virtual int  open(int mode, int rate, int a3, int a4, int a5, int a6, int interleave); // FS2 0x00146cf0
    virtual void close();                                                // FS2 0x00146c80
    virtual u_int getStatus();                                           // FS2 0x00146c30
    virtual int  getFree();                                              // FS2 0x00146be0
    virtual int  write(const void *data, const void *dataR, int size);   // FS2 0x00146b60
    virtual void end();                                                  // FS2 0x00146b20
    virtual int  setUserData(int data);                                  // FS2 0x00146ac0
    virtual int  getUserData();                                          // FS2 0x00146a70
    virtual int  addCallback(int offset, void (*func)(int, int), int arg); // FS2 0x001469f0
    virtual void setAttr(int attr);                                      // FS2 0x001469a0
    virtual int  muteIfEnded();                                          // FS2 0x00146930
};

// The feeder's states (+0).
enum SoundFeederState
{
    SoundFeeder_Idle     = 0,
    SoundFeeder_Feeding  = 1,
    SoundFeeder_Paused   = 2,
    SoundFeeder_Stopping = 3,
    SoundFeeder_Ended    = 4,
};
// layout: host -- EE 0x18 (the two pointers widen on the host).
class SoundFeeder
{
public:
    SoundFeeder();                                                       // FS2 0x001463a0
    ~SoundFeeder();                                                      // FS2 0x00146310: deleting (obj, 1)
    // 0 feeding; 1 over; -2 no source, -3 no sink, -1 a bad state.
    int  start();                                                        // FS2 0x00146210
    // Moves what fits; 1 when not feeding.
    int  update();                                                       // FS2 0x00145de0
    int  setSource(StreamRing *source);                                  // FS2 0x00146300
    int  setSink(SoundSink *sink);                                       // FS2 0x001462f0

    int         m_state;        // FS2 +0x0: SoundFeederState
    StreamRing *m_source;       // FS2 +0x4
    SoundSink  *m_sink;         // FS2 +0x8
    int         m_fed;          // FS2 +0xc: bytes moved since setSource / setSink
    int         m_volumeL;      // FS2 +0x10
    int         m_volumeR;      // FS2 +0x14
};

// ──────────────────────────────────────────────────────────────────────
// SoundSeBank: an XVAB sound effect bank, plain (rxSndSe) and panned
// (SDPR2) effects from it.

// SoundSeBank::m_state.
enum SoundSeBankState
{
    SoundSeBank_Error    = -1,
    SoundSeBank_Empty    = 0,
    SoundSeBank_Loading  = 1,
    SoundSeBank_Loaded   = 2,
    SoundSeBank_Freeing  = 3,
};

// rxSndSeGetStatus bits isPlaying tests.
constexpr u_int kSoundSeStatusSounding = 0x2 | 0x8;

// layout: EE 0x10.
class SoundSeBank
{
public:
    SoundSeBank(const void *xvab, bool wait, u_int iopAddr, u_int spuAddr); // FS2 0x001476b0
    ~SoundSeBank();                                                      // FS2 0x001475c0: deleting (obj, flag)
    // Loads xvab (wait: until it is there); the rxSndXvab id, < 0 failed.
    int  load(const void *xvab, bool wait, u_int iopAddr, u_int spuAddr); // FS2 0x00147a80
    // Frees the bank (force: retry, then free its memory by hand). The state.
    int  unload(bool force);                                             // FS2 0x00147840
    void poll();                                                         // FS2 0x001476f0
    int  getState();                                                     // FS2 0x00147790: SoundSeBankState

    int  play(int prog, int note, int volL, int volR);                   // FS2 0x001471b0: the voice
    void stop(int voice);                                                // FS2 0x00147500
    int  setVolume(int voice, int volL, int volR);                       // FS2 0x001473e0
    int  setAttr(int voice, int attr);                                   // FS2 0x00147440
    // 1 for no voice, else 0.
    int  setPitch(int voice, float pitch);                               // FS2 0x00146f40
    bool isPlaying(int voice);                                           // FS2 0x00146fa0

    // A panned handle at angle (radians) and spread (0..1); pair picks the
    // two-voice type. -2 for a spread over 1.
    int  playPan(float angle, float spread, int prog, int note, int volume, bool pair); // FS2 0x00147240
    void stopPan(int handle);                                            // FS2 0x00147560
    int  setPan(float angle, float spread, int handle, int volume);      // FS2 0x00147050
    int  setPanAttr(int handle, bool on);                                // FS2 0x00147490
    int  setPanPitchF(int handle, float pitch);                          // FS2 0x00146ee0
    bool isPanPlaying(int handle);                                       // FS2 0x00146ff0

    int   m_state;              // FS2 +0x0: SoundSeBankState
    int   m_id;                 // FS2 +0x4: rxSndXvab id, -1 none
    u_int m_iopAddr;            // FS2 +0x8
    u_int m_spuAddr;            // FS2 +0xc
};

// ──────────────────────────────────────────────────────────────────────
// SoundConfig: the output, volume and reverb settings (one, gSoundConfig).

// One SPU2 core's reverb (setEffect). layout: EE 0x18.
struct SoundEffect
{
    // Inline in the ROM: each takes the sound semaphore.
    void setMode(int mode);
    void setDepth(int depthL, int depthR);
    void setFeedback(int feedback);
    void setDelay(int delay);
    int  core() const { return m_core == 1 ? 1 : 0; }

    int m_core;                 // FS2 +0x0: 0 / 1
    int m_mode;                 // FS2 +0x4
    int m_depthL;               // FS2 +0x8
    int m_depthR;               // FS2 +0xc
    int m_feedback;             // FS2 +0x10
    int m_delay;                // FS2 +0x14
};

// setEffect's targets.
enum SoundEffectCore
{
    SoundEffectCore_0    = 0,
    SoundEffectCore_1    = 1,
    SoundEffectCore_Both = 2,
};

// layout: EE 0x4c.
class SoundConfig
{
public:
    SoundConfig();                                                       // FS2 0x00148130
    // Inline in the ROM (soundShutdown): silences everything.
    ~SoundConfig();
    // SoundOutput (game/sysdata.h): 0 mono, 1 stereo, 2 surround.
    void setOutputMode(int mode);                                        // FS2 0x00147ee0
    void setEffect(int which, int mode, int depthL, int depthR, int feedback, int delay); // FS2 0x00147be0: SoundEffectCore

private:
    // Inline in the ROM.
    void setStereo(bool on);
    void setSurround(bool on);

public:
    int         m_masterVolume; // FS2 +0x0
    int         m_bgmVolume;    // FS2 +0x4
    int         m_seVolume;     // FS2 +0x8
    int         m_pcmVolume;    // FS2 +0xc
    int         m_adpcmVolume;  // FS2 +0x10
    int         m_outputMode;   // FS2 +0x14: SoundOutput
    SoundEffect m_effect[2];    // FS2 +0x18
    u_char      m_stereo;       // FS2 +0x48: rxSndSetOutputMode
    u_char      m_surround;     // FS2 +0x49: sdpr2SetSurround
};
static_assert(sizeof(SoundEffect) == 0x18, "SoundEffect is 0x18 bytes on the EE");
static_assert(sizeof(SoundConfig) == 0x4c, "SoundConfig is 0x4c bytes on the EE");
static_assert(sizeof(SoundSeBank) == 0x10, "SoundSeBank is 0x10 bytes on the EE");

extern SoundConfig *gSoundConfig;   /* sbss 0x31addc (gp-0x7a94) */

#endif /* _SOUND_SOUND_H */
