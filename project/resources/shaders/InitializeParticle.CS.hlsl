struct Particle
{
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};

static const uint32_t kMaxParticles = 1024;

RWStructuredBuffer<Particle> gParticles : register(u0);

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex < kMaxParticles)
    {
        gParticles[particleIndex] = (Particle) 0;

        gParticles[particleIndex].translate = float32_t3(
            (float32_t) (particleIndex % 32) * 0.3f,
            0.0f,
            (float32_t) (particleIndex / 32) * 0.3f
        );

        gParticles[particleIndex].scale = float32_t3(0.2f, 0.2f, 0.2f);
        gParticles[particleIndex].color = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
        gParticles[particleIndex].lifeTime = 1.0f;
        gParticles[particleIndex].currentTime = 0.0f;
    }
}