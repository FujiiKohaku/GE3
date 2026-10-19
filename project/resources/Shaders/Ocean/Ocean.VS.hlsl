#include "Ocean.hlsli"
#include "OceanWakeShape.hlsli"

struct VertexInput
{
    float4 position : POSITION0;
};

void AddGerstnerWave(
    float2 basePosition,
    float2 direction,
    float waveNumber,
    float amplitude,
    float speed,
    float steepness,
    float time,
    float phaseOffset,
    inout float3 displacement,
    inout float2 heightGradient)
{
    direction = normalize(direction);
    float phase = dot(direction, basePosition) * waveNumber + time * speed + phaseOffset;
    float sinePhase = sin(phase);
    float cosinePhase = cos(phase);

    displacement.xz += direction * (steepness * amplitude * cosinePhase);
    displacement.y += amplitude * sinePhase;
    heightGradient += direction * (amplitude * waveNumber * cosinePhase);
}

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

    // Broad spatial modulation creates calm and rough patches without a hard
    // boundary. The two incommensurate bands take a long distance to repeat.
    float regionNoise = sin(p.x * 0.0041f + p.y * 0.0027f + time * 0.07f) * 0.5f +
        sin(p.x * -0.0019f + p.y * 0.0053f - time * 0.045f) * 0.5f;
    float localStrength = lerp(0.58f, 1.22f, regionNoise * 0.5f + 0.5f);

    float3 displacement = 0.0f;
    float2 gradient = 0.0f;
#define OCEAN_WAVE(dx, dz, f, a, speed, steepness, phase) \
    AddGerstnerWave(p, float2(dx, dz), frequency * f, amplitude * a * localStrength, speed, steepness, time, phase, displacement, gradient);
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
    gradient += wake.xz;
    output.position = mul(float4(worldPosition, 1.0f), gOcean.viewProjection);
    output.worldPosition = worldPosition;
    output.normal = normalize(float3(-gradient.x, 1.0f, -gradient.y));
    output.waveData = float2(displacement.y / max(amplitude * localStrength, 0.0001f), length(gradient));
    output.wakeData = float2(wake.y, length(wake.xz));
    return output;
}
