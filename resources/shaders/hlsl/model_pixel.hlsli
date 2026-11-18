/*
 * The pixel's side of per-pixel lighting (model_static_pixel.hlsli has the vertex's): model_light.hlsli's functions on
 * the place and the normal blended across the face. The blend shortens a normal wherever the corners' differ, and VU1
 * lights by the normal as it is, not normalised (an instance's matrix may scale it), so the pixel's normal takes the
 * length the corners' normals blend to.
 */

#include "model_light.hlsli"

// What the pixels take of the pass (model_renderer.cpp's ModelPixelUniforms): model_static.hlsli's uniforms of the
// same names.
cbuffer ModelPixelUniforms : register(b1, space3)
{
    float4 uEye;
    float4 uPointPosition; // w the weight
    float4 uPointColor;    // w the slope
    float4 uPointShade;
    float4 uSpotScale;     // x, y; z the limit ahead
    float4 uSpotFalloff;
};

static const float kGsColorScale = 255.0 / 128.0;

InstanceLights pixelLightsOf(PixelVaryings input)
{
    InstanceLights lights;
    lights.ambient = input.vAmbient;
    lights.colors[0] = input.vLightColor0;
    lights.colors[1] = input.vLightColor1;
    lights.colors[2] = input.vLightColor2;
    lights.directions[0] = input.vLightDirection0;
    lights.directions[1] = input.vLightDirection1;
    lights.directions[2] = input.vLightDirection2;
    return lights;
}

float3 pixelNormal(PixelVaryings input)
{
    float length = sqrt(dot(input.vNormal, input.vNormal));
    return length > 0.0 ? input.vNormal * (input.vUvFog.w / length) : float3(0.0, 0.0, 0.0);
}

// model_static.hlsli's litColor at the pixel: no cut to a byte, which only the vertex had to take.
float4 pixelLitColor(PixelVaryings input)
{
    InstanceLights lights = pixelLightsOf(input);
    if ((flagsOf(lights) & kFlagUnlit) != 0)
    {
        return float4(input.vColor / 255.0, kVertexAlpha);
    }
    PointLight pointSource;
    pointSource.position = uPointPosition;
    pointSource.color = uPointColor;
    pointSource.shade = uPointShade;
    float3 lighted = lightedColor(lights, pointSource, input.vColor, input.vPosition, pixelNormal(input));
    return float4(clamp(lighted, 0.0, 255.0) / 255.0, kVertexAlpha);
}
