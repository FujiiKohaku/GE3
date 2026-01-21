#include "object3d.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// ---------------------------------
// Well íËã`ÅiêÊÇ…Åj
// ---------------------------------
struct Well {
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};

StructuredBuffer<Well> gMatrixPalette : register(t0);

// ---------------------------------
// ì¸óÕ
// ---------------------------------
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
    float32_t4 weight   : WEIGHT0;
    int32_t4   index    : INDEX0;
};

struct Skinned {
    float32_t4 position;
    float32_t3 normal;
};

// ---------------------------------
// Skinning
// ---------------------------------
Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;

    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);

    skinnedPosition += mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinnedPosition += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinnedPosition += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinnedPosition += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;

    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    skinnedNormal += mul(input.normal, (float3x3)gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x;
    skinnedNormal += mul(input.normal, (float3x3)gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y;
    skinnedNormal += mul(input.normal, (float3x3)gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z;
    skinnedNormal += mul(input.normal, (float3x3)gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;

    skinned.normal = normalize(skinnedNormal);
    skinned.position = skinnedPosition;

    return skinned;
}

// ---------------------------------
// main
// ---------------------------------
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    Skinned skinned = Skinning(input);

    output.position =
        mul(skinned.position, gTransformationMatrix.WVP);

    output.worldPosition =
        mul(skinned.position, gTransformationMatrix.World).xyz;

    output.normal =
        normalize(
            mul(
                skinned.normal,
                (float3x3)gTransformationMatrix.WorldInverseTranspose
            )
        );

    output.texcoord = input.texcoord;

    return output;
}
