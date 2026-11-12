// sdk/libdma.cpp -- the DMAC on the host: the channels exist and everything finishes at once. GS packets are
// not sent anywhere; the frame reaches the renderer through the draw modules.
#include "libdma.h"

#include <array>

namespace
{

constexpr std::size_t kChannelCount = 10;

std::array<sceDmaChan, kChannelCount> s_channels{};

} // namespace

int sceDmaReset(int)
{
    return 0;
}

sceDmaChan *sceDmaGetChan(int id)
{
    return &s_channels.at(static_cast<std::size_t>(id));
}

void sceDmaSend(sceDmaChan *, void *) {}

void sceDmaSendN(sceDmaChan *, void *, int) {}

int sceDmaSync(sceDmaChan *, int, int)
{
    return 0;
}
