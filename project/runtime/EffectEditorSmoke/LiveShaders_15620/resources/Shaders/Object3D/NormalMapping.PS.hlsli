#ifndef KOHAKU_NORMAL_MAPPING
#define KOHAKU_NORMAL_MAPPING
Texture2D<float4> gNormalTexture : register(t3);
#include "NormalMappingCommon.hlsli"

// Derive a cotangent frame from the actual UVs; no extra vertex attributes.
float3 ApplyNormalMap(float3 worldPosition, float3 geometricNormal, float2 uv)
{
    float3 N = geometricNormal * rsqrt(max(dot(geometricNormal, geometricNormal), 0.000001f));
    if (gMaterial.normalMapEnabled == 0 || gMaterial.normalMapStrength <= 0.0f)
    {
        return N;
    }
    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    float3 detail = gNormalTexture.Sample(gSampler, uv).xyz * 2.0f - 1.0f;
    return ApplyNormalDetail(N, dp1, dp2, duv1, duv2, detail, gMaterial.normalMapStrength, gMaterial.normalMapFlipY);
}
#endif
