#include "ShadowDenoiseCommon.hlsli"
struct TemporalOutput {
    float4 signal : SV_Target0;
    float4 geometry : SV_Target1;
};
TemporalOutput main(float4 position : SV_Position) {
    int2 pixel = int2(position.xy);
    uint width, height;
    sceneDepth.GetDimensions(width, height);
    uint2 size = uint2(width, height);
    float current = currentSignal.Load(int3(pixel, 0));
    TemporalOutput output;
    output.signal = float4(1, 0, 1, 1);
    output.geometry = 0;
    if (!IsEligible(pixel)) { return output; }
    float3 normal = sceneNormal.Load(int3(pixel, 0)).xyz * 2 - 1;
    if (dot(normal, normal) < 0.000001f) { return output; }
    normal = normalize(normal);
    float3 world = WorldPosition(pixel, size);
    float linearDepth = mul(float4(world, 1), currentView).z;
    output.signal = float4(current, 1, current, current * current);
    // Half-float history geometry must not overflow; preserve the raw signal
    // outside its supported depth range and omit history/filter guidance.
    if (!isfinite(linearDepth) || linearDepth <= 0 || linearDepth > 65000) { return output; }
    output.geometry = float4(normal * 0.5f + 0.5f, linearDepth);
    if (hasHistory == 0) { return output; }
    float expectedDepth = mul(float4(world, 1), previousView).z;
    float3 expectedNormal = normal;
    float4 receiverReprojection = reprojectionTexture.Load(int3(pixel, 0));
    float4 previousClip = mul(float4(world, 1), previousViewProjection);
    if (previousClip.w <= 0 && (hasMotionVectors & 2) == 0) { return output; }
    float2 projectedUv = previousClip.xy / max(previousClip.w, 0.000001f) * float2(0.5f, -0.5f) + 0.5f;
    float2 historyUv = projectedUv;
    if (hasMotionVectors != 0) {
        float2 uv = (float2(pixel) + 0.5f) / float2(size);
        historyUv = uv - motionVectors.Load(int3(pixel, 0)) - jitterDeltaUv;
        // Prefer actual previous depth/normal/identity; legacy inputs remain conservative.
        if ((hasMotionVectors & 2) != 0) {
            if (!all(isfinite(receiverReprojection)) || receiverReprojection.w <= 0 || abs(receiverReprojection.z) <= 0) { return output; }
            expectedDepth = abs(receiverReprojection.z); expectedNormal = DecodeReprojectionNormal(receiverReprojection.xy);
        } else if (any(abs(historyUv - projectedUv) * float2(size) > 2.0f)) { return output; }
    }
    float2 halfTexel = 0.5f / float2(size);
    if (any(historyUv < halfTexel) || any(historyUv > 1 - halfTexel)) { return output; }
    int2 historyPixel = int2(historyUv * float2(size));
    if ((hasMotionVectors & 2) != 0 && abs(previousReprojectionTexture.Load(int3(historyPixel, 0)).w) != receiverReprojection.w) { return output; }
    float4 previous = historySignal.Load(int3(historyPixel, 0));
    float4 geometry = historyGeometry.Load(int3(historyPixel, 0));
    if (previous.y < 1 || geometry.w <= 0) { return output; }
    if (abs(geometry.w - expectedDepth) > max(0.03f, expectedDepth * 0.002f)) { return output; }
    float3 previousNormal = normalize(geometry.xyz * 2 - 1);
    if (dot(expectedNormal, previousNormal) < 0.95f) { return output; }
    if (hasSceneChanges != 0 && abs(current - previous.x) > 0.02f) { return output; }
    float minimumValue = current;
    float maximumValue = current;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            int2 neighbor = clamp(pixel + int2(x, y), int2(0, 0), int2(size) - 1);
            if (IsEligible(neighbor)) {
                float value = currentSignal.Load(int3(neighbor, 0));
                minimumValue = min(minimumValue, value); maximumValue = max(maximumValue, value);
            }
        }
    }
    float count = min(previous.y + 1, float(maxHistoryFrames));
    float historyWeight = 1 - 1 / count;
    float historyValue = clamp(previous.x, minimumValue - 0.1f, maximumValue + 0.1f);
    output.signal = float4(saturate(lerp(current, historyValue, historyWeight)), count,
        lerp(current, previous.z, historyWeight), lerp(current * current, previous.w, historyWeight));
    return output;
}
