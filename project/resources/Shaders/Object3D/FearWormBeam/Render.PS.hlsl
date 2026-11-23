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
    const float kTwoPi = 6.28318530718f;
    const float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), materialParameters.uvTransform).xy;
    const float3 normal = normalize(input.normal);
    const float3 view = normalize(gCamera.worldPosition - input.worldPosition);
    const float facing = saturate(dot(normal, view));
    const float softEdge = smoothstep(0.0f, 0.5f, facing);

    // 周方向は周期的にし、円柱のUV継ぎ目を隠す。
    const float turbulence = sin(uv.x * kTwoPi * 5.0f + sin(uv.y * kTwoPi * 3.0f))
        * sin(uv.y * kTwoPi * 11.0f - uv.x * kTwoPi * 3.0f);
    const float stream = 0.5f + 0.5f * sin(uv.y * kTwoPi * 16.0f
        + sin(uv.x * kTwoPi * 7.0f) + turbulence);
    const float intensity = 0.65f + 0.35f * stream + 0.15f * turbulence;
    const float alpha = materialParameters.color.a * softEdge * (0.7f + 0.3f * stream);

    PixelShaderOutput output = (PixelShaderOutput)0;
    output.color = float4(materialParameters.color.rgb * intensity, alpha);
    output.encodedNormal = float4(normal * 0.5f + 0.5f, input.position.z);
    return output;
}
