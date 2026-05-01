#include "Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
ConstantBuffer<PerFrame> gPerFrame : register(b0);

static const uint32_t kMaxGPUParticle = 1024;

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t index = DTid.x;

    if (index >= kMaxGPUParticle)
    {
        return;
    }

    if (gParticles[index].color.a == 0.0f)
    {
        return;
    }

    gParticles[index].translate +=
        gParticles[index].velocity * gPerFrame.deltaTime;

    gParticles[index].currentTime += gPerFrame.deltaTime;

    float lifeRate =
        gParticles[index].currentTime / gParticles[index].lifeTime;

    lifeRate = saturate(lifeRate);

    float3 startColor = float3(1.0f, 1.0f, 0.8f);
    float3 middleColor = float3(1.0f, 0.45f, 0.0f);
    float3 endColor = float3(0.15f, 0.03f, 0.0f);

    float3 currentColor;

    if (lifeRate < 0.25f)
    {
        float t = lifeRate / 0.25f;
        currentColor = lerp(startColor, middleColor, t);
    }
    else
    {
        float t = (lifeRate - 0.25f) / 0.75f;
        currentColor = lerp(middleColor, endColor, t);
    }

    gParticles[index].color.rgb = currentColor;

    float alpha = 1.0f - lifeRate;
    gParticles[index].color.a = saturate(alpha);
}