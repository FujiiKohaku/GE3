#ifndef KOHAKU_LOCAL_LIGHTING
#define KOHAKU_LOCAL_LIGHTING
#if defined(KOHAKU_RT_LOCAL_SHADOW_CAPTURE)
#include "RtLocalShadowSelection.hlsli"
#endif
ConstantBuffer<PointLightCollection> gPointLights : register(b3);
ConstantBuffer<SpotLightCollection> gSpotLights : register(b4);

uint2 GetClusterLightMasks(float3 worldPosition) {
    uint2 masks = uint2(0xffffffffu, 0xffu);
    if (gAmbientLight.clusterSettings.w < 0.5f) { return masks; }
    float4 viewPosition = mul(float4(worldPosition, 1.0f), gAmbientLight.clusterView);
    if (viewPosition.z < gAmbientLight.clusterSettings.x || viewPosition.z >= gAmbientLight.clusterSettings.y) { return masks; }
    float4 clip = mul(viewPosition, gAmbientLight.clusterProjection);
    float2 uv = clip.xy / clip.w * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv < 0.0f) || any(uv >= 1.0f)) { return masks; }
    uint2 tile = uint2(uv * float2(12, 8));
    uint slice = min(uint(log(viewPosition.z / gAmbientLight.clusterSettings.x) * gAmbientLight.clusterSettings.z), 15u);
    return gAmbientLight.clusterMasks[(slice * 8 + tile.y) * 12 + tile.x].xy;
}

float3 ShadeLocalLights(float3 baseColor, float3 normal, float3 view, float3 worldPosition,
    float3 geometricNormal, bool hasSpecular) {
    if (GetDirectLightingStrength(gAmbientLight) <= 0) { return 0; }
    float3 result = 0.0f;
    uint2 masks = GetClusterLightMasks(worldPosition);
#if defined(KOHAKU_RT_TRACE_LOCAL_SHADOWS)
    if (shouldShadeSelectedLocalLights) { masks &= rtLocalLightMasks.xy; }
#endif
    while (masks.x != 0) {
        uint lightIndex = firstbitlow(masks.x);
        masks.x &= masks.x - 1;
        PointLight light = gPointLights.lights[lightIndex];
        if (light.isActive == 0 || light.intensity <= 0.0f || light.radius <= 0.0f) { continue; }
        float3 delta = light.position - worldPosition;
        float distance = length(delta);
        if (distance >= light.radius || distance < 0.00001f) { continue; }
        float3 direction = delta / distance;
        float attenuation = pow(saturate(1.0f - distance / light.radius), light.decay);
        float visibility = 1.0f;
#ifdef KOHAKU_HAS_LOCAL_SHADOWS
        visibility = PointShadowVisibility(lightIndex, light.position, worldPosition, geometricNormal);
#endif
        float3 radiance = light.color.rgb * light.intensity * attenuation * visibility;
        float3 contribution = baseColor * radiance * saturate(dot(normal, direction));
        if (hasSpecular) { contribution += radiance * SurfaceSpecular(gMaterial, baseColor, normal, view, direction); }
        result += contribution;
#if defined(KOHAKU_RT_LOCAL_SHADOW_CAPTURE)
        if (shouldReceiveRtLocalShadow != 0 && (rtLocalLightMasks.x & (1u << lightIndex)) != 0) {
            capturedRtLocalLight += contribution * GetDirectLightingStrength(gAmbientLight);
        }
#endif
    }
    while (masks.y != 0) {
        uint lightIndex = firstbitlow(masks.y);
        masks.y &= masks.y - 1;
        SpotLight light = gSpotLights.lights[lightIndex];
        if (light.isActive == 0 || light.intensity <= 0.0f || light.distance <= 0.0f) { continue; }
        float3 delta = worldPosition - light.position;
        float distance = length(delta);
        if (distance >= light.distance || distance < 0.00001f) { continue; }
        float3 direction = -delta / distance;
        float cone = saturate((dot(-direction, light.direction) - light.cosAngle) / max(light.cosFalloffStart - light.cosAngle, 0.001f));
        if (cone <= 0.0f) { continue; }
        float attenuation = pow(saturate(1.0f - distance / light.distance), light.decay);
        float visibility = 1.0f;
#ifdef KOHAKU_HAS_LOCAL_SHADOWS
        visibility = SpotShadowVisibility(lightIndex, worldPosition, geometricNormal);
#endif
        float3 radiance = light.color.rgb * light.intensity * attenuation * cone * visibility;
        float3 contribution = baseColor * radiance * saturate(dot(normal, direction));
        if (hasSpecular) { contribution += radiance * SurfaceSpecular(gMaterial, baseColor, normal, view, direction); }
        result += contribution;
#if defined(KOHAKU_RT_LOCAL_SHADOW_CAPTURE)
        if (shouldReceiveRtLocalShadow != 0 && (rtLocalLightMasks.y & (1u << lightIndex)) != 0) {
            capturedRtLocalLight += contribution * GetDirectLightingStrength(gAmbientLight);
        }
#endif
    }
    return result * GetDirectLightingStrength(gAmbientLight);
}
#endif
