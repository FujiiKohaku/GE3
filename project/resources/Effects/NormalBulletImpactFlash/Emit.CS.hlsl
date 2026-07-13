#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
static const float kPi = 3.14159265f;

float32_t Hash1d(float32_t seed)
{
    return frac(sin(seed * 78.233f) * 43758.5453f);
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

    float32_t seed =
        float32_t(DTid.x) +
        gPerFrame.time *
        13.0f;

    float32_t angle =
        float32_t(DTid.x) /
        max(float32_t(gEmitter.count), 1.0f) *
        kPi *
        2.0f;

    float32_t randomA =
        Hash1d(seed + 0.31f);

    float32_t randomB =
        Hash1d(seed + 1.79f);

    bool isCenterFlash = false;

    if (DTid.x == 0)
    {
        isCenterFlash = true;
    }

    float32_t2 planarDirection =
        float32_t2(
            cos(angle),
            sin(angle));

    float32_t3 velocity =
        float32_t3(
            planarDirection.x,
            0.0f,
            planarDirection.y) *
        (3.5f + randomA * 3.0f);

    float32_t scale =
        gEffectSettings.startScale *
        (0.72f + randomB * 0.42f);

    float32_t lifeTime =
        0.10f +
        randomA *
        0.07f;

    if (isCenterFlash)
    {
        velocity =
            float32_t3(
                0.0f,
                0.0f,
                0.0f);

        scale =
            gEffectSettings.startScale *
            1.45f;

        lifeTime =
            0.07f;
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
        angle +
        randomA *
        0.25f;

    gParticles[particleIndex].rotationSpeed =
        0.0f;
}
