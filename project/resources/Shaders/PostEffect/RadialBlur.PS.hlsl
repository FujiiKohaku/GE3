#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 direction =
        input.texcoord - radialBlurCenter;

    float3 outputColor = float3(0.0f, 0.0f, 0.0f);

    for (int sampleIndex = 0;
        sampleIndex < radialBlurSampleCount;
        sampleIndex++)
    {
        float percent =
            (float) sampleIndex /
            (float) radialBlurSampleCount;

        float2 texcoord =
            input.texcoord -
            direction *
            radialBlurWidth *
            percent;

        outputColor +=
            gTexture.Sample(
                gSampler,
                texcoord).rgb;
    }

    outputColor /=
        (float) radialBlurSampleCount;

    return float4(outputColor, 1.0f);
}