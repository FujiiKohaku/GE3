#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float Hash(float2 value)
{
    return frac(sin(dot(value, float2(127.1f, 311.7f))) * 43758.5453f);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, input.texcoord);
    uint widthPixels;
    uint heightPixels;
    gTexture.GetDimensions(widthPixels, heightPixels);
    float aspectRatio = float(widthPixels) / max(float(heightPixels), 1.0f);
    float2 position = input.texcoord - radialBlurCenter;
    position.x *= aspectRatio;
    float radius = length(position);
    float centerMask = smoothstep(0.15f, 0.34f, radius);
    if (centerMask <= 0.0f || boostSparkIntensity <= 0.0f) {
        return color;
    }

    static const float kPi = 3.14159265f;
    static const float kSectorCount = 96.0f;
    float angle = atan2(position.y, position.x);
    float sectorPosition = (angle + kPi) / (2.0f * kPi) * kSectorCount;
    float sectorIndex = floor(sectorPosition);
    float3 sparkColor = float3(0.0f, 0.0f, 0.0f);
    // Only three nearby angular sectors are evaluated per pixel.
    [unroll]
    for (int neighborIndex = -1; neighborIndex <= 1; ++neighborIndex) {
        float unwrappedSector = sectorIndex + float(neighborIndex);
        float seedSector = unwrappedSector - floor(unwrappedSector / kSectorCount) * kSectorCount;
        float phase = boostSparkElapsedSeconds * 3.0f + Hash(float2(seedSector, 1.0f));
        float cycleIndex = floor(phase);
        float age = frac(phase);
        float seed = Hash(float2(seedSector, cycleIndex));
        float sparkAngle = (unwrappedSector + 0.15f + seed * 0.7f) / kSectorCount * 2.0f * kPi - kPi;
        float2 direction = float2(cos(sparkAngle), sin(sparkAngle));
        float along = dot(position, direction);
        float across = abs(position.x * direction.y - position.y * direction.x);
        float headRadius = 0.20f + age * (0.65f + seed * 0.35f);
        float trailLength = 0.007f + seed * 0.025f;
        float halfWidth = (0.55f + seed * 0.6f) / max(float(heightPixels), 1.0f);
        float pixelWidth = 1.0f / max(float(heightPixels), 1.0f);
        float stroke = 1.0f - smoothstep(halfWidth, halfWidth + pixelWidth, across);
        stroke *= smoothstep(headRadius - trailLength, headRadius, along);
        stroke *= 1.0f - smoothstep(headRadius, headRadius + pixelWidth * 2.0f, along);
        float lifetime = smoothstep(0.0f, 0.08f, age) * (1.0f - smoothstep(0.7f, 1.0f, age));
        float flicker = 0.35f + 0.65f * Hash(float2(seedSector + cycleIndex * 17.0f,
            floor(boostSparkElapsedSeconds * 42.0f)));
        float isVisible = step(0.58f, seed);
        float3 tint = lerp(float3(1.8f, 0.45f, 0.08f), float3(2.2f, 1.7f, 0.9f), seed);
        sparkColor += tint * stroke * lifetime * flicker * isVisible;
    }
    color.rgb += sparkColor * centerMask * boostSparkIntensity;
    return color;
}
