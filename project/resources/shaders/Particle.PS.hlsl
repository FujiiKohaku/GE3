#include "Particle.hlsli"

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = tex.Sample(smp, input.texcoord);
    return color;
}
