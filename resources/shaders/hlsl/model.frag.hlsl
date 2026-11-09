/*
 * The GS's MODULATE, as in ui.frag.hlsl, then its fog.
 */

Texture2D uTexture : register(t0, space2);
SamplerState uTextureSampler : register(s0, space2);

cbuffer ModelFragmentUniforms : register(b0, space3)
{
    float4 uFogColor;
    float4 uParams; // x = alpha test reference, 0..1 in the same 0x80 scale; y = 1: ALPHA_1 0x49;
                    // z = every vertex's alpha (VU address 25's w), 1 = 0x80: below 1 only for a face's frame
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
    // MODULATE takes the texel's alpha times the vertex's: the vertices carry 0x80, so the rest is the scale.
    color.a *= uParams.z;
    if (color.a < uParams.x)
    {
        discard;
    }
    color.rgb = lerp(uFogColor.rgb, color.rgb, input.vFog);
    // Cd * As + Cd: the blend multiplies the frame by this colour, so here it is As, which fog leaves alone.
    color.rgb = lerp(color.rgb, color.aaa, uParams.y);
    return color;
}
