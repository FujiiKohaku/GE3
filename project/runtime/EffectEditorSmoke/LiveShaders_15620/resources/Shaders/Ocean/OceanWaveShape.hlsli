// Shared scalar math keeps CPU water queries and GPU displacement in agreement.
// Noise returns value, X derivative and Z derivative.
OCEAN_WAVE_INLINE float OceanWaveHash(float x, float z)
{
    float value = x * 0.1031f + z * 0.11369f;
    value -= OCEAN_WAVE_FLOOR(value);
    value *= value + 33.33f;
    value *= value + value;
    return value - OCEAN_WAVE_FLOOR(value);
}

OCEAN_WAVE_INLINE OCEAN_WAVE_VECTOR OceanWaveNoise(float x, float z)
{
    float cellX = OCEAN_WAVE_FLOOR(x);
    float cellZ = OCEAN_WAVE_FLOOR(z);
    float localX = x - cellX;
    float localZ = z - cellZ;
    float blendX = localX * localX * (3.0f - 2.0f * localX);
    float blendZ = localZ * localZ * (3.0f - 2.0f * localZ);
    float slopeX = 6.0f * localX * (1.0f - localX);
    float slopeZ = 6.0f * localZ * (1.0f - localZ);
    float bottomLeft = OceanWaveHash(cellX, cellZ);
    float bottomRight = OceanWaveHash(cellX + 1.0f, cellZ);
    float topLeft = OceanWaveHash(cellX, cellZ + 1.0f);
    float topRight = OceanWaveHash(cellX + 1.0f, cellZ + 1.0f);
    float bottom = bottomLeft + (bottomRight - bottomLeft) * blendX;
    float top = topLeft + (topRight - topLeft) * blendX;
    float derivativeX = ((bottomRight - bottomLeft) * (1.0f - blendZ)
        + (topRight - topLeft) * blendZ) * slopeX;
    float derivativeZ = (top - bottom) * slopeZ;
    return OCEAN_WAVE_MAKE(bottom + (top - bottom) * blendZ, derivativeX, derivativeZ);
}

struct OceanWaveSample
{
    OCEAN_WAVE_VECTOR displacement;
    OCEAN_WAVE_VECTOR derivativeX;
    OCEAN_WAVE_VECTOR derivativeZ;
};

OCEAN_WAVE_INLINE OceanWaveSample EvaluateOceanWave(float x, float z, float time,
    float amplitude, float frequency, float directionX, float directionZ,
    float frequencyScale, float amplitudeScale, float speed, float steepness, float phaseOffset)
{
    float inverseLength = 1.0f / OCEAN_WAVE_SQRT(directionX * directionX + directionZ * directionZ);
    directionX *= inverseLength;
    directionZ *= inverseLength;
    float waveNumber = frequency * frequencyScale;
    // Broad envelopes differ per component and drift slowly through world space.
    float envelopeScale = waveNumber * 0.18f;
    float phaseScale = waveNumber * 0.12f;
    OCEAN_WAVE_VECTOR envelope = OceanWaveNoise(x * envelopeScale + phaseOffset * 13.7f - time * 0.018f,
        z * envelopeScale - phaseOffset * 9.3f + time * 0.011f);
    OCEAN_WAVE_VECTOR warp = OceanWaveNoise(x * phaseScale - phaseOffset * 7.1f + time * 0.009f,
        z * phaseScale + phaseOffset * 17.9f - time * 0.013f);
    float baseAmplitude = amplitude * amplitudeScale;
    float height = baseAmplitude * (0.40f + envelope.x * 0.95f);
    float heightX = baseAmplitude * 0.95f * envelope.y * envelopeScale;
    float heightZ = baseAmplitude * 0.95f * envelope.z * envelopeScale;
    float phase = (directionX * x + directionZ * z) * waveNumber
        + time * speed + phaseOffset + (warp.x - 0.5f) * 0.70f;
    float phaseX = directionX * waveNumber + warp.y * phaseScale * 0.70f;
    float phaseZ = directionZ * waveNumber + warp.z * phaseScale * 0.70f;
    float sinePhase = OCEAN_WAVE_SIN(phase);
    float cosinePhase = OCEAN_WAVE_COS(phase);
    float horizontalX = steepness * (heightX * cosinePhase - height * sinePhase * phaseX);
    float horizontalZ = steepness * (heightZ * cosinePhase - height * sinePhase * phaseZ);
    OceanWaveSample sample;
    sample.displacement = OCEAN_WAVE_MAKE(directionX * steepness * height * cosinePhase,
        height * sinePhase, directionZ * steepness * height * cosinePhase);
    sample.derivativeX = OCEAN_WAVE_MAKE(directionX * horizontalX,
        heightX * sinePhase + height * cosinePhase * phaseX, directionZ * horizontalX);
    sample.derivativeZ = OCEAN_WAVE_MAKE(directionX * horizontalZ,
        heightZ * sinePhase + height * cosinePhase * phaseZ, directionZ * horizontalZ);
    return sample;
}
