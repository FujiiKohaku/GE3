#include "Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);

static const uint32_t kMaxGPUParticle = 1024;

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex >= kMaxGPUParticle)
    {
        return;
    }

    if (gParticles[particleIndex].color.a == 0.0f)
    {
        return;
    }

    gParticles[particleIndex].translate +=
        gParticles[particleIndex].velocity * gPerFrame.deltaTime;

    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

    float lifeRate =
        gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime;

    lifeRate = saturate(lifeRate);

    float3 startColor = float3(1.0f, 1.0f, 0.8f);
    float3 middleColor = float3(1.0f, 0.45f, 0.0f);
    float3 endColor = float3(0.15f, 0.03f, 0.0f);

    float3 currentColor;

    if (lifeRate < 0.25f)
    {
        float t = lifeRate / 0.25f;
        currentColor = lerp(startColor, middleColor, t);
    }
    else
    {
        float t = (lifeRate - 0.25f) / 0.75f;
        currentColor = lerp(middleColor, endColor, t);
    }

    gParticles[particleIndex].color.rgb = currentColor;

    float alpha = 1.0f - lifeRate;
    gParticles[particleIndex].color.a = saturate(alpha);

    if (gParticles[particleIndex].color.a == 0.0f)
    {
        gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);

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
}