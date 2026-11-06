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
    float sideFade = 1.0f - smoothstep(0.05f, 1.0f, abs(input.texcoord.x * 2.0f - 1.0f));
    float tailFade = pow(saturate(input.texcoord.y), 1.5f);
    float core = pow(sideFade, 5.0f);
    float3 color = lerp(materialParameters.color.rgb, float3(0.94f, 0.78f, 1.0f), core * 0.65f);
    output.color = float4(color * 1.65f, sideFade * sideFade * tailFade * materialParameters.color.a);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
    return output;
}
