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

    if (freeListIndex < 0 ||
        freeListIndex >= int32_t(gEmitter.maxParticles))
    {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];

    RandomGenerator generator;
    generator.seed = ((float32_t3) DTid + gPerFrame.time) * 12.345f;

    float32_t3 random = generator.Generate3d();
    float32_t3 emitterOffset =
        MakeEmitterOffset(gEffectSettings.emitterShape, random, gEmitter.radius);
    float32_t3 radialDirection = normalize(emitterOffset);

    // Each particle receives a different orbital plane so the singularity
    // surrounds the center in 360 degrees instead of forming one flat ring.
    float32_t3 randomAxis = generator.Generate3d() * 2.0f - 1.0f;
    float32_t3 tangentDirection = cross(randomAxis, radialDirection);
    if (length(tangentDirection) <= 0.001f)
    {
        tangentDirection = cross(float32_t3(0.0f, 1.0f, 0.0f), radialDirection);
    }
    if (length(tangentDirection) <= 0.001f)
    {
        tangentDirection = cross(float32_t3(1.0f, 0.0f, 0.0f), radialDirection);
    }
    tangentDirection = normalize(tangentDirection);

    float orbitSpeed = lerp(2.5f, 4.5f, generator.Generate1d());
    float inwardSpeed = lerp(0.5f, 1.2f, generator.Generate1d());

    gParticles[particleIndex].translate = gEmitter.translate + emitterOffset;
    gParticles[particleIndex].velocity =
        gEffectSettings.velocity +
        tangentDirection * orbitSpeed -
        radialDirection * inwardSpeed;

    float scale = max(gEffectSettings.startScale, 0.0f);
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = max(gEffectSettings.lifeTime, 0.01f);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = gEffectSettings.startRotation;
    gParticles[particleIndex].rotationSpeed = gEffectSettings.rotationSpeed;
}
