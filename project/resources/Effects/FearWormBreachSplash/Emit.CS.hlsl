#include "../../Shaders/Common/Particle.hlsli"
ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || dispatchId.x >= gEmitter.count) { return; }
    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= int32_t(gEmitter.maxParticles)) {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }
    uint32_t particleIndex = gFreeList[freeListIndex];
    float seed = float(dispatchId.x) + gPerFrame.time * 7.53f;
    float3 randomValue = frac(sin(float3(seed, seed + 17.0f, seed + 39.0f) * 12.9898f) * 43758.5453f);
    float angle = randomValue.x * 6.283185307f;
    float radialSpeed = 4.0f + randomValue.y * 10.0f;
    float scale = gEffectSettings.startScale * (0.5f + randomValue.z * 0.5f);
    gParticles[particleIndex].translate = gEmitter.translate
        + MakeEmitterOffset(gEffectSettings.emitterShape, randomValue, gEmitter.radius);
    gParticles[particleIndex].velocity = float3(cos(angle) * radialSpeed,
        gEffectSettings.velocity.y * (0.55f + randomValue.z * 0.45f), sin(angle) * radialSpeed);
    gParticles[particleIndex].scale = float3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = gEffectSettings.lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = 0.0f;
    gParticles[particleIndex].rotationSpeed = 0.0f;
}
