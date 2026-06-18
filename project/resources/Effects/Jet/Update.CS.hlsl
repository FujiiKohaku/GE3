#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);

static const uint32_t kMaxGPUParticle = 1024;

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex >= kMaxGPUParticle || gParticles[particleIndex].color.a == 0.0f)
    {
        return;
    }

    gParticles[particleIndex].translate += gParticles[particleIndex].velocity * gPerFrame.deltaTime;
    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

    float lifeRate = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
    gParticles[particleIndex].scale *= 1.0f + gPerFrame.deltaTime * 0.8f;
    gParticles[particleIndex].color.rgb = lerp(float3(0.8f, 0.95f, 1.0f), float3(0.1f, 0.25f, 0.8f), lifeRate);
    gParticles[particleIndex].color.a = saturate(1.0f - lifeRate);

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
