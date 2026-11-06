#include "../Object3d.hlsli"

ConstantBuffer<Material> materialParameters : register(b0);
ConstantBuffer<Camera> cameraParameters : register(b2);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output = (PixelShaderOutput)0;
    float3 normal = normalize(input.normal);
    float facing = saturate(dot(normal, normalize(cameraParameters.worldPosition - input.worldPosition)));
    float core = pow(facing, 4.0f);
    float3 color = lerp(materialParameters.color.rgb, float3(3.0f, 2.6f, 3.2f), core);
    output.color = float4(color, smoothstep(0.05f, 0.8f, facing) * materialParameters.color.a);
    output.encodedNormal = float4(normal * 0.5f + 0.5f, input.position.z);
    return output;
}
