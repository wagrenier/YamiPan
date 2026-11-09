#include "yamipan/iop/rpc_servers.h"

#include <cstring>

#include "yamipan/iop/kernel.h"

namespace yamipan::iop
{

void RpcServers::add(std::uint32_t number, Handler handler, void *buffer, const void *queue)
{
    std::lock_guard lock(m_lock);
    m_servers.push_back(Server{number, handler, buffer, queue, false});
}

void RpcServers::serve(const void *queue)
{
    std::lock_guard lock(m_lock);
    for (Server &server : m_servers)
        if (server.queue == queue)
            server.serving = true;
}

const RpcServers::Server *RpcServers::find(std::uint32_t number) const
{
    for (const Server &server : m_servers)
        if (server.number == number && server.serving && server.handler != nullptr)
            return &server;
    return nullptr;
}

bool RpcServers::serving(std::uint32_t number) const
{
    std::lock_guard lock(m_lock);
    return find(number) != nullptr;
}

void *RpcServers::call(Kernel &kernel, std::uint32_t number, std::uint32_t command, const void *data, int size)
{
    Server server;
    {
        std::lock_guard lock(m_lock);
        const Server *found = find(number);
        if (found == nullptr)
            return nullptr;
        server = *found;
    }

    // The handler reads the buffer it registered, as the SIF had copied the call's data there.
    kernel.enter();
    void *argument = const_cast<void *>(data);
    if (server.buffer != nullptr && data != nullptr && size > 0)
    {
        std::memcpy(server.buffer, data, static_cast<std::size_t>(size));
        argument = server.buffer;
    }
    void *reply = server.handler(command, argument, size);
    kernel.leave();
    return reply;
}

} // namespace yamipan::iop
