Texture2D<float> currentSignal : register(t0);
Texture2D<float> sceneDepth : register(t1);
Texture2D<float4> sceneNormal : register(t2);
Texture2D<float4> directionalLight : register(t3);
Texture2D<float2> motionVectors : register(t4);
Texture2D<float4> historySignal : register(t5);
Texture2D<float4> historyGeometry : register(t6);
Texture2D<float4> reprojectionTexture : register(t7);
Texture2D<float4> previousReprojectionTexture : register(t8);
#include "../MotionVector/Reprojection.hlsli"
cbuffer DenoiseParameters : register(b0) {
    row_major float4x4 inverseViewProjection;
    row_major float4x4 previousViewProjection;
    row_major float4x4 previousView;
    row_major float4x4 currentView;
    float2 jitterDeltaUv;
    uint hasHistory;
    uint hasMotionVectors;
    uint maxHistoryFrames;
    uint hasSceneChanges;
};
cbuffer FilterParameters : register(b1) { uint filterStep; };
bool IsEligible(int2 pixel) {
    float depth = sceneDepth.Load(int3(pixel, 0));
    float4 light = directionalLight.Load(int3(pixel, 0));
    return depth < 1 && light.a >= 0 && abs(light.a - depth) <= 0.000001f
        && sceneNormal.Load(int3(pixel, 0)).a >= 0 && any(abs(light.rgb) >= 0.000001f);
}
float3 WorldPosition(int2 pixel, uint2 size) {
    float2 uv = (float2(pixel) + 0.5f) / float2(size);
    float4 world = mul(float4(uv.x * 2 - 1, 1 - uv.y * 2, sceneDepth.Load(int3(pixel, 0)), 1), inverseViewProjection);
    return world.xyz / world.w;
}
