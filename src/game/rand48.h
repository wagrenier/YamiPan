/*
 *  game/rand48.h -- the random numbers of a stage.
 *
 *  StageScene +0xe4, a stage's only: a sub-scene leaves it null. All that
 *  reads it is the script property Prop_Random100, through StageScene::random.
 *  The generator is java.util.Random's without the increment: the multiplier
 *  0x5deece66d, 48 bits of state, the top 31 of them taken, and the same way
 *  of throwing away the top of the range, off by two (n + 1 where Java has
 *  n - 1). The file name is the port's: no source string (architecture.md).
 */
#ifndef GAME_RAND48_H
#define GAME_RAND48_H

#include "sdk/scetypes.h"

constexpr u_long kRand48Multiplier = 0x5deece66dull;
constexpr u_long kRand48StateMask  = 0xffffffffffffull;

// EE 8 bytes.
struct Rand48
{
    Rand48();                                                            // FS2 0x002682e0
    // A number in 0..n-1. n of 0x7fffffff never comes back: every draw is
    // thrown away. Every caller in the ROM passes 100.
    int next(int n);                                                     // FS2 0x002681e0
    void seed(u_int value);                                              // FS2 0x00268290

    u_long m_state;             // FS2 +0x0
};

static_assert(sizeof(Rand48) == 0x8, "Rand48 EE size");
static_assert(offsetof(Rand48, m_state) == 0x0, "Rand48::m_state");

#endif /* GAME_RAND48_H */
