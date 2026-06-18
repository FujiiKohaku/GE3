struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 color : COLOR0;
};

struct VertexShaderInput
{
    float32_t4 position : POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding;
    float32_t4x4 uvTransform;
    float32_t alphaReference;
    float32_t3 padding2;
};

struct ParticleCS
{
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
    float32_t rotation;
    float32_t rotationSpeed;
    float32_t2 padding;
};

struct PerView
{
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};
struct EmitterSphere
{
    float32_t3 translate;
    float radius;
    uint32_t count;
    float frequency;
    float frequencyTime;
    uint32_t emit;
};
struct PerFrame
{
    float32_t time;
    float32_t deltaTime;
};

struct EffectSettings
{
    float32_t4 startColor;
    float32_t4 endColor;
    float32_t3 velocity;
    float32_t lifeTime;
    float32_t startScale;
    float32_t endScale;
    float32_t startRotation;
    float32_t rotationSpeed;
    int32_t emitterShape;
    int32_t enableGravity;
    int32_t enableDrag;
    int32_t enableNoise;
    int32_t enableAttraction;
    float32_t gravity;
    float32_t drag;
    float32_t noiseStrength;
    float32_t attractionStrength;
    float32_t3 padding;
};

float32_t3 MakeEmitterOffset(int32_t shape, float32_t3 random, float32_t radius)
{
    float32_t3 centered = (random - 0.5f) * 2.0f;

    if (shape == 1)
    {
        return centered * radius;
    }

    if (shape == 2)
    {
        float height = abs(centered.y);
        float coneRadius = (1.0f - saturate(height)) * radius;
        return float32_t3(centered.x * coneRadius, height * radius, centered.z * coneRadius);
    }

    if (shape == 3)
    {
        float32_t2 xz = centered.xz;
        float xzLength = max(length(xz), 0.0001f);
        xz = xz / xzLength;
        return float32_t3(xz.x * radius, centered.y * radius, xz.y * radius);
    }

    if (shape == 4)
    {
        float32_t2 xz = centered.xz;
        float xzLength = max(length(xz), 0.0001f);
        xz = xz / xzLength;
        return float32_t3(xz.x * radius, 0.0f, xz.y * radius);
    }

    float sphereLength = max(length(centered), 0.0001f);
    return centered / sphereLength * radius;
}

float32_t3 MakeNoise(uint32_t particleIndex, float32_t time)
{
    float32_t3 seed = float32_t3(
        particleIndex * 12.9898f,
        particleIndex * 78.233f + time,
        particleIndex * 37.719f - time);
    return frac(sin(seed) * 43758.5453f) * 2.0f - 1.0f;
}
