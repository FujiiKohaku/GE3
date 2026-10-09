#ifndef KOHAKU_ENVIRONMENT_LIGHTING
#define KOHAKU_ENVIRONMENT_LIGHTING
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
        diffuse += gEnvironmentTexture.SampleLevel(gSampler, sampleDirection, maxMip).rgb;
    }
    float metallic = saturate(gMaterial.metallic);
    float3 result = baseColor * (1.0f - metallic) * diffuse / float(kEnvironmentSamples) * gAmbientLight.environmentSettings.x;
    if (hasSpecular) {
        float3 reflected = reflect(-view, normal);
        float roughness = clamp(gMaterial.roughness, 0.08f, 1.0f);
        float3 reflection = gEnvironmentTexture.SampleLevel(gSampler, reflected, roughness * maxMip).rgb;
        float3 f0 = lerp(0.04f.xxx, saturate(baseColor), metallic);
        float3 fresnel = f0 + (1.0f - f0) * pow(1.0f - saturate(dot(normal, view)), 5.0f);
        float3 specular = reflection * fresnel * gMaterial.specularStrength * gAmbientLight.environmentSettings.y * (1.0f - roughness * 0.5f);
        result += specular;
#if defined(KOHAKU_RT_REFLECTION_CAPTURE)
        capturedReflectionEnvironment += specular * GetIndirectLightingStrength(gAmbientLight);
#endif
    }
    return result * GetIndirectLightingStrength(gAmbientLight);
}
#endif
