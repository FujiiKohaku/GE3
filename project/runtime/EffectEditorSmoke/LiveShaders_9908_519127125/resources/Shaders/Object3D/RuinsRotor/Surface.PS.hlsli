// Each material uses an integer UV band plus ordinary 0..1 texture coordinates.
float3 ResolveRotorSurface(float2 uv, float3 baseColor)
{
    float surfaceIndex = floor(uv.x * 0.5f);
    gMaterial.metallic = 0.0f;
    gMaterial.roughness = 0.88f;
    gMaterial.specularStrength = 0.12f;
    if (surfaceIndex > 0.5f && surfaceIndex < 1.5f) {
        gMaterial.metallic = 0.65f;
        gMaterial.roughness = 0.48f;
        gMaterial.specularStrength = 0.45f;
    }
    if (surfaceIndex > 1.5f) {
        gMaterial.roughness = 0.35f;
        gMaterial.specularStrength = 0.25f;
        if (surfaceIndex > 2.5f) { return baseColor * 2.5f; }
        return baseColor * 3.5f;
    }
    return 0.0f;
}
