#include "Common.hlsli"
#include "../Object3D/Object3d.hlsli"
#include "../Common/AlphaMask.hlsli"
ConstantBuffer<Material> maskMaterial : register(b5);
Texture2D<float4> maskTexture : register(t0);
SamplerState maskSampler : register(s0);
float2 main(MotionVectorOutput input) : SV_TARGET0 {
    float2 uv = mul(float4(input.texcoord, 0, 1), maskMaterial.uvTransform).xy;
    float alpha = maskTexture.SampleLevel(maskSampler, uv, 0).a;
    if (ShouldRejectAlpha(alpha, maskMaterial.color.a, maskMaterial.alphaCutoff)) { discard; }
    if (input.currentClip.w <= 0.00001f || input.previousClip.w <= 0.00001f) { return float2(0, 0); }
    float2 currentUv = input.currentClip.xy / input.currentClip.w * float2(0.5f, -0.5f) + 0.5f;
    float2 previousUv = input.previousClip.xy / input.previousClip.w * float2(0.5f, -0.5f) + 0.5f;
    return currentUv - previousUv;
}
