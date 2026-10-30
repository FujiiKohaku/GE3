// Shared directional wake height and derivatives for CPU sampling and the vertex shader.
#ifndef OCEAN_WAKE_VECTOR
#define OCEAN_WAKE_VECTOR float3
#define OCEAN_WAKE_MAKE(x, y, z) float3(x, y, z)
#define OCEAN_WAKE_EXP(x) exp(x)
#define OCEAN_WAKE_SQRT(x) sqrt(x)
#define OCEAN_WAKE_ABS(x) abs(x)
#define OCEAN_WAKE_CLAMP(x, a, b) clamp(x, a, b)
#define OCEAN_WAKE_INLINE
#endif

// Symmetric narrow crests with flanking troughs. Integral across each cross-section is zero.
OCEAN_WAKE_INLINE OCEAN_WAKE_VECTOR OceanWakeProfile(float dx, float dz, float age, float strength, float scale,
    float forwardX, float forwardZ)
{
    if (age < 0.0f || age >= 2.5f || strength < 0.005f) return OCEAN_WAKE_MAKE(0.0f, 0.0f, 0.0f);
    float side = (dx * forwardZ - dz * forwardX) / scale;
    float behind = -(dx * forwardX + dz * forwardZ) / scale;
    float center = 1.0f + age * 1.6f;
    float length = 2.6f + age * 0.6f;
    float width = 0.90f + age * 0.16f;
    float separation = 1.8f + age * 3.2f + behind * 0.42f;
    if (behind <= -0.5f || behind > center + 4.0f * length
        || OCEAN_WAKE_ABS(side) > separation + 4.0f * width) return OCEAN_WAKE_MAKE(0.0f, 0.0f, 0.0f);
    float startup = OCEAN_WAKE_CLAMP(age / 0.08f, 0.0f, 1.0f);
    startup = startup * startup * (3.0f - 2.0f * startup);
    float fade = 1.0f - age / 2.5f;
    float amplitude = 0.64f * strength * fade * fade * startup;
    // Fade in behind the source; a traveling longitudinal envelope keeps each stamp local.
    float front = OCEAN_WAKE_CLAMP((behind + 0.5f) / 1.5f, 0.0f, 1.0f);
    float mask = front * front * (3.0f - 2.0f * front);
    float maskDerivative = 4.0f * front * (1.0f - front);
    float along = (behind - center) / length;
    float envelope = OCEAN_WAKE_EXP(-along * along);
    float envelopeDerivative = envelope * (maskDerivative - mask * 2.0f * along / length);
    envelope *= mask;
    float right = (side - separation) / width;
    float left = (side + separation) / width;
    float rightGaussian = OCEAN_WAKE_EXP(-right * right);
    float leftGaussian = OCEAN_WAKE_EXP(-left * left);
    float rightHeight = (1.0f - 2.0f * right * right) * rightGaussian;
    float leftHeight = (1.0f - 2.0f * left * left) * leftGaussian;
    float rightSlope = (4.0f * right * right * right - 6.0f * right) * rightGaussian;
    float leftSlope = (4.0f * left * left * left - 6.0f * left) * leftGaussian;
    float height = amplitude * envelope * (rightHeight + leftHeight);
    float sideSlope = amplitude * envelope * (rightSlope + leftSlope) / width;
    float behindSlope = amplitude * (envelopeDerivative * (rightHeight + leftHeight)
        + envelope * 0.42f * (leftSlope - rightSlope) / width);
    return OCEAN_WAKE_MAKE((sideSlope * forwardZ - behindSlope * forwardX) / scale, height,
        (-sideSlope * forwardX - behindSlope * forwardZ) / scale);
}

// Expanding impact ring, shared by CPU height sampling and VS normals.
OCEAN_WAKE_INLINE OCEAN_WAKE_VECTOR OceanRadialWakeProfile(float dx, float dz, float age, float strength, float scale)
{
    if (age < 0.0f || age >= 2.5f || strength < 0.005f || scale <= 0.0f) {
        return OCEAN_WAKE_MAKE(0.0f, 0.0f, 0.0f);
    }
    float distance = OCEAN_WAKE_SQRT(dx * dx + dz * dz + 0.01f);
    float radius = scale + age * 8.0f;
    float width = scale * 0.55f + age * 0.45f;
    float offset = (distance - radius) / width;
    if (OCEAN_WAKE_ABS(offset) > 4.0f) {
        return OCEAN_WAKE_MAKE(0.0f, 0.0f, 0.0f);
    }
    float startup = OCEAN_WAKE_CLAMP(age / 0.08f, 0.0f, 1.0f);
    startup = startup * startup * (3.0f - 2.0f * startup);
    float fade = 1.0f - age / 2.5f;
    float amplitude = strength * startup * fade * fade;
    float gaussian = OCEAN_WAKE_EXP(-offset * offset);
    float height = amplitude * (1.0f - 2.0f * offset * offset) * gaussian;
    float slope = amplitude * (4.0f * offset * offset * offset - 6.0f * offset) * gaussian / width;
    float inverseDistance = 1.0f / distance;
    return OCEAN_WAKE_MAKE(slope * dx * inverseDistance, height, slope * dz * inverseDistance);
}

OCEAN_WAKE_INLINE OCEAN_WAKE_VECTOR EvaluateOceanWakeProfile(float dx, float dz, float age, float strength,
    float scale, float forwardX, float forwardZ, float waveKind)
{
    if (waveKind > 0.5f) {
        float reveal = 1.0f;
        if (forwardX > 0.0f) {
            reveal = OCEAN_WAKE_CLAMP((age - forwardX) / 0.08f, 0.0f, 1.0f);
            reveal = reveal * reveal * (3.0f - 2.0f * reveal);
        }
        return OceanRadialWakeProfile(dx, dz, age, strength * reveal, scale);
    }
    return OceanWakeProfile(dx, dz, age, strength, scale, forwardX, forwardZ);
}

// Soft saturation bounds overlapping impulses without clipping their normals.
OCEAN_WAKE_INLINE OCEAN_WAKE_VECTOR LimitOceanWake(OCEAN_WAKE_VECTOR wave)
{
    float factor = 1.90f / (1.90f + OCEAN_WAKE_ABS(wave.y));
    return OCEAN_WAKE_MAKE(wave.x * factor * factor, wave.y * factor, wave.z * factor * factor);
}

// Keep the whole ocean covered, concentrating existing vertices around the aircraft.
OCEAN_WAKE_INLINE float OceanFocusedCoordinate(float t, float minimum, float maximum, float focus)
{
    float extent = maximum - minimum;
    float radius = extent < 128.0f ? extent * 0.25f : 32.0f;
    focus = OCEAN_WAKE_CLAMP(focus, minimum + radius, maximum - radius);
    if (t < 0.25f) return minimum + (focus - radius - minimum) * (t * 4.0f);
    if (t <= 0.75f) return focus - radius + (t - 0.25f) * (radius * 4.0f);
    return focus + radius + (maximum - focus - radius) * ((t - 0.75f) * 4.0f);
}

#undef OCEAN_WAKE_VECTOR
#undef OCEAN_WAKE_MAKE
#undef OCEAN_WAKE_EXP
#undef OCEAN_WAKE_SQRT
#undef OCEAN_WAKE_ABS
#undef OCEAN_WAKE_CLAMP
#undef OCEAN_WAKE_INLINE
