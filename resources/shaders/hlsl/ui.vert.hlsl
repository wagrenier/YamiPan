/*
 * The game's 2D draws. Positions come in frame units, the 512x512 frame the GS drew into, which always
 * fills the scene target: how that sits on a window of any shape is DisplayGeometry's business, not this
 * shader's. SDL_GPU's clip space has +Y up, the frame's y runs down.
 */

cbuffer UiVertexUniforms : register(b0, space1)
{
    float4 uFrameToClip; // xy = scale, zw = offset
    float4 uTexelToUv;   // xy = 1 / the texture's size
};

struct VSInput
{
    float2 aPosition : TEXCOORD0;
    float2 aTexel : TEXCOORD1;
    float4 aColor : TEXCOORD2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float4 vColor : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.aPosition * uFrameToClip.xy + uFrameToClip.zw, 0.0, 1.0);
    output.vUV = input.aTexel * uTexelToUv.xy;
    output.vColor = input.aColor;
    return output;
}
