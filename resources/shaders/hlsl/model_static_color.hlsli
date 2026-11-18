/*
 * A batch as cached by ModelRenderer: placed, lit and fogged here, then model.frag. uDraw.x picks the pass: 0 the
 * colour (VU1's first kick), 1 the reflection (VU1 0x77f), drawn added over the material. model_static.vert and
 * model_skinned.vert are this, without and with SKINNED.
 */

#include "model_static.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float4 vColor : TEXCOORD1;
    float vFog : TEXCOORD2;
};

// VU1 0x77f, model_light.hlsli's reflectionUv and reflectionColor at the vertex.
void reflection(StaticInput input, float3 position, float3 normal, inout VSOutput output)
{
    float normalLength = sqrt(dot(normal, normal));
    if (normalLength > 0.0)
    {
        normal /= normalLength;
    }
    output.vUV = reflectionUv(uEye.xyz, position, normal);
    output.vColor = float4(reflectionColor(instanceLightsOf(input), normal), kVertexAlpha);
}

VSOutput main(StaticInput input)
{
    Placed placed = place(input);
    float3 position = placed.position;
    float3 normal = placed.normal;

    VSOutput output;
    output.position = mul(float4(position, 1.0), uWorldToClip);
    output.vFog = fogOf(output.position.w);
    if (uDraw.x == 0.0)
    {
        output.vUV = scrolledUv(input);
        output.vColor = litColor(input, position, normal);
    }
    else
    {
        reflection(input, position, normal, output);
    }
    return output;
}
