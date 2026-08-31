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
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    if (freeListIndex < 0 || freeListIndex >= kMaxGPUParticle)
    {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];

    float32_t3 rndSeed = float32_t3(
        (float)DTid.x * 29.71f + gPerFrame.time * 8.3f,
        (float)DTid.x * 53.37f - gPerFrame.time * 4.1f,
        (float)DTid.x * 81.19f + gPerFrame.time * 6.7f
    );
    float32_t3 rnd = frac(sin(rndSeed) * 43758.5453f);

    float scale = lerp(gEffectSettings.startScale * 0.6f, gEffectSettings.startScale * 1.5f, rnd.x);
    float lifeTime = lerp(gEffectSettings.lifeTime * 0.7f, gEffectSettings.lifeTime * 1.3f, rnd.y);

    float32_t3 spawnOffset = (rnd - 0.5f) * 2.0f * gEmitter.radius;

    gParticles[particleIndex].translate = gEmitter.translate + spawnOffset;
    gParticles[particleIndex].velocity = gEffectSettings.velocity + (rnd - 0.5f) * 1.2f;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = rnd.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (rnd.x - 0.5f) * gEffectSettings.rotationSpeed;
}
