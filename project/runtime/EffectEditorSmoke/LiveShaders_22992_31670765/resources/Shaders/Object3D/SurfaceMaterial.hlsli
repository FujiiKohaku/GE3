#ifndef KOHAKU_SURFACE_MATERIAL
#define KOHAKU_SURFACE_MATERIAL
Material ApplyMetallicRoughnessSample(Material material, float2 roughnessMetallic) {
    material.roughness = saturate(material.roughness * roughnessMetallic.x);
    material.metallic = saturate(material.metallic * roughnessMetallic.y);
    return material;
}
#endif
