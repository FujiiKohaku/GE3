#include "../../Shaders/Common/Particle.hlsli"
ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 id : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || id.x >= gEmitter.count) return;
    int32_t freeIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
    if (freeIndex < 0 || freeIndex >= int32_t(gEmitter.maxParticles)) {
        InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
        return;
    }
    uint32_t index = gFreeList[freeIndex];
    float angle = float(id.x) * 2.3999632f;
    float size = 0.15f + float(id.x % 3u) * 0.07f;
    gParticles[index].translate = gEmitter.translate + float3(cos(angle), float(id.x % 3u) * 0.5f - 0.5f, sin(angle)) * gEmitter.radius;
    gParticles[index].velocity = float3(cos(angle) * 0.9f, 0.8f + float(id.x % 3u) * 0.4f, sin(angle) * 0.9f);
    gParticles[index].scale = float3(size, size * 0.35f, size * 0.8f);
    gParticles[index].lifeTime = gEffectSettings.lifeTime;
    gParticles[index].currentTime = 0.0f;
    gParticles[index].color = gEffectSettings.startColor;
    gParticles[index].rotation = angle;
    gParticles[index].rotationSpeed = gEffectSettings.rotationSpeed;
}
