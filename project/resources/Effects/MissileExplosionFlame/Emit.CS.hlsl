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

float32_t3 Random3(uint32_t index, float32_t salt)
{
    float32_t base = (float32_t) index + salt + gPerFrame.time * 53.81f;
    return float32_t3(
        Hash(base * 15.127f),
        Hash(base * 61.721f),
        Hash(base * 91.317f));
}

float32_t3 RandomUnitVector(float32_t3 random)
{
    float32_t3 direction = random * 2.0f - 1.0f;
    return direction / max(length(direction), 0.0001f);
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

    float32_t3 random = Random3(DTid.x, 23.4f);
    float32_t3 direction = RandomUnitVector(random);
    float shell = gEmitter.radius * (0.65f + Hash((float32_t) DTid.x * 2.91f + 4.2f) * 0.8f);
    float speed = 7.0f + Hash((float32_t) DTid.x * 8.41f + 1.4f) * 9.0f;
    float scale = gEffectSettings.startScale * (0.65f + Hash((float32_t) DTid.x * 6.19f + 0.7f) * 1.15f);
    float lifeTime = gEffectSettings.lifeTime * (0.7f + Hash((float32_t) DTid.x * 7.83f + 6.6f) * 0.65f);

    gParticles[particleIndex].translate =
        gEmitter.translate + direction * shell;
    gParticles[particleIndex].velocity =
        direction * speed + gEffectSettings.velocity;
    gParticles[particleIndex].scale =
        float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime =
        max(lifeTime, 0.01f);
    gParticles[particleIndex].currentTime =
        0.0f;
    gParticles[particleIndex].color =
        gEffectSettings.startColor;
    gParticles[particleIndex].rotation =
        0.0f;
    gParticles[particleIndex].rotationSpeed =
        0.0f;
}
