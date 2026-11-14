/*
 * What VU1 does to light a point of a model, as functions of where the point is and which way it faces:
 * model_static.hlsli runs them on each vertex, as the ROM does; model_pixel.hlsli on each pixel, the host's option
 * (DisplaySettings::perPixelLighting), from the same values blended across the face.
 * model_renderer.cpp's litColor / addPointLight / reflectionVertex / spotVertex / shadowVertex do the same on the CPU,
 * for batches too large to cache: change them together.
 */

#ifndef MODEL_LIGHT_HLSLI
#define MODEL_LIGHT_HLSLI

// ModelInstanceFlag (frame.h), in ModelInstance::ambient's w.
static const uint kFlagUnlit = 1;
static const uint kFlagPoint = 2;
static const uint kFlagPointFlat = 4;
static const uint kFlagFirstBoneOnly = 8;

static const float kVertexColorUnit = 255.0; // LOI at 0x57e, 0x32b
static const float kVertexAlpha = 128.0 / 255.0; // VU address 25's w: every vertex's alpha, as a UBYTE4_NORM

// The pass's point light, kernel 0x545: where it is (w the weight), its colour (w the slope) and its shade.
struct PointLight
{
    float4 position;
    float4 color;
    float4 shade;
};

// What an instance lights by (ModelInstance): ambient (w the flags), and three lights' colours and directions. The w
// lanes of the colours are the scroll's u, v and the reflection's scale.
struct InstanceLights
{
    float4 ambient;
    float4 colors[3];
    float4 directions[3];
};

uint flagsOf(InstanceLights lights)
{
    return (uint)lights.ambient.w;
}

// Kernel 0x545 (0x59a flat): what the point light adds to the colour, ahead of the multiply.
float3 pointLight(PointLight light, float3 position, float3 normal, bool flat)
{
    float3 toLight = light.position.xyz - position;
    float distance = sqrt(dot(toLight, toLight));
    float strength = max(0.0, light.position.w + light.color.w * distance);
    if (flat)
    {
        return kVertexColorUnit * light.color.xyz * strength;
    }
    float facing = distance > 0.0 ? dot(normal, toLight) / distance : 0.0;
    float amount = max(0.0, facing) * strength;
    return kVertexColorUnit * (light.shade.xyz * strength + light.color.xyz * amount);
}

// VU1 0x2a9, 0x2bb: the colour (0..255, the point light added when the instance takes it) times ambient + each
// light's colour * max(0, its direction . the normal), before the clamp to 255. The normal as VU1 has it, not
// normalised.
float3 lightedColor(InstanceLights lights, PointLight pointSource, float3 color, float3 position, float3 normal)
{
    uint flags = flagsOf(lights);
    if ((flags & kFlagPoint) != 0)
    {
        color += pointLight(pointSource, position, normal, (flags & kFlagPointFlat) != 0);
    }
    float3 sum = lights.ambient.xyz;
    [unroll]
    for (int light = 0; light < 3; light++)
    {
        sum += lights.colors[light].xyz * max(0.0, dot(normal, lights.directions[light].xyz));
    }
    return color * sum;
}

// VU1 0x77f: the reflection's texture is looked up by the eye's ray bounced off the normal, in the world's x, y (z is
// up). The normal is normalised first.
float2 reflectionUv(float3 eye, float3 position, float3 normal)
{
    float3 toEye = eye - position;
    float along = dot(toEye, normal);
    float3 ray = toEye - 2.0 * along * normal;
    float rayLength = sqrt(dot(ray, ray));
    return rayLength > 0.0 ? ray.xy / rayLength * 0.5 + 0.5 : float2(0.5, 0.5);
}

// The reflection's colour: the first light's, none on a face turned from that light. Only y and z of the light count:
// the ROM has loaded the level over the row with x (the integer reads as 0).
float3 reflectionColor(InstanceLights lights, float3 normal)
{
    float facing = lights.directions[0].y * normal.y + lights.directions[0].z * normal.z;
    float3 color = floor(clamp(128.0 * lights.colors[0].xyz * lights.colors[2].w, 0.0, 255.0)) / 255.0; // loi 128
    return facing > 0.0 ? color : float3(0.0, 0.0, 0.0);
}

// The spot, VU1 0x32d..0x347, at a point in the light's space: the picture's coordinates still to be divided by q (the
// GS's projective ST), and the light's reach there, 0..255. Behind the light a point has no reach and takes a
// harmless q.
struct SpotPoint
{
    float3 projected;
    float reach;
};

SpotPoint spotOf(float3 placed, float4 scale, float4 falloff)
{
    float ahead = placed.z;
    bool behind = ahead < 0.0;
    float q = behind ? 1.0 : ahead;
    SpotPoint spot;
    spot.reach = 0.0;
    if (!behind && ahead < scale.z)
    {
        float divisor = falloff.x + falloff.y * ahead + falloff.z * ahead * ahead;
        spot.reach = min(kVertexColorUnit / divisor, kVertexColorUnit);
    }
    spot.projected = float3(0.5 * q + placed.x * scale.x, 0.5 * q + placed.y * scale.y, q);
    return spot;
}

// A shadow, VU1 0x3e8..0x3fb, at a point in its projection: past the shadow's own plane (z not negative) nothing,
// otherwise w, from 0x80 there to nothing at the far end; 0..255.
float shadowStrength(float4 placed)
{
    return placed.z < 0.0 ? clamp(placed.w, 0.0, 255.0) : 0.0;
}

// What a per-pixel pass hands from its vertices to its pixels: model_static_pixel.hlsli writes it, model_pixel.hlsli
// reads it. One struct on both sides, so that every backend links them alike. PROJECTED: the spot's or a shadow's pass.
struct PixelVaryings
{
    float4 position : SV_Position;
    float4 vUvFog : TEXCOORD0;  // xy the UV, scrolled; z the fog; w the length of the normal VU1 lights by
    float3 vColor : TEXCOORD1;  // the vertex's own colour, 0..255, unlit
    float3 vPosition : TEXCOORD2; // in the world
    float3 vNormal : TEXCOORD3;   // in the world, as placed
    nointerpolation float4 vAmbient : TEXCOORD4; // the instance's InstanceLights, the same at every corner
    nointerpolation float4 vLightColor0 : TEXCOORD5;
    nointerpolation float4 vLightColor1 : TEXCOORD6;
    nointerpolation float4 vLightColor2 : TEXCOORD7;
    nointerpolation float4 vLightDirection0 : TEXCOORD8;
    nointerpolation float4 vLightDirection1 : TEXCOORD9;
    nointerpolation float4 vLightDirection2 : TEXCOORD10;
#ifdef PROJECTED
    float4 vProjected : TEXCOORD11; // the spot's light space (xyz), or the shadow's projection (xyzw): both affine
#endif
};

#endif
