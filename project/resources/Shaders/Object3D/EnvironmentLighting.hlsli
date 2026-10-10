#ifndef KOHAKU_ENVIRONMENT_LIGHTING
#define KOHAKU_ENVIRONMENT_LIGHTING
#include "../Atmosphere/SkyLighting.hlsli"
float3 SampleEnvironmentLighting(float3 direction, float roughness, float cubeMip) {
    if (gAmbientLight.environmentSettings.z > 0.5f) {
        return FilteredSkyLightingRadiance(direction, roughness, normalize(-gDirectionalLight.direction),
            gDirectionalLight.color.rgb, gDirectionalLight.intensity, gAmbientLight.atmosphereSettings,
            gAmbientLight.skyColor.rgb, gAmbientLight.groundColor.rgb, gAmbientLight.environmentSettings.w);
    }
    return gEnvironmentTexture.SampleLevel(gSampler, direction, cubeMip).rgb;
}
// 少数サンプルによる環境積分の近似。静的キューブマップの粗いMipを併用する。
float3 EnvironmentLighting(float3 baseColor, float3 normal, float3 view, bool hasSpecular) {
    if (GetIndirectLightingStrength(gAmbientLight) <= 0) { return 0; }
    if (gAmbientLight.environmentSettings.x <= 0.0f && gAmbientLight.environmentSettings.y <= 0.0f) { return 0.0f; }
    uint width, height, levels;
    gEnvironmentTexture.GetDimensions(0, width, height, levels);
    float maxMip = float(max(levels, 1u) - 1u);
    float3 axis = float3(0, 1, 0);
    if (abs(normal.y) > 0.95f) { axis = float3(1, 0, 0); }
    float3 tangent = normalize(cross(axis, normal));
    float3 bitangent = cross(normal, tangent);
    float3 diffuse = 0.0f;
    const uint kEnvironmentSamples = 6;
    for (uint index = 0; index < kEnvironmentSamples; ++index) {
        float fraction = (float(index) + 0.5f) / float(kEnvironmentSamples);
        float angle = float(index) * 2.39996323f;
        float3 sampleDirection = tangent * (cos(angle) * sqrt(fraction)) +
            bitangent * (sin(angle) * sqrt(fraction)) + normal * sqrt(1.0f - fraction);
        diffuse += SampleEnvironmentLighting(sampleDirection, 0.0f, maxMip);
    }
    float metallic = saturate(gMaterial.metallic);
    float specularStrength = 0; if (hasSpecular) { specularStrength = gMaterial.specularStrength; }
    float3 result = baseColor * SurfaceDiffuseWeight(baseColor, metallic, specularStrength, dot(normal, view))
        * diffuse / float(kEnvironmentSamples) * gAmbientLight.environmentSettings.x;
    if (hasSpecular) {
        float3 reflected = reflect(-view, normal);
        float roughness = clamp(gMaterial.roughness, 0.08f, 1.0f);
        float3 reflection = SampleEnvironmentLighting(reflected, roughness, roughness * maxMip);
        float3 fresnel = SurfaceFresnel(baseColor, metallic, dot(normal, view));
        float3 specular = reflection * fresnel * saturate(gMaterial.specularStrength) * gAmbientLight.environmentSettings.y * (1.0f - roughness * 0.5f);
        result += specular;
#if defined(KOHAKU_RT_REFLECTION_CAPTURE)
        capturedReflectionEnvironment += specular * GetIndirectLightingStrength(gAmbientLight);
#endif
    }
    return result * GetIndirectLightingStrength(gAmbientLight);
}
#endif
