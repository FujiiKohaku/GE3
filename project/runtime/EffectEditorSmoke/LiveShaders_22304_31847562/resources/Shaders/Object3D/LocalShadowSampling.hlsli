#ifndef KOHAKU_LOCAL_SHADOW_SAMPLING
#define KOHAKU_LOCAL_SHADOW_SAMPLING
#define KOHAKU_HAS_LOCAL_SHADOWS
cbuffer LocalShadowSettings : register(b8) {
    float4x4 gLocalShadowMatrices[8];
    float4 gLocalShadowParameters;
    float4 gPointShadowSlots[8];
    float4 gSpotShadowSlots[2];
};
Texture2DArray<float> gLocalShadowMaps : register(t4);

float SampleLocalShadow(int faceIndex, float3 worldPosition, float3 normal) {
    if (gReceiveShadow == 0 || faceIndex < 0 || faceIndex >= 8) { return 1.0f; }
    float4 clip = mul(float4(worldPosition + normal * gLocalShadowParameters.z, 1.0f), gLocalShadowMatrices[faceIndex]);
    if (clip.w <= 0.00001f) { return 1.0f; }
    float3 projected = clip.xyz / clip.w;
    float2 uv = projected.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv < 0.0f) || any(uv > 1.0f) || projected.z <= 0.0f || projected.z >= 1.0f) { return 1.0f; }
    float visibility = 0.0f;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float2 sampleUv = clamp(uv + float2(x, y) * gLocalShadowParameters.x,
                gLocalShadowParameters.x * 0.5f, 1.0f - gLocalShadowParameters.x * 0.5f);
            visibility += gLocalShadowMaps.SampleCmpLevelZero(gShadowSampler, float3(sampleUv, faceIndex), projected.z - gLocalShadowParameters.y);
        }
    }
    return lerp(1.0f, visibility / 9.0f, gLocalShadowParameters.w);
}
int PointShadowFace(float3 direction) {
    float3 absoluteDirection = abs(direction);
    if (absoluteDirection.x >= absoluteDirection.y && absoluteDirection.x >= absoluteDirection.z) {
        if (direction.x >= 0.0f) { return 0; }
        return 1;
    }
    if (absoluteDirection.y >= absoluteDirection.z) {
        if (direction.y >= 0.0f) { return 2; }
        return 3;
    }
    if (direction.z >= 0.0f) { return 4; }
    return 5;
}
float PointShadowVisibility(uint lightIndex, float3 lightPosition, float3 worldPosition, float3 normal) {
    int slot = int(gPointShadowSlots[lightIndex / 4][lightIndex % 4]);
    if (slot < 0) { return 1.0f; }
    return SampleLocalShadow(slot + PointShadowFace(worldPosition - lightPosition), worldPosition, normal);
}
float SpotShadowVisibility(uint lightIndex, float3 worldPosition, float3 normal) {
    int slot = int(gSpotShadowSlots[lightIndex / 4][lightIndex % 4]);
    return SampleLocalShadow(slot, worldPosition, normal);
}
#endif
