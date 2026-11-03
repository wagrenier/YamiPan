// sdk/libvu0.h -- the VU0 library's vector and matrix types. FS2's reconstructed code calls no
// sceVu0* function (its VU0 macro code is scalar C++ in place); it only uses the types, 16-aligned
// as in Sony's header so that a member lands at its EE offset.
#pragma once

#include "scetypes.h"

typedef float sceVu0FVECTOR[4] __attribute__((aligned(16)));
typedef int sceVu0IVECTOR[4] __attribute__((aligned(16)));
typedef float sceVu0FMATRIX[4][4] __attribute__((aligned(16)));

static_assert(sizeof(sceVu0FVECTOR) == 16 && alignof(sceVu0FVECTOR) == 16);
static_assert(sizeof(sceVu0IVECTOR) == 16 && alignof(sceVu0IVECTOR) == 16);
static_assert(sizeof(sceVu0FMATRIX) == 64 && alignof(sceVu0FMATRIX) == 16);
