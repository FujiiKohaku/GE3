#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

static const uint32_t kMaxGPUParticle = 1024;

[numthreads(256, 1, 1)]
void main(uint32_t3 id : SV_DispatchThreadID)
{
    const uint32_t particleIndex = id.x;
    if (particleIndex >= kMaxGPUParticle || gParticles[particleIndex].color.a <= 0.0f) return;

    const bool isCenterFlash = length(gParticles[particleIndex].velocity) < 0.01f;
    if (!isCenterFlash) {
        gParticles[particleIndex].velocity *=
            pow(max(gEffectSettings.drag, 0.01f), gPerFrame.deltaTime * 60.0f);
    }
    gParticles[particleIndex].translate +=
        gParticles[particleIndex].velocity * gPerFrame.deltaTime;
    gParticles[particleIndex].rotation +=
        gParticles[particleIndex].rotationSpeed * gPerFrame.deltaTime;
    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

    const float lifeRate = saturate(
        gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
    const float inverseLifeRate = 1.0f - lifeRate;
    const float purpleRate = saturate((lifeRate - 0.16f) / 0.84f);
    float4 color = lerp(
        float4(1.0f, 1.0f, 1.0f, 1.0f),
        float4(0.42f, 0.01f, 1.0f, 0.0f),
        purpleRate);

    float scale;
    if (isCenterFlash) {
        scale = lerp(
            gEffectSettings.startScale * 1.85f,
            gEffectSettings.startScale * 0.20f,
            lifeRate);
        color.a = inverseLifeRate * inverseLifeRate;
    } else {
        scale = lerp(
            gEffectSettings.startScale * 0.22f,
            gEffectSettings.startScale * 1.55f,
            saturate(lifeRate * 2.2f));
        color.a = inverseLifeRate * inverseLifeRate * 0.95f;
    }

    gParticles[particleIndex].scale = float3(scale, scale, scale);
    gParticles[particleIndex].color = color;
    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].color.a <= 0.0f) {
        gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
        gParticles[particleIndex].color.a = 0.0f;
        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if ((freeListIndex + 1) < kMaxGPUParticle) {
            gFreeList[freeListIndex + 1] = particleIndex;
        } else {
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        }
    }
}
