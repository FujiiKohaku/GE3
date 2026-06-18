#include "../../Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
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

    RandomGenerator generator;
    generator.seed = ((float32_t3) DTid + gPerFrame.time) * 7.531f;

    float32_t3 random = generator.Generate3d() - 0.5f;
    float32_t3 direction = normalize(float32_t3(random.x * 0.8f, 0.35f + abs(random.y), random.z * 0.8f));
    float speed = 0.7f + generator.Generate1d() * 1.6f;
    float scale = 0.35f + generator.Generate1d() * 0.65f;
    float gray = 0.35f + generator.Generate1d() * 0.25f;

    gParticles[particleIndex].translate = gEmitter.translate + random * gEmitter.radius;
    gParticles[particleIndex].velocity = direction * speed;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = 1.4f + generator.Generate1d() * 1.4f;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = float32_t4(gray, gray, gray, 0.8f);
}
