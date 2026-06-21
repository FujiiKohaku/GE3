#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
float Hash(float value)
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

    float seed = (float) DTid.x + gPerFrame.time * 71.0f;
    float randomX = Hash(seed * 11.7f + 0.13f) * 2.0f - 1.0f;
    float randomY = Hash(seed * 5.3f + 2.71f) * 2.0f - 1.0f;
    float randomDepth = Hash(seed * 13.1f + 6.6f) * 0.6f - 0.3f;
    float randomScale = 0.92f + Hash(seed * 3.9f + 8.4f) * 0.16f;
    float randomLife = 0.85f + Hash(seed * 9.1f + 1.9f) * 0.3f;
    float randomRotation = (Hash(seed * 17.7f + 4.6f) * 2.0f - 1.0f) * 0.06f;

    gParticles[particleIndex].translate =
        gEmitter.translate +
        float3(randomX * gEmitter.radius, randomY * gEmitter.radius * 0.6f, randomDepth);
    gParticles[particleIndex].velocity =
        gEffectSettings.velocity;
    gParticles[particleIndex].scale =
        float3(gEffectSettings.startScale * randomScale, gEffectSettings.endScale * randomScale, 1.0f);
    gParticles[particleIndex].lifeTime =
        max(gEffectSettings.lifeTime * randomLife, 0.05f);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = randomRotation;
    gParticles[particleIndex].rotationSpeed = 0.0f;
}
