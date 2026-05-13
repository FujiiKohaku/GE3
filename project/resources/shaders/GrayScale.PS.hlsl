#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    float grayScale = dot(textureColor.rgb,float3(0.2125f, 0.7154f, 0.0721f));

    float4 outputColor;

    outputColor.rgb = float3(grayScale, grayScale, grayScale);
    outputColor.a = textureColor.a;

    return outputColor;
}