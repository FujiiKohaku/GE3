Texture2D<float4> sceneTexture : register(t0);
Texture2D<float> depthTexture : register(t1);
Texture2D<float4> normalTexture : register(t2);
Texture2D<float4> reflectionTexture : register(t3);
Texture2D<float2> depthPyramid : register(t4);
Texture2D<float4> hitTexture : register(t5);
Texture2D<float4> historyTexture : register(t6);
Texture2D<float4> historyHitTexture : register(t7);
Texture2D<float2> motionTexture : register(t8);
SamplerState linearSampler : register(s0);
cbuffer Parameters : register(b0) {
    row_major float4x4 projection;
    row_major float4x4 inverseProjection;
    row_major float4x4 view;
    float4 settings;
    float4 traversal;
    row_major float4x4 currentToPreviousView;
    row_major float4x4 previousProjection;
    float4 temporal;
    float4 filter;
};
struct PixelInput { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; };
float3 Reconstruct(float2 uv, float depth) {
    float4 position = mul(float4(uv * float2(2, -2) + float2(-1, 1), depth, 1), inverseProjection);
    return position.xyz / position.w;
}
int2 GetPixel(float2 uv) {
    uint width, height; depthTexture.GetDimensions(width, height);
    return clamp(int2(uv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
}
// Reflective floors store roughness in the encoded normal's length.
// Normal direction remains unchanged for existing normal/depth consumers.
float GetReflectionRoughness(float4 encoded) {
    return saturate(length(encoded.xyz * 2 - 1) - 1);
}
