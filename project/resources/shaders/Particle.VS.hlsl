#include "Particle.hlsli"

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 pos = float4(input.position, 1.0f);
    output.position = mul(pos, viewProjection);
    output.texcoord = input.texcoord;
    return output;
}
