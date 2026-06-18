#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;

class RandomGenerator
{
    float32_t3 seed;

    float32_t3 Generate3d()
    {
        seed = frac(sin(seed * 12.9898f) * 43758.5453f);
        return seed;
    }

    float32_t Generate1d()
    {
        float32_t result = frac(sin(seed.x * 78.233f) * 43758.5453f);
        seed.x = result;
        return result;
    }
};

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

    RandomGenerator generator;
    generator.seed = ((float32_t3) DTid + gPerFrame.time) * 12.345f;

    float32_t3 random = generator.Generate3d();
    float32_t3 randomDirection = (random - 0.5f) * 2.0f;

    float length = sqrt(
        randomDirection.x * randomDirection.x +
        randomDirection.y * randomDirection.y +
        randomDirection.z * randomDirection.z);

    if (length > 0.0f)
    {
        randomDirection = randomDirection / length;
    }

    float speed = 5.0f + generator.Generate1d() * 8.0f;

    gParticles[particleIndex].translate =
        gEmitter.translate + MakeEmitterOffset(gEffectSettings.emitterShape, random, gEmitter.radius);

    gParticles[particleIndex].velocity =
        randomDirection * speed + gEffectSettings.velocity;

    float scale = max(gEffectSettings.startScale, 0.0f);

    gParticles[particleIndex].scale =
        float32_t3(scale, scale, scale);

    gParticles[particleIndex].lifeTime =
        max(gEffectSettings.lifeTime, 0.01f);

    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = gEffectSettings.startRotation;
    gParticles[particleIndex].rotationSpeed = gEffectSettings.rotationSpeed;
}
