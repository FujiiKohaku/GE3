#include "Sprite.hlsli"

// ===============================
// Transform（VS用）
// ===============================
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// ===============================
// Vertex Input（Sprite）
// ===============================
struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

// ===============================
// Vertex Shader
// ===============================
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
