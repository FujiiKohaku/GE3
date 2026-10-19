#include "../../Shaders/Common/Particle.hlsli"
ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || id.x >= gEmitter.count) return;
    int index;
    InterlockedAdd(gFreeListIndex[0], -1, index);
    if (index < 0 || index >= (int)gEmitter.maxParticles) { InterlockedAdd(gFreeListIndex[0], 1, index); return; }
    uint slot = gFreeList[index];
    float3 random = frac(sin(float3(id.x * 13.7f + gEmitter.translate.x,
        id.x * 31.3f + gEmitter.translate.z, id.x * 7.1f + gEmitter.translate.x * 0.7f)) * 43758.5453f);
    bool mist = (id.x % 3) == 0;
    float2 forward = gEmitter.direction.xz;
    forward = dot(forward, forward) > 0.001f ? normalize(forward) : float2(0, 1);
    float2 side = float2(forward.y, -forward.x);
    float strength = saturate(gEffectSettings.velocity.y);
    // Balanced fans emerge on both sides instead of disappearing behind the aircraft.
    float lateralSign = (id.x % 2) == 0 ? -1.0f : 1.0f;
    float lateralSpeed = lateralSign * (3.0f + random.x * 3.5f);
    float2 horizontal = side * lateralSpeed - forward * (0.3f + random.z * 0.7f);
    gParticles[slot].translate = gEmitter.translate + float3(side.x, 0, side.y) * lateralSign * (1.0f + random.x * 0.7f);
    float upwardSpeed = mist ? 5.0f + random.y * 2.0f : 3.0f + random.y * 2.5f;
    gParticles[slot].velocity = float3(horizontal.x, upwardSpeed, horizontal.y) * (0.5f + strength);
    float size = mist ? 0.5f : 0.12f;
    gParticles[slot].scale = size;
    gParticles[slot].lifeTime = mist ? 0.65f : 0.45f;
    gParticles[slot].currentTime = 0;
    gParticles[slot].color = float4(0.8f, 0.95f, 1.0f, strength * (mist ? 0.22f : 0.75f));
    gParticles[slot].rotation = random.z * 6.28f;
    gParticles[slot].rotationSpeed = mist ? 1.0f : -1.0f; // particle type marker
}
