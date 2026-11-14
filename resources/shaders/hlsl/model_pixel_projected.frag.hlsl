/*
 * model_spot.frag with what the vertex worked out worked out here (model_pixel.hlsli): uParams.w 0 the spot, its
 * reach and its picture's coordinates from this pixel's place in the light's space, over this pixel's lit colour; 1 a
 * shadow, its strength from this pixel's place in its projection, so its plane and its fade cut through a face where
 * they fall instead of where its corners say.
 */

#define PROJECTED 1
#include "model_pixel.hlsli"

Texture2D uTexture : register(t0, space2);
SamplerState uTextureSampler : register(s0, space2);

cbuffer ModelFragmentUniforms : register(b0, space3)
{
    float4 uFogColor; // unused here: FOGCOL is 0
    float4 uParams;   // x = alpha test reference, 0..1 in the same 0x80 scale; w = 1: a shadow's pass
};

float4 main(PixelVaryings input) : SV_Target0
{
    float2 uv;
    float4 vertexColor;
    if (uParams.w == 0.0)
    {
        SpotPoint spot = spotOf(input.vProjected.xyz, uSpotScale, uSpotFalloff);
        uv = spot.projected.xy / spot.projected.z;
        vertexColor = float4(pixelLitColor(input).rgb, clamp(spot.reach, 0.0, 255.0) / 255.0);
    }
    else
    {
        uv = input.vProjected.xy;
        vertexColor = float4(0.0, 0.0, 0.0, shadowStrength(input.vProjected) / 255.0);
    }

    float4 color = saturate(uTexture.Sample(uTextureSampler, uv) * vertexColor * kGsColorScale);
    if (color.a < uParams.x)
    {
        discard;
    }
    color.rgb *= input.vUvFog.z;
    return color;
}
