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
        ((float32_t3) DTid + gPerFrame.time * 1.37f) * 31.417f;

    float32_t3 random = generator.Generate3d();
    float32_t3 offset =
        MakeEmitterOffset(0, random, gEmitter.radius);
    float32_t3 radialDirection = normalize(offset);
    float32_t3 randomAxis =
        generator.Generate3d() * 2.0f - 1.0f;
    float32_t3 tangentDirection =
        cross(radialDirection, randomAxis);
    if (length(tangentDirection) <= 0.001f)
    {
        tangentDirection =
            cross(radialDirection, float32_t3(0.0f, 1.0f, 0.0f));
    }
    tangentDirection = normalize(tangentDirection);

    float scale =
        lerp(0.08f, gEffectSettings.startScale, generator.Generate1d());
    gParticles[particleIndex].translate =
        gEmitter.translate + offset;
    gParticles[particleIndex].velocity =
        tangentDirection *
        lerp(1.1f, 2.4f, generator.Generate1d()) -
        radialDirection * 0.35f;
    gParticles[particleIndex].scale =
        float32_t3(scale, scale * 0.55f, scale * 1.4f);
    gParticles[particleIndex].lifeTime =
        lerp(2.8f, 4.8f, generator.Generate1d());
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color =
        gEffectSettings.startColor;
    gParticles[particleIndex].rotation =
        generator.Generate1d() * 6.28318530718f;
    gParticles[particleIndex].rotationSpeed =
        lerp(-8.0f, 8.0f, generator.Generate1d());
}
