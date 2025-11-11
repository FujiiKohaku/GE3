// Particle.hlsli
struct VertexShaderInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

cbuffer Transform : register(b0)
{
    float4x4 viewProjection;
};
