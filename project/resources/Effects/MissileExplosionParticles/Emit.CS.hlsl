#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || dispatchThreadId.x >= gEmitter.count) { return; }

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= int32_t(gEmitter.maxParticles)) {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    // A Fibonacci sphere keeps a small burst evenly spread over the surface.
    const float kGoldenAngle = 2.39996323f;
    float height = 1.0f - 2.0f * (float(dispatchThreadId.x) + 0.5f) / float(gEmitter.count);
    float angle = float(dispatchThreadId.x) * kGoldenAngle + gPerFrame.time * 17.0f;
    float horizontalRadius = sqrt(max(0.0f, 1.0f - height * height));
    float3 direction = float3(cos(angle) * horizontalRadius, height, sin(angle) * horizontalRadius);
    float speed = 8.0f + 4.0f * frac(float(dispatchThreadId.x) * 0.61803399f);

    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate + direction * gEmitter.radius;
    particle.velocity = direction * speed + gEffectSettings.velocity;
    particle.scale = float3(gEffectSettings.startScale, gEffectSettings.startScale, gEffectSettings.startScale);
    particle.lifeTime = max(gEffectSettings.lifeTime, 0.01f);
    particle.color = gEffectSettings.startColor;
    uint32_t particleIndex = gFreeList[freeListIndex];
    gParticles[particleIndex] = particle;
}
