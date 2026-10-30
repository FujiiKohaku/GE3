#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    int sampleCount = clamp(motionBlurSampleCount, 1, 32);
    float2 direction = motionBlurDirection;
    float directionLength = length(direction);
    if (directionLength > 0.0001f)
    {
        direction /= directionLength;
    }

    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float denominator = max(float(sampleCount - 1), 1.0f);
    for (int index = 0; index < 32; ++index)
    {
        if (index >= sampleCount)
        {
            break;
        }

        float position = float(index) / denominator - 0.5f;
        float2 sampleUV = saturate(input.texcoord + direction * motionBlurStrength * position);
        color += gTexture.Sample(gSampler, sampleUV);
    }

    return color / float(sampleCount);
}
