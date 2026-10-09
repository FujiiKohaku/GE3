RaytracingAccelerationStructure scene : register(t0);
RWTexture2D<float4> outputImage : register(u0);
cbuffer CameraParameters : register(b0) {
    row_major float4x4 inverseViewProjection;
    float3 cameraPosition;
    uint debugMode;
    float nearClip;
    float farClip;
};

#include "HitMaterial.hlsli"

struct RayPayload { float3 color; };

[shader("raygeneration")]
void RayGeneration() {
    uint2 pixel = DispatchRaysIndex().xy;
    float2 uv = (float2(pixel) + 0.5f) / float2(DispatchRaysDimensions().xy);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 world = mul(float4(ndc, 0.0f, 1.0f), inverseViewProjection);
    float3 nearPosition = world.xyz / world.w;
    RayDesc ray;
    ray.Origin = cameraPosition;
    ray.Direction = normalize(nearPosition - cameraPosition);
    float forwardCosine = length(nearPosition - cameraPosition) / nearClip;
    ray.TMin = length(nearPosition - cameraPosition);
    ray.TMax = farClip * forwardCosine;
    RayPayload payload;
    payload.color = float3(0.03f, 0.04f, 0.06f);
    TraceRay(scene, RAY_FLAG_NONE, 1, 0, 1, 0, ray, payload);
    outputImage[pixel] = float4(payload.color, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.color = float3(0.03f, 0.04f, 0.06f);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attributes) {
    uint3 vertexIds = PrimitiveIndex() * 3 + uint3(0, 1, 2);
    if (hasIndices != 0) { vertexIds = indices.Load3(PrimitiveIndex() * 12); }
    float3 weights = float3(1.0f - attributes.barycentrics.x - attributes.barycentrics.y,
        attributes.barycentrics.x, attributes.barycentrics.y);
    float3 normal = 0;
    float2 uv = 0;
    for (uint index = 0; index < 3; ++index) {
        uint offsetBytes = vertexIds[index] * kVertexStrideBytes;
        normal += asfloat(vertices.Load3(offsetBytes + 24)) * weights[index];
        uv += asfloat(vertices.Load2(offsetBytes + 16)) * weights[index];
    }
    float3 worldNormal = mul(normal, (float3x3)WorldToObject3x4());
    float normalLength = length(worldNormal);
    if (normalLength > 0.000001f) { worldNormal /= normalLength; }
    if (debugMode == 1) {
        float2 transformedUv = mul(float4(uv, 0, 1), uvTransform).xy;
        payload.color = baseTexture.SampleLevel(textureSampler, transformedUv, 0).rgb * materialColor.rgb;
    } else if (debugMode == 2) {
        uint instanceId = InstanceID() + 1;
        payload.color = frac(float3(0.618034f, 0.414214f, 0.732051f) * float(instanceId));
    } else {
        payload.color = worldNormal * 0.5f + 0.5f;
    }
}

[shader("anyhit")]
void AnyHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attributes) {
    if (ShouldIgnoreMaterialHit(attributes)) { IgnoreHit(); }
}
