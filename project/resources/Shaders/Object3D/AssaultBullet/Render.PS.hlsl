#include "../Object3d.hlsli"

ConstantBuffer<Material> materialParameters : register(b0);
ConstantBuffer<Camera> gCamera : register(b2);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    const float3 kOutlineColor = float3(0.006f, 0.015f, 0.055f);
    const float3 kColdCoreColor = float3(0.48f, 0.90f, 1.0f);
    const float3 kHotCoreColor = float3(1.0f, 0.70f, 0.35f);

    const float3 normal = normalize(input.normal);
    const float3 viewDirection = normalize(gCamera.worldPosition - input.worldPosition);
    const float facing = saturate(dot(normal, viewDirection));
    const float edgeSoftness = clamp(fwidth(facing) * 0.5f, 0.015f, 0.06f);
    const float bodyMask = smoothstep(0.49f - edgeSoftness, 0.68f + edgeSoftness, facing);
    const float coreMask = smoothstep(0.78f, 0.96f, facing);
    const float3 bodyColor = materialParameters.color.rgb;
    const float heatTint = saturate((bodyColor.r - bodyColor.b + 0.88f) * 0.58f);
    const float3 coreColor = lerp(kColdCoreColor, kHotCoreColor, heatTint);

    PixelShaderOutput output = (PixelShaderOutput)0;
    output.color = float4(lerp(kOutlineColor, lerp(bodyColor, coreColor, coreMask), bodyMask),
        saturate(materialParameters.color.a));
    output.encodedNormal = float4(normal * 0.5f + 0.5f, input.position.z);
    return output;
}
