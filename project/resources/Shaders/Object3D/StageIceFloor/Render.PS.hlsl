#include "../Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct FloorPixelOutput
{
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
};

FloorPixelOutput main(VertexShaderOutput input)
{
    // Floor appearance is independent of the ice object shaders.
    const float3 kFloorTint = float3(0.84f, 0.94f, 1.0f);

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 textureColor = gTexture.Sample(gSampler, uv);
    float3 N = normalize(input.normal);
    float faceLight = saturate(dot(N, normalize(float3(-0.6f, 0.8f, -0.5f))) * 0.5f + 0.5f);
    float3 shade = lerp(float3(0.33f, 0.52f, 0.68f), float3(0.70f, 0.84f, 0.93f),
        smoothstep(0.35f, 0.37f, faceLight));
    shade = lerp(shade, float3(0.91f, 0.96f, 1.0f), smoothstep(0.71f, 0.73f, faceLight));
    float3 color = gMaterial.color.rgb * textureColor.rgb * shade * kFloorTint *
        float3(0.82f, 0.88f, 0.92f);

    // The floor is matte: no environment reflection, specular highlight or rim glow.

    FloorPixelOutput output;
    output.color = float4(color, gMaterial.color.a * textureColor.a);
    output.encodedNormal = float4(N * 0.5f + 0.5f, input.position.z);
    return output;
}
