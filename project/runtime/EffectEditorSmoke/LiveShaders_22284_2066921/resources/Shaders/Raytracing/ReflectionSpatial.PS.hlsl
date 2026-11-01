#include "ReflectionFilter.hlsli"
float4 main(PixelInput input) : SV_Target0 {
    int2 pixel = int2(input.position.xy);
    float4 centerGuide = guideTexture.Load(int3(pixel, 0));
    float4 center = signalTexture.Load(int3(pixel, 0));
    if (centerGuide.w <= 0) { return center; }
    float roughnessValue = saturate(length(centerGuide.xyz * 2 - 1) - 1);
    if (roughnessValue < 0.02f) { return center; }
    float3 normal = GuideNormal(centerGuide);
    float4 statistics = currentStatistics.Load(int3(pixel, 0));
    float variance = 1;
    float confidence = 1;
    if (historyValidation.y > 0.5f && statistics.z >= 1) {
        variance = max(statistics.y - statistics.x * statistics.x, 0);
        confidence = rsqrt(statistics.z);
    }
    uint width, height; signalTexture.GetDimensions(width, height);
    int stepPixels = max(int(round(roughnessValue * roughnessValue * 6)), 1) * int(filterStep);
    float4 sum = 0; float weightSum = 0;
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            int2 neighbor = clamp(pixel + int2(offsetX, offsetY) * stepPixels, int2(0, 0), int2(width, height) - 1);
            float4 guide = guideTexture.Load(int3(neighbor, 0));
            if (composition.z > 1.5f && !HasMatchingLocalMaterial(FullReflectionPixel(pixel), FullReflectionPixel(neighbor))) { continue; }
            if (guide.w <= 0 || abs(guide.w - centerGuide.w) > max(0.03f, centerGuide.w * 0.005f) * stepPixels
                || dot(normal, GuideNormal(guide)) < 0.98f) { continue; }
            float kernel = 1;
            if (offsetX == 0) { kernel *= 2; }
            if (offsetY == 0) { kernel *= 2; }
            float depthWeight = exp(-abs(guide.w - centerGuide.w) / max(0.02f, centerGuide.w * 0.002f));
            float4 neighborSignal = signalTexture.Load(int3(neighbor, 0));
            float luminanceDifference = abs(SignalLuminance(center.rgb) - SignalLuminance(neighborSignal.rgb));
            float luminanceWeight = exp(-luminanceDifference / max(2 * sqrt(variance) + confidence, 0.05f));
            float weight = kernel * depthWeight * luminanceWeight;
            sum += neighborSignal * weight; weightSum += weight;
        }
    }
    if (weightSum <= 0) { return center; }
    float filterStrength = saturate(sqrt(variance) * 3 + confidence);
    return lerp(center, sum / weightSum, filterStrength);
}
