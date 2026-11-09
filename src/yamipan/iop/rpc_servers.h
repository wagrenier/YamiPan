// The IOP side of SIF RPC: the servers an IOP module registers, and the EE's calls into them. On the
// IOP a server's thread looped in sceSifRpcLoop taking requests off its queue; here sceSifRpcLoop only
// marks its servers serving and returns, and a call runs on the caller's thread holding the big kernel
// lock, as any IOP context does (RosePan's sdk/iop_sif.cpp). RXSNDDRV keeps its queue and serve records
// static for that reason (iop/rxsnddrv/rpc.cpp).
#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace yamipan::iop
{

class Kernel;

class RpcServers
{
public:
    // (command, data, size) -> the reply buffer, as sceSifRpcFunc.
    using Handler = void *(*)(std::uint32_t command, void *data, int size);

    // sceSifRegisterRpc. `buffer` is where the server reads a call's data from.
    void add(std::uint32_t number, Handler handler, void *buffer, const void *queue);

    // sceSifRpcLoop: every server on `queue` starts serving.
    void serve(const void *queue);

    bool serving(std::uint32_t number) const;

    // Copies `data` into the server's buffer and runs its handler under the big kernel lock. The reply
    // buffer, or null when nothing serves `number`.
    void *call(Kernel &kernel, std::uint32_t number, std::uint32_t command, const void *data, int size);

private:
    struct Server
    {
        std::uint32_t number = 0;
        Handler handler = nullptr;
        void *buffer = nullptr;
        const void *queue = nullptr;
        bool serving = false;
    };

    const Server *find(std::uint32_t number) const;

    mutable std::mutex m_lock;
    std::vector<Server> m_servers;
};

} // namespace yamipan::iop
