#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
static const float kPi = 3.14159265f;

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0)
    {
        return;
    }

    if (DTid.x >= gEmitter.count)
    {
        return;
    }

    int32_t freeListIndex;
    InterlockedAdd(
        gFreeListIndex[0],
        -1,
        freeListIndex);

    if (freeListIndex < 0 ||
        freeListIndex >= kMaxGPUParticle)
    {
        InterlockedAdd(
            gFreeListIndex[0],
            1,
            freeListIndex);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];

    float slotRate =
        ((float)DTid.x + 0.5f) /
        (float)gEmitter.count;
    float spawnAngle =
        slotRate * kPi * 2.0f +
        gPerFrame.time * 0.35f;

    float32_t3 spawnOffset;
    spawnOffset.x = cos(spawnAngle) * gEmitter.radius;
    spawnOffset.y =
        sin(spawnAngle * 2.0f) * gEmitter.radius * 0.32f;
    spawnOffset.z =
        sin(spawnAngle) * gEmitter.radius * 0.60f;

    float32_t3 velocity = gEffectSettings.velocity;
    float scale = gEffectSettings.startScale;
    float lifeTime = gEffectSettings.lifeTime;

    gParticles[particleIndex].translate =
        gEmitter.translate + spawnOffset;
    gParticles[particleIndex].velocity = velocity;
    gParticles[particleIndex].scale =
        float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color =
        gEffectSettings.startColor;
    gParticles[particleIndex].rotation =
        gEffectSettings.startRotation;
    gParticles[particleIndex].rotationSpeed =
        gEffectSettings.rotationSpeed;
}
