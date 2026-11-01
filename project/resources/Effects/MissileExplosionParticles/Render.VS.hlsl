#include "../Common/ParticleRenderCommon.hlsli"

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    return BuildBillboardParticleVertex(input, instanceId);
}
