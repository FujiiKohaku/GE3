#include "../../Common/Particle.hlsli"
#include "ParticleField.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
ConstantBuffer<ParticleFieldCollection> gFieldCollection : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

static const uint32_t kMaxParticles = 1024;

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t particleIndex = dispatchThreadId.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    ParticleCS particle = gParticles[particleIndex];
    if (particle.lifeTime <= 0.0f || particle.currentTime >= particle.lifeTime)
    {
        return;
    }

    float32_t3 fieldForce = CalculateParticleFieldForce(
        particle.translate,
        gFieldCollection);
    particle.velocity += fieldForce * gPerFrame.deltaTime;
    gParticles[particleIndex] = particle;
}
