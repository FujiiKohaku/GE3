#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const float kPi = 3.14159265f;

float RandomFlameValue(float seed)
{
    return frac(sin(seed * 12.9898f) * 43758.5453f);
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || dispatchId.x >= gEmitter.count) { return; }
    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= int(gEmitter.maxParticles)) {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }
    uint particleIndex = gFreeList[freeListIndex];
    float seed = float(dispatchId.x) * 31.73f + gPerFrame.time * 97.11f;
    float angle = (float(dispatchId.x) + RandomFlameValue(seed) * 0.18f)
        * (2.0f * kPi / max(float(gEmitter.count), 1.0f));
    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate;
    particle.lifeTime = max(gEffectSettings.lifeTime * lerp(0.90f, 1.10f, RandomFlameValue(seed + 8.2f)), 0.01f);
    particle.color = gEffectSettings.startColor;
    particle.rotation = angle - kPi * 0.5f;
    particle.padding.x = angle;
    particle.padding.y = RandomFlameValue(seed + 19.3f);
    gParticles[particleIndex] = particle;
}

