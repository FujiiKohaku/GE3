Texture2D<float4> sceneTexture : register(t0);
Texture2D<float> depthTexture : register(t1);
Texture2D<float4> normalTexture : register(t2);
Texture2D<float4> materialTexture : register(t3);
Texture2D<float2> motionTexture : register(t4);
Texture2D<float2> depthPyramid : register(t5);
Texture2D<float4> indirectTexture : register(t6);
Texture2D<float4> metadataTexture : register(t7);
Texture2D<float4> historyTexture : register(t8);
Texture2D<float4> historyMetadataTexture : register(t9);
SamplerState linearSampler : register(s0);
cbuffer Parameters : register(b0) {
    row_major float4x4 projection;
    row_major float4x4 inverseProjection;
    row_major float4x4 view;
    row_major float4x4 currentToPreviousView;
    row_major float4x4 previousProjection;
    float4 tracing;
    float4 sampling;
    float4 temporal;
    float4 composition;
};
struct PixelInput { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; };
struct IndirectOutput {
    float4 indirectColor : SV_TARGET0;
    float4 metadata : SV_TARGET1;
};
int2 GetPixel(float2 uv)
{
    uint width, height;
    depthTexture.GetDimensions(width, height);
    return clamp(int2(uv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
}
float3 Reconstruct(float2 uv, float depth)
{
    float4 position = mul(float4(uv * float2(2, -2) + float2(-1, 1), depth, 1), inverseProjection);
    return position.xyz / position.w;
}
bool IsValidSurface(float depth, float4 encodedNormal, float4 material)
{
    float normalDepth = encodedNormal.a;
    if (normalDepth >= 2) { normalDepth -= 2; }
    if (normalDepth <= -2) { normalDepth = -normalDepth - 2; }
    return depth < 0.999999f && normalDepth >= 0 && abs(normalDepth - depth) < 0.004f && material.a > 0;
}
float GetMetallic(float4 material)
{
    return saturate((material.a * 255 - 1) / 254);
}
float3 GetWorldNormal(float4 encoded)
{
    return normalize(encoded.xyz * 2 - 1);
}
float3 ReconstructLinear(float2 uv, float viewDepth)
{
    float3 ray = Reconstruct(uv, 0.5f);
    return ray * (viewDepth / ray.z);
}
float GetGeometryWeight(float4 center, float4 neighbor)
{
    if (center.w <= 0 || neighbor.w <= 0) { return 0; }
    if (dot(center.xyz, neighbor.xyz) < 0.95f) { return 0; }
    float difference = abs(center.w - neighbor.w);
    float tolerance = max(0.05f, center.w * 0.01f);
    if (difference > tolerance * 3) { return 0; }
    return exp(-difference / tolerance) * pow(saturate(dot(center.xyz, neighbor.xyz)), 16);
}
