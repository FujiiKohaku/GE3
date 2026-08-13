#include "Ocean.hlsli"

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
    AddGerstnerWave(p, float2(0.98f, 0.18f), frequency * 0.61f,
        amplitude * 0.36f * localStrength, 0.56f, 0.72f, time, 0.4f, displacement, gradient);
    AddGerstnerWave(p, float2(-0.24f, 0.97f), frequency * 0.89f,
        amplitude * 0.27f * localStrength, 0.77f, 0.66f, time, 2.1f, displacement, gradient);
    AddGerstnerWave(p, float2(0.63f, 0.78f), frequency * 1.31f,
        amplitude * 0.19f * localStrength, 1.03f, 0.58f, time, 4.7f, displacement, gradient);
    AddGerstnerWave(p, float2(-0.91f, 0.42f), frequency * 1.86f,
        amplitude * 0.115f * localStrength, 1.39f, 0.48f, time, 1.3f, displacement, gradient);
    AddGerstnerWave(p, float2(0.15f, -0.99f), frequency * 2.63f,
        amplitude * 0.065f * localStrength, 1.82f, 0.36f, time, 5.6f, displacement, gradient);

    worldPosition += displacement;
    output.position = mul(float4(worldPosition, 1.0f), gOcean.viewProjection);
    output.worldPosition = worldPosition;
    output.normal = normalize(float3(-gradient.x, 1.0f, -gradient.y));
    output.waveData = float2(displacement.y / max(amplitude * localStrength, 0.0001f), length(gradient));
    return output;
}
