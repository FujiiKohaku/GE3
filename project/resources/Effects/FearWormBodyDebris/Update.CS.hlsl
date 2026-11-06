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
    float dt = gPerFrame.deltaTime;
    gParticles[index].velocity.y += gEffectSettings.gravity * dt;
    gParticles[index].velocity *= pow(max(gEffectSettings.drag, 0.0f), dt * 60.0f);
    gParticles[index].translate += gParticles[index].velocity * dt;
    gParticles[index].rotation += gParticles[index].rotationSpeed * dt;
    gParticles[index].currentTime += dt;
    float remaining = 1.0f - saturate(gParticles[index].currentTime / gParticles[index].lifeTime);
    gParticles[index].color.a = saturate(remaining * 4.0f);
    if (remaining <= 0.0f) {
        gParticles[index].scale = float3(0.0f, 0.0f, 0.0f);
        gParticles[index].color.a = 0.0f;
        int32_t freeIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
        if (freeIndex + 1 < int32_t(gEmitter.maxParticles)) gFreeList[freeIndex + 1] = index;
        else InterlockedAdd(gFreeListIndex[0], -1);
    }
}
