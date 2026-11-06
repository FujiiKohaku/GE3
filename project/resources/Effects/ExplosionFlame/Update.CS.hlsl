#include "../../Shaders/Common/Particle.hlsli"
#include "../Common/ExplosionPartSettings.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

static const float kPi = 3.14159265f;

[numthreads(256, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchId.x;
    if (particleIndex >= gEmitter.maxParticles) { return; }
    ParticleCS particle = gParticles[particleIndex];
    if (particle.color.a <= 0.0f) { return; }
    particle.currentTime += gPerFrame.deltaTime * kExplosionPlaybackSpeed;
    float lifeRatio = saturate(particle.currentTime / particle.lifeTime);
    float expansion = 1.0f - pow(1.0f - lifeRatio, 3.0f);
    float ringRadius = lerp(gEmitter.radius, 3.2f, expansion) * kExplosionSizeScale;
    float angle = particle.padding.x + lifeRatio * 0.65f;
    float3 direction = float3(cos(angle), sin(angle), 0.0f);
    particle.translate = gEmitter.translate + direction * ringRadius;
    particle.translate.z += sin(angle * 3.0f + particle.padding.y * 4.0f) * ringRadius * 0.10f;
    particle.velocity = direction;
    particle.rotation = angle - kPi * 0.5f;
    float flameHeight = lerp(gEffectSettings.startScale, gEffectSettings.endScale, expansion)
        * kExplosionSizeScale * (1.0f - smoothstep(0.48f, 1.0f, lifeRatio) * 0.6f);
    particle.scale = float3(max(ringRadius * 0.80f, 0.15f), flameHeight, flameHeight);
    particle.color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, lifeRatio);
    particle.color.a = gEffectSettings.startColor.a * (1.0f - smoothstep(0.35f, 1.0f, lifeRatio));
    if (particle.currentTime >= particle.lifeTime) {
        particle.scale = 0.0f;
        particle.color.a = 0.0f;
        int freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if (freeListIndex + 1 < int(gEmitter.maxParticles)) {
            gFreeList[freeListIndex + 1] = particleIndex;
        } else {
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        }
    }
    gParticles[particleIndex] = particle;
}

