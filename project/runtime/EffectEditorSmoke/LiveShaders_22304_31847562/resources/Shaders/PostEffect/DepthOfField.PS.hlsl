#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);

static const float kTwoPi = 6.28318530718f;
static const int kSampleCount = 12;

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width;
    uint height;
    gTexture.GetDimensions(width, height);

    float depth = gDepthTexture.Sample(gSampler, input.texcoord);
    float blurFactor = saturate(abs(depth - focusDepth) / max(focusRange, 0.0001f));
    float2 texelSize = 1.0f / float2(width, height);
    float2 blurScale = texelSize * depthOfFieldRadius * blurFactor;

    float4 color = gTexture.Sample(gSampler, input.texcoord);
    for (int index = 0; index < kSampleCount; ++index)
    {
        float angle = kTwoPi * float(index) / float(kSampleCount);
        float2 offset = float2(cos(angle), sin(angle)) * blurScale;
        color += gTexture.Sample(gSampler, saturate(input.texcoord + offset));
    }

    return color / float(kSampleCount + 1);
}
