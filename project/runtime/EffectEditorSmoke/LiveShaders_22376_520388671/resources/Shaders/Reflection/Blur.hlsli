#include "Common.hlsli"
float4 BlurReflection(PixelInput input, float2 axis)
{
    int2 centerPixel = GetPixel(input.texcoord);
    float4 encoded = normalTexture.Load(int3(centerPixel, 0));
    if (encoded.a < 2) { return 0; }
    float4 center = reflectionTexture.Load(int3(int2(input.position.xy), 0));
    // Object3d clamps material roughness to 0.08; treat that minimum as a sharp mirror.
    const float kMinimumMaterialRoughness = 0.08f;
    float blurRoughness = saturate((GetReflectionRoughness(encoded) - kMinimumMaterialRoughness) / (1 - kMinimumMaterialRoughness));
    float radiusPx = clamp(filter.x, 0, 32) * blurRoughness;
    if (radiusPx < 0.25f) { return center; }
    uint width, height;
    depthTexture.GetDimensions(width, height);
    float2 sampleStep = axis * radiusPx / (4 * float2(width, height));
    float3 centerNormal = normalize(encoded.xyz * 2 - 1);
    float centerDepth = Reconstruct(input.texcoord, depthTexture.Load(int3(centerPixel, 0))).z;
    float4 sum = 0;
    float weightSum = 0;
    for (int sampleIndex = -4; sampleIndex <= 4; ++sampleIndex) {
        float2 uv = input.texcoord + sampleStep * float(sampleIndex);
        if (any(uv < 0) || any(uv > 1)) { continue; }
        int2 samplePixel = GetPixel(uv);
        float4 sampleNormal = normalTexture.Load(int3(samplePixel, 0));
        if (sampleNormal.a < 2 || dot(centerNormal, normalize(sampleNormal.xyz * 2 - 1)) < 0.98f) { continue; }
        if (abs(GetReflectionRoughness(sampleNormal) - GetReflectionRoughness(encoded)) > 0.1f) { continue; }
        float sampleDepth = Reconstruct(uv, depthTexture.Load(int3(samplePixel, 0))).z;
        float depthTolerance = max(0.1f, centerDepth * 0.04f);
        if (abs(sampleDepth - centerDepth) > depthTolerance) { continue; }
        float weight = exp(-float(sampleIndex * sampleIndex) / 8.0f);
        weight *= exp(-abs(sampleDepth - centerDepth) / depthTolerance);
        // Filter premultiplied radiance/confidence together, preserving soft silhouettes.
        sum += reflectionTexture.SampleLevel(linearSampler, uv, 0) * weight;
        weightSum += weight;
    }
    if (weightSum <= 0.00001f) { return center; }
    return sum / weightSum;
}
