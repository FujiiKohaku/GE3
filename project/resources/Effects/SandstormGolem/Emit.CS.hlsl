#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

float Hash(uint32_t id, float salt)
{
    return frac(
        sin((float32_t(id) + 1.0f) * (12.9898f + salt)) *
        43758.5453f);
}

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 ||
        dispatchThreadId.x >= gEmitter.count)
    {
        return;
    }

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 ||
        freeListIndex >= int32_t(gEmitter.maxParticles))
    {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];
    float angle =
        Hash(particleIndex, 1.73f) * 6.28318530718f;
    float groundRadius =
        sqrt(Hash(particleIndex, 8.41f)) * gEmitter.radius;
    float height =
        Hash(particleIndex, 4.19f) * 0.16f;
    float32_t3 groundPosition =
        float32_t3(
            cos(angle) * groundRadius,
            height,
            sin(angle) * groundRadius);

    gParticles[particleIndex].translate =
        gEmitter.translate + groundPosition;
    gParticles[particleIndex].velocity =
        float32_t3(
            sin(angle) * 0.18f,
            Hash(particleIndex, 7.37f) * 0.25f,
            -cos(angle) * 0.18f);

    float scale =
        lerp(0.088f, 0.220f, Hash(particleIndex, 2.61f));
    gParticles[particleIndex].scale =
        float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime =
        max(gEffectSettings.lifeTime, 0.01f);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color =
        gEffectSettings.startColor;
    gParticles[particleIndex].color.a = 0.0f;
    gParticles[particleIndex].rotation =
        Hash(particleIndex, 5.33f) * 6.28318530718f;
    gParticles[particleIndex].rotationSpeed =
        lerp(-5.0f, 5.0f, Hash(particleIndex, 9.17f));
}
