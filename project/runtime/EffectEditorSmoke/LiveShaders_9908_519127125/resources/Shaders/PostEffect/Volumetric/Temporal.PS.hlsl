#include "Common.hlsli"
Texture2D<float4> gCurrent : register(t2);
Texture2D<float> gCurrentTransmittance : register(t3);
Texture2D<float4> gHistory : register(t5);
Texture2D<float> gHistoryTransmittance : register(t6);
struct TemporalOutput {
    float4 scatteringAndDepth : SV_Target0;
    float transmittance : SV_Target1;
};
TemporalOutput main(VertexShaderOutput input) {
    uint width, height; gCurrent.GetDimensions(width, height);
    int2 pixel = int2(input.position.xy);
    float4 current = gCurrent.Load(int3(pixel, 0));
    float transmission = gCurrentTransmittance.Load(int3(pixel, 0));
    TemporalOutput output; output.scatteringAndDepth = current; output.transmittance = transmission;
    if (temporalControls.x < 0.5f) { return output; }
    uint fullWidth, fullHeight; gDepth.GetDimensions(fullWidth, fullHeight);
    float depth = 1;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) { depth = min(depth, gDepth.Load(int3(min(pixel * 2 + int2(x, y), int2(fullWidth - 1, fullHeight - 1)), 0))); }
    }
    float3 previousPosition = ReconstructRelative(input.texcoord, depth) + previousCameraDelta.xyz;
    float4 clip = mul(float4(previousPosition, 1), previousRelativeViewProjection);
    if (clip.w <= 0 || !all(isfinite(clip))) { return output; }
    float2 uv = clip.xy / clip.w * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv < 0) || any(uv >= 1)) { return output; }
    float predictedDistance = min(length(previousPosition), 60000.0f);
    float2 location = uv * float2(width, height) - 0.5f;
    int2 origin = int2(floor(location)); float2 fraction = frac(location);
    float3 history = 0; float historyTransmission = 0; float totalWeight = 0;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            int2 samplePixel = clamp(origin + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            float4 previous = gHistory.Load(int3(samplePixel, 0));
            if (abs(previous.a - predictedDistance) > max(0.1f, predictedDistance * 0.01f)) { continue; }
            float2 weight = 1 - fraction; if (x == 1) { weight.x = fraction.x; } if (y == 1) { weight.y = fraction.y; }
            float combinedWeight = weight.x * weight.y;
            history += previous.rgb * combinedWeight;
            historyTransmission += gHistoryTransmittance.Load(int3(samplePixel, 0)) * combinedWeight;
            totalWeight += combinedWeight;
        }
    }
    if (totalWeight < 0.0001f) { return output; }
    float3 minimum = current.rgb; float3 maximum = current.rgb;
    float minimumTransmission = transmission; float maximumTransmission = transmission;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            int2 neighbor = clamp(pixel + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            float4 sample = gCurrent.Load(int3(neighbor, 0));
            if (abs(sample.a - current.a) > max(0.1f, current.a * 0.01f)) { continue; }
            float sampleTransmission = gCurrentTransmittance.Load(int3(neighbor, 0));
            minimum = min(minimum, sample.rgb); maximum = max(maximum, sample.rgb);
            minimumTransmission = min(minimumTransmission, sampleTransmission); maximumTransmission = max(maximumTransmission, sampleTransmission);
        }
    }
    history = clamp(history / totalWeight, minimum, maximum);
    historyTransmission = clamp(historyTransmission / totalWeight, minimumTransmission, maximumTransmission);
    output.scatteringAndDepth.rgb = lerp(current.rgb, history, temporalControls.y);
    output.transmittance = saturate(lerp(transmission, historyTransmission, temporalControls.y));
    return output;
}
