#include "../Object3d.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

cbuffer VertexShaderParameters : register(b1)
{
    // x: gameplay animation time, y: local amplitude, z: phase, w: part type.
    // w = 0 for bell; tentacles use 1 + end/root radius ratio.
    float4 animation;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

#include "Deformation.hlsli"

struct JellyfishVertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float3 surface : TEXCOORD1;
};

JellyfishVertexOutput main(VertexShaderInput input)
{
    float3 position = input.position.xyz;
    float3 normal = input.normal;
    if (animation.w < 0.5f)
    {
        DeformBell(position, normal, animation);
    }
    else
    {
        DeformTentacle(position, normal, animation);
    }

    float4 localPosition = float4(position, input.position.w);
    JellyfishVertexOutput output;
    output.position = mul(localPosition, gTransformationMatrix.WVP);
    output.worldPosition = mul(localPosition, gTransformationMatrix.World).xyz;
    output.normal = normalize(mul(normal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    output.texcoord = input.texcoord;
    output.surface = float3(animation.x, animation.z, animation.w);
    return output;
}
