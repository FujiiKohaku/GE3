#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t particleIndex = dispatchThreadId.x;
    if (particleIndex >= gEmitter.maxParticles) { return; }
    ParticleCS particle = gParticles[particleIndex];
    if (particle.color.a <= 0.0f) { return; }

    particle.currentTime += gPerFrame.deltaTime;
    particle.translate += particle.velocity * gPerFrame.deltaTime;
    float lifeRate = saturate(particle.currentTime / particle.lifeTime);
    float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, lifeRate);
    particle.scale = float3(scale, scale, scale);
    particle.color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, lifeRate);
    particle.color.a *= 1.0f - lifeRate;

    if (particle.currentTime >= particle.lifeTime || particle.color.a <= 0.0f) {
        particle.scale = float3(0.0f, 0.0f, 0.0f);
        particle.color.a = 0.0f;
        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if (freeListIndex + 1 < int32_t(gEmitter.maxParticles)) {
            gFreeList[freeListIndex + 1] = particleIndex;
        } else {
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        }
    }
    gParticles[particleIndex] = particle;
}
