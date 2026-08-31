#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

void ReleaseParticle(uint32_t particleIndex)
{
    gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);
    gParticles[particleIndex].color.a = 0.0f;

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
    if ((freeListIndex + 1) < int32_t(gEmitter.maxParticles))
    {
        gFreeList[freeListIndex + 1] = particleIndex;
    }
    else
    {
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    }
}

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t particleIndex = dispatchThreadId.x;
    if (particleIndex >= gEmitter.maxParticles)
    {
        return;
    }

    ParticleCS particle = gParticles[particleIndex];
    if (particle.color.a <= 0.0f)
    {
        return;
    }

    float32_t3 toCenter = gEmitter.translate - particle.translate;
    float distanceToCenter = length(toCenter);
    float32_t3 inwardDirection = float32_t3(0.0f, 0.0f, 0.0f);
    if (distanceToCenter > 0.0001f)
    {
        inwardDirection = toCenter / distanceToCenter;
    }

    float gravityRamp =
        1.0f - saturate(distanceToCenter / max(gEmitter.radius, 0.001f));
    float gravityStrength = lerp(15.0f, 42.0f, gravityRamp);
    particle.velocity +=
        inwardDirection *
        gravityStrength *
        gPerFrame.deltaTime;

    if (gEffectSettings.enableDrag != 0)
    {
        particle.velocity *=
            pow(
                max(gEffectSettings.drag, 0.0f),
                gPerFrame.deltaTime * 60.0f);
    }

    if (gEffectSettings.enableNoise != 0)
    {
        particle.velocity +=
            MakeNoise(particleIndex, gPerFrame.time) *
            gEffectSettings.noiseStrength *
            gPerFrame.deltaTime;
    }

    particle.translate += particle.velocity * gPerFrame.deltaTime;
    particle.currentTime += gPerFrame.deltaTime;
    particle.rotation +=
        particle.rotationSpeed *
        gPerFrame.deltaTime;

    float newDistanceToCenter =
        length(gEmitter.translate - particle.translate);
    float lifeRate =
        saturate(particle.currentTime / max(particle.lifeTime, 0.01f));
    float absorptionScale =
        smoothstep(0.12f, 1.15f, newDistanceToCenter);
    float baseScale =
        lerp(
            gEffectSettings.startScale,
            gEffectSettings.endScale,
            lifeRate) *
        absorptionScale;
    float proximity =
        1.0f -
        saturate(
            newDistanceToCenter /
            max(gEmitter.radius, 0.001f));
    float stretch = lerp(1.4f, 8.5f, proximity * proximity);
    particle.scale = float32_t3(
        baseScale * 0.30f,
        baseScale * stretch,
        baseScale * 0.20f);

    float redShift =
        smoothstep(0.20f, 0.88f, proximity);
    float32_t4 outerColor =
        float32_t4(0.08f, 0.58f, 1.0f, 1.0f);
    float32_t4 innerColor =
        float32_t4(1.0f, 0.035f, 0.008f, 1.0f);
    particle.color = lerp(outerColor, innerColor, redShift);
    float hotBand =
        1.0f -
        smoothstep(
            0.0f,
            0.14f,
            abs(proximity - 0.72f));
    particle.color.rgb +=
        float32_t3(1.0f, 0.32f, 0.02f) * hotBand * 0.75f;
    particle.color.a *=
        (1.0f - lifeRate) *
        smoothstep(0.10f, 0.80f, newDistanceToCenter);

    gParticles[particleIndex] = particle;

    if (newDistanceToCenter <= 0.12f ||
        particle.currentTime >= particle.lifeTime ||
        particle.color.a <= 0.001f)
    {
        ReleaseParticle(particleIndex);
    }
}
