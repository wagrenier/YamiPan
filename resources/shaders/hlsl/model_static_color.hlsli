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

// The texture is looked up by the eye's ray bounced off the normal, in the world's x, y (z is up), and the vertex takes
// the first light's colour, none on a face turned from that light. Only y and z of the light count: the ROM has loaded
// the level over the row with x (the integer reads as 0).
void reflection(StaticInput input, float3 position, float3 normal, inout VSOutput output)
{
    float normalLength = sqrt(dot(normal, normal));
    if (normalLength > 0.0)
    {
        normal /= normalLength;
    }
    float3 toEye = uEye.xyz - position;
    float along = dot(toEye, normal);
    float3 ray = toEye - 2.0 * along * normal;
    float rayLength = sqrt(dot(ray, ray));
    output.vUV = rayLength > 0.0 ? ray.xy / rayLength * 0.5 + 0.5 : float2(0.5, 0.5);

    float facing = input.iLightDirection0.y * normal.y + input.iLightDirection0.z * normal.z;
    float3 lit = floor(clamp(128.0 * input.iLightColor0.xyz * input.iLightColor2.w, 0.0, 255.0)) / 255.0; // loi 128
    output.vColor = float4(facing > 0.0 ? lit : float3(0.0, 0.0, 0.0), kVertexAlpha);
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
