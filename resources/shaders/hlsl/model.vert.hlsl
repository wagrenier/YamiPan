/*
 * The game's models. Vertices come lit and in the world; the matrix is the game's own world -> screen with the
 * display's shape put in on the host side (ModelRenderer::recordPass). Fog is the GS's: a value per vertex, linear
 * in 1 / z like Q, which the fragment shader mixes in.
 */

cbuffer ModelVertexUniforms : register(b0, space1)
{
    row_major float4x4 uWorldToClip;
    float4 uFog; // x = A, y = B of DrawSysFog: the value (A + B / z) of 255; z = near value, w = far value, its limits
};

struct VSInput
{
    float3 aPosition : TEXCOORD0;
    float2 aUv : TEXCOORD1;
    float4 aColor : TEXCOORD2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float4 vColor : TEXCOORD1;
    float vFog : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.aPosition, 1.0), uWorldToClip);
    output.vUV = input.aUv;
    output.vColor = input.aColor;
    output.vFog = clamp(uFog.x + uFog.y / output.position.w, uFog.w, uFog.z) / 255.0; // VU1 0x243, 0x247
    return output;
}
