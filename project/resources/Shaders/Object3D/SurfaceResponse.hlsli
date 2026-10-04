// Shared opaque material response in linear HDR space.
float GetDirectLightingStrength(AmbientLight light)
{
    if (light.componentSettings.w > 1.5f) { return 0; }
    return max(light.componentSettings.x, 0);
}
float GetIndirectLightingStrength(AmbientLight light)
{
    if (light.componentSettings.w > 0.5f && light.componentSettings.w < 1.5f) { return 0; }
    return max(light.componentSettings.y, 0);
}
float3 HemisphereAmbient(AmbientLight light, float3 normal)
{
    float blend = saturate(normalize(normal).y * 0.5f + 0.5f);
    float3 hemisphere = lerp(light.groundColor.rgb, light.skyColor.rgb, blend);
    return max(light.color.rgb, 0.0f) * max(light.color.a, 0.0f) * hemisphere * GetIndirectLightingStrength(light);
}

float3 SurfaceSpecular(Material material, float3 baseColor, float3 N, float3 V, float3 L)
{
    float nl = saturate(dot(N, L));
    float nv = max(saturate(dot(N, V)), 0.001f);
    float3 sum = L + V;
    float3 H = sum * rsqrt(max(dot(sum, sum), 0.000001f));
    float nh = saturate(dot(N, H));
    float vh = saturate(dot(V, H));
    float roughness = clamp(material.roughness, 0.08f, 1.0f);
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denominator = nh * nh * (alpha2 - 1.0f) + 1.0f;
    float distribution = alpha2 / max(3.14159265f * denominator * denominator, 0.00001f);
    float k = (roughness + 1.0f) * (roughness + 1.0f) * 0.125f;
    float geometry = nv / (nv * (1.0f - k) + k);
    geometry *= nl / max(nl * (1.0f - k) + k, 0.00001f);
    float3 f0 = lerp(0.04f.xxx, saturate(baseColor), saturate(material.metallic));
    float grazing = 1.0f - vh;
    float3 fresnel = f0 + (1.0f - f0) * pow(grazing, 5.0f);
    return fresnel * distribution * geometry * nl * material.specularStrength / max(4.0f * nv * nl, 0.0001f);
}
