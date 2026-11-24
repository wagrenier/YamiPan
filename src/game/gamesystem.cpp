// game/gamesystem.cpp -- main, the boot, the main loop and GameSystem (game/gamesystem.h).

#include "game/gamesystem.h"
#include "yamipan/os/yamipan_time.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "game/datetime.h"
#include "game/debugsys.h"
#include "game/gamehooks.h"
#include "game/gfxtarget.h"
#include "game/heap.h"
#include "game/memcard.h"
#include "game/scene.h"
#include "game/spkrom.h"
#include "game/thread.h"
#include "sdk/eekernel.h"
#include "sdk/eeregs.h"
#include "sdk/eestruct.h"
#include "sdk/libdma.h"
#include "sdk/libgraph.h"
#include "sdk/libpad.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// The heap meter's full scale: 27 MiB.
constexpr float kHeapMeterFullScale = 28311552.0f;

// GamePacketManager's two frame buffers.
constexpr u_int kPacketBufferSize = 0xc0000;

// gsClearVram clears GS memory in 16 blocks of 256 x 256 32-bit pixels
// (64K words each: all 4 MB).
constexpr int kVramClearBlocks = 16;
constexpr int kVramClearBlockWords = 256 * 256;

// AddDmacHandler cause: D_STAT bit 15, BEIS (bus error).
constexpr int kDmacCauseBusError = 15;

// DMA tag id CNT (bits 28..30): the next qwords follow the tag.
constexpr u_long kDmaTagCnt = (u_long)1 << 28;

GameSystem *gGameSystem;                /* sbss 0x31ad84 */
int         gArgc;                      /* sbss 0x31ad88 */
char      **gArgv;                      /* sbss 0x31ad8c */
u_char      gDmacBusError;              /* sbss 0x31ad90 */
u_char      gGfxFlipNoPackets;          /* sbss 0x31ad94 */
u_char      gGfxPutDrawEnv = 1;         /* sdata 0x31a890 */
u_char      gGfxKickDisplayPacket = 1;  /* sdata 0x31a894 */

// The packet buffers GameSystem gives its GamePacketManager (only users
// found: GameSystem::GameSystem).
alignas(64) static u_long128 sPacketBuffer0[kPacketBufferSize / sizeof(u_long128)]; /* bss 0x32e280 */
alignas(64) static u_long128 sPacketBuffer1[kPacketBufferSize / sizeof(u_long128)]; /* bss 0x3ee280 */

// The perf meter's thread names and colours (data 0x2f3810, 0x2f3830).
static const char *const sThreadNames[ThreadSlot_Count] = {
    "Th Ctrl", "Th Main", "Th 02", "Th Game", "Th 04", "Th Extra", "Th 06", "Th Effect",
};
static const u_int sThreadColours[6] = {
    PerfMeterColour_Blue, PerfMeterColour_Green, PerfMeterColour_Red,
    PerfMeterColour_Magenta, PerfMeterColour_Cyan, PerfMeterColour_Yellow,
};

// The packet gsClearVram sends: a REFE tag pointing at the GIF tag after
// it, then one PACKED A+D loop of 14 registers, the draw env (8) and the
// clear (6): 15 qwords.
// layout: host -- the DMA tag holds a pointer; EE 0x100 on the stack.
struct GsClearPacket
{
    sceDmaTag     tag;          // FS2 sp+0x60
    sceGifTag     giftag;       // FS2 sp+0x70
    sceGsDrawEnv1 env;          // FS2 sp+0x80
    sceGsClear    clear;        // FS2 sp+0x100
};

static void gameInit(const char *bootPath);
static void gsClearVram();
static void gameThreadSwitchHook(u_int slot, void *arg);
static void gameMainThread(void *arg);
static int  gameFrameReady(FrameSync *sync);
static int  dmacBusErrorHandler(int cause);

// ──────────────────────────────────────────────────────────────────────
// A stage starts (stageThread, before its loop): the stage's heap mark
// starts again from the current top and the perf meter forgets the last
// stage's peaks.
// FS2 0x0012cce0

void GameSystem::beginStage()
{
    gGlobalHeap.setMark(HeapMark_Stage);
    m_perfMeter.resetStats();
}

// ──────────────────────────────────────────────────────────────────────
// End of frame: wait for the GS path to drain. A DMAC bus error since the
// last frame resets the GS and DMA first; a path that does not drain is
// reset until it does, and the next flip then skips the display packet.
// The caller passes gGameSystem, but nothing of it is read.
// FS2 0x0012cd20

void GameSystem::endFrameSync()
{
    if (gDmacBusError) {
        sceGsResetPath();
        sceGsResetGraph(1, SCE_GS_INTERLACE, SCE_GS_NTSC, SCE_GS_FRAME);
        sceDmaReset(1);
        gDmacBusError = 0;
    }
    gGfxKickDisplayPacket = 1;
    if (sceGsSyncPath(0, 0) < 0) {
        do {
            sceDmaReset(1);
            sceGsResetPath();
            sceGsResetGraph(1, SCE_GS_INTERLACE, SCE_GS_PAL, SCE_GS_FIELD);
        } while (sceGsSyncPath(1, 1) != 0);
        gGfxKickDisplayPacket = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Once per frame, with the vsyncs the frame took: R3 on port 1 dumps the
// meter's groups; with the meter on, heap and packet use become bars and a
// GS FINISH closes list 13; then the packet lists end, the meter and the
// master clock step, and the frame's chain goes out on VIF1.
// FS2 0x0012cdf0

void GameSystem::flipFrame(int vsyncs)
{
    if (m_pad1.m_pressed & SCE_PADj)
        m_perfMeter.dumpGroups();

    if (m_meterMode != MeterMode_Off) {
        m_perfMeter.addBar((float)gGlobalHeap.markedUsage(HeapMark_Frame) / kHeapMeterFullScale,
                           PerfMeterColour_Cyan, PerfMeterGroup_Heap, 0);
        float stage = (float)gGlobalHeap.markedUsage(HeapMark_Stage) / kHeapMeterFullScale;
        if (stage > 1.0f)
            m_perfMeter.addBar(stage, PerfMeterColour_Red, PerfMeterGroup_Heap, 0);
        else
            m_perfMeter.addBar(stage, PerfMeterColour_Magenta, PerfMeterGroup_Heap, 0);
        m_perfMeter.addBar((float)gGlobalHeap.markedUsage(HeapMark_Base) / kHeapMeterFullScale,
                           PerfMeterColour_Yellow, PerfMeterGroup_HeapAll, 0);
        gGlobalHeap.setMark(HeapMark_Frame);
        m_perfMeter.addBar((float)m_packets.GetUsedSize() / (float)m_packets.GetBufferSize(),
                           PerfMeterColour_White, PerfMeterGroup_Packet, 0);

        sceVif1Packet *pkt = m_packets.openList(PacketList_Finish);
        m_perfMeter.drawBars(pkt);
        if (m_meterMode == MeterMode_Bars)
            m_perfMeter.drawDetail(pkt, PerfMeterGroup_Detail);
        // A CNT tag over a VIF NOP + DIRECT of one A+D GIF loop: GS FINISH.
        // PC: the host draw module reads the lists by id; FINISH draws nothing.
        u_long *p = (u_long *)pkt->pCurrent;
        p[0] = kDmaTagCnt | 2;
        p[1] = (u_long)SCE_VIF1_SET_DIRECT(2, 0) << 32 | SCE_VIF1_SET_NOP(0);
        p[2] = SCE_GIF_SET_TAG(1, 1, 0, 0, SCE_GIF_PACKED, 1);
        p[3] = SCE_GIF_PACKED_AD;
        p[4] = 0;
        p[5] = SCE_GS_FINISH;
        pkt->pCurrent = (decltype(pkt->pCurrent))(p + 6);
        m_packets.closeList();

        m_perfMeter.collectStats(PerfMeterGroup_Detail);
    }

    m_packets.endFrame();
    m_perfMeter.beginFrame();
    m_clock.update(vsyncs);
    FlushCache(WRITEBACK_DCACHE);
    m_packets.kickAndSwap();
}

// ──────────────────────────────────────────────────────────────────────
// PC: the native frame rate (game/gamerate.h). The master clock's own ticks say which 25 Hz frames ended: it is the
// root and never paused, and flipFrame advances it before the frame's work.

// The ROM's 25 frames a second, in master-clock ticks (300 a second) a frame.
constexpr int kPalFramesPerSecond = 25;

int gameVsyncsPerFrame()
{
    return YamiPan_GameRate() == kPalFramesPerSecond ? kVsyncsPerFrame : 1;
}

// 12 at 25, 6 at 50, 5 at 60: whole ticks, so the clock stays in real seconds.
int gameClockTicksPerFrame()
{
    return kClockTicksPerFrame * kPalFramesPerSecond / YamiPan_GameRate();
}

int palTicks()
{
    if (YamiPan_GameRate() == kPalFramesPerSecond)
        return 1;   // the ROM's pace: every frame is one, from the first
    const MasterClock &clock = gGameSystem->m_clock;
    return (int)(clock.m_time / kClockTicksPerFrame - (clock.m_time - clock.m_delta) / kClockTicksPerFrame);
}

float palScale()
{
    return (float)gameClockTicksPerFrame() / (float)kClockTicksPerFrame;
}

// An ease that closes `rate` of the distance each 25 Hz frame closes 1 - (1 - rate)^s of it in a frame of s of
// one, so two 50 Hz frames close what one 25 Hz frame did. The ROM's rate itself at 25 (not 1 - (1 - rate): bits).
float palEase(float rate)
{
    const float share = palScale();
    if (share == 1.0f)
        return rate;
    return 1.0f - powf(1.0f - rate, share);
}

int palFrame()
{
    return (int)(gGameSystem->m_clock.m_time / kClockTicksPerFrame);
}

// The master clock's ticks past the last 25 Hz frame's end, as a share of one: a step taken at that end is drawn
// from the place before it, reaching it when the next step is due, so what is kept in 25 Hz steps is drawn one 25 Hz
// frame late and moves every frame.
float palDrawShare()
{
    if (YamiPan_GameRate() == kPalFramesPerSecond)
        return 1.0f;   // the ROM's pace: the newest place
    return (float)(gGameSystem->m_clock.m_time % kClockTicksPerFrame) / (float)kClockTicksPerFrame;
}

namespace {

// A turn as a quaternion, x y z w.
struct PalTurn
{
    float x, y, z, w;
};

// The unit turn of three unit axes (rows): Shepperd's method. False for one with no quaternion (mirrored).
bool palTurnOf(PalTurn &out, const float m[3][3])
{
    const float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                      m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                      m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (!(det > 0.5f))
        return false;
    const float trace = m[0][0] + m[1][1] + m[2][2];
    if (trace > 0.0f) {
        const float s = sqrtf(trace + 1.0f) * 2.0f;
        out = {(m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s, 0.25f * s};
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        const float s = sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
        out = {0.25f * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s, (m[2][1] - m[1][2]) / s};
    } else if (m[1][1] > m[2][2]) {
        const float s = sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
        out = {(m[0][1] + m[1][0]) / s, 0.25f * s, (m[1][2] + m[2][1]) / s, (m[0][2] - m[2][0]) / s};
    } else {
        const float s = sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
        out = {(m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25f * s, (m[1][0] - m[0][1]) / s};
    }
    return true;
}

// palTurnOf's inverse.
void palAxesOf(float m[3][3], const PalTurn &q)
{
    m[0][0] = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    m[0][1] = 2.0f * (q.x * q.y - q.z * q.w);
    m[0][2] = 2.0f * (q.x * q.z + q.y * q.w);
    m[1][0] = 2.0f * (q.x * q.y + q.z * q.w);
    m[1][1] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    m[1][2] = 2.0f * (q.y * q.z - q.x * q.w);
    m[2][0] = 2.0f * (q.x * q.z - q.y * q.w);
    m[2][1] = 2.0f * (q.y * q.z + q.x * q.w);
    m[2][2] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
}

// The unit axes of a placement and their lengths. False for a flat one.
bool palAxesSplit(float unit[3][3], float length[3], const float m[4][4])
{
    for (int axis = 0; axis < 3; axis++) {
        length[axis] = sqrtf(m[axis][0] * m[axis][0] + m[axis][1] * m[axis][1] + m[axis][2] * m[axis][2]);
        if (!(length[axis] > 0.0f))
            return false;
        for (int lane = 0; lane < 3; lane++)
            unit[axis][lane] = m[axis][lane] / length[axis];
    }
    return true;
}

} // namespace

void palBlendMatrix(float out[4][4], const float from[4][4], const float to[4][4], float share)
{
    float fromUnit[3][3], toUnit[3][3], fromLength[3], toLength[3];
    PalTurn a, b;
    if (share >= 1.0f || !palAxesSplit(fromUnit, fromLength, from) || !palAxesSplit(toUnit, toLength, to) ||
        !palTurnOf(a, fromUnit) || !palTurnOf(b, toUnit)) {
        memcpy(out, to, sizeof(float[4][4]));
        return;
    }
    if (share < 0.0f)
        share = 0.0f;

    // The shorter way round, then the slerp; nearly the same turn, a straight line.
    float cosine = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (cosine < 0.0f) {
        b = {-b.x, -b.y, -b.z, -b.w};
        cosine = -cosine;
    }
    float wa = 1.0f - share;
    float wb = share;
    if (cosine < 0.9995f) {
        const float angle = acosf(cosine);
        const float sine = sinf(angle);
        wa = sinf((1.0f - share) * angle) / sine;
        wb = sinf(share * angle) / sine;
    }
    PalTurn turn = {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
    const float norm = sqrtf(turn.x * turn.x + turn.y * turn.y + turn.z * turn.z + turn.w * turn.w);
    turn = {turn.x / norm, turn.y / norm, turn.z / norm, turn.w / norm};

    float axes[3][3];
    palAxesOf(axes, turn);
    for (int axis = 0; axis < 3; axis++) {
        const float length = fromLength[axis] + (toLength[axis] - fromLength[axis]) * share;
        for (int lane = 0; lane < 3; lane++)
            out[axis][lane] = axes[axis][lane] * length;
        out[axis][3] = to[axis][3];
    }
    for (int lane = 0; lane < 3; lane++)
        out[3][lane] = from[3][lane] + (to[3][lane] - from[3][lane]) * share;
    out[3][3] = to[3][3];
}

// ──────────────────────────────────────────────────────────────────────
// The global object: three pads (port 0, port 1, port 1 masked) whose maps
// point at the options' one, the packet manager on the two frame buffers,
// the debug meter and the master clock (12 ticks per 2-vsync frame).
// FS2 0x0012d0d0
// callcheck: Pad, setDefaults, GamePacketManager, PerfMeter, MasterClock, ~Clock (member constructors through the inline GamePad ctor; the delete-expression's deleting destructor)

GameSystem::GameSystem()
    : m_meterMode(MeterMode_Off),
      m_pad0(0, true),
      m_pad1(1, true),
      m_maskedPad(1, true),
      m_packets(sPacketBuffer0, kPacketBufferSize, sPacketBuffer1, kPacketBufferSize),
      m_perfMeter(48, 32, 470, 192, 24),
      m_clock(gameClockTicksPerFrame(), gameVsyncsPerFrame(), false, true)   // PC: the ROM's (12, 2) at game_rate 25
{
    ButtonMap *map = &getGameOptions()->buttonMap;
    m_pad0.m_optionsMap = map;
    m_pad1.m_optionsMap = map;
    m_maskedPad.m_optionsMap = map;
    // A child clock made and dropped at once (left over?).
    delete m_clock.createChild();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0012d2b0

void GamePad::setButtonMap(ButtonMap *map)
{
    m_optionsMap = map;
}

// The scale that maps a square stick position onto the unit circle:
// sqrt(1 + (min / max)^2) of the two magnitudes (inline in the ROM).
static inline float stickCircleScale(float x, float y)
{
    float ax = fabsf(x);
    float ay = fabsf(y);
    // PC: both zero divides 0 by 0; the EE gives +MAX and the scaled stick
    // comes out 0, the host would give NaN. Scale 1 keeps the 0.
    if (ax == 0.0f && ay == 0.0f)
        return 1.0f;
    float r = (ay <= ax) ? ay / ax : ax / ay;
    return sqrtf(r * r + 1.0f);
}

// ──────────────────────────────────────────────────────────────────────
// After a read: both sticks squared into a circle (y turned up), their
// lengths and two angles each (atan2(y, x) and atan2(-x, y)), 0 at rest.
// FS2 0x0012d2c0

void GamePad::computeSticks()
{
    float leftScale = stickCircleScale(m_stick[2], m_stick[3]);
    float rightScale = stickCircleScale(m_stick[0], m_stick[1]);
    m_dir[2] = m_stick[2] / leftScale;
    m_dir[3] = -m_stick[3] / leftScale;
    m_dir[0] = m_stick[0] / rightScale;
    m_dir[1] = -m_stick[1] / rightScale;

    m_length[3] = m_length[2] = sqrtf(m_dir[2] * m_dir[2] + m_dir[3] * m_dir[3]);
    m_length[1] = m_length[0] = sqrtf(m_dir[0] * m_dir[0] + m_dir[1] * m_dir[1]);

    if (m_dir[2] == 0.0f && m_dir[3] == 0.0f) {
        m_angle[2] = 0.0f;
        m_angle[3] = 0.0f;
    } else {
        m_angle[2] = atan2f(m_dir[3], m_dir[2]);
        m_angle[3] = atan2f(-m_dir[2], m_dir[3]);
    }
    if (m_dir[0] == 0.0f && m_dir[1] == 0.0f) {
        m_angle[0] = 0.0f;
        m_angle[1] = 0.0f;
    } else {
        m_angle[0] = atan2f(m_dir[1], m_dir[0]);
        m_angle[1] = atan2f(-m_dir[0], m_dir[1]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// GamePad::update's body as a plain member (called directly by 0x239d50).
// FS2 0x0012d4f0

int GamePad::read()
{
    int got = Pad::update();
    if (got)
        computeSticks();
    return got;
}

// ──────────────────────────────────────────────────────────────────────
// CodeWarrior destructor (this, short flags): GamePad's vtable back, Pad's
// destructor, operator delete when flags > 0.
// FS2 0x0012d540
// callcheck: ~Pad, operator_delete (base destructor and the deleting variant, compiler generated)

GamePad::~GamePad()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0012d5a0
// callcheck: Pad, setDefaults (Pad::Pad(bool) as the base constructor; ButtonMap::setDefaults inside the inline init)

GamePad::GamePad()
    : Pad(false)
{
    init();
}

// ──────────────────────────────────────────────────────────────────────
// The boot, from main with argv[0]: heap, rx (thread priorities 15 / 79 /
// 81) and SPK.ROM, pads, a cleared GS, render targets, sound (PAL), the
// H-blank counter, both RNGs seeded from the RTC, system data and options,
// the global object, the memory card, and the DMAC bus-error handler.
// FS2 0x0012d620
// callcheck: operator_new, GameSystem (the new-expression)

static void gameInit(const char *bootPath)
{
    initGlobalHeap(0);
    rxInit(bootPath, 15, 79, 81);
    padSysInit();
    sceGsSyncV(0);
    gsClearVram();
    sceGsSyncV(0);
    gfxInitRenderTargets();
    soundInit(1);
    while (sceGsSyncV(0) == 0) {
    }
    rcnt1Start();

    // The ROM copies the date before summing it (an inlined by-value helper?).
    DateTime now;
    getLocalDateTime(&now, 0);
    u_int seed = now.month + now.day + now.hour + now.second + now.minute;
    srand(seed);
    rndInit(seed & 0xff);

    systemDataInit();
    getGameOptions()->setDefaults();
    gGameSystem = new GameSystem;
    debugSysInit();
    memcardInit();
    AddDmacHandler(kDmacCauseBusError, dmacBusErrorHandler, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Clears all of GS memory to black, one 256 x 256 block per pass, before
// the render targets are set up.
// PC: there is no GS memory (no GS emulator); the host SDK's GIF channel
// drops the packet, and host render targets start cleared.
// FS2 0x0012d770

static void gsClearVram()
{
    // PC: zeroed; the ROM leaves the tag's PRE / PRIM bits as stack garbage.
    GsClearPacket pkt = {};
    pkt.tag.qwc = 15;
    pkt.tag.id = 0;             // REFE
    pkt.tag.next = (sceDmaTag *)&pkt.giftag;
    pkt.clear.rgbaq.Q = 1.0f;   // sceGsSetDefClear writes it again
    pkt.giftag.NLOOP = 14;
    pkt.giftag.EOP = 1;
    pkt.giftag.FLG = SCE_GIF_PACKED;
    pkt.giftag.NREG = 1;
    pkt.giftag.REGS0 = SCE_GIF_PACKED_AD;
    sceGsSetDefDrawEnv(&pkt.env, SCE_GS_PSMCT32, 256, 256, SCE_GS_ZNOUSE, SCE_GS_PSMZ32);
    sceGsSetDefClear(&pkt.clear, SCE_GS_ZNOUSE, 0x780, 0x700, 256, 512, 0, 0, 0, 0, 0);

    for (int i = 0, addr = 0; i < kVramClearBlocks; i++, addr += kVramClearBlockWords) {
        pkt.env.frame1.FBP = addr / 2048;
        sceDmaSync(sceDmaGetChan(SCE_DMA_GIF), 0, 0x7fffffff);
        FlushCache(WRITEBACK_DCACHE);
        sceDmaSend(sceDmaGetChan(SCE_DMA_GIF), &pkt.tag);
        sceGsSyncPath(0, 0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Entry point (crt0 calls it, then exit): boot, run the thread system with
// the main loop in slot 0 until the game ends, then tear everything down.
// PC: the platform layer's entry calls this (bootstrap 6).
// FS2 0x0012d910
// callcheck: __init_cpp, __destroy_global_chain (CodeWarrior's static constructors and destructors: the host C++ runtime runs them)
// callcheck: ~MasterClock, ~PerfMeter, ~GamePacketManager, ~Pad, operator_delete (the delete-expression: GameSystem's implicit destructor with the GamePad destructors inline)

int main(int argc, char **argv)
{
    gameInit(argv[0]);
    gArgc = argc;
    gArgv = argv;
    threadSysInit(gameFrameReady, gameVsyncsPerFrame());   // PC: the ROM's 2 at game_rate 25
    gGfxPutDrawEnv = 1;
    gGfxKickDisplayPacket = 1;
    gGfxFlipNoPackets = 0;
    threadSetSwitchHook(gameThreadSwitchHook, &gGameSystem->m_perfMeter);
    threadRunMain(gameMainThread);
    threadSetSwitchHook(0, 0);
    threadSysShutdown();

    memcardEnd();
    debugSysShutdown();
    soundShutdown();
    delete gGameSystem;
    gfxReleaseRenderTargets();
    padSysShutdown();
    rxTerm();
    shutdownGlobalHeap();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Thread switch hook: a meter mark per switch, labelled with the slot's
// name, coloured by slot % 6.
// FS2 0x0012dab0

static void gameThreadSwitchHook(u_int slot, void *arg)
{
    ((PerfMeter *)arg)->addBar(0.0f, sThreadColours[slot % 6], PerfMeterGroup_Thread, sThreadNames[slot]);
}

// ──────────────────────────────────────────────────────────────────────
// The main loop (thread slot 0): starts the scene controller in slot 1,
// then per frame waits for the frame sync, flips, reads the pads, steps
// sound, lets the other slots run and waits for the GS.
// FS2 0x0012db00

static void gameMainThread(void *arg)
{
    (void)arg;
    threadSlotCreate(gameSceneThread, ThreadSlot_Main, 0, 0, 0, 0);
    for (;;) {
        debugSysUpdate();
        int vsyncs = frameSyncWait();
        gGameSystem->flipFrame(vsyncs);
        padSysUpdate();
        soundUpdate();
        threadYield(1);
        gGameSystem->endFrameSync();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The thread system's per-vsync callback: a frame is ready when the main
// loop waits and the interval has passed. The display flips either way;
// packets and draw env go out only on a ready frame (as the flags allow),
// never while gGfxFlipNoPackets is set.
// FS2 0x0012db70

static int gameFrameReady(FrameSync *sync)
{
    bool ready = false;
    if (sync->requested == 1)
        ready = sync->interval <= sync->count;

    if (ready) {
        if (!gGfxFlipNoPackets)
            gfxFlipDisplay(1, gGfxKickDisplayPacket, gGfxPutDrawEnv);
        else
            gfxFlipDisplay(1, 0, 0);
        return 1;
    }
    if (!gGfxFlipNoPackets)
        gfxFlipDisplay(0, 1, 1);
    else
        gfxFlipDisplay(0, 0, 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// DMAC handler for BEIS: acknowledge the bus error (write 1 to clear) and
// flag it for GameSystem::endFrameSync's reset.
// FS2 0x0012dc20

static int dmacBusErrorHandler(int cause)
{
    ((volatile tD_STAT *)D_STAT)->BEIS = 1;
    gDmacBusError = 1;
    ExitHandler();
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// EE slot 0 (+0x8) of GamePad's vtable 0x311020: read, then the sticks.
// FS2 0x0012dc60

int GamePad::update()
{
    int got = Pad::update();
    if (got)
        computeSticks();
    return got;
}

// ──────────────────────────────────────────────────────────────────────
// EE slot 0 (+0x8) of MaskedGamePad's vtable 0x311030: GamePad::update's
// body, then everything cleared while L1 + L2 are held.
// FS2 0x0012dcb0

int MaskedGamePad::update()
{
    int got = Pad::update();
    if (got)
        computeSticks();
    if ((m_held & (SCE_PADL2 | SCE_PADL1)) == (SCE_PADL2 | SCE_PADL1))
        clear();
    return got;
}
