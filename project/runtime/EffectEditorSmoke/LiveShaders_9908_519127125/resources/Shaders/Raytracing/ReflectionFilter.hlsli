#include "ReflectionParameters.hlsli"
Texture2D<float4> signalTexture : register(t0);
Texture2D<float4> guideTexture : register(t1);
Texture2D<float4> historyTexture : register(t2);
Texture2D<float4> historyGuide : register(t3);
Texture2D<float> depthTexture : register(t4);
Texture2D<float4> surfaceTexture : register(t5);
Texture2D<float4> environmentTexture : register(t6);
Texture2D<float4> sceneTexture : register(t7);
Texture2D<float2> motionTexture : register(t8);
Texture2D<float4> materialTexture : register(t9);
Texture2D<float4> localLightTexture : register(t10);
Texture2D<float4> currentHit : register(t11);
Texture2D<float4> previousHit : register(t12);
Texture2D<float4> previousStatistics : register(t13);
Texture2D<float4> currentStatistics : register(t14);
Texture2D<float4> indirectLightingTexture : register(t15);
#include "../Object3D/LightingEnergy.hlsli"
Texture2D<float4> reprojectionTexture : register(t16);
Texture2D<float4> previousReprojectionTexture : register(t17);
#include "../MotionVector/Reprojection.hlsli"
SamplerState linearSampler : register(s0);
cbuffer FilterStep : register(b1) { uint filterStep; };
struct PixelInput { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; };
int2 ReflectionPixel(float2 uv) {
    uint width, height; signalTexture.GetDimensions(width, height);
    return clamp(int2(uv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
}
int2 FullReflectionPixel(int2 pixel) {
    uint width, height; depthTexture.GetDimensions(width, height);
    return min(pixel * 2 + 1, int2(width, height) - 1);
}
float3 GuideNormal(float4 guide) { return normalize(guide.xyz * 2 - 1); }
float SignalLuminance(float3 signal) {
    return log2(1 + dot(max(signal, 0), float3(0.2126f, 0.7152f, 0.0722f)));
}
float ReceiverMaterialKey(int2 pixel) {
    // Exact 24-bit integer in float32; includes albedo, metallic, roughness and specular strength.
    uint4 material = uint4(round(saturate(materialTexture.Load(int3(pixel, 0))) * 255));
    float4 surface = surfaceTexture.Load(int3(pixel, 0));
    uint value = material.x + material.y * 257u + material.z * 65537u + material.w * 16777619u;
    value ^= uint(round(saturate(surface.a) * 255)) * 2246822519u;
    value ^= uint(round(clamp(length(surface.xyz * 2 - 1) - 1, 0, 2) * 127)) * 3266489917u;
    value ^= value >> 16;
    return float(value & 0x00ffffffu);
}
bool HasMatchingLocalMaterial(int2 firstPixel, int2 secondPixel) {
    float4 firstMaterial = materialTexture.Load(int3(firstPixel, 0));
    float4 secondMaterial = materialTexture.Load(int3(secondPixel, 0));
    float4 firstSurface = surfaceTexture.Load(int3(firstPixel, 0));
    float4 secondSurface = surfaceTexture.Load(int3(secondPixel, 0));
    return all(abs(firstMaterial.rgb - secondMaterial.rgb) <= 0.06f) && abs(firstMaterial.a - secondMaterial.a) <= 0.01f
        && abs(firstSurface.a - secondSurface.a) <= 0.1f
        && abs(length(firstSurface.xyz * 2 - 1) - length(secondSurface.xyz * 2 - 1)) <= 0.1f;
}
