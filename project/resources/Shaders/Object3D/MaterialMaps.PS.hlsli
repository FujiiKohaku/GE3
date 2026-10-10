#include "SurfaceMaterial.hlsli"
Texture2D<float4> gMetallicRoughnessTexture : register(t5);
Material ResolveRasterMaterial(Material material, float2 uv) {
    if (material.metallicRoughnessMapEnabled != 0) {
        material = ApplyMetallicRoughnessSample(material, gMetallicRoughnessTexture.Sample(gSampler, uv).gb);
    }
    return material;
}
