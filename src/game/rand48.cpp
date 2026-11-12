// The random numbers of a stage, 0x2681e0..0x268310 (game/rand48.h).

#include "game/rand48.h"

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002681e0

int Rand48::next(int n)
{
    int value;
    int left;
    do {
        m_state = (m_state * kRand48Multiplier) & kRand48StateMask;
        value   = (int)(m_state >> 17);
        // The EE's div by 0 leaves the dividend in HI and does not trap.
        left = n != 0 ? value % n : value;
    } while ((int)((u_int)n + 1 + ((u_int)value - (u_int)left)) < 0);
    return left;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00268290

void Rand48::seed(u_int value)
{
    m_state = (value ^ kRand48Multiplier) & kRand48StateMask;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002682e0

Rand48::Rand48()
{
    m_state = kRand48Multiplier;
}
