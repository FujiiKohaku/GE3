#include "../../Shaders/Common/Particle.hlsli"
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    if (i >= gEmitter.maxParticles || gParticles[i].color.a <= 0) return;
    bool mist = gParticles[i].rotationSpeed > 0;
    float dt = gPerFrame.deltaTime;
    gParticles[i].velocity.y -= (mist ? 2.0f : 9.8f) * dt;
    gParticles[i].translate += gParticles[i].velocity * dt;
    float oldTime = gParticles[i].currentTime;
    float life = gParticles[i].lifeTime;
    gParticles[i].currentTime += dt;
    float age = saturate(gParticles[i].currentTime / life);
    float size = mist ? lerp(0.5f, 1.6f, age) : lerp(0.12f, 0.04f, age);
    gParticles[i].scale = size;
    // Preserve the per-burst intensity while fading each particle.
    gParticles[i].color.a *= max(0.0f, life - gParticles[i].currentTime) / max(0.0001f, life - oldTime);
    if (gParticles[i].currentTime >= life || gParticles[i].translate.y < gEmitter.translate.y - 0.15f) {
        gParticles[i].color.a = 0;
        gParticles[i].scale = 0;
        int freeIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
        if (freeIndex + 1 < (int)gEmitter.maxParticles) gFreeList[freeIndex + 1] = i;
        else InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
    }
}
