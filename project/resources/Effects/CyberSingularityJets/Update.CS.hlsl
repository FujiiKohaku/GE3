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

    float axialDirection = -1.0f;
    if (particle.velocity.y >= 0.0f)
    {
        axialDirection = 1.0f;
    }
    particle.velocity.y +=
        axialDirection * 5.5f * gPerFrame.deltaTime;
    particle.velocity.xz +=
        MakeNoise(particleIndex, gPerFrame.time).xz *
        gEffectSettings.noiseStrength *
        gPerFrame.deltaTime;
    particle.velocity *=
        pow(max(gEffectSettings.drag, 0.0f), gPerFrame.deltaTime * 60.0f);
    particle.translate += particle.velocity * gPerFrame.deltaTime;
    particle.currentTime += gPerFrame.deltaTime;

    float lifeRate =
        saturate(particle.currentTime / max(particle.lifeTime, 0.01f));
    float pulse =
        0.76f + sin(gPerFrame.time * 24.0f + particleIndex) * 0.24f;
    particle.color =
        lerp(
            gEffectSettings.startColor,
            gEffectSettings.endColor,
            lifeRate);
    particle.color.a *= (1.0f - lifeRate) * pulse;
    particle.scale.xz *=
        pow(0.52f, gPerFrame.deltaTime);
    particle.scale.y *=
        pow(1.35f, gPerFrame.deltaTime);
    gParticles[particleIndex] = particle;

    if (particle.currentTime >= particle.lifeTime ||
        particle.color.a <= 0.001f)
    {
        ReleaseParticle(particleIndex);
    }
}
