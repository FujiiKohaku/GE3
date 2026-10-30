#include "../../Common/Fog.hlsli"
#define VOLUME_CONSTANT_REGISTER b12
#define VOLUME_SHADOW_REGISTER t6
#define VOLUME_LOCAL_SHADOW_REGISTER t7
#include "../../PostEffect/Volumetric/VolumeData.hlsli"
#include "../../PostEffect/Volumetric/RaymarchFunctions.hlsli"
ConstantBuffer<FogData> gObjectFog : register(b11);
cbuffer ObjectFogFlags : register(b13) { uint gObjectFogFlags; };

// Integrate only as far as this surface, rather than reusing the background depth.
float3 ApplyObjectVolume(float3 color, float3 worldPosition)
{
    float3 delta = worldPosition - cameraAndDistance.xyz;
    float surfaceDistance = length(delta);
    float3 ray = delta / max(surfaceDistance, 0.000001f);
    float distance = min(surfaceDistance, cameraAndDistance.w);
    int samples = clamp(int(settings.y), 8, 64);
    float stepLength = distance / samples;
    float g = clamp(settings.x, -0.8f, 0.8f);
    float cosAngle = dot(ray, -lightDirectionAndDensity.xyz);
    float phase = (1.0f - g * g) / pow(max(1.0f + g * g - 2.0f * g * cosAngle, 0.04f), 1.5f);
    float scattering = 0.0f;
    float3 spotScattering = 0.0f;
    float transmission = 1.0f;
    for (int index = 0; index < samples; ++index) {
        float3 position = ray * ((index + 0.5f) * stepLength);
        float density = DensityAt(position);
        if (density <= 0.0f) continue;
        float segmentTransmission = exp(-density * stepLength);
        if (settings.w != 0.0f && lightColorAndIntensity.w > 0.0f)
            scattering += ShadowVisibility(cameraAndDistance.xyz + position) * transmission * (1.0f - segmentTransmission);
        if (spotCountAndBias.x > 0.0f)
            spotScattering += SpotScattering(position, ray, g) * transmission * (1.0f - segmentTransmission);
        transmission *= segmentTransmission;
        if (transmission < 0.001f) break;
    }
    float3 light = (scattering * phase * lightColorAndIntensity.rgb * lightColorAndIntensity.w + spotScattering) * temporalControls.w;
    if (fogColorAndEnabled.w == 0.0f) transmission = 1.0f;
    return color * transmission + fogColorAndEnabled.rgb * temporalControls.w * (1.0f - transmission) + clamp(light, 0.0f, 8.0f);
}

float4 ApplyObjectFog(float4 color, float3 worldPosition)
{
    if ((gObjectFogFlags & 2u) != 0u) color.rgb = ApplyObjectVolume(color.rgb, worldPosition);
    if ((gObjectFogFlags & 1u) != 0u && gObjectFog.isEnabled != 0 && gObjectFog.distanceEnabled != 0) {
        float distance = length(worldPosition - gCamera.worldPosition);
        color = ApplyFog(color, gObjectFog.color.rgb, CalcDistanceFog(distance, gObjectFog.distance));
    }
    // Preserve coverage: fogging the surface must never remove the background fog.
    return color;
}
