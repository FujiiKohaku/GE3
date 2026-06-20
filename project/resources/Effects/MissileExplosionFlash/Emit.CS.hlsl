#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;

float32_t Hash(float32_t value)
{
    return frac(sin(value) * 43758.5453f);
}

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
    float offset = Hash((float32_t) DTid.x * 17.17f + 1.3f) * gEmitter.radius;
    float32_t3 direction = normalize(float32_t3(
        Hash((float32_t) DTid.x * 3.1f + 2.2f) * 2.0f - 1.0f,
        Hash((float32_t) DTid.x * 5.3f + 7.4f) * 2.0f - 1.0f,
        Hash((float32_t) DTid.x * 9.7f + 4.6f) * 2.0f - 1.0f));

    gParticles[particleIndex].translate =
        gEmitter.translate + direction * offset;
    gParticles[particleIndex].velocity =
        direction * 4.0f;
    gParticles[particleIndex].scale =
        float32_t3(gEffectSettings.startScale, gEffectSettings.startScale, gEffectSettings.startScale);
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
