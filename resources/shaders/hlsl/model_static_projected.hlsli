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
        // model_light.hlsli's spotOf at the vertex; the colour pass's lit colour stays with it.
        float3 placed = mul(float4(position, 1.0), uSpotWorldToLight).xyz;
        SpotPoint spot = spotOf(placed, uSpotScale, uSpotFalloff);
        output.vProjected = spot.projected;
        float4 lit = litColor(input, position, vertex.normal);
        output.vColor = float4(lit.rgb, floor(clamp(spot.reach, 0.0, 255.0)) / 255.0);
    }
    else
    {
        // model_light.hlsli's shadowStrength at the vertex.
        float4 placed = mul(float4(position, 1.0), uShadowProjections[(uint)uDraw.y]);
        output.vProjected = float3(placed.x, placed.y, 1.0);
        output.vColor = float4(0.0, 0.0, 0.0, floor(shadowStrength(placed)) / 255.0);
    }
    return output;
}
