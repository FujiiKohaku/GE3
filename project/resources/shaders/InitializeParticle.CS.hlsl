struct Particle
{
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};

static const uint kMaxParticles = 1024;

RWStructuredBuffer<Particle> gParticles : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;

    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    gParticles[particleIndex].translate = float3(0.0f, 0.0f, 0.0f);
    gParticles[particleIndex].scale = float3(0.5f, 0.5f, 0.5f);
    gParticles[particleIndex].lifeTime = 0.0f;
    gParticles[particleIndex].velocity = float3(0.0f, 0.0f, 0.0f);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = float4(1.0f, 1.0f, 1.0f, 1.0f);
}