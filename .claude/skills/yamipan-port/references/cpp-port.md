# The C++ port layer

The user's rule (2026-11-03): **the Pan layer is modern, object-oriented C++; the game layer stays
authentic.** YamiPan keeps what MioPan / ReiPan / RosePan offer -- the same window, the same ImGui UI,
the same settings, the same aspect handling -- but written as pragmatic classes, adapted to FS2. This
supersedes the 2026-11-02 rule "renamed, not rewritten" for the Pan layer: the siblings' *abstractions
and hard-won behaviour* are reused, their *code shape* is not. Read a sibling to learn what a feature
must do and why, then write it as a class here.

## Scope

| tree | what it is | rule |
|---|---|---|
| `src/yamipan/` | the Pan layer | modern OO C++ throughout: everything below applies in full |
| `src/sdk/` | SCE's API on the host | SCE's names and structs stay, they are the game's API. `.cpp` files are thin: state lives in `yamipan::` classes. Pan layer behind the names |
| `src/iop/` | the IOP's game code: RXSNDDRV (`src/iop/rxsnddrv/`, RosePan's reconstruction re-addressed to FS2) | authentic, as the game layer below; functions carry `IOP 0x<addr>`, never `FS2 0x` |
| `src/game/`, `src/rena/`, `src/sound/`, `src/rx/`, `src/sdpr2/` | the game layer | **stays authentic**: reconstruction.md, the ROM's shapes, the game's file names (`rxfile.c`). This file does not apply. No game file is restyled for the host's sake |

Where game code reaches the host it does so through a few seams it already includes
(`yamipan/rendering/yamipan_drawui.h`, `yamipan/os/yamipan_time.h`, `yamipan/yamipan_memory.h`). They
keep their `YamiPan_*` functions: thin forwarders onto the classes, so the game side never changes.

## How the code is written

- C++23 (g++ 16). `#pragma once`. No force-included prefix header: a file includes what it uses.
- Names: `PascalCase` types, `camelCase` functions, `m_camelCase` members, `kPascalCase` constants,
  `snake_case.h` files. Everything host-side is in `namespace yamipan` (sub-namespaces by directory).
- Ownership is RAII: `std::unique_ptr`, value members, the `gpu::` handles. No raw `new` / `delete` /
  `malloc` in host code. A class that owns a resource deletes or defines its copy operations.
- State lives in objects, not file-scope globals. The one service locator is `Application::instance()`,
  for the edge where game code (which cannot be given references) reaches the host; host classes take
  their collaborators by reference in the constructor.
- `enum class` over `#define` / plain enums; `constexpr` over macros; `std::span`,
  `std::string_view`, `std::filesystem::path`, `std::optional`, `std::expected` over pointer + size
  pairs, `char *` and out-parameters.
- Threads and locks come from the standard library (`std::mutex`, `std::condition_variable`,
  `std::atomic`), not SDL's, except `os::Thread` for named threads with a chosen stack size.
  **A wait with a deadline or a duration goes through `os::TimedWaiter`**, never `wait_until` /
  `wait_for` / `std::this_thread::sleep_for`: under MinGW those land on the 15.6 ms system tick
  (hazards.md "Platform layer", 2026-11-07). Untimed waits on a condition variable are fine.
- Fallible start-up answers `std::expected<T, std::string>`; run-time trouble logs and degrades.
- Pragmatism beats ceremony: no interface with one implementation, no getter for a plain settings
  struct, no abstraction a second user does not need. Short free functions in an anonymous namespace
  are fine. Inheritance where there is a real family (`ui::Panel`).
- Comments say why, in full sentences. Carry over the siblings' explanations of behaviour that bit
  them (present passes, HDR, pacing); they are the expensive part.

## The map

```
src/main.cpp                     the process entry: Application(args).run()
src/common/yamipan_stub.h        YAMIPAN_STUB / YAMIPAN_BRIDGE (the tools' contract)
src/sdk/                         libgraph, libdma, libpad, ... : SCE's API, forwarding to the classes
         iop.cpp, iop_host.h,    the IOP libraries RXSNDDRV imports (thbase.h & co are macros onto
         libsd.h, thbase.h ...   YamiPan_Iop*), forwarding to iop::Kernel, RpcServers and Spu2
src/yamipan/
  core/    Application           owns everything; the service locator
           Logger (log::)        yamipan.log + stdout, [W] / [E], SDL's messages captured
           Paths                 user directory, data folder discovery, resources
           Config, IniFile       typed sections, defaults < yamipan.ini < environment
           MainThread            run a callable on the window's thread
  os/      Thread                named thread with a stack size
           TimedWaiter           a wait until a deadline, to ~1 ms, that wake() cuts short; sleepFor
  ee/      Kernel                the EE as FS2 sees it: one running thread by priority
           Thread, Semaphore     EE thread + semaphore objects behind CreateThread etc.
           InterruptController   INTC / DMAC handlers, DIntr, the vblank clock, alarms
  iop/     Kernel                the IOP as RXSNDDRV sees it: one big kernel lock over its threads,
                                 event flags, V-blank at the EE's video standard
           SystemMemory          the IOP's 2 MB; an IOP address (u_int) is an offset into it
           RpcServers            the SIF RPC servers a module registers; a call runs under the lock
           Spu2                  libsd's chip: registers, 2 MB of sound RAM, transfers that complete
           Spu2Voices            the SPU2's synthesiser: 48 ADPCM voices, ADSR, noise, pitch
                                 modulation, the PCM ring, the mixer thread
           Spu2Reverb            the SPU2's effect unit, one network per core, in sound RAM
  audio/   AudioOutput           the sound card: one SDL audio stream, 16-bit stereo
  gpu/     handles, Device       RAII over SDL_GPU, shader loading
  rendering/
           DisplayGeometry       THE source of display size and aspect (port feature #1)
           Renderer              the render thread: frames in, pictures out
           FrameBlend            smooth motion: between two game frames, the cameras, lights, model
                                 instances, bones and particles blended by the time since the newer
                                 frame's field (roadmap-framerate.md)
           UiCanvas              the game's 2D draw module (game/uidrawer.cpp & co call it)
           ModelRenderer         the game's 3D draw module (rena/rena_draw.cpp calls it): lights and
                                 places batches, draws them with a depth buffer
           TextureStore          TIM2 pictures decoded to RGBA8, cached by content
           FramePalettePass      the frame through a 256-colour ramp, FilmOverlayPass the jack's
                                 film: whole-frame passes between the frame's render passes;
                                 FrameFilterPass the game's shrink / blur / grow (gsFrameFilter),
                                 its VRAM blocks as scratch textures
           PresentPass           grade + HDR output, Screenshotter the smoke shots
  media/   MpegStream, MpegDecoder  libmpeg on the host: PSS demux, FFmpeg MPEG-2
  io/      InputSystem           keyboard + gamepad -> a DualShock 2 report, bindings
  ui/      Overlay, Panel        the ImGui menu bar and its panels
                                 (teleport_panel.cpp: F2, game/teleport.h, the one panel calling into the game)
```

## Threads

- **Main thread** (`Application::run`): SDL init, the window, the event pump. A window drag blocks
  here on Windows, so nothing else may live here.
- **Render thread** (`Renderer`): owns the GPU device's work, ImGui and the present. It re-presents
  the last game frame when none is new, so the window, the overlay and resizing stay alive through
  loads. Game frames arrive as whole `Frame` objects through a mailbox; a vsync stall never holds the
  EE side.
- **EE threads** (`ee::Kernel`): each EE thread is a host thread, but **exactly one runs game code at
  a time**, the highest-priority ready one, which is what FS2's cooperative slots rely on (the
  open question of 2026-11-02, game/thread.cpp). A kernel call is the switch point. A wake-up of a
  higher priority from an interrupt hands the CPU over at once; the preempted thread parks at its next
  kernel call. `DIntr` / `EIntr` hold off the interrupt thread.
- **Interrupt thread** (`ee::InterruptController`): the vblank clock, alarms, INTC handlers. The game
  chooses PAL or NTSC through `sceGsResetGraph`; the clock follows.
- **Audio thread** (`iop::Spu2Voices`, `YamiPanAudio`): mixes 256-frame blocks and keeps the sound card
  about 42 ms ahead; it takes only the synthesiser's own lock, never the big kernel lock.
- **IOP threads** (`iop::Kernel`): RXSNDDRV's tick, DMA and RPC threads,
  detached host threads that run IOP code only while holding the big kernel lock, which every IOP context
  (and the EE's RPC calls into the driver) takes, so IOP code runs one context at a time.

## What is kept from the siblings' contract

`yamipan.ini` next to `memcard/` in `%APPDATA%/Mikompilation/YamiPan/`, hand-editable, defaults < file
< environment; the `YAMIPAN_*` environment hooks in the tools README ("Platform contract"); the game
thread's name `YamiPanGame`; `[stub]`, `[W]`, `[E]` lines on stdout; shots in `YAMIPAN_SHOT_DIR`.
