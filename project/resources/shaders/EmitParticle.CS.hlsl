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

    particleIndex = particleIndex % kMaxGPUParticle;

    RandomGenerator generator;
    generator.seed = ((float32_t3) DTid + gPerFrame.time) * 12.345f;

    float32_t3 randomDirection = generator.Generate3d();
    randomDirection = (randomDirection - 0.5f) * 2.0f;

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
        gEmitter.translate + randomDirection * 0.2f;

    gParticles[particleIndex].velocity =
        randomDirection * speed;

    float scale = 0.2f + generator.Generate1d() * 0.8f;
    gParticles[particleIndex].scale =
        float32_t3(scale, scale, scale);

    gParticles[particleIndex].lifeTime =
        0.4f + generator.Generate1d() * 0.5f;

    gParticles[particleIndex].currentTime = 0.0f;

    float colorRandom = generator.Generate1d();

    gParticles[particleIndex].color = float32_t4(
        1.0f,
        0.2f + colorRandom * 0.6f,
        0.0f,
        1.0f
    );
}