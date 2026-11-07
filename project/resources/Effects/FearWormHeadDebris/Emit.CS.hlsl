#include "../../Shaders/Common/Particle.hlsli"
ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 id : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || id.x >= gEmitter.count) return;
    int32_t freeIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
    if (freeIndex < 0 || freeIndex >= int32_t(gEmitter.maxParticles)) {
        InterlockedAdd(gFreeListIndex[0], 1);
        return;
    }
    uint32_t index = gFreeList[freeIndex];
    float seed = float(id.x) * 7.31f + gPerFrame.time * 13.7f;
    float3 randomValue = frac(sin(float3(seed, seed + 17.0f, seed + 39.0f) * 12.9898f) * 43758.5453f);
    float angle = (float(id.x) + randomValue.x * 0.6f) * 2.3999632f;
    float speed = 5.0f + randomValue.y * 11.0f;
    bool isLarge = id.x < 12u;
    float size = 0.10f + randomValue.z * 0.22f;
    float verticalSpeed = 5.0f + randomValue.z * 14.0f;
    float lifetimeSeconds = gEffectSettings.lifeTime * (0.45f + randomValue.y * 0.45f);
    float4 color = float4(1.0f, 0.42f + randomValue.y * 0.4f, 0.08f, 1.0f);
    if (isLarge) {
        size = 0.65f + randomValue.z * 0.8f;
        speed *= 0.65f;
        verticalSpeed = 8.0f + randomValue.z * 9.0f;
        lifetimeSeconds = gEffectSettings.lifeTime;
        color = float4(0.95f, 0.90f, 0.78f, 1.0f);
        if ((id.x % 3u) == 0u) color = float4(0.78f, 0.52f, 0.19f, 1.0f);
    }
    gParticles[index].translate = gEmitter.translate + float3(cos(angle), randomValue.y - 0.5f, sin(angle)) * gEmitter.radius;
    gParticles[index].velocity = float3(cos(angle) * speed, verticalSpeed, sin(angle) * speed);
    gParticles[index].scale = float3(size, size * (0.22f + randomValue.x * 0.35f), size * (0.5f + randomValue.y));
    gParticles[index].lifeTime = lifetimeSeconds;
    gParticles[index].currentTime = 0.0f;
    gParticles[index].color = color;
    gParticles[index].rotation = randomValue.x * 6.2831853f;
    float spinDirection = 1.0f;
    if (randomValue.y < 0.5f) spinDirection = -1.0f;
    gParticles[index].rotationSpeed = spinDirection * (3.0f + randomValue.z * 9.0f);
}
