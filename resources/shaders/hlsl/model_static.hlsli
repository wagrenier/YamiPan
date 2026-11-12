/*
 * What VU1 does to a model's vertex, on the GPU: ModelRenderer keeps each batch of the model file here once
 * (ModelStaticVertex, ModelSkinnedVertex with SKINNED defined) and hands over, per run of batches, the instance that
 * places and lights it (ModelInstance) and, skinned, the palette.
 * This is model_renderer.cpp's litColor / addPointLight / reflectionVertex / spotVertex / shadowVertex, which still
 * do the same for batches too large to cache: change both together.
 */

// The prefix is model.vert's ModelVertexUniforms, so one push serves both paths.
cbuffer ModelVertexUniforms : register(b0, space1)
{
    row_major float4x4 uWorldToClip;
    float4 uFog; // as in model.vert.hlsl
    float4 uEye; // the camera in the world: the reflection's ray starts there
    float4 uPointPosition; // the scene's point light (kernel 0x545); w the weight
    float4 uPointColor;    // w the slope
    float4 uPointShade;
    row_major float4x4 uSpotWorldToLight; // the spot's pass (VU1 0x32d), sceVu0 layout
    float4 uSpotScale;    // x, y; z the limit ahead
    float4 uSpotFalloff;  // reach = 255 / (x + y ahead + z ahead^2)
    row_major float4x4 uShadowProjections[7]; // a shadow's pass (VU1 0x3e8), sceVu0 layout
};

cbuffer ModelDrawUniforms : register(b1, space1)
{
    float4 uDraw; // x: which pass of the vertex (each shader says); y: the shadow
};

struct StaticInput
{
    float3 aPosition : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    int2 aUv : TEXCOORD2;
    uint4 aColor : TEXCOORD3; // alpha, never lit: a skinned vertex's bone count
    float4 iPlaceX : TEXCOORD4;
    float4 iPlaceY : TEXCOORD5;
    float4 iPlaceZ : TEXCOORD6;
    float4 iAmbient : TEXCOORD7; // w: the flags below
    float4 iLightColor0 : TEXCOORD8; // w: scroll u
    float4 iLightColor1 : TEXCOORD9; // w: scroll v
    float4 iLightColor2 : TEXCOORD10; // w: the reflection's scale
    float4 iLightDirection0 : TEXCOORD11; // w: where the palette starts in uPalette, in matrices
    float4 iLightDirection1 : TEXCOORD12;
    float4 iLightDirection2 : TEXCOORD13;
#ifdef SKINNED
    uint4 aBones : TEXCOORD14; // the palette's matrices; 255: none
    float4 aWeights : TEXCOORD15;
#endif
};

static const uint kFlagUnlit = 1;
static const uint kFlagPoint = 2;
static const uint kFlagPointFlat = 4;
static const uint kFlagFirstBoneOnly = 8;

static const float kVertexColorUnit = 255.0; // LOI at 0x57e, 0x32b
static const float kVertexAlpha = 128.0 / 255.0; // VU address 25's w: every vertex's alpha, as a UBYTE4_NORM

uint flagsOf(StaticInput input)
{
    return (uint)input.iAmbient.w;
}

// A vertex in the world, with its normal as VU1 takes it.
struct Placed
{
    float3 position;
    float3 normal;
};

#ifdef SKINNED
StructuredBuffer<float4> uPalette : register(t0, space0); // the frame's bone matrices, four rows each, sceVu0 layout

// VU1 0x08a (four bones), 0x0bd (two), 0x0e1 (three), 0x113 (one): the sum of each bone's matrix times the weight,
// the weights as they stand. The normal goes through the same sum and is normalised, except with one bone, which
// only turns it. ModelFlag_Distant takes every vertex's first bone alone. The instance's matrix is unit here.
Placed place(StaticInput input)
{
    uint bones = (flagsOf(input) & kFlagFirstBoneOnly) != 0 ? 1 : input.aColor.a;
    uint base = (uint)input.iLightDirection0.w;
    Placed placed;
    placed.position = float3(0.0, 0.0, 0.0);
    placed.normal = float3(0.0, 0.0, 0.0);
    [unroll]
    for (uint bone = 0; bone < 4; bone++)
    {
        uint index = input.aBones[bone];
        if (bone >= bones || index == 255)
        {
            continue; // an unused slot: VU1 adds nothing
        }
        float weight = bones == 1 ? 1.0 : input.aWeights[bone];
        uint row = (base + index) * 4;
        float3 x = uPalette[row].xyz;
        float3 y = uPalette[row + 1].xyz;
        float3 z = uPalette[row + 2].xyz;
        float3 t = uPalette[row + 3].xyz;
        placed.position += weight * (x * input.aPosition.x + y * input.aPosition.y + z * input.aPosition.z + t);
        placed.normal += weight * (x * input.aNormal.x + y * input.aNormal.y + z * input.aNormal.z);
    }
    if (bones > 1)
    {
        float length = sqrt(dot(placed.normal, placed.normal));
        if (length > 0.0)
        {
            placed.normal /= length;
        }
    }
    return placed;
}
#else
// Through the instance's matrix; the normal turned by it and not normalised, as VU1 takes it.
Placed place(StaticInput input)
{
    float4 position = float4(input.aPosition, 1.0);
    Placed placed;
    placed.position = float3(dot(input.iPlaceX, position), dot(input.iPlaceY, position), dot(input.iPlaceZ, position));
    placed.normal = float3(dot(input.iPlaceX.xyz, input.aNormal), dot(input.iPlaceY.xyz, input.aNormal),
                           dot(input.iPlaceZ.xyz, input.aNormal));
    return placed;
}
#endif

float2 scrolledUv(StaticInput input)
{
    return float2(input.aUv) / 4096.0 + float2(input.iLightColor0.w, input.iLightColor1.w); // ITOF12, then VIF's row
}

float fogOf(float w)
{
    return clamp(uFog.x + uFog.y / w, uFog.w, uFog.z) / 255.0; // VU1 0x243, 0x247
}

// Kernel 0x545 (0x59a flat): what the point light adds to the vertex colour, ahead of the multiply.
float3 pointLight(float3 position, float3 normal, bool flat)
{
    float3 toLight = uPointPosition.xyz - position;
    float distance = sqrt(dot(toLight, toLight));
    float strength = max(0.0, uPointPosition.w + uPointColor.w * distance);
    if (flat)
    {
        return kVertexColorUnit * uPointColor.xyz * strength;
    }
    float facing = distance > 0.0 ? dot(normal, toLight) / distance : 0.0;
    float amount = max(0.0, facing) * strength;
    return kVertexColorUnit * (uPointShade.xyz * strength + uPointColor.xyz * amount);
}

// VU1 0x2a9, 0x2bb: the vertex colour times ambient + each light's colour * max(0, its direction . the normal),
// clamped to 255 and cut to a byte as the host's vertex was; alpha 0x80. Unlit (0x29e): the colour as it is.
float4 litColor(StaticInput input, float3 position, float3 normal)
{
    uint flags = flagsOf(input);
    float3 color = float3(input.aColor.rgb);
    if ((flags & kFlagUnlit) != 0)
    {
        return float4(color / 255.0, kVertexAlpha);
    }
    if ((flags & kFlagPoint) != 0)
    {
        color += pointLight(position, normal, (flags & kFlagPointFlat) != 0);
    }
    float3 sum = input.iAmbient.xyz;
    sum += input.iLightColor0.xyz * max(0.0, dot(normal, input.iLightDirection0.xyz));
    sum += input.iLightColor1.xyz * max(0.0, dot(normal, input.iLightDirection1.xyz));
    sum += input.iLightColor2.xyz * max(0.0, dot(normal, input.iLightDirection2.xyz));
    return float4(floor(clamp(color * sum, 0.0, 255.0)) / 255.0, kVertexAlpha);
}
