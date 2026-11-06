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

// TEST_1 of the spot's pass is the opaque pass's (0x5050b): where picture * reach is under it, nothing is added.
constexpr float kSpotAlphaRef = kOpaqueAlphaRef;

// What VU1 scales a light's colours and the spot's reach by (LOI at 0x57e, 0x32b).
constexpr float kVertexColorUnit = 255.0f;

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
constexpr float kGsTexelUnit = 16.0f; // the GS's UV register

// A vertex's colour in the GS's scale (0x80 = 1.0): what the VU1 program does, 0x2a9 and 0x2bb. The light is
// ambient + each light's colour * max(0, its direction . the turned normal), the vertex colour is multiplied by
// it and clamped to 255. The vertex's alpha is never loaded (lqi.xyz): every vertex takes VU address 25's w, 0x80
// but for a ModelMaterial_Kind1's frames, which are not written. The spotlight kernel 0x545 is not here yet.
std::uint32_t litColor(const YamiPanModelLights &lights, const float *worldNormal, const float *color)
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
        const float lit = color[channel] * sum[channel];
        out |= static_cast<std::uint32_t>(std::clamp(lit, 0.0f, 255.0f)) << (channel * 8);
    }
    return out;
}

// The same place in the reflection's pass, VU1 0x77f: the texture is looked up by the eye's ray bounced off the
// normal, in the world's x, y (z is up), and it takes the first light's colour, none on a face turned from that light.
// Only y and z of the light count: the ROM has loaded the level over the row with x (the integer reads as 0).
ModelVertex reflectionVertex(const ModelVertex &vertex, const float *worldNormal, const std::array<float, 3> &eye,
                             const YamiPanModelLights &lights, float scale)
{
    float normal[3];
    std::copy_n(worldNormal, 3, normal);
    const float normalLength =
        std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (normalLength > 0.0f)
    {
        for (float &axis : normal)
            axis /= normalLength;
    }
    const float toEye[3] = {eye[0] - vertex.x, eye[1] - vertex.y, eye[2] - vertex.z};
    const float along = toEye[0] * normal[0] + toEye[1] * normal[1] + toEye[2] * normal[2];
    const float ray[3] = {toEye[0] - 2.0f * along * normal[0], toEye[1] - 2.0f * along * normal[1],
                          toEye[2] - 2.0f * along * normal[2]};
    const float length = std::sqrt(ray[0] * ray[0] + ray[1] * ray[1] + ray[2] * ray[2]);

    ModelVertex out = vertex;
    out.u = length > 0.0f ? ray[0] / length * 0.5f + 0.5f : 0.5f;
    out.v = length > 0.0f ? ray[1] / length * 0.5f + 0.5f : 0.5f;
    out.color = static_cast<std::uint32_t>(kVertexAlpha) << 24;
    const float facing = lights.directions[0][1] * normal[1] + lights.directions[0][2] * normal[2];
    if (facing > 0.0f)
    {
        for (int channel = 0; channel < 3; channel++)
        {
            const float lit = 128.0f * lights.colors[0][channel] * scale; // loi 128
            out.color |= static_cast<std::uint32_t>(std::clamp(lit, 0.0f, 255.0f)) << (channel * 8);
        }
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

// The point light on a vertex's colour, kernel 0x545 (0x59a for a flat material): what it adds, ahead of the
// multiply in litColor. `out` starts as the vertex's own colour.
void addPointLight(const YamiPanModelPointLight &light, bool flat, const float *position, const float *worldNormal,
                   float *out)
{
    const float toLight[3] = {light.position[0] - position[0], light.position[1] - position[1],
                              light.position[2] - position[2]};
    const float distance =
        std::sqrt(toLight[0] * toLight[0] + toLight[1] * toLight[1] + toLight[2] * toLight[2]);
    const float strength = std::max(0.0f, light.weight + light.slope * distance);
    if (strength == 0.0f)
        return;

    if (flat)
    {
        for (int channel = 0; channel < 3; channel++)
            out[channel] += kVertexColorUnit * light.color[channel] * strength;
        return;
    }

    const float facing =
        distance > 0.0f ? (worldNormal[0] * toLight[0] + worldNormal[1] * toLight[1] + worldNormal[2] * toLight[2]) /
                              distance
                        : 0.0f;
    const float amount = std::max(0.0f, facing) * strength;
    for (int channel = 0; channel < 3; channel++)
        out[channel] += kVertexColorUnit * (light.shade[channel] * strength + light.color[channel] * amount);
}

// A vertex of the spot's pass, VU1 0x32d..0x347. Behind the light it has no reach and takes a harmless q; the first
// pass's fog and lit colour stay with it.
ModelSpotVertex spotVertex(const YamiPanModelSpotLight &spot, const ModelVertex &vertex)
{
    const float *const matrix = &spot.worldToLight[0][0];
    float placed[3];
    for (int axis = 0; axis < 3; axis++)
    {
        placed[axis] = matrix[0 + axis] * vertex.x + matrix[4 + axis] * vertex.y + matrix[8 + axis] * vertex.z +
                       matrix[12 + axis];
    }

    const float ahead = placed[2];
    const bool behind = ahead < 0.0f;
    const float q = behind ? 1.0f : ahead;

    float reach = 0.0f;
    if (!behind && ahead < spot.limit)
    {
        const float falloff = spot.falloff[0] + spot.falloff[1] * ahead + spot.falloff[2] * ahead * ahead;
        reach = std::min(kVertexColorUnit / falloff, kVertexColorUnit);
    }

    ModelSpotVertex out;
    out.x = vertex.x;
    out.y = vertex.y;
    out.z = vertex.z;
    out.s = 0.5f * q + placed[0] * spot.scaleX;
    out.t = 0.5f * q + placed[1] * spot.scaleY;
    out.q = q;
    out.color = (vertex.color & 0x00ffffffu) | static_cast<std::uint32_t>(reach) << 24;
    return out;
}

// A vertex of a shadow's pass, VU1 0x3e8..0x3fb. Past the shadow's own plane (the projection's z not negative) it has
// no alpha, otherwise w, which fades from 0x80 there to nothing at the box's far end.
ModelShadowVertex shadowVertex(const float *projection, const ModelVertex &vertex)
{
    float placed[4];
    for (int lane = 0; lane < 4; lane++)
    {
        placed[lane] = projection[0 + lane] * vertex.x + projection[4 + lane] * vertex.y +
                       projection[8 + lane] * vertex.z + projection[12 + lane];
    }

    ModelShadowVertex out;
    out.x = vertex.x;
    out.y = vertex.y;
    out.z = vertex.z;
    out.s = placed[0];
    out.t = placed[1];
    out.q = 1.0f;
    out.color = placed[2] < 0.0f ? static_cast<std::uint32_t>(std::max(placed[3], 0.0f)) << 24 : 0u;
    return out;
}

// Whether every vertex lies beyond one side of a shadow's box: VU1 0x618 asks it of the batch's bounds.
bool outsideShadowBox(const float *clip, const std::vector<ModelVertex> &batch)
{
    unsigned common = 0x3f;
    for (const ModelVertex &vertex : batch)
    {
        unsigned sides = 0;
        for (int lane = 0; lane < 3; lane++)
        {
            const float placed = clip[0 + lane] * vertex.x + clip[4 + lane] * vertex.y + clip[8 + lane] * vertex.z +
                                 clip[12 + lane];
            sides |= placed > 1.0f ? 1u << (lane * 2) : 0u;
            sides |= placed < -1.0f ? 2u << (lane * 2) : 0u;
        }
        common &= sides;
        if (common == 0)
            return false;
    }
    return true;
}

// Whether a triangle shows its front to the eye, VU1 0x13c..0x147; `flip` for a strip's odd faces (0x171..) and a
// mirrored mesh, since the ROM measures in the mesh's space.
bool facesEye(const ModelVertex &a, const ModelVertex &b, const ModelVertex &c, const std::array<float, 3> &eye,
              bool flip)
{
    const float e1[3] = {a.x - b.x, a.y - b.y, a.z - b.z};
    const float e2[3] = {c.x - b.x, c.y - b.y, c.z - b.z};
    const float normal[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                             e1[0] * e2[1] - e1[1] * e2[0]};
    const float toEye = normal[0] * (c.x - eye[0]) + normal[1] * (c.y - eye[1]) + normal[2] * (c.z - eye[2]);
    return flip ? toEye < 0.0f : toEye > 0.0f;
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
    m_spotVertexShader =
        m_device.loadShader("model_spot.vert", SDL_GPU_SHADERSTAGE_VERTEX, {.samplers = 0, .uniformBuffers = 1});
    m_spotFragmentShader =
        m_device.loadShader("model_spot.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, {.samplers = 1, .uniformBuffers = 1});
}

// Callers hold m_lock.
std::shared_ptr<Image> ModelRenderer::imageOf(const void *texture)
{
    if (texture == nullptr)
        return nullptr;
    const auto found = m_images.find(texture);
    if (found != m_images.end())
        return found->second;
    std::shared_ptr<Image> image = m_renderer.textures().imageOf(*static_cast<const Tim2Texture *>(texture));
    m_images.emplace(texture, image);
    return image;
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
    if (pass.spot.active != 0)
        out.spotImage = imageOf(pass.spot.texture);
    if (pass.shadows.count != 0)
        out.shadowImage = imageOf(pass.shadows.texture);
    std::copy_n(pass.eye, 3, layer.eye.begin());
    layer.point = pass.point;
    layer.spot = pass.spot;
    layer.spot.active = out.spotImage != nullptr;
    layer.shadows = pass.shadows;
    if (out.shadowImage == nullptr)
        layer.shadows.count = 0;
    layer.open = true;
}

void ModelRenderer::setMaterial(const void *list, const YamiPanModelMaterial &material)
{
    std::scoped_lock lock(m_lock);
    Layer &layer = m_layers[list];

    endReflection(layer);
    layer.image = imageOf(material.texture);
    layer.reflection = imageOf(material.reflection);
    layer.reflectionScale = material.reflectionScale;
    layer.scrollU = static_cast<float>(material.scrollU) / kUvUnit;
    layer.scrollV = static_cast<float>(material.scrollV) / kUvUnit;
    layer.blended = material.blended != 0;
    layer.blend = BlendMode::Alpha;
    layer.fixedFog = -1;
    layer.unlit = material.unlit != 0;
    layer.pointFlat = material.pointFlat != 0;
    layer.noSpot = material.noSpot != 0;
    layer.noShadow = material.noShadow != 0;
}

void ModelRenderer::setEffect(const void *list, const YamiPanModelEffect &effect)
{
    std::scoped_lock lock(m_lock);
    Layer &layer = m_layers[list];

    endReflection(layer);
    layer.reflection = nullptr;
    layer.image = imageOf(effect.texture);
    layer.blended = true;
    layer.blend = blendModeOf(effect.alpha1);
    layer.fixedFog = effect.fog;
}

// The strip's two triangles, in the batch's vertex stream: an effect is a run of these.
void ModelRenderer::drawQuad(const void *list, const YamiPanModelQuad &quad)
{
    std::scoped_lock lock(m_lock);
    Layer &layer = m_layers[list];
    if (layer.passes.empty())
        return;

    const Image *const image = layer.image.get();
    const float scaleU = image != nullptr ? 1.0f / (kGsTexelUnit * static_cast<float>(image->width)) : 0.0f;
    const float scaleV = image != nullptr ? 1.0f / (kGsTexelUnit * static_cast<float>(image->height)) : 0.0f;
    const auto firstVertex = static_cast<std::uint32_t>(layer.vertices.size());
    for (const int corner : {0, 1, 2, 2, 1, 3})
    {
        ModelVertex &vertex = layer.vertices.emplace_back();
        vertex.x = quad.position[corner][0];
        vertex.y = quad.position[corner][1];
        vertex.z = quad.position[corner][2];
        vertex.u = quad.uv[corner][0] * scaleU;
        vertex.v = quad.uv[corner][1] * scaleV;
        vertex.color = quad.color;
    }

    constexpr std::uint32_t kQuadVertices = 6;
    if (layer.draws.size() > layer.passes.back().firstDraw)
    {
        ModelDraw &last = layer.draws.back();
        if (last.image == layer.image && last.blended && last.blend == layer.blend &&
            last.fixedFog == layer.fixedFog && last.firstVertex + last.vertexCount == firstVertex)
        {
            last.vertexCount += kQuadVertices;
            return;
        }
    }
    ModelDraw &draw = layer.draws.emplace_back();
    draw.image = layer.image;
    draw.blended = true;
    draw.firstVertex = firstVertex;
    draw.vertexCount = kQuadVertices;
    draw.firstSpotVertex = static_cast<std::uint32_t>(layer.spotVertices.size());
    draw.firstShadowVertex = static_cast<std::uint32_t>(layer.shadowVertices.size());
    draw.blend = layer.blend;
    draw.fixedFog = layer.fixedFog;
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
    const bool pointLit = layer.point.active != 0 && layer.lights.takesPoint != 0;
    const bool reflecting = layer.reflection != nullptr && !layer.unlit;
    layer.reflectionBatch.resize(reflecting ? static_cast<std::size_t>(batch.count) : 0);

    // The ROM measures a face in the mesh's space, so a mirroring matrix turns its front around in the world.
    const float determinant = matrix[0] * (matrix[5] * matrix[10] - matrix[6] * matrix[9]) -
                              matrix[1] * (matrix[4] * matrix[10] - matrix[6] * matrix[8]) +
                              matrix[2] * (matrix[4] * matrix[9] - matrix[5] * matrix[8]);
    const bool mirrored = determinant < 0.0f;

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
        // A batch can come with no UVs (node 4 has a triangle of 3 vertices, on the plain path, no
        // list): the ROM sends none and its VU keeps the last batch's. The host has none to keep.
        vertex.u = (batch.uvs != nullptr ? static_cast<float>(batch.uvs[index * 2]) / kUvUnit : 0.0f) + layer.scrollU;
        vertex.v = (batch.uvs != nullptr ? static_cast<float>(batch.uvs[index * 2 + 1]) / kUvUnit : 0.0f) + layer.scrollV;
        if (layer.unlit)
        {
            vertex.color = rawColor(batch.colors + index * 4);
            return vertex;
        }
        if (reflecting)
        {
            layer.reflectionBatch[static_cast<std::size_t>(index)] =
                reflectionVertex(vertex, worldNormal, layer.eye, layer.lights, layer.reflectionScale);
        }
        const std::uint8_t *const source = batch.colors + index * 4;
        float color[3] = {static_cast<float>(source[0]), static_cast<float>(source[1]), static_cast<float>(source[2])};
        if (pointLit)
        {
            const float position[3] = {vertex.x, vertex.y, vertex.z};
            addPointLight(layer.point, layer.pointFlat, position, worldNormal, color);
        }
        vertex.color = litColor(layer.lights, worldNormal, color);
        return vertex;
    };

    layer.batch.resize(static_cast<std::size_t>(batch.count));
    for (int index = 0; index < batch.count; index++)
        layer.batch[static_cast<std::size_t>(index)] = vertexAt(index);

    const bool spotted = layer.spot.active != 0 && layer.lights.takesSpot != 0 && !layer.unlit && !layer.noSpot;
    const bool shadowed =
        layer.shadows.count != 0 && layer.lights.takesShadow != 0 && !layer.unlit && !layer.blended && !layer.noShadow;
    const bool cull = layer.lights.cullBack != 0;
    const auto firstVertex = static_cast<std::uint32_t>(layer.vertices.size());
    const auto firstSpot = static_cast<std::uint32_t>(layer.spotVertices.size());
    const auto firstShadow = static_cast<std::uint32_t>(layer.shadowVertices.size());
    layer.triangles.clear();
    const auto emit = [&](int a, int b, int c, bool flip) {
        const ModelVertex &first = layer.batch[static_cast<std::size_t>(a)];
        const ModelVertex &second = layer.batch[static_cast<std::size_t>(b)];
        const ModelVertex &third = layer.batch[static_cast<std::size_t>(c)];
        if (cull && !facesEye(first, second, third, layer.eye, flip != mirrored))
            return;
        layer.vertices.push_back(first);
        layer.vertices.push_back(second);
        layer.vertices.push_back(third);
        if (reflecting)
        {
            layer.reflectionVertices.push_back(layer.reflectionBatch[static_cast<std::size_t>(a)]);
            layer.reflectionVertices.push_back(layer.reflectionBatch[static_cast<std::size_t>(b)]);
            layer.reflectionVertices.push_back(layer.reflectionBatch[static_cast<std::size_t>(c)]);
        }
        if (spotted)
        {
            layer.spotVertices.push_back(spotVertex(layer.spot, first));
            layer.spotVertices.push_back(spotVertex(layer.spot, second));
            layer.spotVertices.push_back(spotVertex(layer.spot, third));
        }
        if (shadowed)
            layer.triangles.insert(layer.triangles.end(), {a, b, c});
    };

    if (batch.strip)
    {
        for (int index = 2; index < batch.count; index++)
            emit(index - 2, index - 1, index, (index & 1) != 0);
    }
    else
    {
        for (int index = 0; index + 2 < batch.count; index += 3)
            emit(index, index + 1, index + 2, false);
    }

    // Every shadow whose box the batch may touch lays itself over the faces that were kept.
    for (int shadow = 0; shadowed && !layer.triangles.empty() && shadow < layer.shadows.count; shadow++)
    {
        if (outsideShadowBox(&layer.shadows.clip[shadow][0][0], layer.batch))
            continue;
        layer.shadowBatch.resize(layer.batch.size());
        for (std::size_t index = 0; index < layer.batch.size(); index++)
            layer.shadowBatch[index] = shadowVertex(&layer.shadows.projection[shadow][0][0], layer.batch[index]);
        for (const int index : layer.triangles)
            layer.shadowVertices.push_back(layer.shadowBatch[static_cast<std::size_t>(index)]);
    }

    // Runs of one state join into one draw: a mesh is many batches.
    const auto count = static_cast<std::uint32_t>(layer.vertices.size()) - firstVertex;
    const auto spotCount = static_cast<std::uint32_t>(layer.spotVertices.size()) - firstSpot;
    const auto shadowCount = static_cast<std::uint32_t>(layer.shadowVertices.size()) - firstShadow;
    if (layer.draws.size() > layer.passes.back().firstDraw)
    {
        ModelDraw &last = layer.draws.back();
        if (last.image == layer.image && last.blended == layer.blended &&
            last.firstVertex + last.vertexCount == firstVertex &&
            last.firstSpotVertex + last.spotVertexCount == firstSpot &&
            last.firstShadowVertex + last.shadowVertexCount == firstShadow)
        {
            last.vertexCount += count;
            last.spotVertexCount += spotCount;
            last.shadowVertexCount += shadowCount;
            return;
        }
    }
    layer.draws.push_back({layer.image, layer.blended, firstVertex, count, firstSpot, spotCount, firstShadow,
                           shadowCount});
}

void ModelRenderer::endReflection(Layer &layer)
{
    if (layer.reflectionVertices.empty())
        return;
    ModelDraw draw;
    draw.image = layer.reflection;
    draw.blended = true;
    draw.firstVertex = static_cast<std::uint32_t>(layer.vertices.size());
    draw.vertexCount = static_cast<std::uint32_t>(layer.reflectionVertices.size());
    draw.firstSpotVertex = static_cast<std::uint32_t>(layer.spotVertices.size());
    draw.firstShadowVertex = static_cast<std::uint32_t>(layer.shadowVertices.size());
    draw.blend = BlendMode::Additive;
    layer.vertices.insert(layer.vertices.end(), layer.reflectionVertices.begin(), layer.reflectionVertices.end());
    layer.reflectionVertices.clear();
    layer.draws.push_back(std::move(draw));
}

void ModelRenderer::endPass(const void *list)
{
    std::scoped_lock lock(m_lock);
    Layer &layer = m_layers[list];
    if (!layer.open)
        return;
    endReflection(layer);
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
    const auto spotOffset = static_cast<std::uint32_t>(frame.modelSpotVertices.size());
    const auto shadowOffset = static_cast<std::uint32_t>(frame.modelShadowVertices.size());
    const auto drawOffset = static_cast<std::uint32_t>(frame.modelDraws.size());
    frame.modelVertices.insert(frame.modelVertices.end(), layer.vertices.begin(), layer.vertices.end());
    frame.modelSpotVertices.insert(frame.modelSpotVertices.end(), layer.spotVertices.begin(), layer.spotVertices.end());
    frame.modelShadowVertices.insert(frame.modelShadowVertices.end(), layer.shadowVertices.begin(),
                                     layer.shadowVertices.end());
    for (ModelDraw &draw : layer.draws)
    {
        draw.firstVertex += vertexOffset;
        draw.firstSpotVertex += spotOffset;
        draw.firstShadowVertex += shadowOffset;
        frame.modelDraws.push_back(std::move(draw));
    }
    for (ModelPass &pass : layer.passes)
    {
        pass.firstDraw += drawOffset;
        pass.uiDrawsBefore = static_cast<std::uint32_t>(frame.draws.size());
        frame.modelPasses.push_back(pass);
    }
    layer.vertices.clear();
    layer.spotVertices.clear();
    layer.shadowVertices.clear();
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
    uploadStream(copy, m_vertices, frame.modelVertices.data(),
                 static_cast<unsigned>(frame.modelVertices.size() * sizeof(ModelVertex)));
    uploadStream(copy, m_spotVertices, frame.modelSpotVertices.data(),
                 static_cast<unsigned>(frame.modelSpotVertices.size() * sizeof(ModelSpotVertex)));
    uploadStream(copy, m_shadowVertices, frame.modelShadowVertices.data(),
                 static_cast<unsigned>(frame.modelShadowVertices.size() * sizeof(ModelShadowVertex)));
}

void ModelRenderer::uploadStream(SDL_GPUCopyPass *copy, VertexStream &stream, const void *data, unsigned bytes)
{
    if (bytes == 0)
        return;

    if (bytes > stream.capacity || !stream.buffer)
    {
        stream.capacity = std::max({bytes * 2, stream.capacity, kInitialVertexCapacity});
        stream.buffer = m_device.createVertexBuffer(stream.capacity);
        stream.upload = m_device.createUploadBuffer(stream.capacity);
    }

    void *const mapped = SDL_MapGPUTransferBuffer(m_device.get(), stream.upload.get(), true);
    if (mapped == nullptr)
        return;
    std::memcpy(mapped, data, bytes);
    SDL_UnmapGPUTransferBuffer(m_device.get(), stream.upload.get());

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = stream.upload.get();

    SDL_GPUBufferRegion destination{};
    destination.buffer = stream.buffer.get();
    destination.size = bytes;

    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
}

SDL_GPUGraphicsPipeline *ModelRenderer::pipelineFor(BlendMode blend)
{
    if (const auto found = m_pipelines.find(blend); found != m_pipelines.end())
        return found->second.get();
    const bool blended = blend != BlendMode::None;

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
    target.blend_state = blendStateOf(PipelineKey{blend, ColorMask::All});

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
    m_pipelines.emplace(blend, gpu::GraphicsPipeline(m_device.get(), pipeline));
    return pipeline;
}

// A later GS pass over a model's vertices: the same places and shaders, only the blend differs.
SDL_GPUGraphicsPipeline *ModelRenderer::createProjectedPipeline(const SDL_GPUColorTargetDescription &target)
{
    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.pitch = sizeof(ModelSpotVertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    const std::array<SDL_GPUVertexAttribute, 3> attributes{{
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelSpotVertex, x)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelSpotVertex, s)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(ModelSpotVertex, color)},
    }};

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = m_spotVertexShader.get();
    info.fragment_shader = m_spotFragmentShader.get();
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes.data();
    info.vertex_input_state.num_vertex_attributes = attributes.size();
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = false;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = kDepthFormat;

    SDL_GPUGraphicsPipeline *const pipeline = SDL_CreateGPUGraphicsPipeline(m_device.get(), &info);
    if (pipeline == nullptr)
        gpu::logGpuError("SDL_CreateGPUGraphicsPipeline(model projected)");
    return pipeline;
}

// The second GS pass over a lit vertex: ALPHA_1 0x58, the picture * what the last pass left in the frame's alpha
// added on top, whose own alpha is not written (FBMSK 0xff000000), against the first pass's depth. CLAMP_1 repeats.
SDL_GPUGraphicsPipeline *ModelRenderer::spotPipeline()
{
    if (m_spotPipeline)
        return m_spotPipeline.get();

    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    target.blend_state.enable_blend = true;
    target.blend_state.enable_color_write_mask = true;
    target.blend_state.color_write_mask =
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;

    m_spotPipeline = gpu::GraphicsPipeline(m_device.get(), createProjectedPipeline(target));
    return m_spotPipeline.get();
}

// A shadow's GS pass (0x2fdb60): ALPHA_1 0x44, black over the frame by its alpha; TEST_1 0x50000, no alpha test,
// the first pass's depth; CLAMP_1 5. The GS does not blend alpha: the frame's alpha takes the shadow's, and the spot
// that follows reads it (FBMSK is 0 in this pass).
SDL_GPUGraphicsPipeline *ModelRenderer::shadowPipeline()
{
    if (m_shadowPipeline)
        return m_shadowPipeline.get();

    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    target.blend_state.enable_blend = true;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;

    m_shadowPipeline = gpu::GraphicsPipeline(m_device.get(), createProjectedPipeline(target));
    return m_shadowPipeline.get();
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

    if (pass.drawCount != 0 && m_vertices.buffer)
    {
        SDL_GPUBufferBinding vertices{};
        vertices.buffer = m_vertices.buffer.get();
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertices, 1);

        ModelVertexUniforms vertexUniforms{};
        buildWorldToClip(pass, layout, vertexUniforms.worldToClip);
        vertexUniforms.fog[0] = pass.fogA;
        vertexUniforms.fog[1] = pass.fogB;
        vertexUniforms.fog[2] = pass.fogNearValue;
        vertexUniforms.fog[3] = pass.fogFarValue;
        SDL_PushGPUVertexUniformData(commands, 0, &vertexUniforms, sizeof(vertexUniforms));

        int pushedFog = -1;
        for (std::uint32_t index = 0; index < pass.drawCount; index++)
        {
            const ModelDraw &draw = frame.modelDraws[pass.firstDraw + index];
            SDL_GPUGraphicsPipeline *const pipeline = pipelineFor(draw.blended ? draw.blend : BlendMode::None);
            if (pipeline == nullptr || draw.vertexCount == 0)
                continue;
            SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

            if (draw.fixedFog != pushedFog)
            {
                // An effect has one fog for all it draws: the same value at either limit.
                const float fog = static_cast<float>(draw.fixedFog);
                vertexUniforms.fog[0] = draw.fixedFog >= 0 ? fog : pass.fogA;
                vertexUniforms.fog[1] = draw.fixedFog >= 0 ? 0.0f : pass.fogB;
                vertexUniforms.fog[2] = draw.fixedFog >= 0 ? fog : pass.fogNearValue;
                vertexUniforms.fog[3] = draw.fixedFog >= 0 ? fog : pass.fogFarValue;
                SDL_PushGPUVertexUniformData(commands, 0, &vertexUniforms, sizeof(vertexUniforms));
                pushedFog = draw.fixedFog;
            }

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

            // The passes VU1 kicks after the first one, each from its own stream; neither takes the fog's colour.
            const auto drawOver = [&](SDL_GPUGraphicsPipeline *over, const Image *picture, const VertexStream &stream,
                                      bool repeat, float alphaRef, std::uint32_t vertexCount, std::uint32_t first) {
                if (over == nullptr || picture == nullptr || picture->texture == nullptr)
                    return;
                SDL_BindGPUGraphicsPipeline(renderPass, over);
                SDL_GPUBufferBinding overVertices{};
                overVertices.buffer = stream.buffer.get();
                SDL_BindGPUVertexBuffers(renderPass, 0, &overVertices, 1);

                binding.texture = picture->texture->get();
                binding.sampler = m_renderer.samplerFor(true, repeat);
                SDL_BindGPUFragmentSamplers(renderPass, 0, &binding, 1);

                ModelFragmentUniforms overUniforms{};
                overUniforms.params[0] = alphaRef;
                SDL_PushGPUFragmentUniformData(commands, 0, &overUniforms, sizeof(overUniforms));

                SDL_DrawGPUPrimitives(renderPass, vertexCount, 1, first, 0);
                SDL_BindGPUVertexBuffers(renderPass, 0, &vertices, 1);
            };

            if (draw.shadowVertexCount != 0)
            {
                drawOver(shadowPipeline(), pass.shadowImage.get(), m_shadowVertices, false, 0.0f,
                         draw.shadowVertexCount, draw.firstShadowVertex);
            }
            if (draw.spotVertexCount != 0)
            {
                drawOver(spotPipeline(), pass.spotImage.get(), m_spotVertices, true, kSpotAlphaRef,
                         draw.spotVertexCount, draw.firstSpotVertex);
            }
        }
    }
    SDL_EndGPURenderPass(renderPass);
}

} // namespace yamipan::rendering
