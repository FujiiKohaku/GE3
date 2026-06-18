#include "../../Common/Particle.hlsli"

StructuredBuffer<ParticleCS> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    ParticleCS particle = gParticles[instanceId];

    float32_t4x4 worldMatrix = gPerView.billboardMatrix;

    float32_t rotationSin = sin(particle.rotation);
    float32_t rotationCos = cos(particle.rotation);
    float32_t4 billboardX = worldMatrix[0];
    float32_t4 billboardY = worldMatrix[1];

    worldMatrix[0] = (billboardX * rotationCos + billboardY * rotationSin) * particle.scale.x;
    worldMatrix[1] = (-billboardX * rotationSin + billboardY * rotationCos) * particle.scale.y;
    worldMatrix[2] *= particle.scale.z;

    worldMatrix[3].xyz = particle.translate;

    float32_t4x4 wvpMatrix = mul(worldMatrix, gPerView.viewProjection);

    output.position = mul(input.position, wvpMatrix);
    output.texcoord = input.texcoord;
    output.normal = input.normal;
    output.color = particle.color;

    return output;
}
