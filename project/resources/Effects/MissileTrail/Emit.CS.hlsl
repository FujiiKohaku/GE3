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

    float randomX = Hash((float32_t) DTid.x * 13.1f + gPerFrame.time * 31.7f) * 2.0f - 1.0f;
    float randomY = Hash((float32_t) DTid.x * 19.9f + gPerFrame.time * 47.3f) * 2.0f - 1.0f;
    float randomSpeed = Hash((float32_t) DTid.x * 7.7f + 3.5f);
    float randomScale = Hash((float32_t) DTid.x * 5.1f + 8.2f);
    float randomLife = Hash((float32_t) DTid.x * 3.3f + 2.9f);

    float32_t3 direction =
        normalize(float32_t3(randomX * 0.045f, randomY * 0.045f, -1.0f));
    float baseSpeed = length(gEffectSettings.velocity);
    baseSpeed = max(baseSpeed, 12.0f);

    float scale = gEffectSettings.startScale * (0.75f + randomScale * 0.55f);
    float lifeTime = gEffectSettings.lifeTime * (0.8f + randomLife * 0.45f);

    float32_t t = (float32_t)DTid.x / (float32_t)gEmitter.count;
    float32_t3 velocity =
        direction * (baseSpeed * (0.75f + randomSpeed * 0.45f)) +
        gEffectSettings.velocity;
    float32_t dt = gPerFrame.deltaTime * (1.0f - t);
    float32_t dragFactor = gEffectSettings.enableDrag != 0 ? pow(max(gEffectSettings.drag, 0.0f), dt * 30.0f) : 1.0f;

    gParticles[particleIndex].translate =
        lerp(gEmitter.prevTranslate, gEmitter.translate, t) +
        float32_t3(randomX, randomY, 0.0f) * gEmitter.radius +
        velocity * dt * dragFactor;
    gParticles[particleIndex].velocity = velocity;
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
