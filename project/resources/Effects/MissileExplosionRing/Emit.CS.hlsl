#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || DTid.x >= gEmitter.count)
    {
        return;
    }

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    if (freeListIndex < 0 || freeListIndex >= kMaxGPUParticle)
    {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];
    float scaleOffset = 1.0f + (float32_t) DTid.x * 0.08f;

    gParticles[particleIndex].translate =
        gEmitter.translate;
    gParticles[particleIndex].velocity =
        float32_t3(0.0f, 0.0f, 0.0f);
    gParticles[particleIndex].scale =
        float32_t3(
            gEffectSettings.startScale * scaleOffset,
            gEffectSettings.startScale * scaleOffset,
            gEffectSettings.startScale * scaleOffset);
    gParticles[particleIndex].lifeTime =
        max(gEffectSettings.lifeTime, 0.01f);
    gParticles[particleIndex].currentTime =
        0.0f;
    gParticles[particleIndex].color =
        gEffectSettings.startColor;
    gParticles[particleIndex].rotation =
        0.0f;
    gParticles[particleIndex].rotationSpeed =
        0.0f;
}
