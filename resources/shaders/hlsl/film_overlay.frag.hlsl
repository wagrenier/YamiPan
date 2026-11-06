/*
 * The sight-jack's film: a colour over the frame, as thick as the film's picture is bright. The GS
 * composes the picture in a buffer past the frame, each corner a frame of the film mirrored about
 * the frame's centre, and draws that through a CLUT of alpha. Here the corners are drawn one at a
 * time, scissored, and the CLUT is the three lines it was filled by: clear below the band, solid
 * above it, the ramp across it, in 128ths as the game has it.
 *
 * The GS filters after the CLUT and this before it: the edges of what is solid are a little softer
 * or harder, nothing else.
 */

Texture2D uShown : register(t0, space2);
SamplerState uShownSampler : register(s0, space2);
Texture2D uLeaving : register(t1, space2);
SamplerState uLeavingSampler : register(s1, space2);

cbuffer FilmUniforms : register(b0, space3)
{
    float4 uColor; // rgb the colour, a how much of the leaving frame is over the shown one
    float4 uBand;  // x start of the ramp, y its length, z clear below it, w solid above it
};

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    static const float kGsOne = 128.0;

    /* 0 at the frame's edge, 1 at its centre: the picture's far corner meets the other three there. */
    float2 corner = 1.0 - abs(2.0 * input.vUV - 1.0);
    float shown = uShown.Sample(uShownSampler, corner).g;
    float leaving = uLeaving.Sample(uLeavingSampler, corner).g;
    float green = floor(lerp(shown, leaving, uColor.a) * 255.0 + 0.5);

    float alpha;
    if (green < uBand.z)
        alpha = 0.0;
    else if (green > uBand.w)
        alpha = 1.0;
    else
        alpha = clamp(floor((green - uBand.x) * kGsOne / uBand.y), 0.0, kGsOne) / kGsOne;
    return float4(uColor.rgb, alpha);
}
