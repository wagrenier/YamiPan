/*
 * The picture a lamp throws: the GS's MODULATE with its projective ST, the alpha test of the pass, and FOGCOL 0, so
 * that fog takes away what it adds. The blend (model_renderer.cpp) multiplies by what the first pass left in alpha.
 */

Texture2D uTexture : register(t0, space2);
SamplerState uTextureSampler : register(s0, space2);

cbuffer ModelFragmentUniforms : register(b0, space3)
{
    float4 uFogColor; // unused here: FOGCOL is 0
    float4 uParams;   // x = alpha test reference, 0..1 in the same 0x80 scale
};

struct PSInput
{
    float4 position : SV_Position;
    float3 vProjected : TEXCOORD0;
    float4 vColor : TEXCOORD1;
    float vFog : TEXCOORD2;
};

static const float kGsColorScale = 255.0 / 128.0;

float4 main(PSInput input) : SV_Target0
{
    float2 uv = input.vProjected.xy / input.vProjected.z;
    float4 color = saturate(uTexture.Sample(uTextureSampler, uv) * input.vColor * kGsColorScale);
    if (color.a < uParams.x)
    {
        discard;
    }
    color.rgb *= input.vFog;
    return color;
}
