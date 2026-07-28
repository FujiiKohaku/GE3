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

    float32_t3 toCenter =
        gEmitter.translate - particle.translate;
    float distanceToCenter = length(toCenter);
    float32_t3 inwardDirection = float32_t3(0.0f, 0.0f, 0.0f);
    if (distanceToCenter > 0.0001f)
    {
        inwardDirection = toCenter / distanceToCenter;
    }

    float proximity =
        1.0f -
        saturate(distanceToCenter / max(gEmitter.radius, 0.001f));
    particle.velocity +=
        inwardDirection *
        lerp(5.0f, 38.0f, proximity * proximity) *
        gPerFrame.deltaTime;
    particle.velocity *=
        pow(max(gEffectSettings.drag, 0.0f), gPerFrame.deltaTime * 60.0f);
    particle.velocity +=
        MakeNoise(particleIndex, gPerFrame.time) *
        gEffectSettings.noiseStrength *
        gPerFrame.deltaTime;
    particle.translate += particle.velocity * gPerFrame.deltaTime;
    particle.currentTime += gPerFrame.deltaTime;
    particle.rotation += particle.rotationSpeed * gPerFrame.deltaTime;

    float lifeRate =
        saturate(particle.currentTime / max(particle.lifeTime, 0.01f));
    float redShift = smoothstep(0.35f, 0.90f, proximity);
    particle.color =
        lerp(
            float32_t4(0.32f, 0.12f, 0.66f, 1.0f),
            float32_t4(1.0f, 0.045f, 0.005f, 1.0f),
            redShift);
    particle.color.a =
        (1.0f - lifeRate) *
        smoothstep(0.10f, 0.75f, distanceToCenter);

    float tidalStretch = lerp(1.0f, 3.2f, proximity * proximity);
    particle.scale.z *=
        pow(tidalStretch, gPerFrame.deltaTime * 1.5f);
    particle.scale.xy *=
        pow(0.78f, proximity * gPerFrame.deltaTime);
    gParticles[particleIndex] = particle;

    if (distanceToCenter <= 0.11f ||
        particle.currentTime >= particle.lifeTime ||
        particle.color.a <= 0.001f)
    {
        ReleaseParticle(particleIndex);
    }
}
