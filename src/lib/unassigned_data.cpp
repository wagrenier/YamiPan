// lib/unassigned_data.cpp -- FS2 globals whose owning TU is not cut yet: the stand-ins in game/gamehooks.h
// promise them, and the build must define them. Each moves to its TU when that is written.
#include "game/gamehooks.h"

Memcard *gMenuMemcard;                  /* sbss 0x31ae94 */
