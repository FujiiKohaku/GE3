#include "Skybox.hlsli"

TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float Hash21(float2 value)
{
    value = frac(value * float2(123.34f, 456.21f));
    value += dot(value, value + 45.32f);
    return frac(value.x * value.y);
}

float ValueNoise(float2 value)
{
    float2 cell = floor(value);
    float2 local = frac(value);
    local = local * local * (3.0f - 2.0f * local);

    float bottom = lerp(Hash21(cell), Hash21(cell + float2(1.0f, 0.0f)), local.x);
    float top = lerp(Hash21(cell + float2(0.0f, 1.0f)), Hash21(cell + float2(1.0f, 1.0f)), local.x);
    return lerp(bottom, top, local.y);
}

float FractalNoise(float2 value)
{
    float result = ValueNoise(value) * 0.58f;
    result += ValueNoise(value * 2.07f + 17.3f) * 0.28f;
    result += ValueNoise(value * 4.13f + 31.7f) * 0.14f;
    return result;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float3 direction = normalize(input.texcoord);
    float height = saturate(direction.y * 0.5f + 0.5f);

    const float3 horizonColor = float3(0.56f, 0.84f, 1.0f);
    const float3 upperColor = float3(0.08f, 0.36f, 0.72f);
    float3 skyColor = lerp(horizonColor, upperColor, smoothstep(0.30f, 0.92f, height));

    // Static graphic cloud masses. Their hard light edge and blue underside
    // read like painted animation backgrounds instead of an HDR photograph.
    float longitude = atan2(direction.z, direction.x) * 0.15915494f;
    float latitude = asin(clamp(direction.y, -1.0f, 1.0f)) * 0.31830989f;
    float2 cloudUv = float2(longitude * 7.0f, latitude * 4.2f);
    float cloudNoise = FractalNoise(cloudUv);
    float cloudRegion = smoothstep(-0.10f, 0.08f, direction.y) *
        (1.0f - smoothstep(0.62f, 0.82f, direction.y));
    float cloudMask = smoothstep(0.54f, 0.61f, cloudNoise) * cloudRegion;
    float cloudCore = smoothstep(0.66f, 0.76f, cloudNoise) * cloudRegion;
    float3 cloudShadow = float3(0.44f, 0.68f, 0.88f);
    float3 cloudLight = float3(0.96f, 0.985f, 1.0f);
    skyColor = lerp(skyColor, cloudShadow, cloudMask * 0.88f);
    skyColor = lerp(skyColor, cloudLight, cloudCore);

    float3 sunDirection = normalize(float3(-0.42f, 0.58f, 0.70f));
    float sunDisk = smoothstep(0.996f, 0.9992f, dot(direction, sunDirection));
    float sunGlow = pow(saturate(dot(direction, sunDirection)), 48.0f);
    skyColor += float3(1.0f, 0.83f, 0.48f) * sunGlow * 0.12f;
    skyColor = lerp(skyColor, float3(1.0f, 0.93f, 0.68f), sunDisk);

    return float4(saturate(skyColor), 1.0f);
}
