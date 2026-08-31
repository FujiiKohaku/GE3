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
    if (particleIndex >= kMaxGPUParticle || gParticles[particleIndex].color.a == 0.0f) return;

    gParticles[particleIndex].velocity.y += 0.32f * gPerFrame.deltaTime;
    gParticles[particleIndex].velocity.xz += float2(
        sin(gPerFrame.time * 3.1f + particleIndex * 0.73f),
        cos(gPerFrame.time * 2.7f + particleIndex * 1.17f)) * 0.12f * gPerFrame.deltaTime;
    gParticles[particleIndex].velocity *= pow(0.965f, gPerFrame.deltaTime * 60.0f);
    gParticles[particleIndex].translate += gParticles[particleIndex].velocity * gPerFrame.deltaTime;
    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

    const float lifeRate = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
    const float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, lifeRate);
    gParticles[particleIndex].scale = float3(scale, scale, scale);
    gParticles[particleIndex].color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, lifeRate);

    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime) {
        gParticles[particleIndex].scale = 0.0f;
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
