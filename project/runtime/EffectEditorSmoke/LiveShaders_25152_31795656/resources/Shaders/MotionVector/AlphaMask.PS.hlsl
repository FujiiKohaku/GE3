#include "Common.hlsli"
#include "../Object3D/Object3d.hlsli"
#include "../Common/AlphaMask.hlsli"
ConstantBuffer<Material> maskMaterial : register(b5);
Texture2D<float4> maskTexture : register(t0);
SamplerState maskSampler : register(s0);
MotionPixelOutput main(MotionVectorOutput input) {
    float2 uv = mul(float4(input.texcoord, 0, 1), maskMaterial.uvTransform).xy;
    float alpha = maskTexture.SampleLevel(maskSampler, uv, 0).a;
    if (ShouldRejectAlpha(alpha, maskMaterial.color.a, maskMaterial.alphaCutoff)) { discard; }
    return BuildMotionPixel(input);
}
