#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const float kPi = 3.14159265f;

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0)
    {
        return;
    }

    if (dispatchThreadId.x >= gEmitter.count)
    {
        return;
    }

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    if (freeListIndex < 0 || freeListIndex >= int32_t(gEmitter.maxParticles))
    {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];

    float32_t3 randomSeed = float32_t3(
        (float)dispatchThreadId.x * 23.41f + gPerFrame.time * 3.7f,
        (float)dispatchThreadId.x * 51.89f - gPerFrame.time * 1.9f,
        (float)dispatchThreadId.x * 89.13f + gPerFrame.time * 4.3f
    );
    float32_t3 randomValue = frac(sin(randomSeed) * 43758.5453f);

    float32_t3 boxOffset = float32_t3(
        (randomValue.x - 0.5f) * 2.0f * gEmitter.radius,
        (randomValue.y - 0.5f) * 14.0f,
        (randomValue.z - 0.5f) * 2.0f * gEmitter.radius
    );

    float scale = lerp(gEffectSettings.startScale * 0.7f, gEffectSettings.startScale * 1.3f, randomValue.x);
    float lifeTime = lerp(gEffectSettings.lifeTime * 0.8f, gEffectSettings.lifeTime * 1.3f, randomValue.y);

    gParticles[particleIndex].translate = gEmitter.translate + boxOffset;
    gParticles[particleIndex].velocity = gEffectSettings.velocity + (randomValue - 0.5f) * 0.2f;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = randomValue.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (randomValue.x - 0.5f) * gEffectSettings.rotationSpeed;
}
