#include "ReflectionFilter.hlsli"
struct TemporalOutput {
    float4 signal : SV_Target0;
    float4 guide : SV_Target1;
    float4 hit : SV_Target2;
    float4 statistics : SV_Target3;
};
TemporalOutput main(PixelInput input) {
    int2 pixel = int2(input.position.xy);
    int2 fullPixel = FullReflectionPixel(pixel);
    TemporalOutput output;
    output.signal = signalTexture.Load(int3(pixel, 0));
    output.guide = guideTexture.Load(int3(pixel, 0));
    output.hit = currentHit.Load(int3(pixel, 0));
    float luminance = SignalLuminance(output.signal.rgb);
    output.statistics = float4(luminance, luminance * luminance, 1, ReceiverMaterialKey(fullPixel));
    if (output.guide.w <= 0) { output.statistics = 0; return output; }
    if (temporal.x < 0.5f) { return output; }
    float roughnessValue = saturate(length(output.guide.xyz * 2 - 1) - 1);
    bool isMirror = composition.z < 0.5f && roughnessValue < 0.02f;
    uint fullWidth, fullHeight; depthTexture.GetDimensions(fullWidth, fullHeight);
    float2 uv = (float2(fullPixel) + 0.5f) / float2(fullWidth, fullHeight);
    float depth = depthTexture.Load(int3(fullPixel, 0));
    float3 worldPosition = ReflectionWorld(uv, depth);
    float expectedPreviousDepth = mul(float4(worldPosition, 1), previousView).z;
    float3 expectedPreviousNormal = GuideNormal(output.guide);
    float4 receiverReprojection = reprojectionTexture.Load(int3(fullPixel, 0));
    float4 previousClip = mul(float4(worldPosition, 1), previousViewProjection);
    if (previousClip.w <= 0 && historyValidation.z < 0.5f) { return output; }
    float2 previousUv = previousClip.xy / max(previousClip.w, 0.000001f) * float2(0.5f, -0.5f) + 0.5f;
    if (options.w > 0.5f) {
        float2 motionUv = uv - motionTexture.Load(int3(fullPixel, 0)) - temporal.zw;
        // Metadata validates the actual previous geometry; legacy callers retain conservative motion rejection.
        if (historyValidation.z > 0.5f) {
            if (!all(isfinite(receiverReprojection)) || receiverReprojection.w <= 0 || abs(receiverReprojection.z) <= 0) { return output; }
            expectedPreviousDepth = abs(receiverReprojection.z);
            if (receiverReprojection.z > 0) { expectedPreviousNormal = DecodeReprojectionNormal(receiverReprojection.xy); }
        } else if (any(abs(motionUv - previousUv) * float2(fullWidth, fullHeight) > 3)) { return output; }
        previousUv = motionUv;
    }
    if (any(previousUv < 0) || any(previousUv >= 1)) { return output; }
    if (historyValidation.z > 0.5f) {
        int2 previousFullPixel = FullReflectionPixel(ReflectionPixel(previousUv));
        if (abs(previousReprojectionTexture.Load(int3(previousFullPixel, 0)).w) != receiverReprojection.w) { return output; }
    }
    int2 previousPixel = ReflectionPixel(previousUv);
    float4 oldGuide = historyGuide.Load(int3(previousPixel, 0));
    float previousDepth = expectedPreviousDepth;
    if (oldGuide.w <= 0 || abs(oldGuide.w - previousDepth) > max(0.03f, previousDepth * 0.005f)
        || dot(GuideNormal(oldGuide), expectedPreviousNormal) < 0.98f
        || abs(length(oldGuide.xyz * 2 - 1) - length(output.guide.xyz * 2 - 1)) > 0.02f) { return output; }
    float4 oldStatistics = previousStatistics.Load(int3(previousPixel, 0));
    if (!all(isfinite(oldStatistics)) || oldStatistics.z < 1 || oldStatistics.w != output.statistics.w) { return output; }
    float4 oldSignal = historyTexture.Load(int3(previousPixel, 0));
    if (!all(isfinite(oldSignal))) { return output; }
    if (isMirror) {
        float4 oldHit = previousHit.Load(int3(previousPixel, 0));
        if ((output.hit.w > 0) != (oldHit.w > 0)) { return output; }
        if (output.hit.w > 0) {
            float distanceTolerance = max(0.03f, output.hit.w * 0.01f);
            if (distance(output.hit.xyz, oldHit.xyz) > distanceTolerance || abs(output.hit.w - oldHit.w) > distanceTolerance) { return output; }
        }
    }
    // A changed scene invalidates only pixels whose fresh estimate disagrees.
    // Uncertain stochastic samples restart rather than retain stale lighting.
    if (historyValidation.x > 0.5f) {
        float3 tolerance = max(0.001f.xxx, max(output.signal.rgb, oldSignal.rgb) * 0.02f);
        if (any(abs(output.signal.rgb - oldSignal.rgb) > tolerance) || abs(output.signal.a - oldSignal.a) > 0.02f) { return output; }
    }
    float4 minimumValue = output.signal; float4 maximumValue = output.signal;
    float neighborhoodMean = 0; float neighborhoodSquare = 0; float neighborhoodCount = 0;
    uint width, height; signalTexture.GetDimensions(width, height);
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            int2 neighborPixel = clamp(pixel + int2(offsetX, offsetY), int2(0, 0), int2(width, height) - 1);
            float4 neighborGuide = guideTexture.Load(int3(neighborPixel, 0));
            if (neighborGuide.w <= 0 || dot(GuideNormal(neighborGuide), GuideNormal(output.guide)) < 0.98f
                || abs(neighborGuide.w - output.guide.w) > max(0.03f, output.guide.w * 0.005f)
                || ReceiverMaterialKey(FullReflectionPixel(neighborPixel)) != output.statistics.w) { continue; }
            float4 neighborSignal = signalTexture.Load(int3(neighborPixel, 0));
            minimumValue = min(minimumValue, neighborSignal); maximumValue = max(maximumValue, neighborSignal);
            float neighborLuminance = SignalLuminance(neighborSignal.rgb);
            neighborhoodMean += neighborLuminance; neighborhoodSquare += neighborLuminance * neighborLuminance; neighborhoodCount += 1;
        }
    }
    neighborhoodMean /= max(neighborhoodCount, 1);
    float neighborhoodVariance = max(neighborhoodSquare / max(neighborhoodCount, 1) - neighborhoodMean * neighborhoodMean, 0);
    float temporalVariance = max(oldStatistics.y - oldStatistics.x * oldStatistics.x, 0);
    float sigma = sqrt(max(neighborhoodVariance, temporalVariance));
    float count = min(oldStatistics.z + 1, 32);
    float historyWeight = min(clamp(temporal.y, 0, 0.95f), 1 - 1 / count);
    float oldLuminance = SignalLuminance(oldSignal.rgb);
    float clippedLuminance = clamp(oldLuminance, neighborhoodMean - max(2 * sigma, 0.01f), neighborhoodMean + max(2 * sigma, 0.01f));
    float oldLinearLuminance = exp2(oldLuminance) - 1;
    if (oldLinearLuminance > 0.00001f) { oldSignal.rgb *= max(exp2(clippedLuminance) - 1, 0) / oldLinearLuminance; }
    oldSignal = clamp(oldSignal, minimumValue, maximumValue);
    output.signal = lerp(output.signal, oldSignal, historyWeight);
    output.statistics.xy = lerp(float2(luminance, luminance * luminance), oldStatistics.xy, historyWeight);
    output.statistics.z = count;
    return output;
}
