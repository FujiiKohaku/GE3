#ifndef KOHAKU_SURFACE_LIGHTING
#define KOHAKU_SURFACE_LIGHTING
float3 StandardSurfaceDirect(float3 baseColor, float3 normal, float3 viewDirection, float3 lightDirection) {
    float specularStrength = 0;
    if (gMaterial.shininess > 0) { specularStrength = gMaterial.specularStrength; }
    float3 result = baseColor * SurfaceDiffuseWeight(baseColor, gMaterial.metallic, specularStrength, dot(normal, viewDirection))
        * saturate(dot(normal, lightDirection));
    if (gMaterial.shininess > 0) { result += SurfaceSpecular(gMaterial, baseColor, normal, viewDirection, lightDirection); }
    return result;
}
#if defined(KOHAKU_ENVIRONMENT_LIGHTING)
float3 StandardSurfaceIndirect(float3 baseColor, float3 normal, float3 viewDirection) {
    float3 ambient = 0;
    if (gAmbientLight.environmentSettings.x <= 0) {
        ambient = baseColor * SurfaceDiffuseWeight(baseColor, gMaterial.metallic, gMaterial.specularStrength, dot(normal, viewDirection))
            * HemisphereAmbient(gAmbientLight, normal);
    }
    return ambient + EnvironmentLighting(baseColor, normal, viewDirection, true);
}
float3 LegacySurfaceEnvironment(float3 normal, float3 viewDirection) {
    // Standard IBL owns the reflection when configured; legacy is its fallback.
    if (gMaterial.enableLighting == kShadingStandard && gAmbientLight.environmentSettings.y > 0 && gMaterial.specularStrength > 0) { return 0; }
    return SampleEnvironmentLighting(reflect(-viewDirection, normal), saturate(gMaterial.roughness), saturate(gMaterial.roughness) * 5.0f)
        * gMaterial.environmentCoefficient * GetIndirectLightingStrength(gAmbientLight);
}
#endif
float3 ToonSurfaceIndirect(float3 baseColor, float3 normal, float3 viewDirection) {
    return baseColor * SurfaceDiffuseWeight(baseColor, gMaterial.metallic, gMaterial.specularStrength, dot(normal, viewDirection))
        * HemisphereAmbient(gAmbientLight, normal) * 0.30f;
}
float3 ToonSurfaceDirect(float3 baseColor, float3 normal, float3 viewDirection, float3 lightDirection) {
    return SurfaceDiffuseWeight(baseColor, gMaterial.metallic, gMaterial.specularStrength, dot(normal, viewDirection))
        * ShadeToonSurface(baseColor, normal, viewDirection, lightDirection,
        gDirectionalLight.color.rgb, gDirectionalLight.intensity) * GetDirectLightingStrength(gAmbientLight)
        + gDirectionalLight.color.rgb * gDirectionalLight.intensity
        * SurfaceSpecular(gMaterial, baseColor, normal, viewDirection, lightDirection) * GetDirectLightingStrength(gAmbientLight);
}
float3 ShadowToonSurfaceDirect(float3 baseColor, float3 normal, float3 viewDirection, float3 lightDirection) {
    float faceLight = saturate(dot(normal, lightDirection) * 0.5f + 0.5f);
    float diffuse = lerp(0.10f, 0.65f, smoothstep(0.34f, 0.38f, faceLight));
    diffuse = lerp(diffuse, 1.0f, smoothstep(0.68f, 0.72f, faceLight));
    return baseColor * SurfaceDiffuseWeight(baseColor, gMaterial.metallic, gMaterial.specularStrength, dot(normal, viewDirection))
        * diffuse + SurfaceSpecular(gMaterial, baseColor, normal, viewDirection, lightDirection);
}
float3 ShadowToonSurfaceIndirect(float3 baseColor, float3 normal, float3 viewDirection) {
    float rim = smoothstep(0.72f, 0.88f, 1 - saturate(dot(normal, viewDirection)));
    return baseColor * SurfaceDiffuseWeight(baseColor, gMaterial.metallic, gMaterial.specularStrength, dot(normal, viewDirection))
        * HemisphereAmbient(gAmbientLight, normal) * 1.2f * (1 + rim * 0.12f);
}
#endif
