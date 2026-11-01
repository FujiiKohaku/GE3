#ifndef KOHAKU_SKY_LIGHTING
#define KOHAKU_SKY_LIGHTING
#include "Scattering.hlsli"
// Shared linear HDR sky. The direct solar disk is excluded from environment
// lighting because directional lighting already evaluates that energy.
float3 SkyLightingRadiance(float3 direction, float3 sunDirection, float3 sunColor,
    float sunIntensity, float4 atmosphere, float3 skyTint, float3 groundTint, float strength) {
    float3 upper = skyTint * max(sunIntensity, 0.0f)
        * smoothstep(-0.12f, 0.10f, sunDirection.y);
    if (atmosphere.x > 0.5f) {
        upper = AtmosphereRadiance(direction, sunDirection, sunColor, sunIntensity, atmosphere.w)
            * atmosphere.z;
    }
    float3 lower = groundTint * max(sunIntensity, 0.0f)
        * smoothstep(-0.12f, 0.10f, sunDirection.y) * 0.25f;
    return max(lerp(lower, upper, smoothstep(-0.12f, 0.02f, direction.y)) * strength, 0.0f);
}
float3 FilteredSkyLightingRadiance(float3 direction, float roughness, float3 sunDirection,
    float3 sunColor, float sunIntensity, float4 atmosphere, float3 skyTint, float3 groundTint, float strength) {
    float3 result = SkyLightingRadiance(direction, sunDirection, sunColor, sunIntensity,
        atmosphere, skyTint, groundTint, strength);
    if (roughness < 0.02f) { return result; }
    float3 axis = float3(0, 1, 0);
    if (abs(direction.y) > 0.95f) { axis = float3(1, 0, 0); }
    float3 tangent = normalize(cross(axis, direction));
    float3 bitangent = cross(direction, tangent);
    float spread = roughness * roughness * 1.5f;
    result *= 2.0f;
    result += SkyLightingRadiance(normalize(direction + tangent * spread), sunDirection, sunColor,
        sunIntensity, atmosphere, skyTint, groundTint, strength);
    result += SkyLightingRadiance(normalize(direction - tangent * spread), sunDirection, sunColor,
        sunIntensity, atmosphere, skyTint, groundTint, strength);
    result += SkyLightingRadiance(normalize(direction + bitangent * spread), sunDirection, sunColor,
        sunIntensity, atmosphere, skyTint, groundTint, strength);
    result += SkyLightingRadiance(normalize(direction - bitangent * spread), sunDirection, sunColor,
        sunIntensity, atmosphere, skyTint, groundTint, strength);
    return result / 6.0f;
}
#endif
