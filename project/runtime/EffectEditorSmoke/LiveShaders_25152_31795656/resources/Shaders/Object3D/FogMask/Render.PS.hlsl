#include "../Object3d.hlsli"
#include "../../Common/AlphaMask.hlsli"
ConstantBuffer<Material> gMaskMaterial : register(b0);
Texture2D<float4> gMaskTexture : register(t0);
SamplerState gMaskSampler : register(s0);
cbuffer ObjectFogFlags : register(b13) { uint gObjectFogFlags; };

float main(VertexShaderOutput input) : SV_Target0
{
    float2 uv = mul(float4(input.texcoord, 0, 1), gMaskMaterial.uvTransform).xy;
    float alpha = gMaskTexture.Sample(gMaskSampler, uv).a;
    if (ShouldRejectAlpha(alpha, gMaskMaterial.color.a, gMaskMaterial.alphaCutoff)) discard;
    return (gObjectFogFlags & 1u) != 0u ? 1.0f : 0.0f;
}
