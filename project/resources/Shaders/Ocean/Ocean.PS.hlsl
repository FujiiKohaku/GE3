#include "Ocean.hlsli"

float HashNoise(float2 p)
{
    float2 cell = floor(p);
    float2 fraction = frac(p);
    fraction = fraction * fraction * (3.0f - 2.0f * fraction);
    float a = frac(sin(dot(cell, float2(127.1f, 311.7f))) * 43758.5453f);
    float b = frac(sin(dot(cell + float2(1.0f, 0.0f), float2(127.1f, 311.7f))) * 43758.5453f);
    float c = frac(sin(dot(cell + float2(0.0f, 1.0f), float2(127.1f, 311.7f))) * 43758.5453f);
    float d = frac(sin(dot(cell + 1.0f, float2(127.1f, 311.7f))) * 43758.5453f);
    return lerp(lerp(a, b, fraction.x), lerp(c, d, fraction.x), fraction.y);
}

float Fbm(float2 p)
{
    float value = 0.0f;
    float weight = 0.52f;
    float2x2 rotation = float2x2(0.80f, -0.60f, 0.60f, 0.80f);
    [unroll]
    for (int octave = 0; octave < 5; ++octave)
    {
        value += HashNoise(p) * weight;
        p = mul(p, rotation) * 2.07f + float2(13.7f, 7.9f);
        weight *= 0.49f;
    }
    return value;
}

float4 main(OceanVertexOutput input) : SV_TARGET
{
    float3 N = normalize(input.normal);
    float time = gOcean.cameraPositionAndTime.w;

    // Derive fine normals from irregular FBM instead of parallel sine bands.
    float2 finePosition = input.worldPosition.xz * 0.16f + float2(time * 0.09f, -time * 0.065f);
    float2 fineWarp = float2(Fbm(finePosition * 0.41f + 8.3f), Fbm(finePosition * 0.37f - 12.6f)) - 0.5f;
    finePosition += fineWarp * 1.7f;
    const float fineStep = 0.18f;
    float fineCenter = Fbm(finePosition);
    float2 fineGradient = float2(
        Fbm(finePosition + float2(fineStep, 0.0f)) - fineCenter,
        Fbm(finePosition + float2(0.0f, fineStep)) - fineCenter) * 0.72f;
    N = normalize(N + float3(-fineGradient.x, 0.0f, -fineGradient.y));

    float3 V = normalize(gOcean.cameraPositionAndTime.xyz - input.worldPosition);
    float height01 = saturate(input.waveData.x * 0.5f + 0.5f);

    // Mix broad surface variation into the height tint so trough/crest colors
    // form patches rather than contour-like stripes.
    float colorNoise = Fbm(input.worldPosition.xz * 0.012f + float2(time * 0.011f, -time * 0.008f));
    float heightTint = smoothstep(0.05f, 0.95f, height01);
    float colorBlend = saturate(heightTint * 0.48f + colorNoise * 0.52f);
    float3 water = lerp(gOcean.deepColor.rgb, gOcean.crestColor.rgb, colorBlend);
    float fresnel = pow(1.0f - saturate(dot(N, V)), 5.0f);
    water = lerp(water, float3(0.24f, 0.67f, 0.88f), fresnel * 0.76f);

    float3 lightDirection = normalize(float3(-0.35f, 0.82f, -0.44f));
    float3 halfVector = normalize(lightDirection + V);
    float sunGlint = pow(saturate(dot(N, halfVector)), 190.0f);
    water += float3(1.0f, 0.92f, 0.72f) * sunGlint * 1.4f;

    // Advected, domain-warped FBM breaks crest lines into unrelated patches.
    // Large noise controls groups of foam while smaller noise cuts holes and
    // ragged edges into those groups.
    float2 flow = float2(time * 0.018f, -time * 0.013f);
    float2 largePosition = input.worldPosition.xz * 0.018f + flow;
    float2 domainWarp = float2(
        Fbm(largePosition * 0.73f + 31.4f),
        Fbm(largePosition * 0.81f - 19.7f)) - 0.5f;
    float largePatch = Fbm(largePosition + domainWarp * 2.8f);

    float2 detailPosition = input.worldPosition.xz * 0.072f - flow * 1.7f + domainWarp * 1.3f;
    float detailPatch = Fbm(detailPosition);
    float holes = Fbm(detailPosition * 1.91f + float2(-time * 0.021f, time * 0.016f));

    float brokenCoverage = smoothstep(0.43f, 0.70f, largePatch * 0.64f + detailPatch * 0.36f);
    brokenCoverage *= 1.0f - smoothstep(0.70f, 0.88f, holes);

    float randomCrestThreshold = 0.63f + (largePatch - 0.5f) * 0.25f + (detailPatch - 0.5f) * 0.13f;
    float crest = smoothstep(randomCrestThreshold, randomCrestThreshold + 0.19f, height01);
    float steep = smoothstep(0.055f, 0.17f, input.waveData.y);
    float isolatedFlecks = smoothstep(0.72f, 0.91f, detailPatch) *
        smoothstep(0.42f, 0.74f, height01);
    float foamSignal = crest * (0.10f + brokenCoverage * 0.90f) +
        steep * brokenCoverage * 0.34f + isolatedFlecks * 0.28f;
    float foam = smoothstep(0.24f, 0.68f, foamSignal);

    return float4(lerp(water, float3(0.86f, 0.97f, 1.0f), foam * 0.92f), 1.0f);
}
