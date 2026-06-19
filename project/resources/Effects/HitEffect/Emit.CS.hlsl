#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
static const float kPi = 3.14159265f;

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

    uint32_t particleIndex =
        gFreeList[freeListIndex];

    RandomGenerator generator;

    generator.seed =
        (float32_t3(DTid.x, DTid.x * 17.0f, gPerFrame.time + 0.37f)) *
        23.417f;

    float32_t3 random =
        generator.Generate3d();

    bool isCenterFlash = false;

    if (DTid.x == 0)
    {
        isCenterFlash = true;
    }

    float32_t3 velocity =
        float32_t3(0.0f, 0.0f, 0.0f);

    float scale =
        gEffectSettings.startScale;

    float lifeTime =
        0.18f + random.x * 0.10f;

    float rotationSpeed =
        gEffectSettings.rotationSpeed *
        (0.6f + generator.Generate1d() * 0.8f);

    if (isCenterFlash)
    {
        scale = 3.2f;
        lifeTime = 0.055f;
        rotationSpeed = 0.0f;
    }
    else
    {
        float angle =
            random.x *
            kPi *
            2.0f;

        float elevation =
            (random.y - 0.5f) *
            0.65f;

        float speed =
            22.0f +
            random.z *
            26.0f;

        float32_t3 direction =
            normalize(
                float32_t3(
                    cos(angle),
                    sin(angle),
                    elevation));

        velocity =
            direction *
            speed;

        scale =
            0.55f +
            generator.Generate1d() *
            0.75f;
    }

    gParticles[particleIndex].translate =
        gEmitter.translate;

    gParticles[particleIndex].velocity =
        velocity;

    gParticles[particleIndex].scale =
        float32_t3(
            scale,
            scale,
            scale);

    gParticles[particleIndex].lifeTime =
        lifeTime;

    gParticles[particleIndex].currentTime =
        0.0f;

    gParticles[particleIndex].color =
        gEffectSettings.startColor;

    gParticles[particleIndex].rotation =
        gEffectSettings.startRotation +
        random.x *
        kPi *
        2.0f;

    gParticles[particleIndex].rotationSpeed =
        rotationSpeed;
}
