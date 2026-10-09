RaytracingAccelerationStructure scene : register(t0);
Texture2D<float> sceneDepth : register(t1);
Texture2D<float4> sceneNormal : register(t2);
Texture2D<float4> directionalLight : register(t3);
RWTexture2D<float> shadowVisibility : register(u0);
cbuffer ShadowParameters : register(b0) {
    row_major float4x4 inverseViewProjection;
    float3 lightDirection;
    uint sampleCount;
    float normalBias;
    float rayBias;
    float sunAngularRadiusRadians;
    float maxRayDistance;
    uint sampleFrameIndex;
};
struct ShadowPayload { float visibility; };
#include "HitMaterial.hlsli"
uint HashPixel(uint value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}
[shader("raygeneration")]
void ShadowRayGeneration() {
    uint2 pixel = DispatchRaysIndex().xy;
    float depth = sceneDepth.Load(int3(pixel, 0));
    float4 sun = directionalLight.Load(int3(pixel, 0));
    float4 encodedNormal = sceneNormal.Load(int3(pixel, 0));
    shadowVisibility[pixel] = 1.0f;
    // Full-precision captured depth prevents stale lighting behind unsupported surfaces.
    if (depth >= 1.0f || sun.a < 0.0f || abs(sun.a - depth) > 0.000001f
        || encodedNormal.a < 0.0f || all(abs(sun.rgb) < 0.000001f)) { return; }
    float2 uv = (float2(pixel) + 0.5f) / float2(DispatchRaysDimensions().xy);
    float4 world = mul(float4(uv.x * 2 - 1, 1 - uv.y * 2, depth, 1), inverseViewProjection);
    float3 position = world.xyz / world.w;
    float3 normal = encodedNormal.xyz * 2 - 1;
    if (dot(normal, normal) < 0.000001f) { return; }
    normal = normalize(normal);
    float3 light = normalize(lightDirection);
    if (dot(normal, light) < 0) { normal = -normal; }
    float3 axis = float3(0, 1, 0);
    if (abs(light.y) > 0.99f) { axis = float3(1, 0, 0); }
    float3 tangent = normalize(cross(axis, light));
    float3 bitangent = cross(light, tangent);
    uint seed = pixel.x + pixel.y * DispatchRaysDimensions().x;
    seed ^= sampleFrameIndex * 0x9e3779b9u;
    float rotation = float(HashPixel(seed) & 0x00ffffffu) / 16777216.0f * 6.2831853f;
    float radialShift = 0.5f;
    if (sampleFrameIndex != 0) { radialShift = float(HashPixel(seed ^ 0x85ebca6bu) & 0x00ffffffu) / 16777216.0f; }
    uint rayCount = sampleCount;
    if (sunAngularRadiusRadians == 0) { rayCount = 1; }
    float visibility = 0;
    for (uint index = 0; index < rayCount; ++index) {
        float radius = sqrt((float(index) + radialShift) / float(rayCount)) * tan(sunAngularRadiusRadians);
        float angle = rotation + float(index) * 2.3999632f;
        RayDesc ray;
        ray.Origin = position + normal * normalBias;
        ray.Direction = normalize(light + radius * (cos(angle) * tangent + sin(angle) * bitangent));
        ray.TMin = rayBias;
        ray.TMax = maxRayDistance;
        ShadowPayload payload;
        payload.visibility = 0;
        TraceRay(scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
            2, 0, 1, 0, ray, payload);
        visibility += payload.visibility;
    }
    shadowVisibility[pixel] = visibility / float(rayCount);
}
[shader("miss")]
void ShadowMiss(inout ShadowPayload payload) { payload.visibility = 1.0f; }
[shader("anyhit")]
void ShadowAnyHit(inout ShadowPayload payload, BuiltInTriangleIntersectionAttributes attributes) {
    if (ShouldIgnoreMaterialHit(attributes)) { IgnoreHit(); }
}
