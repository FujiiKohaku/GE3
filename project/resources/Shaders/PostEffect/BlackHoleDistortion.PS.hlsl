#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float2 centerToPixel = uv - blackHoleCenter;
    float aspectRatio = 16.0f / 9.0f;
    float2 metricOffset =
        float2(centerToPixel.x * aspectRatio, centerToPixel.y);
    float distanceFromCenter = length(metricOffset);
    float distortionRadius = max(blackHoleRadius, 0.001f);

    if (distanceFromCenter >= distortionRadius)
    {
        return gTexture.Sample(gSampler, uv);
    }

    float normalizedRadius = distanceFromCenter / distortionRadius;
    float eventHorizonRadius = 0.22f;
    float safeDistance = max(distanceFromCenter, 0.0001f);
    float2 radialDirection = metricOffset / safeDistance;
    float2 tangentDirection =
        float2(-radialDirection.y, radialDirection.x);

    float lensMask =
        1.0f - smoothstep(eventHorizonRadius, 1.0f, normalizedRadius);
    float lensStrength =
        lensMask * lensMask * blackHoleStrength;
    float animatedTwist =
        sin(normalizedRadius * 26.0f - time * 2.4f) * 0.12f;

    float2 sampleMetricOffset =
        radialDirection * distortionRadius * lensStrength * 0.30f +
        tangentDirection * distortionRadius * lensStrength * animatedTwist;
    float2 tidalTremor =
        float2(
            sin(time * 31.0f),
            cos(time * 27.0f)) *
        distortionRadius *
        0.0035f *
        lensMask *
        blackHoleStrength;
    sampleMetricOffset += tidalTremor;
    float2 sampleOffset =
        float2(sampleMetricOffset.x / aspectRatio, sampleMetricOffset.y);

    float chromaticOffset =
        distortionRadius * lensStrength * 0.012f;
    float2 chromaticUvOffset =
        float2(
            radialDirection.x / aspectRatio,
            radialDirection.y) *
        chromaticOffset;

    float2 distortedUv = saturate(uv + sampleOffset);
    float red =
        gTexture.Sample(
            gSampler,
            saturate(distortedUv + chromaticUvOffset)).r;
    float green = gTexture.Sample(gSampler, distortedUv).g;
    float blue =
        gTexture.Sample(
            gSampler,
            saturate(distortedUv - chromaticUvOffset)).b;
    float3 color = float3(red, green, blue);

    // A secondary image folded across the photon sphere makes background
    // silhouettes appear twice, like strong gravitational lensing.
    float doubleImageMask =
        (1.0f - smoothstep(0.0f, 0.18f, abs(normalizedRadius - 0.38f))) *
        smoothstep(eventHorizonRadius, 0.34f, normalizedRadius);
    float foldedRadius =
        distortionRadius *
        (eventHorizonRadius * 1.55f +
         abs(normalizedRadius - 0.38f) * 0.34f);
    float2 foldedMetricOffset =
        -radialDirection * foldedRadius +
        tangentDirection *
        distortionRadius *
        sin(time * 1.7f + normalizedRadius * 18.0f) *
        0.025f;
    float2 foldedUv =
        blackHoleCenter +
        float2(
            foldedMetricOffset.x / aspectRatio,
            foldedMetricOffset.y);
    float3 foldedColor =
        gTexture.Sample(gSampler, saturate(foldedUv)).rgb;
    color =
        lerp(
            color,
            foldedColor * float3(1.12f, 0.78f, 1.28f),
            doubleImageMask * 0.52f * blackHoleStrength);

    float eventHorizon =
        1.0f - smoothstep(
            eventHorizonRadius * 0.72f,
            eventHorizonRadius * 1.04f,
            normalizedRadius);
    color *= 1.0f - eventHorizon;

    float environmentDarkening =
        (1.0f - smoothstep(0.48f, 1.0f, normalizedRadius)) *
        smoothstep(eventHorizonRadius * 0.92f, 0.52f, normalizedRadius);
    color *=
        1.0f -
        environmentDarkening *
        0.42f *
        blackHoleStrength;

    float photonRingDistance =
        abs(normalizedRadius - eventHorizonRadius * 1.18f);
    float photonRing =
        1.0f - smoothstep(0.0f, 0.035f, photonRingDistance);
    float3 photonColor = float3(0.42f, 0.12f, 0.82f);
    color +=
        photonColor *
        photonRing *
        blackHoleStrength *
        1.8f;

    float outerLensGlow =
        (1.0f - smoothstep(0.42f, 1.0f, normalizedRadius)) *
        smoothstep(eventHorizonRadius, 0.42f, normalizedRadius);
    color +=
        float3(0.04f, 0.01f, 0.10f) *
        outerLensGlow *
        blackHoleStrength;

    float tidalRipple =
        sin(normalizedRadius * 54.0f - time * 4.8f) *
        (1.0f - smoothstep(0.35f, 0.95f, normalizedRadius));
    color +=
        float3(0.08f, 0.01f, 0.16f) *
        max(tidalRipple, 0.0f) *
        0.18f *
        blackHoleStrength;

    return float4(color, 1.0f);
}
