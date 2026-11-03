// iop/rxsnddrv/rxsnddrv.h -- the host's side of the rx sound driver, as rx/rxsnd.c reaches it. The
// driver itself (RosePan's src/iop/rxsnddrv, the IOP half of rxsnd) comes with the audio milestone.
#pragma once

#include <stddef.h>

// rxsnd hands the driver EE memory by address: this registers a block before its DMA.
void YamiPan_RxSndNoteEeAddress(const void *ee, size_t size);
