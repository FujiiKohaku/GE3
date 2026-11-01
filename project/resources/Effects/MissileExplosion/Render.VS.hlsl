#include "../Common/ParticleRenderCommon.hlsli"

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    // Normalize the mesh's radius (currently 0.5) so particle scale is the world radius.
    input.position.xyz = normalize(input.position.xyz);
    VertexShaderOutput output = BuildMeshParticleVertex(input, instanceId);
    // This effect does not sample a texture. Carry geometric facing in the unused UV.
    output.texcoord.x = dot(normalize(output.normal), normalize(gPerView.cameraPosition - output.worldPosition));
    return output;
}
