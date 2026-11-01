#include "../Fullscreen.hlsli"
#include "VolumeData.hlsli"
Texture2D<float4> gColor : register(t0);
Texture2D<float> gDepth : register(t1);
SamplerState gPoint : register(s0);
// Camera-relative reconstruction avoids subtracting two large world positions.
float3 ReconstructRelative(float2 uv, float depth) {
    float4 world = mul(float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f), inverseViewProjection);
    return world.xyz / max(world.w, 0.000001f);
}
