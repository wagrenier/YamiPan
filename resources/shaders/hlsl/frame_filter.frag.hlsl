/*
 * One sprite of the game's frame filter (game/framefilter.cpp): the source's top-left part, shown over the
 * whole viewport the pass placed where the GS sprite goes, with the step's alpha. The pipeline blends as
 * ALPHA_1 0x44, (Cs - Cd) * As + Cd, so alpha 1.0 (the GS's 0x80) replaces; the target's alpha takes As,
 * as the GS writes it.
 */

Texture2D uSource : register(t0, space2);
SamplerState uSourceSampler : register(s0, space2);

cbuffer FilterUniforms : register(b0, space3)
{
    float4 uArea; /* xy: the shown part of the source over its whole size; z: the step's alpha */
};

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    float3 color = uSource.Sample(uSourceSampler, input.vUV * uArea.xy).rgb;
    return float4(color, uArea.z);
}
