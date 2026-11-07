// rena/rena_drawtarget.cpp -- the frame's draw target, a function-local static (rena/rena_drawtarget.h).

#include "rena/rena_drawtarget.h"

// FS2 0x002ec110
// The ROM tests the guard byte (gp-0x7940), stores 100, 100, 100 and 1.0f
// at 0x5a52d0 / d4 / d8 / e0 on the first call, sets the guard and answers
// 0x5a52d0: a function-local static with an inline ctor.
DrawTarget *getDrawTarget()
{
    static DrawTarget target;
    return &target;
}
