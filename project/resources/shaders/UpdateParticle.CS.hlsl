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

    float alpha =
        1.0f - (gParticles[index].currentTime / gParticles[index].lifeTime);

    gParticles[index].color.a = saturate(alpha);
}