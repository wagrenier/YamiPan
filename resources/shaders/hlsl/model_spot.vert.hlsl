/*
 * The spot's pass over a model: the place and the fog as model.vert has them, so that it meets the first pass's
 * depth, and the picture's coordinates still to be divided by q, the GS's projective ST.
 */

cbuffer ModelVertexUniforms : register(b0, space1)
{
    row_major float4x4 uWorldToClip;
    float4 uFog; // as in model.vert.hlsl
};

struct VSInput
{
    float3 aPosition : TEXCOORD0;
    float3 aProjected : TEXCOORD1;
    float4 aColor : TEXCOORD2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 vProjected : TEXCOORD0;
    float4 vColor : TEXCOORD1;
    float vFog : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.aPosition, 1.0), uWorldToClip);
    output.vProjected = input.aProjected;
    output.vColor = input.aColor;
    output.vFog = clamp(uFog.x + uFog.y / output.position.w, uFog.w, uFog.z) / 255.0;
    return output;
}
