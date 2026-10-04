// Shared light response for ice objects and their matte floor.
// b1 and b5 are already bound by both static and skinned object renderers.
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);

float3 GetStageIceAmbientRadiance()
{
    return max(gAmbientLight.color.rgb, 0.0f) * max(gAmbientLight.color.a, 0.0f) *
        gAmbientLight.componentSettings.z * GetIndirectLightingStrength(gAmbientLight);
}

float3 GetStageIceDirectRadiance(float visibility)
{
    return max(gDirectionalLight.color.rgb, 0.0f) *
        max(gDirectionalLight.intensity, 0.0f) * visibility * GetDirectLightingStrength(gAmbientLight);
}

float3 GetStageIceDiffuseLighting(float3 normal, float visibility)
{
    float faceLight = saturate(dot(normal, normalize(-gDirectionalLight.direction)) * 0.5f + 0.5f);
    float middleBand = smoothstep(0.35f, 0.37f, faceLight);
    float lightBand = smoothstep(0.71f, 0.73f, faceLight);
    float diffuseBand = lerp(0.08f, 0.60f, middleBand);
    diffuseBand = lerp(diffuseBand, 1.0f, lightBand);
    // Shadows reduce direct light; ambient light keeps shaded faces readable.
    return HemisphereAmbient(gAmbientLight, normal) * gAmbientLight.componentSettings.z +
        GetStageIceDirectRadiance(visibility) * diffuseBand * 0.85f;
}
