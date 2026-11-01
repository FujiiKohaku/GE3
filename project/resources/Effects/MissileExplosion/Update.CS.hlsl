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
    if (particleIndex >= gEmitter.maxParticles || gParticles[particleIndex].color.a <= 0.0f) { return; }
    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;
    float lifeRate = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
    // The full damage volume flashes immediately. Only its color and opacity change.
    float radius = gEffectSettings.endScale;
    gParticles[particleIndex].scale = float32_t3(radius, radius, radius);
    gParticles[particleIndex].color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, lifeRate);
    if (lifeRate >= 1.0f || gParticles[particleIndex].color.a <= 0.0f) {
        gParticles[particleIndex].scale = 0.0f;
        gParticles[particleIndex].color.a = 0.0f;
        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if (freeListIndex + 1 < int32_t(gEmitter.maxParticles)) {
            gFreeList[freeListIndex + 1] = particleIndex;
        } else {
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        }
    }
}
