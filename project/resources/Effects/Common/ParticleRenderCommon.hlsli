#ifndef EFFECT_PARTICLE_RENDER_COMMON_HLSLI
#define EFFECT_PARTICLE_RENDER_COMMON_HLSLI

#include "ParticleCommon.hlsli"

StructuredBuffer<ParticleCS> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput BuildBillboardParticleVertex(VertexShaderInput input, uint32_t instanceId)
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
    float32_t4 worldPosition = mul(input.position, worldMatrix);

    output.position = mul(input.position, wvpMatrix);
    output.texcoord = input.texcoord;
    output.normal = input.normal;
    output.color = particle.color;
    output.worldPosition = worldPosition.xyz;
    output.viewDistance = length(worldPosition.xyz - gPerView.cameraPosition);

    return output;
}

VertexShaderOutput BuildMeshParticleVertex(VertexShaderInput input, uint32_t instanceId)
{
    VertexShaderOutput output;

    ParticleCS particle = gParticles[instanceId];

    float32_t rotationSin = sin(particle.rotation);
    float32_t rotationCos = cos(particle.rotation);

    float32_t4x4 worldMatrix;
    worldMatrix[0] = float32_t4(
        rotationCos * particle.scale.x,
        rotationSin * particle.scale.x,
        0.0f,
        0.0f);
    worldMatrix[1] = float32_t4(
        -rotationSin * particle.scale.y,
        rotationCos * particle.scale.y,
        0.0f,
        0.0f);
    worldMatrix[2] = float32_t4(
        0.0f,
        0.0f,
        particle.scale.z,
        0.0f);
    worldMatrix[3] = float32_t4(
        particle.translate,
        1.0f);

    float32_t4x4 wvpMatrix = mul(worldMatrix, gPerView.viewProjection);
    float32_t4 worldPosition = mul(input.position, worldMatrix);

    output.position = mul(input.position, wvpMatrix);
    output.texcoord = input.texcoord;
    output.normal = input.normal;
    output.color = particle.color;
    output.worldPosition = worldPosition.xyz;
    output.viewDistance = length(worldPosition.xyz - gPerView.cameraPosition);

    return output;
}

#endif
