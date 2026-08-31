#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

static const uint32_t kMaxGPUParticle = 1024;

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex >= kMaxGPUParticle || gParticles[particleIndex].color.a == 0.0f)
    {
        return;
    }

    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;
    gParticles[particleIndex].rotation += gParticles[particleIndex].rotationSpeed * gPerFrame.deltaTime;

    float lifeRate = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);

    // 【EaseOutCubic イージング】スーッと優雅にゆっくり広がって停止する展開演出
    float easeRate = 1.0f - pow(1.0f - lifeRate, 3.0f);
    float currentScale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, easeRate);
    gParticles[particleIndex].scale = float32_t3(currentScale, currentScale, currentScale);

    float alpha = 1.0f - pow(lifeRate, 2.0f);
    gParticles[particleIndex].color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, lifeRate);
    gParticles[particleIndex].color.a *= alpha;

    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].color.a <= 0.001f)
    {
        gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);
        gParticles[particleIndex].color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);

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
