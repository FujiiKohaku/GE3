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

    RandomGenerator generator;
    generator.seed =
        float32_t3(
            (float32_t)DTid.x + 1.0f,
            gPerFrame.time + 3.7f,
            (float32_t)DTid.x * 9.3f) *
        17.731f;

    float32_t3 random = generator.Generate3d();

    float32_t3 spawnOffset;
    spawnOffset.x = (random.x - 0.5f) * gEmitter.radius * 2.0f;
    spawnOffset.y = (random.y - 0.5f) * 0.35f;
    spawnOffset.z = (random.z - 0.5f) * gEmitter.radius * 0.8f;

    float32_t3 velocity = gEffectSettings.velocity;
    velocity.x += (generator.Generate1d() - 0.5f) * 0.55f;
    velocity.y += generator.Generate1d() * 1.25f;
    velocity.z += (generator.Generate1d() - 0.5f) * 0.35f;

    float scale =
        gEffectSettings.startScale *
        (0.78f + generator.Generate1d() * 0.44f);
    float lifeTime =
        gEffectSettings.lifeTime *
        (0.82f + generator.Generate1d() * 0.36f);

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
        gEffectSettings.startRotation +
        (generator.Generate1d() - 0.5f) * 0.16f;
    gParticles[particleIndex].rotationSpeed =
        gEffectSettings.rotationSpeed *
        (generator.Generate1d() - 0.5f);
}
