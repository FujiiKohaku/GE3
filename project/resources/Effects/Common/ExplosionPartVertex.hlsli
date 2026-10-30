#include "ParticleRenderCommon.hlsli"
#include "../Explosion/Explosion.hlsli"

ExplosionVertexOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput billboard = BuildBillboardParticleVertex(input, instanceId);
    ParticleCS particle = gParticles[instanceId];
#if EXPLOSION_PART_KIND == 2
    // 火花の長軸を画面上の移動方向へ向ける。
    float3 cameraRight = gPerView.billboardMatrix[0].xyz;
    float3 cameraUp = gPerView.billboardMatrix[1].xyz;
    float2 projectedVelocity = float2(dot(particle.velocity, cameraRight), dot(particle.velocity, cameraUp));
    float2 along = float2(0.0f, 1.0f);
    float speedSquared = dot(projectedVelocity, projectedVelocity);
    if (speedSquared > 0.0001f) { along = projectedVelocity * rsqrt(speedSquared); }
    float2 across = float2(along.y, -along.x);
    float2 offset = across * input.position.x * particle.scale.x * 0.045f +
        along * input.position.y * particle.scale.y;
    float3 worldPosition = particle.translate + cameraRight * offset.x + cameraUp * offset.y;
    billboard.position = mul(float4(worldPosition, 1.0f), gPerView.viewProjection);
    billboard.worldPosition = worldPosition;
    billboard.viewDistance = length(worldPosition - gPerView.cameraPosition);
#endif
    ExplosionVertexOutput output;
    output.position = billboard.position;
    output.texcoord = billboard.texcoord;
    output.color = billboard.color;
    output.worldPosition = billboard.worldPosition;
    output.viewDistance = billboard.viewDistance;
    output.lifeRatio = saturate(particle.currentTime / max(particle.lifeTime, 0.001f));
    output.shapeSeed = particle.padding.y;
    return output;
}
