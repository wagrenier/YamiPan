/*
 * The passes VU1 kicks after the colour over a cached batch, with a picture projected on it, then model_spot.frag.
 * uDraw.x picks the pass: 0 the spot (VU1 0x32d..0x347), 1 the shadow uDraw.y (VU1 0x3e8..0x3fb).
 * model_static_projected.vert and model_skinned_projected.vert are this, without and with SKINNED.
 */

#include "model_static.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float3 vProjected : TEXCOORD0;
    float4 vColor : TEXCOORD1;
    float vFog : TEXCOORD2;
};

VSOutput main(StaticInput input)
{
    Placed vertex = place(input);
    float3 position = vertex.position;

    VSOutput output;
    output.position = mul(float4(position, 1.0), uWorldToClip);
    output.vFog = fogOf(output.position.w);
    if (uDraw.x == 0.0)
    {
        // Behind the light a vertex has no reach and takes a harmless q; the colour pass's lit colour stays with it.
        float3 placed = mul(float4(position, 1.0), uSpotWorldToLight).xyz;
        float ahead = placed.z;
        bool behind = ahead < 0.0;
        float q = behind ? 1.0 : ahead;
        float reach = 0.0;
        if (!behind && ahead < uSpotScale.z)
        {
            float falloff = uSpotFalloff.x + uSpotFalloff.y * ahead + uSpotFalloff.z * ahead * ahead;
            reach = min(kVertexColorUnit / falloff, kVertexColorUnit);
        }
        output.vProjected = float3(0.5 * q + placed.x * uSpotScale.x, 0.5 * q + placed.y * uSpotScale.y, q);
        float4 lit = litColor(input, position, vertex.normal);
        output.vColor = float4(lit.rgb, floor(clamp(reach, 0.0, 255.0)) / 255.0);
    }
    else
    {
        // Past the shadow's own plane (z not negative) no alpha, otherwise w, from 0x80 there to nothing at the far end.
        float4 placed = mul(float4(position, 1.0), uShadowProjections[(uint)uDraw.y]);
        output.vProjected = float3(placed.x, placed.y, 1.0);
        float strength = placed.z < 0.0 ? floor(clamp(placed.w, 0.0, 255.0)) / 255.0 : 0.0;
        output.vColor = float4(0.0, 0.0, 0.0, strength);
    }
    return output;
}
