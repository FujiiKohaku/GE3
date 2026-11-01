#ifndef KOHAKU_LIGHTING_ENERGY
#define KOHAKU_LIGHTING_ENERGY
float3 SurfaceFresnel(float3 baseColor, float metallic, float cosine) {
    float3 f0 = lerp(0.04f.xxx, saturate(baseColor), saturate(metallic));
    return f0 + (1 - f0) * pow(1 - saturate(cosine), 5);
}
float3 SurfaceDiffuseWeight(float3 baseColor, float metallic, float specularStrength, float cosine) {
    return (1 - SurfaceFresnel(baseColor, metallic, cosine) * saturate(specularStrength)) * (1 - saturate(metallic));
}
#endif
