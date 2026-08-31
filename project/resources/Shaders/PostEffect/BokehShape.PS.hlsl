#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);

static const float kPi = 3.14159265359f;
static const float kTwoPi = 6.28318530718f;
static const int kSampleCount = 24;

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width;
    uint height;
    gTexture.GetDimensions(width, height);

    float depth = gDepthTexture.Sample(gSampler, input.texcoord);
    float blurFactor = saturate(abs(depth - focusDepth) / max(focusRange, 0.0001f));
    float2 texelSize = 1.0f / float2(width, height);
    int sideCount = clamp(bokehSides, 3, 12);
    float sectorAngle = kTwoPi / float(sideCount);
    float polygonNumerator = cos(kPi / float(sideCount));

    float4 color = gTexture.Sample(gSampler, input.texcoord);
    for (int index = 0; index < kSampleCount; ++index)
    {
        float angle = kTwoPi * float(index) / float(kSampleCount);
        float localAngle = fmod(angle + kPi / float(sideCount), sectorAngle) - kPi / float(sideCount);
        float polygonRadius = polygonNumerator / max(cos(localAngle), 0.001f);
        float ring = 0.45f + 0.55f * frac(float(index) * 0.61803398875f);
        float2 direction = float2(cos(angle), sin(angle));
        float2 offset = direction * polygonRadius * ring * bokehRadius * blurFactor * texelSize;
        color += gTexture.Sample(gSampler, saturate(input.texcoord + offset));
    }

    return color / float(kSampleCount + 1);
}
