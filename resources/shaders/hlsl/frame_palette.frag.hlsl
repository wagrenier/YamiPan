/*
 * The frame through a 256-colour ramp: its brightness picks the colour. On the GS the brightness is
 * worked out in buffers past the frame, each channel times its weight and shifted down on its own,
 * and becomes the index of an 8-bit texture over the ramp as a CLUT. These are its weights and its
 * rounding, so the same pixel picks the same entry (white is 251, not 255).
 *
 * The target is half the frame's size and the sampler bilinear, as the GS's first shrink: the pass
 * maps first and is stretched back after, so colours blend, not indices.
 */

Texture2D uFrame : register(t0, space2);
SamplerState uFrameSampler : register(s0, space2);
Texture2D uRamp : register(t1, space2);
SamplerState uRampSampler : register(s1, space2);

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    static const float3 kWeights = float3(0x26, 0x4b, 0x0e);
    static const float kGsOne = 128.0;
    static const float kRampSize = 256.0;

    float4 frame = uFrame.Sample(uFrameSampler, input.vUV);
    float3 bytes = floor(frame.rgb * 255.0 + 0.5);
    float3 parts = floor(bytes * kWeights / kGsOne);
    float index = parts.r + parts.g + parts.b;

    float3 color = uRamp.Sample(uRampSampler, float2((index + 0.5) / kRampSize, 0.5)).rgb;
    return float4(color, frame.a);
}
