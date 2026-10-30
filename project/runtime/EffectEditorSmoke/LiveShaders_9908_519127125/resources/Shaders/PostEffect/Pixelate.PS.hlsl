#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint textureWidth;
    uint textureHeight;
    gTexture.GetDimensions(textureWidth, textureHeight);

    float blockSize = max(pixelSize, 1.0f);
    float2 textureSize = float2(textureWidth, textureHeight);
    float2 pixelPosition = input.texcoord * textureSize;
    float2 blockCenter = (floor(pixelPosition / blockSize) + 0.5f) * blockSize;
    float2 pixelatedUV = saturate(blockCenter / textureSize);

    return gTexture.Sample(gSampler, pixelatedUV);
}
