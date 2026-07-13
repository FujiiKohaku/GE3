#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

static const uint32_t kMaxGPUParticle = 1024;

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex >= kMaxGPUParticle)
    {
        return;
    }

    if (gParticles[particleIndex].color.a <= 0.0f)
    {
        return;
    }

    float32_t speed =
        length(gParticles[particleIndex].velocity);

    bool isCenterFlash = false;

    if (speed < 0.01f)
    {
        isCenterFlash = true;
    }

    if (!isCenterFlash)
    {
        float32_t drag =
            max(
                gEffectSettings.drag,
                0.01f);

        gParticles[particleIndex].velocity *=
            pow(
                drag,
                gPerFrame.deltaTime * 60.0f);
    }

    gParticles[particleIndex].translate +=
        gParticles[particleIndex].velocity *
        gPerFrame.deltaTime;

    gParticles[particleIndex].currentTime +=
        gPerFrame.deltaTime;

    gParticles[particleIndex].rotation +=
        gParticles[particleIndex].rotationSpeed *
        gPerFrame.deltaTime;

    float32_t lifeRate =
        saturate(
            gParticles[particleIndex].currentTime /
            gParticles[particleIndex].lifeTime);

    float32_t inverseLifeRate =
        1.0f -
        lifeRate;

    float32_t4 whiteColor =
    {
        1.0f,
        1.0f,
        0.92f,
        1.0f
    };

    float32_t4 yellowColor =
    {
        1.0f,
        0.78f,
        0.18f,
        1.0f
    };

    float32_t4 redColor =
    {
        1.0f,
        0.12f,
        0.0f,
        0.0f
    };

    float32_t4 currentColor =
        whiteColor;

    if (lifeRate < 0.22f)
    {
        currentColor =
            lerp(
                whiteColor,
                yellowColor,
                lifeRate / 0.22f);
    }
    else
    {
        currentColor =
            lerp(
                yellowColor,
                redColor,
                (lifeRate - 0.22f) / 0.78f);
    }

    if (isCenterFlash)
    {
        float32_t flashScale =
            lerp(
                1.80f,
                0.06f,
                lifeRate);

        gParticles[particleIndex].scale =
            float32_t3(
                flashScale,
                flashScale,
                flashScale);

        currentColor.a =
            inverseLifeRate *
            inverseLifeRate;
    }
    else
    {
        float32_t speedRate =
            saturate(
                speed /
                62.0f);

        float32_t startScale =
            lerp(
                gEffectSettings.startScale * 0.75f,
                gEffectSettings.startScale * 1.35f,
                speedRate);

        float32_t sparkScale =
            lerp(
                startScale,
                gEffectSettings.endScale,
                lifeRate);

        gParticles[particleIndex].scale =
            float32_t3(
                sparkScale,
                sparkScale,
                sparkScale);

        currentColor.a =
            inverseLifeRate *
            saturate(0.45f + speedRate);
    }

    gParticles[particleIndex].color =
        currentColor;

    if (gParticles[particleIndex].currentTime >=
            gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].color.a <= 0.0f)
    {
        gParticles[particleIndex].scale =
            float32_t3(
                0.0f,
                0.0f,
                0.0f);

        gParticles[particleIndex].color.a =
            0.0f;

        int32_t freeListIndex;

        InterlockedAdd(
            gFreeListIndex[0],
            1,
            freeListIndex);

        if ((freeListIndex + 1) < kMaxGPUParticle)
        {
            gFreeList[freeListIndex + 1] =
                particleIndex;
        }
        else
        {
            InterlockedAdd(
                gFreeListIndex[0],
                -1,
                freeListIndex);
        }
    }
}
