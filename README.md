# YamiPan

A function-by-function reconstruction of **Forbidden Siren 2** (PlayStation 2, Europe, `SCES-53851`)
in C++, running natively on PC. It is the fourth such port, after MioPan (Fatal Frame 2), ReiPan
(Fatal Frame 3) and RosePan (Rule of Rose).

> **Unofficial fan project.** YamiPan is not affiliated with, endorsed or sponsored by Sony
> Interactive Entertainment. Forbidden Siren 2 and its names are the property of their owners.
>
> **No game files are included.** This repository holds no executable, no disc data, no BIOS and no
> Sony SDK files. YamiPan does nothing without the game's own files, which you must dump from a disc
> you own.

## Status

Work in progress. About 6,100 of the game's 6,600 functions are reconstructed. The game boots, runs
its menus and stages, and plays its sound and movies. Much of it has not been checked on screen yet,
so expect bugs.

Beyond the original:

- renders at the window's resolution, in widescreen or any other aspect ratio (backdrops and movies
  keep their shape)
- runs the game at 25 Hz as on the console, or at 50 or 60 Hz (`game_rate` in `yamipan.ini`)
- optional per-pixel lighting
- a debug menu with a teleport to any stage, film or event (F2)

## Building

Windows, with CMake 3.28+ and a C++23 compiler (MinGW-w64 GCC or MSVC).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

```bash
cmake --build build
```

CMake fetches SDL3, Dear ImGui and ImPlot itself. Two optional prebuilt tools are not fetched:

- **FFmpeg** in `extern/ffmpeg` (`include/`, `lib/`, `bin/`): without it, the build still works but
  movies stay black.
- **SDL_shadercross** in `extern/shadercross`: only needed to recompile the shaders. Without it, the
  compiled shaders in `resources/shaders/` are used.

`tools/reverse/build.py` wraps the MinGW build and can also smoke-run the game.

## Running

YamiPan needs a folder holding the disc's files (`SPK.ROM`, `ROM.000` and the rest). It looks for
them in this order:

1. the `YAMIPAN_DATA_DIR` environment variable
2. the folder set in the settings
3. next to the executable, or a `data/` folder beside it
4. the working directory, or a `data/` folder in it

Settings (`yamipan.ini`), the log and memory cards go in `%APPDATA%\Mikompilation\YamiPan\`.

## Repository layout

| path | what it is |
|---|---|
| `src/game`, `src/rx`, `src/rena`, `src/sdpr2`, `src/sound`, `src/iop` | the game's own code, reconstructed |
| `src/sdk` | the PS2 library calls the game makes, reimplemented on the platform layer |
| `src/yamipan` | the platform layer: SDL3 window and input, GPU renderer, audio, IOP and SPU2 |
| `resources/shaders` | HLSL shaders and their compiled forms |
| `tools/reverse` | the reverse-engineering tools, built around Ghidra and GhidraMCP (see its README) |
| `yamipan_functions.txt` | every function in the game with its address, name and source file |
| `.claude/skills/yamipan-port` | the porting workflow and the project's notes, written for Claude Code |

Much of the reconstruction is done with Claude Code: the `yamipan-port` skill holds the workflow, the
decisions and what has been learnt about the game.
