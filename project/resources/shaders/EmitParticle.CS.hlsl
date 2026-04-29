#include "Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

class RandomGenerator
{
    float32_t3 seed;

    float32_t3 Generate3d()
    {
        seed = frac(sin(seed * 12.9898f) * 43758.5453f);
        return seed;
    }

    float32_t Generate1d()
    {
        float32_t result = frac(sin(seed.x * 78.233f) * 43758.5453f);
        seed.x = result;
        return result;
    }
};

[numthreads(1, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit != 0)
    {
        RandomGenerator generator;
        generator.seed = ((float32_t3) DTid + gPerFrame.time) * gPerFrame.time;

        for (uint32_t countIndex = 0; countIndex < gEmitter.count; countIndex++)
        {
            float32_t3 randomTranslate = generator.Generate3d();
            randomTranslate = (randomTranslate - 0.5f) * 2.0f;

            gParticles[countIndex].scale = generator.Generate3d() * 0.3f;
            gParticles[countIndex].translate = gEmitter.translate + randomTranslate;
            gParticles[countIndex].velocity = float32_t3(0.0f, 0.0f, 0.0f);
            gParticles[countIndex].lifeTime = 1.0f;
            gParticles[countIndex].currentTime = 0.0f;
            gParticles[countIndex].color.rgb = generator.Generate3d();
            gParticles[countIndex].color.a = 1.0f;
        }
    }
}