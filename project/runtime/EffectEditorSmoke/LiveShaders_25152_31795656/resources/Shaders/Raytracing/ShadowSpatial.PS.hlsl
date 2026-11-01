#include "ShadowDenoiseCommon.hlsli"
float main(float4 position : SV_Position) : SV_Target0 {
    int2 pixel = int2(position.xy);
    float4 centerGeometry = historyGeometry.Load(int3(pixel, 0));
    if (centerGeometry.w <= 0) { return currentSignal.Load(int3(pixel, 0)); }
    uint width, height;
    historyGeometry.GetDimensions(width, height);
    uint2 size = uint2(width, height);
    float3 normal = normalize(centerGeometry.xyz * 2 - 1);
    float depth = centerGeometry.w;
    float centerSignal = currentSignal.Load(int3(pixel, 0));
    float4 statistics = historySignal.Load(int3(pixel, 0));
    float variance = max(statistics.w - statistics.z * statistics.z, 0);
    float confidence = rsqrt(max(statistics.y, 1));
    float sum = 0;
    float weightSum = 0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            int2 neighbor = pixel + int2(x, y) * int(filterStep);
            if (any(neighbor < 0) || any(neighbor >= int2(size))) { continue; }
            float4 neighborGeometry = historyGeometry.Load(int3(neighbor, 0));
            if (neighborGeometry.w <= 0) { continue; }
            float3 neighborNormal = normalize(neighborGeometry.xyz * 2 - 1);
            float normalAgreement = dot(normal, neighborNormal);
            if (normalAgreement < 0.9f) { continue; }
            float neighborDepth = neighborGeometry.w;
            float depthDifference = abs(depth - neighborDepth);
            float tolerance = max(0.02f, depth * 0.005f) * float(filterStep);
            if (depthDifference > tolerance) { continue; }
            float kernel = float(2 - abs(x)) * float(2 - abs(y));
            float weight = kernel * exp(-depthDifference / max(tolerance * 0.5f, 0.00001f)) * pow(saturate(normalAgreement), 32);
            float neighborSignal = currentSignal.Load(int3(neighbor, 0));
            weight *= exp(-abs(centerSignal - neighborSignal) / max(2 * sqrt(variance) + confidence, 0.05f));
            sum += neighborSignal * weight;
            weightSum += weight;
        }
    }
    if (weightSum <= 0) { return currentSignal.Load(int3(pixel, 0)); }
    float filterStrength = saturate(sqrt(variance) * 3 + confidence);
    return saturate(lerp(centerSignal, sum / weightSum, filterStrength));
}
