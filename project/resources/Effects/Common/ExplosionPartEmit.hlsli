#include "../../Shaders/Common/Particle.hlsli"
#include "ExplosionPartSettings.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const float kPi = 3.14159265f;

float RandomExplosionValue(float seed)
{
    return frac(sin(seed * 12.9898f) * 43758.5453f);
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || dispatchId.x >= gEmitter.count) { return; }
    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= int32_t(gEmitter.maxParticles)) {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }
    uint particleIndex = gFreeList[freeListIndex];
    float seed = float(dispatchId.x) * 31.73f + gPerFrame.time * 97.11f;
    float angle = (float(dispatchId.x) + RandomExplosionValue(seed) * 0.4f) *
        (2.0f * kPi / max(float(gEmitter.count), 1.0f));
    float3 direction = float3(cos(angle), sin(angle), RandomExplosionValue(seed + 4.7f) - 0.5f);
    direction = normalize(direction);
    float variation = lerp(0.80f, 1.20f, RandomExplosionValue(seed + 8.2f));
    float delaySeconds = 0.0f;
    float3 velocity = gEffectSettings.velocity;
    float3 position = gEmitter.translate;
#if EXPLOSION_PART_KIND == 1
    // 小さな炎片を中心付近から球状に放出し、粒子の移動で爆発を広げる。
    float verticalDirection = lerp(-0.90f, 0.90f, RandomExplosionValue(seed + 6.1f));
    float horizontalLength = sqrt(max(1.0f - verticalDirection * verticalDirection, 0.0f));
    direction = float3(cos(angle) * horizontalLength, verticalDirection, sin(angle) * horizontalLength);
    position += direction * gEmitter.radius * RandomExplosionValue(seed + 2.4f);
    velocity += direction * lerp(3.5f, 6.5f, RandomExplosionValue(seed + 3.2f));
#elif EXPLOSION_PART_KIND == 2
    velocity += direction * lerp(11.0f, 19.0f, RandomExplosionValue(seed + 3.2f));
#elif EXPLOSION_PART_KIND == 3
    position += direction * gEmitter.radius;
    velocity += direction * 0.8f;
    delaySeconds = 0.12f + float(dispatchId.x) * 0.04f;
#endif
    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate + (position - gEmitter.translate) * kExplosionSizeScale;
    particle.velocity = velocity * kExplosionSizeScale;
    particle.scale = 0.0f;
    particle.lifeTime = max(gEffectSettings.lifeTime * variation, 0.01f);
    particle.currentTime = -delaySeconds;
    particle.color = gEffectSettings.startColor;
    particle.rotation = RandomExplosionValue(seed + 12.1f) * 2.0f * kPi;
#if EXPLOSION_PART_KIND == 3
    // 煙を回転させすぎず、少し上へ流す。
    particle.rotation = (RandomExplosionValue(seed + 12.1f) - 0.5f) * 0.4f;
#endif
    particle.rotationSpeed = gEffectSettings.rotationSpeed;
    particle.padding.x = variation;
    particle.padding.y = RandomExplosionValue(seed + 19.3f);
    gParticles[particleIndex] = particle;
}
