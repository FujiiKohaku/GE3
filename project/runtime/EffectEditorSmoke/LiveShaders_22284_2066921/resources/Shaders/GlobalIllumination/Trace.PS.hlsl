#include "Common.hlsli"

float HashPixel(float2 pixel)
{
    float3 value = frac(float3(pixel.xyx) * 0.1031f);
    value += dot(value, value.yzx + 33.33f);
    return frac((value.x + value.y) * value.z);
}

bool ProjectPosition(float3 position, out float2 uv)
{
    float4 clip = mul(float4(position, 1), projection);
    uv = clip.xy / max(clip.w, 0.000001f) * float2(0.5f, -0.5f) + 0.5f;
    return clip.w > 0 && all(uv > 0.001f) && all(uv < 0.999f);
}

bool FindHit(float3 origin, float3 direction, out float2 hitUv, out float3 hitPosition)
{
    hitUv = 0;
    hitPosition = 0;
    uint stepCount = uint(sampling.y);
    float3 previousPosition = origin;
    float previousDistance = 0;
    for (uint stepIndex = 1; stepIndex <= stepCount; ++stepIndex) {
        float fraction = float(stepIndex) / float(stepCount);
        float distance = tracing.x * fraction * fraction;
        float3 position = origin + direction * distance;
        float2 uv;
        if (!ProjectPosition(position, uv)) { break; }
        int2 pixel = GetPixel(uv);
        float minimumDepth = min(previousPosition.z, position.z);
        float maximumDepth = max(previousPosition.z, position.z);
        bool shouldCheckDepth = true;
        if (tracing.w > 0.5f) {
            // Shared min/max Hi-Z cheaply rejects empty or distant screen tiles.
            float2 bounds = depthPyramid.Load(int3(pixel / 4, 2));
            shouldCheckDepth = bounds.x <= bounds.y && maximumDepth >= bounds.x && minimumDepth <= bounds.y + tracing.y;
            // SSR deliberately excludes reflective floors from its pyramid.
            // GI can receive light from floors, so test those pixels directly.
            if (normalTexture.Load(int3(pixel, 0)).a >= 2) { shouldCheckDepth = true; }
        }
        if (shouldCheckDepth) {
            float depth = depthTexture.Load(int3(pixel, 0));
            if (depth < 0.999999f) {
                float surfaceDepth = Reconstruct(uv, depth).z;
                if (maximumDepth >= surfaceDepth && minimumDepth <= surfaceDepth + tracing.y) {
                    float lowerDistance = previousDistance;
                    float upperDistance = distance;
                    for (uint refineIndex = 0; refineIndex < 4; ++refineIndex) {
                        float middleDistance = (lowerDistance + upperDistance) * 0.5f;
                        float3 middlePosition = origin + direction * middleDistance;
                        float2 middleUv;
                        if (!ProjectPosition(middlePosition, middleUv)) { break; }
                        float middleDepth = depthTexture.Load(int3(GetPixel(middleUv), 0));
                        float difference = middlePosition.z - Reconstruct(middleUv, middleDepth).z;
                        bool hasCrossedSurface = difference >= 0;
                        if (direction.z < 0) { hasCrossedSurface = difference <= 0; }
                        if (hasCrossedSurface) { upperDistance = middleDistance; }
                        else { lowerDistance = middleDistance; }
                    }
                    hitPosition = origin + direction * upperDistance;
                    if (!ProjectPosition(hitPosition, hitUv)) { return false; }
                    float hitDepth = depthTexture.Load(int3(GetPixel(hitUv), 0));
                    float difference = hitPosition.z - Reconstruct(hitUv, hitDepth).z;
                    if (hitDepth < 0.999999f && abs(difference) <= tracing.y && upperDistance > 0.15f) { return true; }
                }
            }
        }
        previousPosition = position;
        previousDistance = distance;
    }
    return false;
}

IndirectOutput main(PixelInput input)
{
    IndirectOutput output;
    output.indirectColor = 0;
    output.metadata = 0;
    int2 pixel = GetPixel(input.texcoord);
    float depth = depthTexture.Load(int3(pixel, 0));
    float4 encodedNormal = normalTexture.Load(int3(pixel, 0));
    float4 material = materialTexture.Load(int3(pixel, 0));
    if (!IsValidSurface(depth, encodedNormal, material)) { return output; }
    float3 worldNormal = GetWorldNormal(encodedNormal);
    float3 normal = normalize(mul(float4(worldNormal, 0), view).xyz);
    float3 position = Reconstruct(input.texcoord, depth);
    if (dot(normal, -position) < 0) { normal = -normal; worldNormal = -worldNormal; }
    output.metadata = float4(worldNormal, position.z);
    output.indirectColor.a = 1;
    float3 axis = float3(0, 1, 0);
    if (abs(normal.y) > 0.95f) { axis = float3(1, 0, 0); }
    float3 tangent = normalize(cross(axis, normal));
    float3 bitangent = cross(normal, tangent);
    float rotation = HashPixel(input.position.xy) + sampling.z * 0.618033989f;
    float3 irradiance = 0;
    uint rayCount = uint(sampling.x);
    for (uint rayIndex = 0; rayIndex < rayCount; ++rayIndex) {
        // Cosine-weighted hemisphere samples estimate diffuse irradiance / pi.
        float radiusSquared = frac((float(rayIndex) + 0.5f) / float(rayCount) + sampling.z * 0.381966011f);
        float angle = (rotation + float(rayIndex) * 0.618033989f) * 6.283185307f;
        float3 direction = tangent * cos(angle) * sqrt(radiusSquared) +
            bitangent * sin(angle) * sqrt(radiusSquared) + normal * sqrt(1 - radiusSquared);
        float2 hitUv;
        float3 hitPosition;
        if (FindHit(position + normal * 0.05f, direction, hitUv, hitPosition)) {
            int2 hitPixel = GetPixel(hitUv);
            float4 hitEncoded = normalTexture.Load(int3(hitPixel, 0));
            float hitDepth = depthTexture.Load(int3(hitPixel, 0));
            float normalDepth = hitEncoded.a;
            if (normalDepth >= 2) { normalDepth -= 2; }
            if (normalDepth <= -2) { normalDepth = -normalDepth - 2; }
            if (normalDepth < 0 || abs(normalDepth - hitDepth) > 0.004f) { continue; }
            float3 hitNormal = normalize(mul(float4(GetWorldNormal(hitEncoded), 0), view).xyz);
            if (dot(hitNormal, -hitPosition) < 0) { hitNormal = -hitNormal; }
            float facing = saturate(dot(hitNormal, -direction) * 4);
            float edgeFade = saturate(min(min(hitUv.x, 1 - hitUv.x), min(hitUv.y, 1 - hitUv.y)) * 20);
            float distanceFade = 1 - smoothstep(tracing.x * 0.5f, tracing.x, length(hitPosition - position));
            float3 radiance = max(sceneTexture.SampleLevel(linearSampler, hitUv, 0).rgb, 0);
            float maximumChannel = max(max(radiance.x, radiance.y), radiance.z);
            radiance *= min(1, tracing.z / max(maximumChannel, 0.0001f));
            irradiance += radiance * facing * edgeFade * distanceFade;
        }
    }
    output.indirectColor.rgb = irradiance / float(rayCount) * material.rgb * (1 - GetMetallic(material));
    return output;
}
