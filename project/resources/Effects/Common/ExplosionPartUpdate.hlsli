#include "../../Shaders/Common/Particle.hlsli"
#include "ExplosionPartSettings.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

[numthreads(256, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchId.x;
    if (particleIndex >= gEmitter.maxParticles) { return; }
    ParticleCS particle = gParticles[particleIndex];
    if (particle.color.a <= 0.0f) { return; }
    float previousTimeSeconds = particle.currentTime;
    float playbackDeltaSeconds = gPerFrame.deltaTime * kExplosionPlaybackSpeed;
    particle.currentTime += playbackDeltaSeconds;
    if (particle.currentTime < 0.0f) {
        particle.scale = 0.0f;
        gParticles[particleIndex] = particle;
        return;
    }
    float activeDeltaSeconds = playbackDeltaSeconds;
    if (previousTimeSeconds < 0.0f) {
        activeDeltaSeconds = particle.currentTime;
    }
    if (gEffectSettings.enableGravity != 0) {
        particle.velocity.y += gEffectSettings.gravity * kExplosionSizeScale * activeDeltaSeconds;
    }
    if (gEffectSettings.enableDrag != 0) {
        particle.velocity *= pow(max(gEffectSettings.drag, 0.0f), activeDeltaSeconds * 60.0f);
    }
    particle.translate += particle.velocity * activeDeltaSeconds;
    particle.rotation += particle.rotationSpeed * activeDeltaSeconds;
    float lifeRatio = saturate(particle.currentTime / particle.lifeTime);
    float remainingGrowth = 1.0f - lifeRatio;
    float growthRatio = 1.0f - remainingGrowth * remainingGrowth * remainingGrowth;
#if EXPLOSION_PART_KIND == 2
    growthRatio = lifeRatio;
#elif EXPLOSION_PART_KIND == 3
    growthRatio = sqrt(lifeRatio);
#endif
    float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, growthRatio) *
        particle.padding.x * kExplosionSizeScale;
#if EXPLOSION_PART_KIND == 1
    // 広がりの後半では炎片自体を小さくし、大きな球の集合に見せない。
    scale *= lerp(1.0f, 0.35f, smoothstep(0.45f, 1.0f, lifeRatio));
#endif
    particle.scale = float3(scale, scale, scale);
    float fadeStart = 0.50f;
#if EXPLOSION_PART_KIND == 3
    fadeStart = 0.20f;
#endif
    particle.color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, lifeRatio);
    particle.color.a = gEffectSettings.startColor.a * (1.0f - smoothstep(fadeStart, 1.0f, lifeRatio));
#if EXPLOSION_PART_KIND == 3
    particle.color.a *= smoothstep(0.0f, 0.15f, lifeRatio);
    particle.color.a = max(particle.color.a, 0.00001f);
#endif
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
