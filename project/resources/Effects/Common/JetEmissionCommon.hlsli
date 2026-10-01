#ifndef EFFECT_JET_EMISSION_COMMON_HLSLI
#define EFFECT_JET_EMISSION_COMMON_HLSLI

float32_t3 GetJetExhaustAxis(float32_t3 emitterDirection)
{
    if (dot(emitterDirection, emitterDirection) < 0.0001f)
    {
        return float32_t3(0.0f, 0.0f, -1.0f);
    }
    return normalize(emitterDirection);
}

float32_t3 RotateJetOffset(float32_t3 localOffset, float32_t3 exhaustAxis)
{
    float32_t3 referenceUp = float32_t3(0.0f, 1.0f, 0.0f);
    if (abs(exhaustAxis.y) > 0.95f)
    {
        referenceUp = float32_t3(1.0f, 0.0f, 0.0f);
    }
    float32_t3 right = normalize(cross(exhaustAxis, referenceUp));
    float32_t3 up = normalize(cross(right, exhaustAxis));
    return right * localOffset.x + up * localOffset.y - exhaustAxis * localOffset.z;
}

float32_t3 MakeJetEmissionDirection(float32_t3 random, float32_t spread, float32_t3 exhaustAxis)
{
    return normalize(RotateJetOffset(
        float32_t3(random.x * spread, random.y * spread, -1.0f), exhaustAxis));
}

#endif
