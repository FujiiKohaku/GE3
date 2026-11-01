#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || dispatchThreadId.x >= gEmitter.count) { return; }
    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= int32_t(gEmitter.maxParticles)) {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }
    uint32_t particleIndex = gFreeList[freeListIndex];
    float radius = gEffectSettings.endScale;
    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate;
    particle.scale = float32_t3(radius, radius, radius);
    particle.lifeTime = max(gEffectSettings.lifeTime, 0.01f);
    particle.color = gEffectSettings.startColor;
    gParticles[particleIndex] = particle;
}
