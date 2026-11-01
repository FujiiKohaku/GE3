#ifndef KOHAKU_SECONDARY_REFLECTION_HLSLI
#define KOHAKU_SECONDARY_REFLECTION_HLSLI
float3 GetHitReflectionEnvironment(float3 baseColor, float3 normal, float3 viewDirection) {
    if (gAmbientLight.environmentSettings.y > 0 && gMaterial.specularStrength > 0) {
        uint width, height, mipCount; gEnvironmentTexture.GetDimensions(0, width, height, mipCount);
        float roughnessValue = clamp(gMaterial.roughness, 0.08f, 1.0f);
        float3 environment = SampleEnvironmentLighting(reflect(-viewDirection, normal), roughnessValue, roughnessValue * float(mipCount - 1));
        return environment * SurfaceFresnel(baseColor, gMaterial.metallic, dot(normal, viewDirection))
            * saturate(gMaterial.specularStrength) * gAmbientLight.environmentSettings.y * (1 - roughnessValue * 0.5f)
            * GetIndirectLightingStrength(gAmbientLight);
    }
    if (environmentEnabled != 0 && raytracingShadingVariant == 0) { return LegacySurfaceEnvironment(normal, viewDirection); }
    return 0;
}
void ApplySecondaryReflection(inout ReflectionPayload payload, float3 baseColor, float3 position,
    float3 normal, float3 geometricNormal, float3 viewDirection) {
    // Only the reflection pass continues Standard radiance rays; shadow and GI paths keep their existing depth.
    if (composition.z > 0.5f || payload.rayKind != 0 || payload.reflectionDepth >= uint(indirectSampling.z)
        || gMaterial.enableLighting != kShadingStandard || gMaterial.specularStrength <= 0
        || gMaterial.roughness > controls.w || GetIndirectLightingStrength(gAmbientLight) <= 0) { return; }
    float roughnessValue = saturate(gMaterial.roughness);
    float3 halfVector = normal;
    float3 axis = float3(0, 1, 0); if (abs(normal.y) > 0.95f) { axis = float3(1, 0, 0); }
    float3 tangent = normalize(cross(axis, normal)); float3 bitangent = cross(normal, tangent);
    float3 localView = float3(dot(viewDirection, tangent), dot(viewDirection, bitangent), dot(viewDirection, normal));
    GgxVisibleFrame samplingFrame = BuildGgxVisibleFrame(localView, roughnessValue);
    uint seed = ReflectionHash(payload.sampleSeed ^ (payload.reflectionDepth * 0x27d4eb2du));
    if (roughnessValue >= 0.02f) {
        float2 sample = (float2(uint2(seed, ReflectionHash(seed ^ 0x165667b1u)) >> 9) + 0.5f) / 8388608.0f;
        float3 localHalfVector = SampleGgxVisibleNormal(samplingFrame, sample);
        halfVector = normalize(tangent * localHalfVector.x + bitangent * localHalfVector.y + normal * localHalfVector.z);
    }
    float3 direction = normalize(reflect(-viewDirection, halfVector));
    float lightCosine = saturate(dot(normal, direction)); if (lightCosine <= 0) { return; }
    float visibility = 1; if (roughnessValue >= 0.02f) { visibility = GgxSmithVisibility(lightCosine, samplingFrame.alpha); }
    float3 sampleWeight = SurfaceFresnel(baseColor, gMaterial.metallic, dot(viewDirection, halfVector))
        * visibility * saturate(gMaterial.specularStrength) * GetIndirectLightingStrength(gAmbientLight);
    float3 environment = GetHitReflectionEnvironment(baseColor, normal, viewDirection);
    float3 offsetNormal = geometricNormal; if (dot(offsetNormal, direction) < 0) { offsetNormal = -offsetNormal; }
    RayDesc ray; ray.Origin = position + offsetNormal * controls.y; ray.Direction = direction;
    ray.TMin = controls.z; ray.TMax = controls.x;
    ReflectionPayload child = (ReflectionPayload)0;
    child.reflectionDepth = payload.reflectionDepth + 1; child.sampleSeed = seed;
    if (historyValidation.w > 0.5f) {
        child.coneWidth = (payload.coneWidth + payload.coneSpread * payload.hitDistance)
            / max(abs(dot(geometricNormal, viewDirection)), 0.05f);
        child.coneSpread = payload.coneSpread;
        if (roughnessValue >= 0.02f) { child.coneSpread += 0.25f * roughnessValue * roughnessValue; }
    }
    TraceRay(scene, RAY_FLAG_NONE, 1, 0, 1, 0, ray, child);
    // Keep parent hit/guide information. A miss retains its environment fallback; a black hit replaces it.
    if (child.hit > 0) { payload.radiance = max(payload.radiance - environment + max(child.radiance, 0) * sampleWeight, 0); }
}
#endif
