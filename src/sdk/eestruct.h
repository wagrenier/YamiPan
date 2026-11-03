// sdk/eestruct.h -- the EE's register bit fields FS2 names.
#pragma once

#include "libdma.h"
#include "scetypes.h"

struct tD_STAT
{
    u_int CIS : 10;
    u_int p0 : 3;
    u_int SIS : 1;
    u_int MEIS : 1;
    u_int BEIS : 1;
    u_int CIM : 10;
    u_int p1 : 3;
    u_int SIM : 1;
    u_int MEIM : 1;
    u_int p2 : 1;
};
static_assert(sizeof(tD_STAT) == 4);
