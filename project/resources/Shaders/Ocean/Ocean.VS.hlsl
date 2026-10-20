#include "Ocean.hlsli"
#include "OceanWakeShape.hlsli"

struct VertexInput
{
    float4 position : POSITION0;
};

#define OCEAN_WAVE_VECTOR float3
#define OCEAN_WAVE_MAKE(x, y, z) float3(x, y, z)
#define OCEAN_WAVE_FLOOR(value) floor(value)
#define OCEAN_WAVE_SQRT(value) sqrt(value)
#define OCEAN_WAVE_SIN(value) sin(value)
#define OCEAN_WAVE_COS(value) cos(value)
#define OCEAN_WAVE_INLINE
#include "OceanWaveShape.hlsli"

OceanVertexOutput main(VertexInput input)
{
    OceanVertexOutput output;
    float3 worldPosition = mul(input.position, gOcean.world).xyz;
    float2 minimum = mul(float4(-0.5f, 0.0f, -0.5f, 1.0f), gOcean.world).xz;
    float2 maximum = mul(float4(0.5f, 0.0f, 0.5f, 1.0f), gOcean.world).xz;
    worldPosition.x = OceanFocusedCoordinate(input.position.x + 0.5f, minimum.x, maximum.x, gOcean.wakeMeta.y);
    worldPosition.z = OceanFocusedCoordinate(input.position.z + 0.5f, minimum.y, maximum.y, gOcean.wakeMeta.z);
    float2 p = worldPosition.xz;
    float time = gOcean.cameraPositionAndTime.w;
    float amplitude = gOcean.waveParameters.x;
    float frequency = gOcean.waveParameters.y;

    float3 displacement = 0.0f;
    float3 tangentX = float3(1.0f, 0.0f, 0.0f);
    float3 tangentZ = float3(0.0f, 0.0f, 1.0f);
#define OCEAN_WAVE(dx, dz, f, a, speed, steepness, phase) \
    { \
        OceanWaveSample wave = EvaluateOceanWave(p.x, p.y, time, amplitude, frequency, dx, dz, f, a, speed, steepness, phase); \
        displacement += wave.displacement; \
        tangentX += wave.derivativeX; \
        tangentZ += wave.derivativeZ; \
    }
#include "OceanWaveLayers.hlsli"
#undef OCEAN_WAVE

    worldPosition += displacement;
    float3 wake = 0.0f;
    [loop]
    for (uint i = 0; i < (uint)gOcean.wakeMeta.x; ++i) {
        OceanWakePoint stamp = gOcean.wakes[i];
        float2 delta = worldPosition.xz - stamp.positionTimeStrength.xy;
        wake += OceanWakeProfile(delta.x, delta.y, time - stamp.positionTimeStrength.z,
            stamp.positionTimeStrength.w, stamp.directionSize.z, stamp.directionSize.x, stamp.directionSize.y);
    }
    wake = LimitOceanWake(wake);
    worldPosition.y += wake.y;
    tangentX.y += dot(wake.xz, tangentX.xz);
    tangentZ.y += dot(wake.xz, tangentZ.xz);
    float3 surfaceNormal = normalize(cross(tangentZ, tangentX));
    float2 gradient = -surfaceNormal.xz / max(surfaceNormal.y, 0.001f);
    output.position = mul(float4(worldPosition, 1.0f), gOcean.viewProjection);
    output.worldPosition = worldPosition;
    output.normal = surfaceNormal;
    output.waveData = float2(displacement.y / max(amplitude, 0.0001f), length(gradient));
    output.wakeData = float2(wake.y, length(wake.xz));
    return output;
}
