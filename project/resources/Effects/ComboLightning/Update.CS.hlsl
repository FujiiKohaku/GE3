#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);
static const uint kMaxGPUParticle = 1024;

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint particleIndex = id.x;
    if (particleIndex >= kMaxGPUParticle ||
        gParticles[particleIndex].color.a == 0.0f) return;

    ParticleCS particle = gParticles[particleIndex];
    particle.currentTime += gPerFrame.deltaTime;
    float lifeRate = saturate(particle.currentTime / particle.lifeTime);
    float flickerWave = sin(
        gPerFrame.time * 110.0f +
        particle.rotationSpeed);
    float flicker =
        lerp(0.18f, 1.0f, step(-0.15f, flickerWave)) *
        (0.88f + 0.12f * sin(gPerFrame.time * 47.0f));
    float revealProgress = saturate(
        (lifeRate - particle.velocity.x) / 0.14f);
    float revealEaseOut =
        1.0f - pow(1.0f - revealProgress, 3.0f);
    float fadeProgress = saturate(
        (lifeRate - 0.62f) / 0.38f);
    float fadeEaseIn = 1.0f - fadeProgress * fadeProgress;
    particle.color.rgb = lerp(
        gEffectSettings.startColor.rgb,
        gEffectSettings.endColor.rgb,
        lifeRate);
    particle.color.a =
        max(revealEaseOut * fadeEaseIn * flicker, 0.001f);
    particle.scale.y =
        max(particle.scale.y * (0.91f + flicker * 0.09f), 0.025f);
    gParticles[particleIndex] = particle;

    if (particle.currentTime >= particle.lifeTime)
    {
        gParticles[particleIndex].color.a = 0.0f;
        gParticles[particleIndex].scale = 0.0f;
        int32_t freeIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
        if ((freeIndex + 1) < kMaxGPUParticle)
        {
            gFreeList[freeIndex + 1] = particleIndex;
        }
        else
        {
            InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
        }
    }
}
