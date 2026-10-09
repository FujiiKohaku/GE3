#include "../Object3D/Object3d.hlsli"
#include "../Common/AlphaMask.hlsli"
ConstantBuffer<Material> maskMaterial : register(b3);
Texture2D<float4> maskTexture : register(t0);
SamplerState maskSampler : register(s0);
void main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0) {
    float2 uv = mul(float4(texcoord, 0, 1), maskMaterial.uvTransform).xy;
    float alpha = maskTexture.SampleLevel(maskSampler, uv, 0).a;
    if (ShouldRejectAlpha(alpha, maskMaterial.color.a, maskMaterial.alphaCutoff)) { discard; }
}
