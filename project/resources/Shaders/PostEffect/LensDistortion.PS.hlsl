#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 centered = input.texcoord * 2.0f - 1.0f;
    float radiusSquared = dot(centered, centered);
    float2 distorted = centered * (1.0f + lensDistortionStrength * radiusSquared);
    float2 sampleUV = distorted * 0.5f + 0.5f;

    if (any(sampleUV < 0.0f) || any(sampleUV > 1.0f))
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    return gTexture.Sample(gSampler, sampleUV);
}
