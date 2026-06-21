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

    if (particleIndex >= kMaxGPUParticle || gParticles[particleIndex].color.a <= 0.0f)
    {
        return;
    }

    if (gEffectSettings.enableNoise != 0)
    {
        gParticles[particleIndex].velocity +=
            MakeNoise(particleIndex, gPerFrame.time) *
            gEffectSettings.noiseStrength *
            gPerFrame.deltaTime;
    }

    gParticles[particleIndex].translate += gParticles[particleIndex].velocity * gPerFrame.deltaTime;
    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

    float lifeRate = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
    float beamPulse = 0.85f + abs(sin(gPerFrame.time * 80.0f + (float) particleIndex)) * 0.35f;
    float radialScale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, lifeRate) * beamPulse;
    gParticles[particleIndex].scale.x = radialScale;
    gParticles[particleIndex].scale.y *= 1.0f + gPerFrame.deltaTime * 3.0f;
    gParticles[particleIndex].scale.z = radialScale;

    float4 core = float4(0.9f, 1.0f, 1.0f, 1.0f);
    float4 blue = float4(0.2f, 0.75f, 1.0f, 0.8f);
    float4 purple = float4(0.5f, 0.12f, 1.0f, 0.0f);
    float4 color;

    if (lifeRate < 0.25f)
    {
        color = lerp(core, blue, lifeRate / 0.25f);
    }
    else
    {
        color = lerp(blue, purple, (lifeRate - 0.25f) / 0.75f);
    }

    color.a *= 1.0f - lifeRate;
    gParticles[particleIndex].color = color;

    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].color.a <= 0.0f)
    {
        ReleaseParticle(particleIndex);
    }
}
