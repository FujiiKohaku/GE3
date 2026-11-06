#include "../Object3d.hlsli"
ConstantBuffer<Material> materialParameters : register(b0);

struct PixelShaderOutput {
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output = (PixelShaderOutput)0;
    float alpha = materialParameters.color.a;
    // Tracking uses dashes; the central line becomes solid once the path locks.
    if (alpha < 0.6f) { alpha *= step(0.30f, frac(input.texcoord.y * 24.0f)); }
    output.color = float4(materialParameters.color.rgb * 1.8f, alpha);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
    return output;
}
