// common/yamipan_stub.h -- marks a function reconstructed code calls but nobody has written yet. The body
// logs once (name + FS2 address) and returns, so the build links and a run shows exactly which missing
// pieces were reached. tools/reverse/stubgen.py writes these, status.py counts them; replace the whole
// definition when the function is reconstructed.
#pragma once

#include <stdio.h>

#define YAMIPAN_STUB(name, fs2_addr)                                                                     \
    do {                                                                                                 \
        static int yamipan_stub_seen_;                                                                   \
        if (!yamipan_stub_seen_) {                                                                       \
            yamipan_stub_seen_ = 1;                                                                      \
            printf("[stub] %s (FS2 0x%08x) reached\n", name, (unsigned)(fs2_addr));                      \
        }                                                                                                \
    } while (0)

// A temporary stand-in that works but is not FS2's code (host-side behaviour so later TUs can run). The
// tools count it exactly like a stub: still to reconstruct, listed in its TU's brief.
#define YAMIPAN_BRIDGE(name, fs2_addr)                                                                   \
    do {                                                                                                 \
        static int yamipan_stub_seen_;                                                                   \
        if (!yamipan_stub_seen_) {                                                                       \
            yamipan_stub_seen_ = 1;                                                                      \
            printf("[stub] %s (FS2 0x%08x) reached (bridge)\n", name, (unsigned)(fs2_addr));             \
        }                                                                                                \
    } while (0)
