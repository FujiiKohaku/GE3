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

    float speed =
        length(gParticles[particleIndex].velocity);

    bool isCenterFlash = false;

    if (speed < 0.01f)
    {
        isCenterFlash = true;
    }

    if (!isCenterFlash)
    {
        float drag =
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

    float lifeRate =
        saturate(
            gParticles[particleIndex].currentTime /
            gParticles[particleIndex].lifeTime);

    float inverseLifeRate =
        1.0f -
        lifeRate;

    float4 whiteColor =
    {
        1.0f,
        1.0f,
        0.9f,
        1.0f
    };

    float4 yellowColor =
    {
        1.0f,
        0.8f,
        0.2f,
        1.0f
    };

    float4 orangeColor =
    {
        1.0f,
        0.3f,
        0.0f,
        0.0f
    };

    float4 currentColor =
        whiteColor;

    if (lifeRate < 0.28f)
    {
        currentColor =
            lerp(
                whiteColor,
                yellowColor,
                lifeRate / 0.28f);
    }
    else
    {
        currentColor =
            lerp(
                yellowColor,
                orangeColor,
                (lifeRate - 0.28f) / 0.72f);
    }

    if (isCenterFlash)
    {
        float flashScale =
            lerp(
                3.6f,
                0.2f,
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
        float sparkScale =
            lerp(
                gEffectSettings.startScale,
                gEffectSettings.endScale,
                lifeRate);

        gParticles[particleIndex].scale =
            float32_t3(
                sparkScale,
                sparkScale,
                sparkScale);

        currentColor.a =
            inverseLifeRate;
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
