#include "object3d.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
cbuffer BuildParameters : register(b1)
{
    float4 gBuild; // x: Z position of the construction wave
};

struct VertexInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct BuildOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float buildDistance : SV_ClipDistance0;
};

BuildOutput main(VertexInput input)
{
    BuildOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    output.buildDistance = gBuild.x - output.worldPosition.z;
    return output;
}
