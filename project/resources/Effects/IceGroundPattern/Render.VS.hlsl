#include "../Common/ParticleCommon.hlsli"

StructuredBuffer<ParticleCS> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    ParticleCS particle = gParticles[instanceId];
    float s = sin(particle.rotation);
    float c = cos(particle.rotation);
    float2 local = input.position.xy * particle.scale.xy;
    float2 rotated = float2(
        local.x * c - local.y * s,
        local.x * s + local.y * c);

    // 床ペタ（XZ平面への水平固定描画）
    float4 worldPosition = float4(
        particle.translate.x + rotated.x,
        particle.translate.y + 0.02f, // 地面とのZファイティング防止用のわずかなオフセット
        particle.translate.z + rotated.y,
        1.0f);

    output.position = mul(worldPosition, gPerView.viewProjection);
    output.texcoord = input.texcoord;
    output.normal = float3(0.0f, 1.0f, 0.0f);
    output.color = particle.color;
    output.worldPosition = worldPosition.xyz;
    output.viewDistance = length(worldPosition.xyz - gPerView.cameraPosition);
    return output;
}
