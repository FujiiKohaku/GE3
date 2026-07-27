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
    if (freeIndex < 0 || freeIndex >= 1024) {
        InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
        return;
    }
    uint32_t index = gFreeList[freeIndex];
    gParticles[index].translate = gEmitter.translate;
    gParticles[index].scale = gEffectSettings.startScale.xxx;
    gParticles[index].velocity = 0.0f;
    gParticles[index].lifeTime = max(gEffectSettings.lifeTime, 0.01f);
    gParticles[index].currentTime = 0.0f;
    gParticles[index].color = gEffectSettings.startColor;
    gParticles[index].rotation = gEffectSettings.startRotation;
    gParticles[index].rotationSpeed = gEffectSettings.rotationSpeed;
}
