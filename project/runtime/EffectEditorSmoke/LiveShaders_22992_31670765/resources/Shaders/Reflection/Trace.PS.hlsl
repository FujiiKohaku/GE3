#include "Common.hlsli"

float3 InterpolateRayPosition(float3 startOverW, float3 endOverW, float startInverseW, float endInverseW, float progress)
{
    return lerp(startOverW, endOverW, progress) / lerp(startInverseW, endInverseW, progress);
}

bool FindHierarchicalHit(float2 startUv, float2 uvDelta, float screenFraction, float2 dimensions,
    float3 startOverW, float3 endOverW, float startInverseW, float endInverseW,
    out float2 hitUv, out float3 hitPosition)
{
    hitUv = 0; hitPosition = 0;
    float2 startPixel = startUv * dimensions;
    float2 pixelDelta = uvDelta * dimensions;
    float progress = 0;
    int mipLevel = 5;
    const uint kMaxTraversalSteps = 192;
    // The tiny probe selects the cell on the forward side of an exact boundary.
    float progressProbe = 0.001f / max(max(abs(pixelDelta.x), abs(pixelDelta.y)), 1);
    for (uint stepIndex = 0; stepIndex < kMaxTraversalSteps && progress < screenFraction; ++stepIndex) {
        float cellSize = float(1 << mipLevel);
        float2 pixel = startPixel + pixelDelta * (progress + progressProbe);
        int2 cell = int2(floor(pixel / cellSize));
        float2 boundary = float2(cell) * cellSize;
        if (pixelDelta.x > 0) { boundary.x += cellSize; }
        if (pixelDelta.y > 0) { boundary.y += cellSize; }
        float nextProgress = screenFraction;
        if (abs(pixelDelta.x) > 0.00001f) { nextProgress = min(nextProgress, (boundary.x - startPixel.x) / pixelDelta.x); }
        if (abs(pixelDelta.y) > 0.00001f) { nextProgress = min(nextProgress, (boundary.y - startPixel.y) / pixelDelta.y); }
        nextProgress = max(nextProgress, progress + progressProbe);
        nextProgress = min(nextProgress, screenFraction);
        float3 rayStart = InterpolateRayPosition(startOverW, endOverW, startInverseW, endInverseW, progress);
        float3 rayEnd = InterpolateRayPosition(startOverW, endOverW, startInverseW, endInverseW, nextProgress);
        float2 bounds = depthPyramid.Load(int3(cell, mipLevel));
        bool hasOverlap = bounds.x <= bounds.y && max(rayStart.z, rayEnd.z) >= bounds.x &&
            min(rayStart.z, rayEnd.z) <= bounds.y + settings.y;
        if (hasOverlap && mipLevel > 0) {
            --mipLevel;
            continue;
        }
        if (hasOverlap) {
            float denominator = (endOverW.z - startOverW.z) - bounds.x * (endInverseW - startInverseW);
            float hitProgress = (progress + nextProgress) * 0.5f;
            if (abs(denominator) > 0.000001f) {
                hitProgress = clamp((bounds.x * startInverseW - startOverW.z) / denominator, progress, nextProgress);
            }
            // Keep the color lookup inside the depth-tested leaf cell.
            float2 hitPixel = clamp(startPixel + pixelDelta * hitProgress, float2(cell) + 0.001f, float2(cell) + 0.999f);
            hitUv = hitPixel / dimensions;
            hitPosition = InterpolateRayPosition(startOverW, endOverW, startInverseW, endInverseW, hitProgress);
            return true;
        }
        progress = nextProgress;
        mipLevel = min(mipLevel + 1, 5);
    }
    return false;
}

float4 ShadeReflection(float2 uv, float hitDistance, float3 normal, float3 surfacePosition, out float4 hitData)
{
    float hitDepth = depthTexture.Load(int3(GetPixel(uv), 0));
    hitData = float4(uv, Reconstruct(uv, hitDepth).z, surfacePosition.z);
    if (settings.w > 3.5f && settings.w < 4.5f) { return float4(uv, saturate(hitDistance / settings.x), 1); }
    if (settings.w > 4.5f) { return float4(0, 1, 0, 1); }
    float edgeFade = saturate(min(min(uv.x, 1 - uv.x), min(uv.y, 1 - uv.y)) * 12);
    float facing = saturate(dot(normal, -normalize(surfacePosition)));
    float fresnel = 0.25f + 0.75f * pow(1 - facing, 5);
    float distanceFade = 1 - 0.2f * saturate(hitDistance / settings.x);
    float confidence = edgeFade * fresnel * distanceFade;
    return float4(sceneTexture.SampleLevel(linearSampler, uv, 0).rgb * confidence, confidence);
}

float4 TraceReflection(PixelInput input, out float4 hitData)
{
    hitData = 0;
    int2 pixel = GetPixel(input.texcoord);
    float depth = depthTexture.Load(int3(pixel, 0));
    float4 encoded = normalTexture.Load(int3(pixel, 0));
    if (settings.w > 1.5f && settings.w < 2.5f) {
        if (depth >= 0.999999f) { return float4(0, 0, 0, 1); }
        float linearDepth = saturate(Reconstruct(input.texcoord, depth).z / settings.x);
        return float4(linearDepth.xxx, 1);
    }
    // Positive alpha above two marks the dedicated reflective floor shader.
    if (depth >= 0.999999f || encoded.a < 2.0f || abs(encoded.a - 2.0f - depth) > 0.004f) { return 0; }
    float3 normal = normalize(mul(float4(encoded.xyz * 2 - 1, 0), view).xyz);
    float3 surfacePosition = Reconstruct(input.texcoord, depth);
    if (dot(normal, -surfacePosition) < 0) { normal = -normal; }
    float3 direction = reflect(normalize(surfacePosition), normal);
    if (settings.w > 2.5f && settings.w < 3.5f) { return float4(direction * 0.5f + 0.5f, 1); }
    float3 origin = surfacePosition + normal * 0.08f;

    // Clip in view space before projection, including rays facing the camera.
    float rayDistance = settings.x;
    float nearDepth = -projection[3][2] / projection[2][2];
    float farDepth = projection[3][2] / (1 - projection[2][2]);
    if (direction.z < -0.00001f) { rayDistance = min(rayDistance, (nearDepth * 1.01f - origin.z) / direction.z); }
    if (direction.z > 0.00001f) { rayDistance = min(rayDistance, (farDepth * 0.99f - origin.z) / direction.z); }
    if (rayDistance <= 0.2f) { return 0; }
    float3 endPosition = origin + direction * rayDistance;
    float4 startClip = mul(float4(origin, 1), projection);
    float4 endClip = mul(float4(endPosition, 1), projection);
    if (startClip.w <= 0 || endClip.w <= 0) { return 0; }
    float startInverseW = rcp(startClip.w);
    float endInverseW = rcp(endClip.w);
    float3 startOverW = origin * startInverseW;
    float3 endOverW = endPosition * endInverseW;
    float2 startUv = startClip.xy * startInverseW * float2(0.5f, -0.5f) + 0.5f;
    float2 endUv = endClip.xy * endInverseW * float2(0.5f, -0.5f) + 0.5f;
    float2 uvDelta = endUv - startUv;
    float screenFraction = 1;
    const float kScreenMargin = 0.0001f;
    if (uvDelta.x > 0) { screenFraction = min(screenFraction, (1 - kScreenMargin - startUv.x) / uvDelta.x); }
    if (uvDelta.x < 0) { screenFraction = min(screenFraction, (kScreenMargin - startUv.x) / uvDelta.x); }
    if (uvDelta.y > 0) { screenFraction = min(screenFraction, (1 - kScreenMargin - startUv.y) / uvDelta.y); }
    if (uvDelta.y < 0) { screenFraction = min(screenFraction, (kScreenMargin - startUv.y) / uvDelta.y); }
    screenFraction = saturate(screenFraction);

    uint width, height;
    depthTexture.GetDimensions(width, height);
    float2 pixelDelta = uvDelta * screenFraction * float2(width, height);
    float pixelLength = max(abs(pixelDelta.x), abs(pixelDelta.y));
    if (pixelLength < 1) { return 0; }
    if (traversal.x > 0.5f) {
        float2 hitUv;
        float3 hitPosition;
        bool hasHit = FindHierarchicalHit(startUv, uvDelta, screenFraction, float2(width, height),
            startOverW, endOverW, startInverseW, endInverseW, hitUv, hitPosition);
        float hitDistance = length(hitPosition - surfacePosition);
        if (hasHit && hitDistance > 0.2f) { return ShadeReflection(hitUv, hitDistance, normal, surfacePosition, hitData); }
        if (settings.w > 4.5f) { return float4(0.5f, 0, 0, 1); }
        return 0;
    }
    const uint kMaxSteps = 64;
    uint stepCount = min(kMaxSteps, uint(ceil(pixelLength)));
    float previousProgress = 0;
    float3 previousPosition = origin;
    for (uint stepIndex = 1; stepIndex <= stepCount; ++stepIndex) {
        float progress = screenFraction * float(stepIndex) / float(stepCount);
        float2 uv = startUv + uvDelta * progress;
        float3 samplePosition = InterpolateRayPosition(startOverW, endOverW, startInverseW, endInverseW, progress);
        float3 segmentEndPosition = samplePosition;
        float sampleDepth = depthTexture.Load(int3(GetPixel(uv), 0));
        float4 hitNormal = normalTexture.Load(int3(GetPixel(uv), 0));
        if (sampleDepth < 0.999999f && hitNormal.a < 2.0f) {
            float sceneDepth = Reconstruct(uv, sampleDepth).z;
            float minimumRayDepth = min(previousPosition.z, samplePosition.z);
            float maximumRayDepth = max(previousPosition.z, samplePosition.z);
            // Test the whole ray segment, rather than only its endpoint.
            if (maximumRayDepth >= sceneDepth && minimumRayDepth <= sceneDepth + settings.y) {
                float lowerProgress = previousProgress;
                float upperProgress = progress;
                for (uint refineIndex = 0; refineIndex < 5; ++refineIndex) {
                    float middleProgress = (lowerProgress + upperProgress) * 0.5f;
                    float2 middleUv = startUv + uvDelta * middleProgress;
                    float3 middlePosition = InterpolateRayPosition(startOverW, endOverW, startInverseW, endInverseW, middleProgress);
                    float middleDepth = depthTexture.Load(int3(GetPixel(middleUv), 0));
                    float difference = middlePosition.z - Reconstruct(middleUv, middleDepth).z;
                    bool hasCrossedSurface = difference >= 0;
                    if (direction.z < 0) { hasCrossedSurface = difference <= 0; }
                    if (hasCrossedSurface) { upperProgress = middleProgress; }
                    else { lowerProgress = middleProgress; }
                }
                uv = startUv + uvDelta * upperProgress;
                samplePosition = InterpolateRayPosition(startOverW, endOverW, startInverseW, endInverseW, upperProgress);
                sampleDepth = depthTexture.Load(int3(GetPixel(uv), 0));
                hitNormal = normalTexture.Load(int3(GetPixel(uv), 0));
                float difference = samplePosition.z - Reconstruct(uv, sampleDepth).z;
                float hitDistance = length(samplePosition - surfacePosition);
                if (sampleDepth < 0.999999f && hitNormal.a < 2.0f && abs(difference) <= settings.y && hitDistance > 0.2f) {
                    return ShadeReflection(uv, hitDistance, normal, surfacePosition, hitData);
                }
            }
        }
        previousProgress = progress;
        previousPosition = segmentEndPosition;
    }
    if (settings.w > 4.5f) { return float4(0.5f, 0, 0, 1); }
    return 0;
}

struct TraceOutput {
    float4 reflection : SV_TARGET0;
    float4 hitData : SV_TARGET1;
};
TraceOutput main(PixelInput input)
{
    TraceOutput output;
    output.reflection = TraceReflection(input, output.hitData);
    return output;
}
