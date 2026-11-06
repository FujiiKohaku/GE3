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
    float3 rnd = frac(sin(float3(seed, seed + 17.0f, seed + 39.0f) * 12.9898f) * 43758.5453f);
    float angle = (float(id.x) + rnd.x * 0.6f) * 6.2831853f / float(gEmitter.count);
    float speed = 4.0f + rnd.y * 5.0f;
    bool gold = (id.x % 3u) == 0u;
    float size = gold ? 0.28f + rnd.z * 0.20f : 0.48f + rnd.z * 0.35f;
    gParticles[index].translate = gEmitter.translate + float3(cos(angle), rnd.y - 0.5f, sin(angle)) * gEmitter.radius;
    gParticles[index].velocity = float3(cos(angle) * speed, 4.0f + rnd.z * 7.0f, sin(angle) * speed);
    gParticles[index].scale = float3(size, size * (0.28f + rnd.x * 0.18f), size * (0.65f + rnd.y * 0.7f));
    gParticles[index].lifeTime = gEffectSettings.lifeTime;
    gParticles[index].currentTime = 0.0f;
    gParticles[index].color = gold ? float4(0.78f, 0.52f, 0.19f, 1.0f) : float4(0.95f, 0.90f, 0.78f, 1.0f);
    gParticles[index].rotation = rnd.x * 6.2831853f;
    gParticles[index].rotationSpeed = (rnd.y < 0.5f ? -1.0f : 1.0f) * (3.0f + rnd.z * 6.0f);
}
