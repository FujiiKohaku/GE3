#include "Surface.hlsli"

ConstantBuffer<TransformationMatrix> gTransformation : register(b0);

ShieldVertexOutput main(float4 position : POSITION0)
{
    ShieldVertexOutput output;
    output.position = mul(position, gTransformation.WVP);
    output.worldPosition = mul(position, gTransformation.World).xyz;
    output.normal = normalize(mul(position.xyz, (float3x3)gTransformation.WorldInverseTranspose));
    output.localPosition = position.xyz;
    return output;
}
