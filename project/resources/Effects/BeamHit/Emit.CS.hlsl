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

    float randomX = Hash((float) DTid.x * 12.1f + gPerFrame.time * 17.3f) * 2.0f - 1.0f;
    float randomY = Hash((float) DTid.x * 23.7f + gPerFrame.time * 29.8f) * 2.0f - 1.0f;
    float randomZ = Hash((float) DTid.x * 31.4f + gPerFrame.time * 41.5f) * 2.0f - 1.0f;
    float32_t3 direction = normalize(float32_t3(randomX, randomY, randomZ));
    float speed = 8.0f + Hash((float) DTid.x * 6.2f) * 20.0f;
    float scale = gEffectSettings.startScale * (0.65f + Hash((float) DTid.x * 4.1f) * 1.0f);

    gParticles[particleIndex].translate = gEmitter.translate + direction * gEmitter.radius;
    gParticles[particleIndex].velocity = direction * speed;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = gEffectSettings.lifeTime * (0.75f + Hash((float) DTid.x * 5.5f) * 0.6f);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = Hash((float) DTid.x * 9.9f) * 6.28318f;
    gParticles[particleIndex].rotationSpeed = gEffectSettings.rotationSpeed;
}
