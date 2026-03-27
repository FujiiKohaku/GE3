#include "Skybox.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    output.texcoord = float3(input.position.x, input.position.y, input.position.z);

    return output;
}