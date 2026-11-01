#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 centered = input.texcoord * 2.0f - 1.0f;
    float radius = length(centered);
    if (radius > 1.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float strength = max(fisheyeStrength, 0.01f);
    float mappedRadius = atan(radius * strength) / atan(strength);
    float2 mapped = float2(0.0f, 0.0f);
    if (radius > 0.0001f)
    {
        mapped = centered * (mappedRadius / radius);
    }

    float2 sampleUV = mapped * 0.5f + 0.5f;
    return gTexture.Sample(gSampler, sampleUV);
}
