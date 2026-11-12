#include "yamipan/gpu/device.h"

#include <array>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <vector>

#include <SDL3/SDL_error.h>

#include "yamipan/core/logger.h"
#include "yamipan/core/paths.h"

namespace yamipan::gpu
{
namespace
{

struct ShaderFormat
{
    SDL_GPUShaderFormat format;
    std::string_view directory;
    std::string_view extension;
    const char *entryPoint;
};

constexpr std::array kShaderFormats{
    ShaderFormat{SDL_GPU_SHADERFORMAT_DXIL, "dxil", ".dxil", "main"},
    ShaderFormat{SDL_GPU_SHADERFORMAT_SPIRV, "spirv", ".spv", "main"},
    ShaderFormat{SDL_GPU_SHADERFORMAT_MSL, "msl", ".msl", "main0"},
};

} // namespace

void logGpuError(std::string_view what)
{
    log::error("{}: {}", what, SDL_GetError());
}

std::expected<std::unique_ptr<Device>, std::string> Device::create(const Paths &paths)
{
    constexpr SDL_GPUShaderFormat formats =
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

    SDL_GPUDevice *const device = SDL_CreateGPUDevice(formats, false, nullptr);
    if (device == nullptr)
        return std::unexpected(std::format("SDL_CreateGPUDevice: {}", SDL_GetError()));

    log::info("GPU driver: {}", SDL_GetGPUDeviceDriver(device));
    return std::unique_ptr<Device>(new Device(device, paths));
}

Device::Device(SDL_GPUDevice *device, const Paths &paths) : m_device(device), m_paths(paths) {}

Device::~Device()
{
    SDL_WaitForGPUIdle(m_device);
    SDL_DestroyGPUDevice(m_device);
}

Shader Device::loadShader(std::string_view name, SDL_GPUShaderStage stage, ShaderResources resources) const
{
    const SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(m_device);
    for (const ShaderFormat &format : kShaderFormats)
    {
        if ((supported & format.format) == 0)
            continue;

        const auto path = m_paths.resource(std::format("resources/shaders/{}/{}{}", format.directory, name,
                                                       format.extension));
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            log::error("Missing shader {}", pathToUtf8(path));
            return {};
        }
        const std::vector<char> code((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        SDL_GPUShaderCreateInfo info{};
        info.code_size = code.size();
        info.code = reinterpret_cast<const Uint8 *>(code.data());
        info.entrypoint = format.entryPoint;
        info.format = format.format;
        info.stage = stage;
        info.num_samplers = resources.samplers;
        info.num_uniform_buffers = resources.uniformBuffers;
        info.num_storage_buffers = resources.storageBuffers;

        SDL_GPUShader *const shader = SDL_CreateGPUShader(m_device, &info);
        if (shader == nullptr)
            logGpuError(std::format("Shader {}", name));
        return Shader(m_device, shader);
    }

    log::error("No shipped shader format fits this GPU backend");
    return {};
}

Texture Device::createTexture(unsigned width, unsigned height, SDL_GPUTextureFormat format,
                              SDL_GPUTextureUsageFlags usage) const
{
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture *const texture = SDL_CreateGPUTexture(m_device, &info);
    if (texture == nullptr)
        logGpuError("SDL_CreateGPUTexture");
    return Texture(m_device, texture);
}

Sampler Device::createSampler(const SDL_GPUSamplerCreateInfo &info) const
{
    SDL_GPUSampler *const sampler = SDL_CreateGPUSampler(m_device, &info);
    if (sampler == nullptr)
        logGpuError("SDL_CreateGPUSampler");
    return Sampler(m_device, sampler);
}

TransferBuffer Device::createUploadBuffer(unsigned size) const
{
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = size;
    return TransferBuffer(m_device, SDL_CreateGPUTransferBuffer(m_device, &info));
}

TransferBuffer Device::createDownloadBuffer(unsigned size) const
{
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = size;
    return TransferBuffer(m_device, SDL_CreateGPUTransferBuffer(m_device, &info));
}

Buffer Device::createVertexBuffer(unsigned size) const
{
    SDL_GPUBufferCreateInfo info{};
    info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    info.size = size;
    return Buffer(m_device, SDL_CreateGPUBuffer(m_device, &info));
}

Buffer Device::createIndexBuffer(unsigned size) const
{
    return createBuffer(SDL_GPU_BUFFERUSAGE_INDEX, size);
}

Buffer Device::createBuffer(SDL_GPUBufferUsageFlags usage, unsigned size) const
{
    SDL_GPUBufferCreateInfo info{};
    info.usage = usage;
    info.size = size;
    return Buffer(m_device, SDL_CreateGPUBuffer(m_device, &info));
}

void Device::uploadTexture(SDL_GPUCopyPass *copy, SDL_GPUTexture *texture, unsigned width, unsigned height,
                           std::span<const std::byte> pixels, TransferBuffer &staging) const
{
    staging = createUploadBuffer(static_cast<unsigned>(pixels.size()));
    if (!staging)
        return;

    void *const mapped = SDL_MapGPUTransferBuffer(m_device, staging.get(), false);
    if (mapped == nullptr)
        return;
    std::memcpy(mapped, pixels.data(), pixels.size());
    SDL_UnmapGPUTransferBuffer(m_device, staging.get());

    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = staging.get();
    source.pixels_per_row = width;
    source.rows_per_layer = height;

    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = width;
    destination.h = height;
    destination.d = 1;

    SDL_UploadToGPUTexture(copy, &source, &destination, false);
}

} // namespace yamipan::gpu
