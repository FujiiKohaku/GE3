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

    gParticles[particleIndex].translate +=
        gParticles[particleIndex].velocity * gPerFrame.deltaTime;
    gParticles[particleIndex].currentTime +=
        gPerFrame.deltaTime;

    float lifeRate = saturate(
        gParticles[particleIndex].currentTime /
        gParticles[particleIndex].lifeTime);
    float expandRate = 1.0f - pow(1.0f - lifeRate, 4.0f);
    float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, expandRate);
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);

    float alpha = pow(1.0f - lifeRate, 3.0f);
    gParticles[particleIndex].color = float4(1.0f, 0.98f, 0.86f, alpha);

    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].color.a <= 0.0f)
    {
        ReleaseParticle(particleIndex);
    }
}
