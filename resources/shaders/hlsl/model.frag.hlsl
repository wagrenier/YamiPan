/*
 * The GS's MODULATE, as in ui.frag.hlsl, then its fog.
 */

Texture2D uTexture : register(t0, space2);
SamplerState uTextureSampler : register(s0, space2);

cbuffer ModelFragmentUniforms : register(b0, space3)
{
    float4 uFogColor;
    float4 uParams; // x = alpha test reference, 0..1 in the same 0x80 scale
};

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float4 vColor : TEXCOORD1;
    float vFog : TEXCOORD2;
};

static const float kGsColorScale = 255.0 / 128.0;

float4 main(PSInput input) : SV_Target0
{
    float4 color = saturate(uTexture.Sample(uTextureSampler, input.vUV) * input.vColor * kGsColorScale);
    if (color.a < uParams.x)
    {
        discard;
    }
    color.rgb = lerp(uFogColor.rgb, color.rgb, input.vFog);
    return color;
}
