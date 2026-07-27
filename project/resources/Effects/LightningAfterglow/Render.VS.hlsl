#include "../Common/ParticleCommon.hlsli"

StructuredBuffer<ParticleCS> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    ParticleCS particle = gParticles[instanceId];
    float4x4 worldMatrix = gPerView.billboardMatrix;
    float s = sin(particle.rotation);
    float c = cos(particle.rotation);
    float4 billboardX = worldMatrix[0];
    float4 billboardY = worldMatrix[1];
    worldMatrix[0] =
        (billboardX * c + billboardY * s) * particle.scale.x;
    worldMatrix[1] =
        (-billboardX * s + billboardY * c) * particle.scale.y * 2.8f;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;
    float4x4 wvp = mul(worldMatrix, gPerView.viewProjection);
    float4 worldPosition = mul(input.position, worldMatrix);
    output.position = mul(input.position, wvp);
    output.texcoord = input.texcoord;
    output.normal = input.normal;
    output.color = particle.color;
    output.worldPosition = worldPosition.xyz;
    output.viewDistance =
        length(worldPosition.xyz - gPerView.cameraPosition);
    return output;
}
