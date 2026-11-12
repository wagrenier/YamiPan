/* ==========================================================================
 *  libmpeg_video.h  (what the movie player asks of the host decoder)
 *
 *  The ROM's IPU consumes the video ring as it decodes. The host decoder takes
 *  its own copy from the demultiplexer, so the player needs to know how much
 *  of the ring is not parsed yet (MoviePlayer::feedIpu).
 * ======================================================================== */

#pragma once

/* Elementary-stream bytes fed but not yet parsed into pictures. */
int YamiPanMpegVideoPending(void);
