#ifndef PARTICLE_FIELD_HLSLI
#define PARTICLE_FIELD_HLSLI

static const int32_t kParticleFieldWind = 0;
static const int32_t kParticleFieldAttractor = 1;
static const int32_t kParticleFieldRepulsor = 2;
static const int32_t kParticleFieldVortex = 3;
static const uint32_t kMaxParticleFields = 16;

struct ParticleField
{
    float32_t3 position;
    float32_t radius;
    float32_t3 direction;
    float32_t strength;
    int32_t type;
    float32_t falloff;
    float32_t2 padding;
};

struct ParticleFieldCollection
{
    ParticleField fields[kMaxParticleFields];
    uint32_t fieldCount;
    float32_t3 padding;
};

float32_t3 GetSafeFieldDirection(float32_t3 direction)
{
    float32_t directionLength = length(direction);
    if (directionLength <= 0.0001f)
    {
        return float32_t3(0.0f, 1.0f, 0.0f);
    }
    return direction / directionLength;
}

float32_t3 CalculateParticleFieldForce(
    float32_t3 particlePosition,
    ParticleFieldCollection fieldCollection)
{
    float32_t3 totalForce = float32_t3(0.0f, 0.0f, 0.0f);
    uint32_t fieldCount = min(fieldCollection.fieldCount, kMaxParticleFields);
    for (uint32_t fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
    {
        ParticleField field = fieldCollection.fields[fieldIndex];
        float32_t3 toCenter = field.position - particlePosition;
        float32_t distanceFromCenter = length(toCenter);
        if (distanceFromCenter >= field.radius)
        {
            continue;
        }

        float32_t influence = saturate(1.0f - distanceFromCenter / field.radius);
        influence = pow(influence, max(field.falloff, 0.01f));
        float32_t3 forceDirection = float32_t3(0.0f, 0.0f, 0.0f);

        if (field.type == kParticleFieldWind)
        {
            forceDirection = GetSafeFieldDirection(field.direction);
        }
        else if (field.type == kParticleFieldAttractor)
        {
            if (distanceFromCenter > 0.0001f)
            {
                forceDirection = toCenter / distanceFromCenter;
            }
        }
        else if (field.type == kParticleFieldRepulsor)
        {
            if (distanceFromCenter > 0.0001f)
            {
                forceDirection = -toCenter / distanceFromCenter;
            }
        }
        else if (field.type == kParticleFieldVortex)
        {
            if (distanceFromCenter > 0.0001f)
            {
                float32_t3 axis = GetSafeFieldDirection(field.direction);
                float32_t3 radialDirection = -toCenter / distanceFromCenter;
                forceDirection = GetSafeFieldDirection(cross(axis, radialDirection));
            }
        }

        totalForce += forceDirection * field.strength * influence;
    }

    return totalForce;
}

#endif
