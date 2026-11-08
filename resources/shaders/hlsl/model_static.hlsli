/*
 * What VU1 does to a rigid model's vertex, on the GPU: ModelRenderer keeps each batch of the model file here once
 * (ModelStaticVertex) and hands over, per run of batches, the instance that places and lights it (ModelInstance).
 * This is model_renderer.cpp's litColor / addPointLight / reflectionVertex / spotVertex / shadowVertex, which still
 * do the same for skinned batches on the game thread: change both together.
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
    uint4 aColor : TEXCOORD3;
    float4 iPlaceX : TEXCOORD4;
    float4 iPlaceY : TEXCOORD5;
    float4 iPlaceZ : TEXCOORD6;
    float4 iParams : TEXCOORD7; // scroll u, v; the reflection's scale; the flags below
    float4 iAmbient : TEXCOORD8;
    float4 iLightColor0 : TEXCOORD9;
    float4 iLightColor1 : TEXCOORD10;
    float4 iLightColor2 : TEXCOORD11;
    float4 iLightDirection0 : TEXCOORD12;
    float4 iLightDirection1 : TEXCOORD13;
    float4 iLightDirection2 : TEXCOORD14;
};

static const uint kFlagUnlit = 1;
static const uint kFlagPoint = 2;
static const uint kFlagPointFlat = 4;

static const float kVertexColorUnit = 255.0; // LOI at 0x57e, 0x32b
static const float kVertexAlpha = 128.0 / 255.0; // VU address 25's w: every vertex's alpha, as a UBYTE4_NORM

float3 placedPosition(StaticInput input)
{
    float4 position = float4(input.aPosition, 1.0);
    return float3(dot(input.iPlaceX, position), dot(input.iPlaceY, position), dot(input.iPlaceZ, position));
}

// Turned by the matrix and not normalised, as VU1 takes it.
float3 placedNormal(StaticInput input)
{
    return float3(dot(input.iPlaceX.xyz, input.aNormal), dot(input.iPlaceY.xyz, input.aNormal),
                  dot(input.iPlaceZ.xyz, input.aNormal));
}

float2 scrolledUv(StaticInput input)
{
    return float2(input.aUv) / 4096.0 + input.iParams.xy; // ITOF12, then VIF's row
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
    uint flags = (uint)input.iParams.w;
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
