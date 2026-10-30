#include "../Common/ParticleRenderCommon.hlsli"
#include "Explosion.hlsli"

ExplosionVertexOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput billboard = BuildBillboardParticleVertex(input, instanceId);
    ParticleCS particle = gParticles[instanceId];
    ExplosionVertexOutput output;
    output.position = billboard.position;
    output.texcoord = billboard.texcoord;
    output.color = billboard.color;
    output.worldPosition = billboard.worldPosition;
    output.viewDistance = billboard.viewDistance;
    output.lifeRatio = saturate(particle.currentTime / max(particle.lifeTime, 0.001f));
    output.shapeSeed = 0.0f;
    return output;
}
