/*
 * A cached batch lit per pixel (DisplaySettings::perPixelLighting): placed and fogged here as in
 * model_static_color.hlsli and model_static_projected.hlsli, but the lighting is left to the pixel, which gets the
 * vertex's own colour, its place and its normal, blended across the face, and the instance's lights as they are. Then
 * model_pixel.frag, or with PROJECTED model_pixel_projected.frag, where uDraw.x picks the pass as in
 * model_static_projected.hlsli (0 the spot, 1 the shadow uDraw.y), the point in the light's space blended across the
 * face too: it is affine in the world.
 * model_static_pixel.vert, model_skinned_pixel.vert and their _projected are this, without and with SKINNED.
 */

#include "model_static.hlsli"

PixelVaryings main(StaticInput input)
{
    Placed placed = place(input);

    PixelVaryings output;
    output.position = mul(float4(placed.position, 1.0), uWorldToClip);
    output.vUvFog = float4(scrolledUv(input), fogOf(output.position.w), sqrt(dot(placed.normal, placed.normal)));
    output.vColor = float3(input.aColor.rgb);
    output.vPosition = placed.position;
    output.vNormal = placed.normal;
    output.vAmbient = input.iAmbient;
    output.vLightColor0 = input.iLightColor0;
    output.vLightColor1 = input.iLightColor1;
    output.vLightColor2 = input.iLightColor2;
    output.vLightDirection0 = input.iLightDirection0;
    output.vLightDirection1 = input.iLightDirection1;
    output.vLightDirection2 = input.iLightDirection2;
#ifdef PROJECTED
    if (uDraw.x == 0.0)
    {
        output.vProjected = float4(mul(float4(placed.position, 1.0), uSpotWorldToLight).xyz, 1.0);
    }
    else
    {
        output.vProjected = mul(float4(placed.position, 1.0), uShadowProjections[(uint)uDraw.y]);
    }
#endif
    return output;
}
