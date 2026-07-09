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

    if (gEffectSettings.enableGravity != 0)
    {
        gParticles[particleIndex].velocity.y +=
            gEffectSettings.gravity *
            gPerFrame.deltaTime;
    }

    if (gEffectSettings.enableDrag != 0)
    {
        gParticles[particleIndex].velocity *=
            pow(
                max(gEffectSettings.drag, 0.0f),
                gPerFrame.deltaTime * 60.0f);
    }

    if (gEffectSettings.enableNoise != 0)
    {
        gParticles[particleIndex].velocity +=
            MakeNoise(
                particleIndex,
                gPerFrame.time) *
            gEffectSettings.noiseStrength *
            gPerFrame.deltaTime;
    }

    if (gEffectSettings.enableAttraction != 0)
    {
        float32_t3 toEmitter =
            gEmitter.translate -
            gParticles[particleIndex].translate;

        float distanceToEmitter =
            length(toEmitter);

        if (distanceToEmitter > 0.0001f)
        {
            gParticles[particleIndex].velocity +=
                toEmitter /
                distanceToEmitter *
                gEffectSettings.attractionStrength *
                gPerFrame.deltaTime;
        }
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

    float scale =
        lerp(
            gEffectSettings.startScale,
            gEffectSettings.endScale,
            lifeRate);

    gParticles[particleIndex].scale =
        float32_t3(
            scale,
            scale,
            scale);

    // Effect.jsonで設定したStartColorとEndColorを使って補間する
    gParticles[particleIndex].color =
        lerp(
            gEffectSettings.startColor,
            gEffectSettings.endColor,
            lifeRate);

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
