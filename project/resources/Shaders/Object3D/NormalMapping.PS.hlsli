#ifndef KOHAKU_NORMAL_MAPPING
#define KOHAKU_NORMAL_MAPPING
Texture2D<float4> gNormalTexture : register(t3);

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
    float3 perpendicular2 = cross(dp2, N);
    float3 perpendicular1 = cross(N, dp1);
    float3 T = perpendicular2 * duv1.x + perpendicular1 * duv2.x;
    float3 B = perpendicular2 * duv1.y + perpendicular1 * duv2.y;
    float frameLength = max(dot(T, T), dot(B, B));
    if (frameLength < 0.000000000001f)
    {
        return N;
    }
    float3 detail = gNormalTexture.Sample(gSampler, uv).xyz * 2.0f - 1.0f;
    detail.xy *= gMaterial.normalMapStrength;
    if (gMaterial.normalMapFlipY > 0.5f)
    {
        detail.y = -detail.y;
    }
    float inverseFrameLength = rsqrt(frameLength);
    float3 result = (T * detail.x + B * detail.y) * inverseFrameLength + N * detail.z;
    return result * rsqrt(max(dot(result, result), 0.000001f));
}
#endif
