#include "Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeCounter : register(u1);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
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

    int32_t particleIndex = 0;
    InterlockedAdd(gFreeCounter[0], 1, particleIndex);

    if (particleIndex >= kMaxGPUParticle)
    {
        return;
    }

    RandomGenerator generator;
    generator.seed = ((float32_t3) DTid + gPerFrame.time) * gPerFrame.time;

    float32_t3 randomTranslate = generator.Generate3d();
    randomTranslate = (randomTranslate - 0.5f) * 2.0f;

    gParticles[particleIndex].scale = generator.Generate3d() * 0.3f;
    gParticles[particleIndex].translate = gEmitter.translate + randomTranslate;
    gParticles[particleIndex].velocity = float32_t3(0.0f, 0.0f, 0.0f);
    gParticles[particleIndex].lifeTime = 1.0f;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color.rgb = generator.Generate3d();
    gParticles[particleIndex].color.a = 1.0f;
}