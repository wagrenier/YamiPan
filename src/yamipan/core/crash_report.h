// What a fault leaves behind in a run with no debugger. The game's bugs are often timing or memory layout,
// and those can go away under gdb: a plain run has to say where it died by itself.
#pragma once

namespace yamipan::crash
{

// Prints the fault and its stack on stdout when the process dies of a fatal signal (a bad access, an
// illegal instruction, an arithmetic fault, an abort) or an exception nobody caught. The frames come from
// <stacktrace>, symbolised where the build has debug information. Call it once, on the main thread.
void install();

// The CRT keeps SIGSEGV, SIGILL and SIGFPE's handlers per thread on Windows, so every thread that should
// report installs them for itself: os::Thread does it for each thread it starts.
void installThread();

} // namespace yamipan::crash
