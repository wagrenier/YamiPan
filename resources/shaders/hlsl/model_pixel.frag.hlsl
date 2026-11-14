/*
 * model.frag with the colour lit here (model_pixel.hlsli): the colour pass, or with uParams.w 1 the reflection's
 * (VU1 0x77f), its ray bounced off this pixel's normal.
 */

#include "model_pixel.hlsli"

Texture2D uTexture : register(t0, space2);
SamplerState uTextureSampler : register(s0, space2);

cbuffer ModelFragmentUniforms : register(b0, space3)
{
    float4 uFogColor;
    float4 uParams; // x, y, z as in model.frag.hlsl; w = 1: the reflection's pass
};

float4 main(PixelVaryings input) : SV_Target0
{
    float2 uv = input.vUvFog.xy;
    float4 vertexColor;
    if (uParams.w == 0.0)
    {
        vertexColor = pixelLitColor(input);
    }
    else
    {
        float3 normal = pixelNormal(input);
        float normalLength = sqrt(dot(normal, normal));
        if (normalLength > 0.0)
        {
            normal /= normalLength;
        }
        uv = reflectionUv(uEye.xyz, input.vPosition, normal);
        vertexColor = float4(reflectionColor(pixelLightsOf(input), normal), kVertexAlpha);
    }

    float4 color = saturate(uTexture.Sample(uTextureSampler, uv) * vertexColor * kGsColorScale);
    color.a *= uParams.z;
    if (color.a < uParams.x)
    {
        discard;
    }
    color.rgb = lerp(uFogColor.rgb, color.rgb, input.vUvFog.z);
    color.rgb = lerp(color.rgb, color.aaa, uParams.y);
    return color;
}
