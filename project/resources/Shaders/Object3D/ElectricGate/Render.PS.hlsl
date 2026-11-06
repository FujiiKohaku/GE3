#include "../Object3d.hlsli"

ConstantBuffer<Material> materialParameters : register(b0);

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
    float distanceFromCenter = abs(input.texcoord.x * 2.0f - 1.0f);
    float glow = 1.0f - smoothstep(0.1f, 1.0f, distanceFromCenter);
    float core = 1.0f - smoothstep(0.02f, 0.3f, distanceFromCenter);
    float3 color = lerp(materialParameters.color.rgb * 2.0f, float3(4.0f, 2.8f, 1.5f), core);
    output.color = float4(color, glow * materialParameters.color.a);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
    return output;
}
