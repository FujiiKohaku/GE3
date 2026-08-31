#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

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
        seed.x = frac(sin(seed.x * 78.233f) * 43758.5453f);
        return seed.x;
    }
};

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || DTid.x >= gEmitter.count)
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
    RandomGenerator generator;
    generator.seed =
        ((float32_t3) DTid + gPerFrame.time * 2.11f) * 19.731f;

    float directionSign = -1.0f;
    if (generator.Generate1d() >= 0.5f)
    {
        directionSign = 1.0f;
    }
    float32_t3 random = generator.Generate3d() - 0.5f;
    float32_t3 offset =
        float32_t3(random.x, directionSign * 0.12f, random.z) *
        gEmitter.radius;
    float lateralSpeed = 0.55f;
    float axialSpeed =
        lerp(8.0f, 14.0f, generator.Generate1d());

    gParticles[particleIndex].translate =
        gEmitter.translate + offset;
    gParticles[particleIndex].velocity =
        float32_t3(
            random.x * lateralSpeed,
            directionSign * axialSpeed,
            random.z * lateralSpeed);
    float scale =
        lerp(0.10f, gEffectSettings.startScale, generator.Generate1d());
    gParticles[particleIndex].scale =
        float32_t3(scale * 0.22f, scale * 3.8f, scale * 0.10f);
    gParticles[particleIndex].lifeTime =
        lerp(0.65f, 1.20f, generator.Generate1d());
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color =
        gEffectSettings.startColor;
    gParticles[particleIndex].rotation = 0.0f;
    gParticles[particleIndex].rotationSpeed = 0.0f;
}
