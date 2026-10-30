#include "Bloom.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float CalculateGaussianWeight(float offset, float sigma)
{
    float sigmaSquared = sigma * sigma;
    float exponent = -(offset * offset) / (2.0f * sigmaSquared);
    return exp(exponent);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 baseColor = gTexture.Sample(gSampler, input.texcoord);

    if (bloomEnabled == 0) {
        return baseColor;
    }

    int radius = blurRadius;
    if (radius < 0) {
        radius = 0;
    }

    if (radius > 32) {
        radius = 32;
    }

    if (radius == 0) {
        return baseColor;
    }

    float sigma = blurSigma;
    if (sigma < 0.01f) {
        sigma = 0.01f;
    }

    uint width;
    uint height;
    gTexture.GetDimensions(width, height);

    float2 texelSize;
    texelSize.x = 1.0f / (float) width;
    texelSize.y = 1.0f / (float) height;

    float2 direction = float2(1.0f, 0.0f);
    if (blurDirection != 0) {
        direction = float2(0.0f, 1.0f);
    }

    float3 outputColor = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;

    for (int sampleOffset = -radius; sampleOffset <= radius; sampleOffset++) {
        float offset = (float) sampleOffset;
        float weight = CalculateGaussianWeight(offset, sigma);
        float2 texcoord = input.texcoord + direction * texelSize * offset;
        float3 sampleColor = gTexture.Sample(gSampler, texcoord).rgb;

        outputColor += sampleColor * weight;
        totalWeight += weight;
    }

    if (totalWeight > 0.0f) {
        outputColor /= totalWeight;
    }

    return float4(outputColor, 1.0f);
}
