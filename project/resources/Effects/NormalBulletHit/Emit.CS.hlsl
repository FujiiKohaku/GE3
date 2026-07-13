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

float32_t3 MakeSphereDirection(float32_t3 random)
{
    float32_t angle =
        random.x *
        kPi *
        2.0f;

    float32_t vertical =
        random.y *
        2.0f -
        1.0f;

    float32_t horizontal =
        sqrt(
            max(
                1.0f - vertical * vertical,
                0.0001f));

    return normalize(
        float32_t3(
            cos(angle) * horizontal,
            vertical,
            sin(angle) * horizontal));
}

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
        (float32_t3(
            DTid.x,
            DTid.x * 31.0f,
            gPerFrame.time + 0.73f)) *
        19.913f;

    float32_t3 random =
        generator.Generate3d();

    float32_t3 direction =
        MakeSphereDirection(random);

    bool isCenterFlash = false;

    if (DTid.x == 0)
    {
        isCenterFlash = true;
    }

    bool isFastShard = false;

    if (DTid.x < gEmitter.count / 3)
    {
        isFastShard = true;
    }

    float32_t speed =
        22.0f +
        generator.Generate1d() *
        25.0f;

    if (isFastShard)
    {
        speed =
            42.0f +
            generator.Generate1d() *
            34.0f;
    }

    float32_t spreadRadius =
        gEmitter.radius *
        (0.2f + generator.Generate1d() * 0.8f);

    float32_t scale =
        0.42f +
        generator.Generate1d() *
        0.48f;

    float32_t lifeTime =
        0.24f +
        generator.Generate1d() *
        0.18f;

    float32_t rotationDirection =
        1.0f;

    if (generator.Generate1d() < 0.5f)
    {
        rotationDirection =
            -1.0f;
    }

    float32_t rotationSpeed =
        gEffectSettings.rotationSpeed *
        (0.5f + generator.Generate1d() * 0.9f) *
        rotationDirection;

    float32_t3 velocity =
        direction *
        speed;

    float32_t3 translate =
        gEmitter.translate +
        direction *
        spreadRadius;

    if (isCenterFlash)
    {
        velocity =
            float32_t3(
                0.0f,
                0.0f,
                0.0f);

        translate =
            gEmitter.translate;

        scale =
            2.05f;

        lifeTime =
            0.10f;

        rotationSpeed =
            0.0f;
    }

    gParticles[particleIndex].translate =
        translate;

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
