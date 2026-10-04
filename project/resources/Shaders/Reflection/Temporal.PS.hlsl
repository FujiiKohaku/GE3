#include "Common.hlsli"

struct TemporalOutput {
    float4 reflection : SV_TARGET0;
    float4 hitData : SV_TARGET1;
};

float3 ReconstructLinear(float2 uv, float viewDepth)
{
    float3 ray = Reconstruct(uv, 0.5f);
    return ray * (viewDepth / ray.z);
}

TemporalOutput main(PixelInput input)
{
    uint width, height;
    reflectionTexture.GetDimensions(width, height);
    int2 pixel = clamp(int2(input.position.xy), int2(0, 0), int2(width, height) - 1);
    float4 current = reflectionTexture.Load(int3(pixel, 0));
    float4 currentHit = hitTexture.Load(int3(pixel, 0));
    TemporalOutput output;
    output.reflection = current;
    output.hitData = currentHit;
    // Never retain a reflection after the current ray loses its surface.
    if (temporal.x < 0.5f || current.a <= 0.00001f || currentHit.w <= 0) { return output; }

    // Engine motion vectors exclude projection jitter; account for it here.
    float2 receiverMotion = motionTexture.Load(int3(GetPixel(input.texcoord), 0)) + temporal.zw;
    float2 historyUv = input.texcoord - receiverMotion;
    float2 halfTexel = 0.5f / float2(width, height);
    if (any(historyUv < halfTexel) || any(historyUv > 1 - halfTexel)) { return output; }
    int2 historyPixel = clamp(int2(historyUv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
    float4 previousHit = historyHitTexture.Load(int3(historyPixel, 0));
    float4 previous = historyTexture.SampleLevel(linearSampler, historyUv, 0);
    if (previous.a <= 0.00001f || previousHit.w <= 0) { return output; }

    float3 previousReceiver = mul(float4(ReconstructLinear(input.texcoord, currentHit.w), 1), currentToPreviousView).xyz;
    float3 previousSurface = mul(float4(ReconstructLinear(currentHit.xy, currentHit.z), 1), currentToPreviousView).xyz;
    if (previousReceiver.z <= 0 || previousSurface.z <= 0) { return output; }
    // Check both the reflective floor and the reflected object, avoiding disocclusion trails.
    if (abs(previousHit.w - previousReceiver.z) > max(0.05f, previousReceiver.z * 0.01f)) { return output; }
    if (abs(previousHit.z - previousSurface.z) > max(0.1f, previousSurface.z * 0.02f)) { return output; }

    float2 hitMotion = motionTexture.Load(int3(GetPixel(currentHit.xy), 0)) + temporal.zw;
    float2 previousHitUv = currentHit.xy - hitMotion;
    uint screenWidth, screenHeight;
    depthTexture.GetDimensions(screenWidth, screenHeight);
    float2 screenSize = float2(screenWidth, screenHeight);
    if (any(abs(previousHit.xy - previousHitUv) * screenSize > 3.0f)) { return output; }
    // Reject a camera cut or missing receiver motion rather than reusing the wrong location.
    float4 previousClip = mul(float4(previousReceiver, 1), previousProjection);
    float2 projectedHistoryUv = previousClip.xy / previousClip.w * float2(0.5f, -0.5f) + 0.5f;
    if (any(abs(projectedHistoryUv - historyUv) * screenSize > 3.0f)) { return output; }

    float3 currentColor = current.rgb / current.a;
    float3 minimumColor = currentColor;
    float3 maximumColor = currentColor;
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            int2 neighborPixel = clamp(pixel + int2(offsetX, offsetY), int2(0, 0), int2(width, height) - 1);
            float4 neighbor = reflectionTexture.Load(int3(neighborPixel, 0));
            if (neighbor.a > 0.00001f) {
                float3 color = neighbor.rgb / neighbor.a;
                minimumColor = min(minimumColor, color);
                maximumColor = max(maximumColor, color);
            }
        }
    }
    float3 previousColor = clamp(previous.rgb / previous.a, minimumColor, maximumColor);
    float motionPixels = max(length(receiverMotion * screenSize), length(hitMotion * screenSize));
    float historyWeight = clamp(temporal.y, 0, 0.95f) * (1 - saturate(motionPixels / 24.0f));
    float confidence = lerp(current.a, previous.a, historyWeight);
    output.reflection = float4(lerp(currentColor, previousColor, historyWeight) * confidence, confidence);
    return output;
}
