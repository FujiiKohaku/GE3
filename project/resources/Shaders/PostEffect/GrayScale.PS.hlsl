#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    float value =
        textureColor.r * 0.2125f +
        textureColor.g * 0.7154f +
        textureColor.b * 0.0721f;

    float3 grayColor = float3(value, value, value);

    textureColor.rgb = lerp(
        textureColor.rgb,
        grayColor,
        grayScaleStrength);

    return textureColor;
}