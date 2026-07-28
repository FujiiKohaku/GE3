#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 id : SV_DispatchThreadID)
{
    uint32_t index = id.x;
    if (index >= gEmitter.maxParticles || gParticles[index].color.a <= 0.0f) return;
    gParticles[index].currentTime += gPerFrame.deltaTime;
    float rate = saturate(gParticles[index].currentTime / gParticles[index].lifeTime);
    float eased = 1.0f - pow(1.0f - rate, 3.0f);
    float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, eased);
    gParticles[index].scale = scale.xxx;
    gParticles[index].rotation += gParticles[index].rotationSpeed * gPerFrame.deltaTime;
    gParticles[index].color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, rate);
    gParticles[index].color.a *=
        smoothstep(0.0f, 0.05f, rate) *
        (1.0f - smoothstep(0.90f, 1.0f, rate));
    if (gParticles[index].currentTime >= gParticles[index].lifeTime) {
        gParticles[index].scale = 0.0f;
        gParticles[index].color.a = 0.0f;
        int32_t freeIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
        if (freeIndex + 1 < gEmitter.maxParticles) gFreeList[freeIndex + 1] = index;
        else InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
    }
}
