#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
static const float kPi = 3.14159265f;

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
    float seed = (float) DTid.x + gPerFrame.time * 59.0f;
    float angle = Hash(seed * 12.3f + 0.4f) * kPi * 2.0f;
    float height = Hash(seed * 7.7f + 3.1f) * 2.0f - 1.0f;
    float speed = 8.0f + Hash(seed * 5.1f + 9.6f) * 12.0f;
    float scale = gEffectSettings.startScale * (0.65f + Hash(seed * 4.3f + 1.2f) * 0.7f);
    float lifeTime = gEffectSettings.lifeTime * (0.75f + Hash(seed * 8.9f + 2.7f) * 0.45f);

    float3 direction =
        normalize(float3(cos(angle), sin(angle), height * 0.45f));

    gParticles[particleIndex].translate =
        gEmitter.translate + direction * gEmitter.radius;
    gParticles[particleIndex].velocity =
        direction * speed;
    gParticles[particleIndex].scale =
        float3(scale, scale, scale);
    gParticles[particleIndex].lifeTime =
        max(lifeTime, 0.05f);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation =
        Hash(seed * 10.1f + 4.5f) * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed =
        gEffectSettings.rotationSpeed * (0.6f + Hash(seed * 2.9f + 6.2f) * 0.8f);
}
