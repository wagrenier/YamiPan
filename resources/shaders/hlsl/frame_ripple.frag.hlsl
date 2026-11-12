/*
 * The frame through the ripple sheet (game/ripplesheet.cpp). The vertices are bent, the frame they
 * carry is not. On the GS this is three steps with the sheet's height in the frame's alpha: the bent
 * frame goes back over itself with the height as its alpha; the frame is pushed away from a colour,
 * (Cd - Cs) * As + Cd with alpha masked (ALPHA_1 0x41); then, where the height is 0x80 or over
 * (DATE, DATM 1), Cd * Ad twice (ALPHA_1 0x99), clamping each time.
 *
 * A height past 0xff has already wrapped in the vertex, as the GS's register takes its low byte.
 */

Texture2D uFrame : register(t0, space2);
SamplerState uFrameSampler : register(s0, space2);

cbuffer RippleUniforms : register(b0, space3)
{
    float4 uTint; // rgb the colour, a its As on the 0x80 scale
};

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float4 vColor : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    static const float kGsOne = 128.0;

    float3 colour = uFrame.Sample(uFrameSampler, input.vUV).rgb;
    colour = saturate((colour - uTint.rgb) * uTint.a + colour);

    float height = floor(input.vColor.a * 255.0 + 0.5);
    if (height >= kGsOne)
    {
        float scale = height / kGsOne;
        colour = saturate(saturate(colour * scale) * scale);
    }
    return float4(colour, 1.0);
}
