#include "Particle.hlsli"

StructuredBuffer<ParticleCS> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    ParticleCS particle = gParticles[instanceId];

    float32_t4x4 worldMatrix = gPerView.billboardMatrix;

    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;

    worldMatrix[3].xyz = particle.translate;

    float32_t4x4 wvpMatrix = mul(worldMatrix, gPerView.viewProjection);

    output.position = mul(input.position, wvpMatrix);
    output.texcoord = input.texcoord;
    output.normal = input.normal;
    output.color = particle.color;

    return output;
}