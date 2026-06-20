#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

static const uint32_t kMaxGPUParticle = 1024;

void ReleaseParticle(uint32_t particleIndex)
{
    gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);
    gParticles[particleIndex].color.a = 0.0f;

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

    if ((freeListIndex + 1) < kMaxGPUParticle)
    {
        gFreeList[freeListIndex + 1] = particleIndex;
    }
    else
    {
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    }
}

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex >= kMaxGPUParticle ||
        gParticles[particleIndex].color.a <= 0.0f)
    {
        return;
    }

    if (gEffectSettings.enableDrag != 0)
    {
        gParticles[particleIndex].velocity *=
            pow(max(gEffectSettings.drag, 0.0f), gPerFrame.deltaTime * 60.0f);
    }

    gParticles[particleIndex].translate +=
        gParticles[particleIndex].velocity * gPerFrame.deltaTime;
    gParticles[particleIndex].currentTime +=
        gPerFrame.deltaTime;

    float lifeRate = saturate(
        gParticles[particleIndex].currentTime /
        gParticles[particleIndex].lifeTime);
    float expandRate = 1.0f - pow(1.0f - lifeRate, 3.0f);
    float pressureFade = 1.0f - lifeRate;

    float speedFactor = saturate(length(gParticles[particleIndex].velocity) / 24.0f);
    float scaleFactor = lerp(0.8f, 1.25f, speedFactor);
    float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, expandRate) * scaleFactor;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);

    float4 whiteCore = float4(1.0f, 0.98f, 0.78f, 1.0f);
    float4 yellowCore = float4(1.0f, 0.72f, 0.14f, 1.0f);
    float4 orangeFire = float4(1.0f, 0.28f, 0.02f, 0.82f);
    float4 emberSmoke = float4(0.22f, 0.06f, 0.01f, 0.0f);
    float4 color;

    if (lifeRate < 0.18f)
    {
        color = lerp(whiteCore, yellowCore, lifeRate / 0.18f);
    }
    else if (lifeRate < 0.62f)
    {
        color = lerp(yellowCore, orangeFire, (lifeRate - 0.18f) / 0.44f);
    }
    else
    {
        color = lerp(orangeFire, emberSmoke, (lifeRate - 0.62f) / 0.38f);
    }

    color.a *= pressureFade * pressureFade;
    gParticles[particleIndex].color = color;

    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].color.a <= 0.0f)
    {
        ReleaseParticle(particleIndex);
    }
}
