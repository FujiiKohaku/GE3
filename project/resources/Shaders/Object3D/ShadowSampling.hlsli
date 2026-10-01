#ifndef KOHAKU_SHADOW_SAMPLING
#define KOHAKU_SHADOW_SAMPLING
cbuffer ShadowSettings : register(b6)
{
    float4x4 gLightViewProjection;
    float4 gShadowParameters;
    float4 gShadowOptions;
};
cbuffer ShadowReceiver : register(b7) { uint gReceiveShadow; };
Texture2D<float> gShadowMap : register(t2);
SamplerComparisonState gShadowSampler : register(s1);

float SampleShadowVisibility(float3 worldPosition, float3 normal)
{
    if (gShadowOptions.x < 0.5f || gReceiveShadow == 0) { return 1.0f; }
    float3 offsetPosition = worldPosition + normalize(normal) * gShadowParameters.z;
    float4 lightPosition = mul(float4(offsetPosition, 1.0f), gLightViewProjection);
    float3 clip = lightPosition.xyz / lightPosition.w;
    float2 uv = clip.xy * float2(0.5f, -0.5f) + 0.5f;
    if (clip.z <= 0.0f || clip.z >= 1.0f || any(uv <= 0.0f) || any(uv >= 1.0f)) { return 1.0f; }
    float visibility = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float2 offset = float2(x, y) * gShadowParameters.x * gShadowOptions.y;
            visibility += gShadowMap.SampleCmpLevelZero(gShadowSampler, uv + offset, clip.z - gShadowParameters.y);
        }
    }
    visibility /= 9.0f;
    float edgeDistance = min(min(uv.x, uv.y), min(1.0f - uv.x, 1.0f - uv.y));
    float edgeFade = smoothstep(0.0f, 0.04f, edgeDistance);
    float depthFade = smoothstep(0.0f, 0.02f, clip.z) * smoothstep(0.0f, 0.02f, 1.0f - clip.z);
    return lerp(1.0f, visibility, edgeFade * depthFade);
}

float ShadowDirectFactor(float visibility)
{
    return lerp(1.0f, visibility, gShadowParameters.w);
}
#endif
