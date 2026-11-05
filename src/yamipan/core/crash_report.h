// What a fault leaves behind in a run with no debugger. The game's bugs are often timing or memory layout,
// and those can go away under gdb: a plain run has to say where it died by itself.
#pragma once

namespace yamipan::crash
{

// Prints the fault and its stack on stdout for any later unhandled exception, on any thread. The addresses
// are offsets into the exe: tools/reverse/crashsym.py turns the lines into functions.
void install();

} // namespace yamipan::crash
