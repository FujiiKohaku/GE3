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

    if (gEffectSettings.enableDrag != 0)
    {
        gParticles[particleIndex].velocity *=
            pow(max(gEffectSettings.drag, 0.0f), gPerFrame.deltaTime * 60.0f);
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
    gParticles[particleIndex].rotation += gParticles[particleIndex].rotationSpeed * gPerFrame.deltaTime;

    float lifeRate = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
    float flash = abs(sin(gPerFrame.time * 90.0f + (float) particleIndex));
    float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, lifeRate) * (0.85f + flash * 0.35f);
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);

    float4 core = float4(0.95f, 1.0f, 1.0f, 1.0f);
    float4 electric = float4(0.35f, 0.75f, 1.0f, 0.85f);
    float4 purple = float4(0.55f, 0.12f, 1.0f, 0.0f);
    float4 color;

    if (lifeRate < 0.35f)
    {
        color = lerp(core, electric, lifeRate / 0.35f);
    }
    else
    {
        color = lerp(electric, purple, (lifeRate - 0.35f) / 0.65f);
    }

    color.a *= 1.0f - lifeRate;
    gParticles[particleIndex].color = color;

    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].color.a <= 0.0f)
    {
        ReleaseParticle(particleIndex);
    }
}
