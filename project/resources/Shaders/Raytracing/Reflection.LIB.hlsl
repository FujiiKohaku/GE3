#include "ReflectionParameters.hlsli"
#include "HitMaterial.hlsli"
#include "../Object3D/Object3d.hlsli"
RaytracingAccelerationStructure scene : register(t0);
Texture2D<float> sceneDepth : register(t1);
Texture2D<float4> reflectionSurface : register(t2);
Texture2D<float4> reflectionEnvironment : register(t3);
Texture2D<float4> receiverMaterial : register(t4);
TextureCube<float4> gEnvironmentTexture : register(t5);
Texture2D<float4> localLightCapture : register(t6);
RWTexture2D<float4> reflectedImage : register(u0);
RWTexture2D<float4> receiverGuide : register(u1);
RWTexture2D<float4> secondaryHit : register(u2);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<AmbientLight> gAmbientLight : register(b2);
static Material gMaterial;
struct ReflectionPayload { float3 radiance; float hit; uint rayKind; float hitDistance; };
static bool shouldReceiveLocalShadow = true;
static bool shouldShadeSelectedLocalLights = false;
uint ReflectionHash(uint value);
float TraceLocalShadow(uint lightIndex, bool isPointLight, float3 lightPosition, float3 worldPosition, float3 normal) {
    uint mask = rtLocalLightMasks.y; if (isPointLight) { mask = rtLocalLightMasks.x; }
    if (!shouldReceiveLocalShadow || (mask & (1u << lightIndex)) == 0) { return 1; }
    float3 delta = lightPosition - worldPosition;
    float distance = length(delta);
    if (distance <= rtLocalShadowControls.z * 2 || dot(normal, delta) <= 0) { return 1; }
    float3 centerDirection = delta / distance;
    float3 axis = float3(0, 1, 0); if (abs(centerDirection.y) > 0.95f) { axis = float3(1, 0, 0); }
    float3 tangent = normalize(cross(axis, centerDirection)); float3 bitangent = cross(centerDirection, tangent);
    uint sampleCount = rtLocalLightMasks.z;
    if (rtLocalShadowControls.x <= 0) { sampleCount = 1; }
    float visibility = 0;
    uint seed = DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x;
    seed ^= ReflectionHash(asuint(worldPosition.x) ^ asuint(worldPosition.y) ^ asuint(worldPosition.z));
    seed ^= lightIndex * 0x85ebca6bu + uint(options.y) * 0x9e3779b9u;
    if (!isPointLight) { seed ^= 0xc2b2ae35u; }
    for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        uint sampleSeed = seed ^ (sampleIndex * 0x27d4eb2du);
        float fraction = (float(ReflectionHash(sampleSeed) & 0x00ffffffu) + 0.5f) / 16777216;
        float angle = float(ReflectionHash(sampleSeed ^ 0x165667b1u) & 0x00ffffffu) / 16777216 * 6.2831853f;
        float3 target = lightPosition + (tangent * cos(angle) + bitangent * sin(angle)) * sqrt(fraction) * rtLocalShadowControls.x;
        RayDesc ray; ray.Origin = worldPosition + normal * rtLocalShadowControls.y;
        float3 direction = target - ray.Origin; float rayDistance = length(direction);
        if (rayDistance <= rtLocalShadowControls.z * 2) { visibility += 1; continue; }
        ray.Direction = direction / rayDistance; ray.TMin = rtLocalShadowControls.z; ray.TMax = rayDistance - rtLocalShadowControls.z;
        ReflectionPayload payload; payload.radiance = 0; payload.hit = 0; payload.rayKind = 1; payload.hitDistance = 0;
        TraceRay(scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 2, 0, 1, 0, ray, payload);
        visibility += payload.radiance.x;
    }
    return visibility / float(sampleCount);
}
float PointShadowVisibility(uint lightIndex, float3 lightPosition, float3 worldPosition, float3 normal) {
    return TraceLocalShadow(lightIndex, true, lightPosition, worldPosition, normal);
}
float SpotShadowVisibility(uint lightIndex, float3 worldPosition, float3 normal);
#define KOHAKU_HAS_LOCAL_SHADOWS 1
#define KOHAKU_RT_TRACE_LOCAL_SHADOWS 1
#include "../Object3D/LocalLighting.hlsli"
float SpotShadowVisibility(uint lightIndex, float3 worldPosition, float3 normal) {
    return TraceLocalShadow(lightIndex, false, gSpotLights.lights[lightIndex].position, worldPosition, normal);
}
#define gSampler textureSampler
#include "../Object3D/EnvironmentLighting.hlsli"
uint ReflectionHash(uint value) {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}
void TraceSelectedLocalLights(uint2 pixel, uint2 fullPixel, float2 uv, float depth, float4 surface) {
    float4 capture = localLightCapture.Load(int3(fullPixel, 0)); if (capture.a < 0) { return; }
    float3 encodedNormal = surface.xyz * 2 - 1; float normalLength = length(encodedNormal);
    if (!isfinite(normalLength) || normalLength < 0.999f) { return; }
    float3 normal = encodedNormal / normalLength; float3 position = ReflectionWorld(uv, depth);
    float viewDepth = mul(float4(position, 1), view).z;
    if (!all(isfinite(position)) || viewDepth <= 0 || viewDepth > 65000) { return; }
    float4 material = receiverMaterial.Load(int3(fullPixel, 0));
    gMaterial = (Material)0; gMaterial.roughness = surface.a;
    gMaterial.metallic = saturate((material.a * 255 - 1) / 254); gMaterial.specularStrength = clamp(normalLength - 1, 0, 2);
    shouldShadeSelectedLocalLights = true; shouldReceiveLocalShadow = true;
    float3 radiance = ShadeLocalLights(material.rgb, normal, normalize(cameraPosition.xyz - position), position, normal, capture.a > 0.5f);
    reflectedImage[pixel] = float4(min(max(radiance, 0), 65000), 1);
    float guideRadius = 0; if (rtLocalShadowControls.x > 0) { guideRadius = 0.5f; }
    receiverGuide[pixel] = float4(normal * (1 + guideRadius) * 0.5f + 0.5f, viewDepth);
}
void TraceDiffuseIndirect(uint2 pixel, float2 uv, float depth, float4 surface) {
    float3 normal = surface.xyz * 2 - 1;
    float normalLength = length(normal);
    if (!isfinite(normalLength) || normalLength < 0.999f) { return; }
    normal /= normalLength;
    float3 position = ReflectionWorld(uv, depth);
    float viewDepth = mul(float4(position, 1), view).z;
    if (!all(isfinite(position)) || viewDepth <= 0 || viewDepth > 65000) { return; }
    float3 toCamera = normalize(cameraPosition.xyz - position);
    if (dot(normal, toCamera) < 0) { normal = -normal; }
    // A fixed guide radius enables diffuse denoising even on smooth materials.
    receiverGuide[pixel] = float4(normal * 1.5f * 0.5f + 0.5f, viewDepth);
    float3 axis = float3(0, 1, 0);
    if (abs(normal.y) > 0.95f) { axis = float3(1, 0, 0); }
    float3 tangent = normalize(cross(axis, normal)); float3 bitangent = cross(normal, tangent);
    uint rayCount = uint(options.x);
    float3 radiance = 0;
    for (uint index = 0; index < rayCount; ++index) {
        uint seed = pixel.x + pixel.y * DispatchRaysDimensions().x;
        seed ^= uint(options.y) * 0x9e3779b9u + index * 0x85ebca6bu;
        float fraction = (float(ReflectionHash(seed) & 0x00ffffffu) + 0.5f) / 16777216;
        float angle = float(ReflectionHash(seed ^ 0xc2b2ae35u) & 0x00ffffffu) / 16777216 * 6.2831853f;
        // Cosine-weighted sampling cancels cosine / pi in the Lambert estimator.
        float radius = sqrt(fraction);
        float3 direction = normalize(tangent * (radius * cos(angle)) + bitangent * (radius * sin(angle))
            + normal * sqrt(1 - fraction));
        RayDesc ray;
        ray.Origin = position + normal * controls.y; ray.Direction = direction;
        ray.TMin = controls.z; ray.TMax = controls.x;
        ReflectionPayload payload; payload.radiance = 0; payload.hit = 0; payload.rayKind = 0; payload.hitDistance = 0;
        TraceRay(scene, RAY_FLAG_NONE, 1, 0, 1, 0, ray, payload);
        radiance += min(max(payload.radiance, 0), composition.w);
    }
    reflectedImage[pixel] = float4(min(radiance / float(rayCount) * GetIndirectLightingStrength(gAmbientLight), 65000), 1);
}
[shader("raygeneration")]
void ReflectionRayGeneration() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 fullPixel = pixel * 2 + 1;
    uint width, height; sceneDepth.GetDimensions(width, height);
    fullPixel = min(fullPixel, uint2(width, height) - 1);
    float2 uv = (float2(fullPixel) + 0.5f) / float2(width, height);
    float depth = sceneDepth.Load(int3(fullPixel, 0));
    float4 surface = reflectionSurface.Load(int3(fullPixel, 0));
    float4 environment = reflectionEnvironment.Load(int3(fullPixel, 0));
    reflectedImage[pixel] = 0; receiverGuide[pixel] = 0; secondaryHit[pixel] = 0;
    if (depth >= 1 || environment.a < 0 || abs(environment.a - depth) > 0.000001f) { return; }
    if (composition.z > 1.5f) { TraceSelectedLocalLights(pixel, fullPixel, uv, depth, surface); return; }
    if (composition.z > 0.5f) { TraceDiffuseIndirect(pixel, uv, depth, surface); return; }
    if (surface.a > controls.w) { return; }
    float3 normal = surface.xyz * 2 - 1;
    float normalLength = length(normal);
    if (!isfinite(normalLength) || normalLength < 0.999f) { return; }
    float specularStrength = clamp(normalLength - 1, 0, 2);
    if (specularStrength < 0.0001f) { return; }
    normal /= normalLength;
    float3 position = ReflectionWorld(uv, depth);
    float viewDepth = mul(float4(position, 1), view).z;
    if (!all(isfinite(position)) || viewDepth <= 0 || viewDepth > 65000) { return; }
    float roughnessValue = saturate(surface.a);
    receiverGuide[pixel] = float4(normal * (1 + roughnessValue) * 0.5f + 0.5f, viewDepth);
    float3 toCamera = normalize(cameraPosition.xyz - position);
    if (dot(normal, toCamera) < 0) { normal = -normal; }
    float4 material = receiverMaterial.Load(int3(fullPixel, 0));
    float metallicValue = saturate((material.a * 255 - 1) / 254);
    float3 f0 = lerp(0.04f.xxx, saturate(material.rgb), metallicValue);
    float3 fresnel = (f0 + (1 - f0) * pow(1 - saturate(dot(normal, toCamera)), 5))
        * specularStrength * GetIndirectLightingStrength(gAmbientLight) * (1 - roughnessValue * 0.5f);
    float3 axis = float3(0, 1, 0);
    if (abs(normal.y) > 0.95f) { axis = float3(1, 0, 0); }
    float3 tangent = normalize(cross(axis, normal)); float3 bitangent = cross(normal, tangent);
    uint rayCount = uint(options.x);
    if (roughnessValue < 0.02f) { rayCount = 1; }
    float3 radiance = 0; float hitCount = 0;
    float3 hitPositionSum = 0; float hitDistanceSum = 0;
    for (uint index = 0; index < rayCount; ++index) {
        float3 halfVector = normal;
        if (roughnessValue >= 0.02f) {
            uint seed = pixel.x + pixel.y * DispatchRaysDimensions().x;
            seed ^= uint(options.y) * 0x9e3779b9u + index * 0x85ebca6bu;
            float fraction = (float(ReflectionHash(seed) & 0x00ffffffu) + 0.5f) / 16777216;
            float angle = float(ReflectionHash(seed ^ 0xc2b2ae35u) & 0x00ffffffu) / 16777216 * 6.2831853f;
            float alpha = max(roughnessValue * roughnessValue, 0.0001f);
            float cosine = sqrt((1 - fraction) / (1 + (alpha * alpha - 1) * fraction));
            float sine = sqrt(max(1 - cosine * cosine, 0));
            halfVector = normalize(normal * cosine + (tangent * cos(angle) + bitangent * sin(angle)) * sine);
        }
        float3 direction = reflect(-toCamera, halfVector);
        if (dot(normal, direction) <= 0) { continue; }
        RayDesc ray;
        ray.Origin = position + normal * controls.y; ray.Direction = normalize(direction);
        ray.TMin = controls.z; ray.TMax = controls.x;
        ReflectionPayload payload; payload.radiance = 0; payload.hit = 0; payload.rayKind = 0; payload.hitDistance = 0;
        TraceRay(scene, RAY_FLAG_NONE, 1, 0, 1, 0, ray, payload);
        radiance += max(payload.radiance, 0) * fresnel; hitCount += payload.hit;
        if (payload.hit > 0) {
            hitPositionSum += ray.Origin + ray.Direction * payload.hitDistance;
            hitDistanceSum += payload.hitDistance;
        }
    }
    reflectedImage[pixel] = float4(min(radiance / float(rayCount), 65000), hitCount / float(rayCount));
    if (hitCount > 0) { secondaryHit[pixel] = float4(hitPositionSum / hitCount, hitDistanceSum / hitCount); }
}
[shader("miss")]
void ReflectionMiss(inout ReflectionPayload payload) {
    payload.hit = 0; payload.radiance = 0; payload.hitDistance = 0;
    if (payload.rayKind == 1) { payload.radiance = 1; }
}
[shader("anyhit")]
void ReflectionAnyHit(inout ReflectionPayload payload, BuiltInTriangleIntersectionAttributes attributes) {
    if (ShouldIgnoreMaterialHit(attributes)) { IgnoreHit(); }
}
[shader("closesthit")]
void ReflectionClosestHit(inout ReflectionPayload payload, BuiltInTriangleIntersectionAttributes attributes) {
    uint3 vertexIds = GetVertexIds(); float3 weights = GetWeights(attributes);
    float3 normal = 0; float2 uv = 0;
    for (uint index = 0; index < 3; ++index) {
        uint offsetBytes = vertexIds[index] * kVertexStrideBytes;
        normal += asfloat(vertices.Load3(offsetBytes + 24)) * weights[index];
        uv += asfloat(vertices.Load2(offsetBytes + 16)) * weights[index];
    }
    normal = normalize(mul(normal, (float3x3)WorldToObject3x4()));
    float3 toCamera = -WorldRayDirection();
    if (dot(normal, toCamera) < 0) { normal = -normal; }
    float3 position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    uv = mul(float4(uv, 0, 1), uvTransform).xy;
    float3 baseColor = max(baseTexture.SampleLevel(textureSampler, uv, 0).rgb * materialColor.rgb, 0);
    payload.hit = 1; payload.radiance = baseColor; payload.hitDistance = RayTCurrent();
    if (lightingMode == 0) { return; }
    shouldReceiveLocalShadow = shouldReceiveShadow != 0; shouldShadeSelectedLocalLights = false;
    gMaterial = (Material)0; gMaterial.color = materialColor; gMaterial.roughness = roughness;
    gMaterial.metallic = metallic; gMaterial.specularStrength = specularStrength; gMaterial.shininess = shininess;
    float3 light = normalize(-gDirectionalLight.direction);
    float visibility = 1;
    if (options.z > 0.5f && gDirectionalLight.intensity > 0 && dot(normal, light) > 0) {
        RayDesc shadowRay;
        shadowRay.Origin = position + normal * controls.y; shadowRay.Direction = light;
        shadowRay.TMin = controls.z; shadowRay.TMax = controls.x;
        ReflectionPayload shadow; shadow.radiance = 0; shadow.hit = 0; shadow.rayKind = 1; shadow.hitDistance = 0;
        TraceRay(scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
            2, 0, 1, 0, shadowRay, shadow);
        visibility = shadow.radiance.x;
    }
    float3 sun = gDirectionalLight.color.rgb * max(gDirectionalLight.intensity, 0) * visibility * GetDirectLightingStrength(gAmbientLight);
    float3 direct = baseColor * saturate(dot(normal, light)) + SurfaceSpecular(gMaterial, baseColor, normal, toCamera, light);
    if (lightingMode == 6) {
        direct = ShadeToonSurface(baseColor, normal, toCamera, light, float4(1, 1, 1, 1).rgb, 1)
            + SurfaceSpecular(gMaterial, baseColor, normal, toCamera, light);
    }
    payload.radiance = baseColor * HemisphereAmbient(gAmbientLight, normal) + EnvironmentLighting(baseColor, normal, toCamera, true)
        + direct * sun + ShadeLocalLights(baseColor, normal, toCamera, position, normal, true);
    if (environmentEnabled != 0) {
        uint width, height, levels; gEnvironmentTexture.GetDimensions(0, width, height, levels);
        payload.radiance += gEnvironmentTexture.SampleLevel(textureSampler, reflect(-toCamera, normal),
            saturate(roughness) * float(max(levels, 1u) - 1u)).rgb * environmentCoefficient * GetIndirectLightingStrength(gAmbientLight);
    }
}
