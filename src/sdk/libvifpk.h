// sdk/libvifpk.h -- the VIF1 packet helper, as much of it as FS2's reconstructed code still calls:
// everything that wrote GS data goes to the host draw modules instead. EE layout: pCurrent +0,
// pBase +4, pDmaTag +8, pVifcode +0xc, 0x20 in all (host pointers make it larger here).
#pragma once

#include "libdma.h"
#include "sce_gs.h"
#include "scetypes.h"

struct sceVif1Packet
{
    u_long128 *pCurrent;
    u_long128 *pBase;
    u_long128 *pDmaTag;
    u_int *pVifcode;
};

void sceVif1PkReset(sceVif1Packet *packet);
void sceVif1PkNext(sceVif1Packet *packet, u_int address, int option);
void sceVif1PkCloseDmaTag(sceVif1Packet *packet);
