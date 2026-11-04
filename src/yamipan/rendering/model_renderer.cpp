#include "yamipan/rendering/model_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

#include "yamipan/rendering/renderer.h"

class Tim2Texture; // rena/rena_2d_texture_tim2.h

namespace yamipan::rendering
{
namespace
{

constexpr unsigned kInitialVertexCapacity = 16 * 1024 * 1024;
constexpr SDL_GPUTextureFormat kDepthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
constexpr float kUvUnit = 4096.0f; // ITOF12
constexpr int kLightCount = 3;

// TEST_1 of the opaque pass (0x5050b): what falls under this alpha never reaches the depth buffer. The ROM
// then blends it in with a second GS pass; the host drops it.
constexpr float kOpaqueAlphaRef = 0x50 / 128.0f;

// Mirrors ModelVertexUniforms in model.vert.hlsl.
struct ModelVertexUniforms
{
    float worldToClip[16];
    float fog[4];
};

// Mirrors ModelFragmentUniforms in model.frag.hlsl.
struct ModelFragmentUniforms
{
    float fogColor[4];
    float params[4];
};

// The matrix from the game's screen space, in rows, to the host's clip space. Screen units are the frame's
// around its centre, y down, and the GS's Z, with zmax at the near plane. The sides widen with the display:
// what a wider picture shows is more of the world, never a stretch (the any-aspect rule).
void buildWorldToClip(const ModelPass &pass, const PictureLayout &layout, float *out)
{
    const double shape = static_cast<double>(layout.aspect) / kFrameDisplayAspect;
    const double scaleX = 1.0 / (pass.halfWidth * std::max(shape, 1.0));
    const double scaleY = -std::min(shape, 1.0) / pass.halfHeight;
    const double depthRange = static_cast<double>(pass.depthMax) - pass.depthMin;

    for (int row = 0; row < 4; row++)
    {
        const double x = pass.viewScreen[row * 4 + 0];
        const double y = pass.viewScreen[row * 4 + 1];
        const double z = pass.viewScreen[row * 4 + 2];
        const double w = pass.viewScreen[row * 4 + 3];
        out[row * 4 + 0] = static_cast<float>((x - pass.centerX * w) * scaleX);
        out[row * 4 + 1] = static_cast<float>((y - pass.centerY * w) * scaleY);
        out[row * 4 + 2] = static_cast<float>((pass.depthMax * w - z) / depthRange);
        out[row * 4 + 3] = static_cast<float>(w);
    }
}

constexpr std::uint32_t kVertexAlpha = 0x80;

// A vertex's colour in the GS's scale (0x80 = 1.0): what the VU1 program does, 0x2a9 and 0x2bb. The light is
// ambient + each light's colour * max(0, its direction . the turned normal), the vertex colour is multiplied by
// it and clamped to 255. The vertex's alpha is never loaded (lqi.xyz): every vertex takes VU address 25's w, 0x80
// but for a ModelMaterial_Kind1's frames, which are not written. The spotlight kernel 0x545 is not here yet.
std::uint32_t litColor(const YamiPanModelLights &lights, const float *worldNormal, const std::uint8_t *color)
{
    float sum[3];
    std::copy_n(lights.ambient, 3, sum);
    for (int index = 0; index < kLightCount; index++)
    {
        const float *const direction = lights.directions[index];
        const float amount = std::max(0.0f, worldNormal[0] * direction[0] + worldNormal[1] * direction[1] +
                                                worldNormal[2] * direction[2]);
        for (int channel = 0; channel < 3; channel++)
            sum[channel] += lights.colors[index][channel] * amount;
    }

    std::uint32_t out = kVertexAlpha << 24;
    for (int channel = 0; channel < 3; channel++)
    {
        const float lit = static_cast<float>(color[channel]) * sum[channel];
        out |= static_cast<std::uint32_t>(std::clamp(lit, 0.0f, 255.0f)) << (channel * 8);
    }
    return out;
}

// A vertex through its bones, VU1 0x08a (four), 0x0bd (two), 0x0e1 (three), 0x113 (one): the sum of each bone's
// matrix times the weight. Weights are floats, used as they stand; a bone id counts VU quadwords (signed). The normal
// goes through the same sum and is normalised, except with one bone, which only turns it.
struct SkinnedVertex
{
    float position[3];
    float normal[3];
};

SkinnedVertex skinVertex(const YamiPanModelBatch &batch, int index)
{
    constexpr int kPaletteIdStep = 4;
    constexpr int kMatrixFloats = 16;

    const float *const position = batch.positions + index * 3;
    const float *const normal = batch.normals + index * 3;
    const std::int8_t *const ids = batch.boneIds + index * batch.bonesPerVertex;
    const float *const weights = batch.weights + index * batch.bonesPerVertex;
    const int bones = batch.firstBoneOnly ? 1 : batch.bonesPerVertex;

    SkinnedVertex out = {};
    for (int bone = 0; bone < bones; bone++)
    {
        if (ids[bone] < 0)
            continue; // an unused slot, -1 with weight 0: VU1 reads whatever lies before the palette and adds nothing
        const float *const matrix = batch.palette + (ids[bone] / kPaletteIdStep) * kMatrixFloats;
        const float weight = bones == 1 ? 1.0f : weights[bone];
        for (int axis = 0; axis < 3; axis++)
        {
            out.position[axis] += weight * (matrix[0 + axis] * position[0] + matrix[4 + axis] * position[1] +
                                            matrix[8 + axis] * position[2] + matrix[12 + axis]);
            out.normal[axis] += weight * (matrix[0 + axis] * normal[0] + matrix[4 + axis] * normal[1] +
                                          matrix[8 + axis] * normal[2]);
        }
    }
    if (bones > 1)
    {
        const float length = std::sqrt(out.normal[0] * out.normal[0] + out.normal[1] * out.normal[1] +
                                       out.normal[2] * out.normal[2]);
        if (length > 0.0f)
        {
            for (float &value : out.normal)
                value /= length;
        }
    }
    return out;
}

// ModelMeshFlag_Unlit: the VU1 program's 0x29e, the same alpha.
std::uint32_t rawColor(const std::uint8_t *color)
{
    return static_cast<std::uint32_t>(color[0]) | static_cast<std::uint32_t>(color[1]) << 8 |
           static_cast<std::uint32_t>(color[2]) << 16 | kVertexAlpha << 24;
}

} // namespace

ModelRenderer::ModelRenderer(Renderer &renderer, gpu::Device &device) : m_renderer(renderer), m_device(device)
{
    m_vertexShader = m_device.loadShader("model.vert", SDL_GPU_SHADERSTAGE_VERTEX, {.samplers = 0, .uniformBuffers = 1});
    m_fragmentShader =
        m_device.loadShader("model.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, {.samplers = 1, .uniformBuffers = 1});
}

void ModelRenderer::beginPass(const void *list, const YamiPanModelPass &pass)
{
    std::scoped_lock lock(m_lock);
    Layer &layer = m_layers[list];

    ModelPass &out = layer.passes.emplace_back();
    std::memcpy(out.viewScreen.data(), pass.viewScreen, sizeof(pass.viewScreen));
    out.centerX = pass.screenCenterX;
    out.centerY = pass.screenCenterY;
    out.halfWidth = pass.screenHalfWidth;
    out.halfHeight = pass.screenHalfHeight;
    out.depthMin = pass.depthMin;
    out.depthMax = pass.depthMax;
    std::copy_n(pass.fogColor, 3, out.fogColor.begin());
    out.fogA = pass.fogA;
    out.fogB = pass.fogB;
    out.fogNearValue = pass.fogNearValue;
    out.fogFarValue = pass.fogFarValue;
    out.clearDepth = pass.clearDepth != 0;
    out.firstDraw = static_cast<std::uint32_t>(layer.draws.size());
    layer.open = true;
}

void ModelRenderer::setMaterial(const void *list, const YamiPanModelMaterial &material)
{
    std::scoped_lock lock(m_lock);
    Layer &layer = m_layers[list];

    layer.image = nullptr;
    if (material.texture != nullptr)
    {
        const auto found = m_images.find(material.texture);
        if (found != m_images.end())
        {
            layer.image = found->second;
        }
        else
        {
            layer.image = m_renderer.textures().imageOf(*static_cast<const Tim2Texture *>(material.texture));
            m_images.emplace(material.texture, layer.image);
        }
    }
    layer.scrollU = static_cast<float>(material.scrollU) / kUvUnit;
    layer.scrollV = static_cast<float>(material.scrollV) / kUvUnit;
    layer.blended = material.blended != 0;
    layer.unlit = material.unlit != 0;
}

void ModelRenderer::setLights(const void *list, const YamiPanModelLights &lights)
{
    std::scoped_lock lock(m_lock);
    m_layers[list].lights = lights;
}

void ModelRenderer::drawBatch(const void *list, const float world[4][4], const YamiPanModelBatch &batch)
{
    std::scoped_lock lock(m_lock);
    Layer &layer = m_layers[list];
    if (!layer.open || batch.count < 3)
        return;

    const float *const matrix = &world[0][0];
    const auto vertexAt = [&](int index) {
        ModelVertex vertex;
        float worldNormal[3];
        if (batch.palette != nullptr)
        {
            const SkinnedVertex skinned = skinVertex(batch, index);
            vertex.x = skinned.position[0];
            vertex.y = skinned.position[1];
            vertex.z = skinned.position[2];
            std::copy_n(skinned.normal, 3, worldNormal);
        }
        else
        {
            const float *const position = batch.positions + index * 3;
            const float *const normal = batch.normals + index * 3;
            vertex.x = matrix[0] * position[0] + matrix[4] * position[1] + matrix[8] * position[2] + matrix[12];
            vertex.y = matrix[1] * position[0] + matrix[5] * position[1] + matrix[9] * position[2] + matrix[13];
            vertex.z = matrix[2] * position[0] + matrix[6] * position[1] + matrix[10] * position[2] + matrix[14];
            for (int axis = 0; axis < 3; axis++)
            {
                worldNormal[axis] =
                    matrix[0 + axis] * normal[0] + matrix[4 + axis] * normal[1] + matrix[8 + axis] * normal[2];
            }
        }
        vertex.u = static_cast<float>(batch.uvs[index * 2]) / kUvUnit + layer.scrollU;
        vertex.v = static_cast<float>(batch.uvs[index * 2 + 1]) / kUvUnit + layer.scrollV;
        vertex.color = layer.unlit ? rawColor(batch.colors + index * 4)
                                   : litColor(layer.lights, worldNormal, batch.colors + index * 4);
        return vertex;
    };

    const auto first = static_cast<std::uint32_t>(layer.vertices.size());
    if (batch.strip)
    {
        // Nothing culls, so a strip's alternating winding does not matter.
        for (int index = 2; index < batch.count; index++)
        {
            layer.vertices.push_back(vertexAt(index - 2));
            layer.vertices.push_back(vertexAt(index - 1));
            layer.vertices.push_back(vertexAt(index));
        }
    }
    else
    {
        for (int index = 0; index + 2 < batch.count; index += 3)
        {
            layer.vertices.push_back(vertexAt(index));
            layer.vertices.push_back(vertexAt(index + 1));
            layer.vertices.push_back(vertexAt(index + 2));
        }
    }

    // Runs of one state join into one draw: a mesh is many batches.
    const auto count = static_cast<std::uint32_t>(layer.vertices.size()) - first;
    if (layer.draws.size() > layer.passes.back().firstDraw)
    {
        ModelDraw &last = layer.draws.back();
        if (last.image == layer.image && last.blended == layer.blended && last.firstVertex + last.vertexCount == first)
        {
            last.vertexCount += count;
            return;
        }
    }
    layer.draws.push_back({layer.image, layer.blended, first, count});
}

void ModelRenderer::endPass(const void *list)
{
    std::scoped_lock lock(m_lock);
    Layer &layer = m_layers[list];
    if (!layer.open)
        return;
    ModelPass &pass = layer.passes.back();
    pass.drawCount = static_cast<std::uint32_t>(layer.draws.size()) - pass.firstDraw;
    layer.open = false;
}

void ModelRenderer::forgetTextures()
{
    std::scoped_lock lock(m_lock);
    m_images.clear();
}

void ModelRenderer::collect(const void *list, Frame &frame)
{
    std::scoped_lock lock(m_lock);
    const auto found = m_layers.find(list);
    if (found == m_layers.end())
        return;

    Layer &layer = found->second;
    const auto vertexOffset = static_cast<std::uint32_t>(frame.modelVertices.size());
    const auto drawOffset = static_cast<std::uint32_t>(frame.modelDraws.size());
    frame.modelVertices.insert(frame.modelVertices.end(), layer.vertices.begin(), layer.vertices.end());
    for (ModelDraw &draw : layer.draws)
    {
        draw.firstVertex += vertexOffset;
        frame.modelDraws.push_back(std::move(draw));
    }
    for (ModelPass &pass : layer.passes)
    {
        pass.firstDraw += drawOffset;
        pass.uiDrawsBefore = static_cast<std::uint32_t>(frame.draws.size());
        frame.modelPasses.push_back(pass);
    }
    layer.vertices.clear();
    layer.draws.clear();
    layer.passes.clear();
    layer.open = false;
}

void ModelRenderer::ensureDepth(int width, int height)
{
    if (m_depth && m_depthWidth == width && m_depthHeight == height)
        return;
    m_depth = m_device.createTexture(static_cast<unsigned>(width), static_cast<unsigned>(height), kDepthFormat,
                                     SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    m_depthWidth = width;
    m_depthHeight = height;
}

void ModelRenderer::uploadVertices(SDL_GPUCopyPass *copy, const Frame &frame)
{
    const auto bytes = static_cast<unsigned>(frame.modelVertices.size() * sizeof(ModelVertex));
    if (bytes == 0)
        return;

    if (bytes > m_vertexCapacity || !m_vertexBuffer)
    {
        m_vertexCapacity = std::max({bytes * 2, m_vertexCapacity, kInitialVertexCapacity});
        m_vertexBuffer = m_device.createVertexBuffer(m_vertexCapacity);
        m_vertexUpload = m_device.createUploadBuffer(m_vertexCapacity);
    }

    void *const mapped = SDL_MapGPUTransferBuffer(m_device.get(), m_vertexUpload.get(), true);
    if (mapped == nullptr)
        return;
    std::memcpy(mapped, frame.modelVertices.data(), bytes);
    SDL_UnmapGPUTransferBuffer(m_device.get(), m_vertexUpload.get());

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = m_vertexUpload.get();

    SDL_GPUBufferRegion destination{};
    destination.buffer = m_vertexBuffer.get();
    destination.size = bytes;

    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
}

SDL_GPUGraphicsPipeline *ModelRenderer::pipelineFor(bool blended)
{
    if (const auto found = m_pipelines.find(blended); found != m_pipelines.end())
        return found->second.get();

    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.pitch = sizeof(ModelVertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    const std::array<SDL_GPUVertexAttribute, 3> attributes{{
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, x)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(ModelVertex, u)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(ModelVertex, color)},
    }};

    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    if (blended)
    {
        target.blend_state.enable_blend = true;
        target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    }

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = m_vertexShader.get();
    info.fragment_shader = m_fragmentShader.get();
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes.data();
    info.vertex_input_state.num_vertex_attributes = attributes.size();
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = !blended;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = kDepthFormat;

    SDL_GPUGraphicsPipeline *const pipeline = SDL_CreateGPUGraphicsPipeline(m_device.get(), &info);
    if (pipeline == nullptr)
        gpu::logGpuError("SDL_CreateGPUGraphicsPipeline(model)");
    m_pipelines.emplace(blended, gpu::GraphicsPipeline(m_device.get(), pipeline));
    return pipeline;
}

void ModelRenderer::recordPass(SDL_GPUCommandBuffer *commands, const Frame &frame, const ModelPass &pass,
                               SDL_GPUTexture *scene, SDL_GPUTexture *white, const PictureLayout &layout)
{
    SDL_GPUColorTargetInfo target{};
    target.texture = scene;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = m_depth.get();
    depth.clear_depth = 1.0f;
    depth.load_op = pass.clearDepth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    SDL_GPURenderPass *const renderPass = SDL_BeginGPURenderPass(commands, &target, 1, &depth);
    if (renderPass == nullptr)
    {
        gpu::logGpuError("SDL_BeginGPURenderPass(model)");
        return;
    }

    if (pass.drawCount != 0 && m_vertexBuffer)
    {
        SDL_GPUBufferBinding vertices{};
        vertices.buffer = m_vertexBuffer.get();
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertices, 1);

        ModelVertexUniforms vertexUniforms{};
        buildWorldToClip(pass, layout, vertexUniforms.worldToClip);
        vertexUniforms.fog[0] = pass.fogA;
        vertexUniforms.fog[1] = pass.fogB;
        vertexUniforms.fog[2] = pass.fogNearValue;
        vertexUniforms.fog[3] = pass.fogFarValue;
        SDL_PushGPUVertexUniformData(commands, 0, &vertexUniforms, sizeof(vertexUniforms));

        for (std::uint32_t index = 0; index < pass.drawCount; index++)
        {
            const ModelDraw &draw = frame.modelDraws[pass.firstDraw + index];
            SDL_GPUGraphicsPipeline *const pipeline = pipelineFor(draw.blended);
            if (pipeline == nullptr || draw.vertexCount == 0)
                continue;
            SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

            const Image *const image = draw.image.get();
            SDL_GPUTextureSamplerBinding binding{};
            binding.texture = image != nullptr && image->texture != nullptr ? image->texture->get() : white;
            binding.sampler = m_renderer.samplerFor(true, true); // CLAMP_1 repeat, TEX1_1 linear
            SDL_BindGPUFragmentSamplers(renderPass, 0, &binding, 1);

            ModelFragmentUniforms fragmentUniforms{};
            std::copy_n(pass.fogColor.begin(), 3, fragmentUniforms.fogColor);
            fragmentUniforms.params[0] = draw.blended ? 0.0f : kOpaqueAlphaRef;
            SDL_PushGPUFragmentUniformData(commands, 0, &fragmentUniforms, sizeof(fragmentUniforms));

            SDL_DrawGPUPrimitives(renderPass, draw.vertexCount, 1, draw.firstVertex, 0);
        }
    }
    SDL_EndGPURenderPass(renderPass);
}

} // namespace yamipan::rendering
