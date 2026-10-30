#include "../Object3D/IceJellyfish/Deformation.hlsli"

cbuffer ShadowWorld : register(b0) { float4x4 gWorld; };
cbuffer ShadowCamera : register(b1) { float4x4 gLightViewProjection; };
cbuffer ShadowAnimation : register(b2) { float4 animation; };

float4 main(float4 inputPosition : POSITION0) : SV_POSITION
{
    float3 position = inputPosition.xyz;
    float3 normal = float3(0.0f, 1.0f, 0.0f);
    if (animation.w < 0.5f) {
        DeformBell(position, normal, animation);
    } else {
        DeformTentacle(position, normal, animation);
    }
    return mul(mul(float4(position, inputPosition.w), gWorld), gLightViewProjection);
}
