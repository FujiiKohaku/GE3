#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);
static const int32_t kMaxGPUParticle = 1024;

float Hash(float value, float seed)
{
    return frac(sin(value * 91.3458f + seed * 37.719f + 17.123f) * 47453.5453f);
}

float2 MainPoint(float segment, float seed)
{
    float lateral = (Hash(segment, seed) - 0.5f) * 0.78f;
    return float2(lateral, segment * 0.18f);
}

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || id.x >= gEmitter.count) return;

    int32_t freeIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
    if (freeIndex < 0 || freeIndex >= kMaxGPUParticle)
    {
        InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
        return;
    }

    uint particleIndex = gFreeList[freeIndex];
    float index = (float)id.x;
    float randomSeed = gPerFrame.time * 29.73f;
    float2 startPoint;
    float2 endPoint;

    if (id.x < 30)
    {
        startPoint = MainPoint(index, randomSeed);
        endPoint = MainPoint(index + 1.0f, randomSeed);
    }
    else
    {
        float branch = floor((index - 30.0f) / 6.0f);
        float segment = fmod(index - 30.0f, 6.0f);
        float rootSegment = 7.0f + branch * 8.0f;
        float2 root = MainPoint(rootSegment, randomSeed);
        float direction = branch == 1.0f ? -1.0f : 1.0f;
        float2 branchDirection = normalize(float2(
            direction * (0.75f + branch * 0.12f),
            0.48f + branch * 0.08f));
        float2 side = float2(-branchDirection.y, branchDirection.x);
        startPoint =
            root +
            branchDirection * segment * 0.19f +
            side * (Hash(index, randomSeed) - 0.5f) * 0.28f;
        endPoint =
            root +
            branchDirection * (segment + 1.0f) * 0.19f +
            side * (Hash(index + 1.0f, randomSeed) - 0.5f) * 0.28f;
    }

    float boltScale = max(gEffectSettings.startScale / 0.06f, 1.0f);
    startPoint *= boltScale;
    endPoint *= boltScale;
    float2 delta = endPoint - startPoint;
    float segmentLength = length(delta);
    float2 midpoint = (startPoint + endPoint) * 0.5f;
    gParticles[particleIndex].translate =
        gEmitter.translate + float3(midpoint.x, midpoint.y, 0.0f);
    float revealDelay;
    if (id.x < 30)
    {
        revealDelay = (29.0f - index) / 29.0f * 0.32f;
    }
    else
    {
        float branchSegment = fmod(index - 30.0f, 6.0f);
        revealDelay = 0.08f + branchSegment / 5.0f * 0.18f;
    }
    gParticles[particleIndex].velocity =
        float3(revealDelay, 0.0f, 0.0f);
    gParticles[particleIndex].scale =
        float3(
            segmentLength * 1.32f,
            (id.x < 30 ? 0.105f : 0.065f) * boltScale,
            1.0f);
    gParticles[particleIndex].lifeTime = gEffectSettings.lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = atan2(delta.y, delta.x);
    gParticles[particleIndex].rotationSpeed =
        Hash(index + 31.0f, randomSeed) * 6.28318f;
}
