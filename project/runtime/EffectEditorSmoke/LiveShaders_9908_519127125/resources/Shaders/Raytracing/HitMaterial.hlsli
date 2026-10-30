#include "../Common/AlphaMask.hlsli"
ByteAddressBuffer vertices : register(t1, space1);
ByteAddressBuffer indices : register(t2, space1);
Texture2D<float4> baseTexture : register(t3, space1);
Texture2D<float4> normalTexture : register(t4, space1);
Texture2D<float4> metallicRoughnessTexture : register(t5, space1);
#include "../Object3D/NormalMappingCommon.hlsli"
SamplerState textureSampler : register(s0, space1);
cbuffer MaterialParameters : register(b1, space1) {
    float4 materialColor;
    int lightingMode;
    int environmentEnabled;
    float shininess;
    float environmentCoefficient;
    row_major float4x4 uvTransform;
    float roughness;
    float metallic;
    float specularStrength;
    float surfacePadding;
    int normalMapEnabled;
    float normalMapStrength;
    float normalMapFlipY;
    float normalMapPadding;
    float alphaCutoff;
    int raytracingShadingVariant;
    int metallicRoughnessMapEnabled;
    float materialPadding;
};
cbuffer GeometryParameters : register(b2, space1) { uint hasIndices; uint shouldReceiveShadow; };
static const uint kVertexStrideBytes = 36;
uint3 GetVertexIds() {
    uint3 vertexIds = PrimitiveIndex() * 3 + uint3(0, 1, 2);
    if (hasIndices != 0) { vertexIds = indices.Load3(PrimitiveIndex() * 12); }
    return vertexIds;
}
float3 GetWeights(BuiltInTriangleIntersectionAttributes attributes) {
    return float3(1.0f - attributes.barycentrics.x - attributes.barycentrics.y,
        attributes.barycentrics.x, attributes.barycentrics.y);
}
bool ShouldIgnoreMaterialHit(BuiltInTriangleIntersectionAttributes attributes) {
    if (alphaCutoff <= 0.0f) { return false; }
    uint3 vertexIds = GetVertexIds();
    float3 weights = GetWeights(attributes);
    float2 uv = 0;
    for (uint index = 0; index < 3; ++index) {
        uv += asfloat(vertices.Load2(vertexIds[index] * kVertexStrideBytes + 16)) * weights[index];
    }
    float2 transformedUv = mul(float4(uv, 0, 1), uvTransform).xy;
    // Explicit mip 0: ray shaders have no raster derivatives.
    float textureAlpha = baseTexture.SampleLevel(textureSampler, transformedUv, 0).a;
    return ShouldRejectAlpha(textureAlpha, materialColor.a, alphaCutoff);
}
