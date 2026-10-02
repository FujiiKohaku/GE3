#include "../Object3d.hlsli"
#include "../ShadowSampling.hlsli"
#include "../StageIceLighting.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
#include "../LocalLighting.hlsli"
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
TextureCube<float4> gEnvironmentTexture : register(t1);
#include "../EnvironmentLighting.hlsli"

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
    float direct = ShadowDirectFactor(SampleShadowVisibility(input.worldPosition, N));
    float3 shade = GetStageIceDiffuseLighting(N, direct);
    float3 color = gMaterial.color.rgb * textureColor.rgb * shade * kFloorTint *
        float3(0.82f, 0.88f, 0.92f);

    color += EnvironmentLighting(gMaterial.color.rgb * textureColor.rgb * kFloorTint, N, float3(0, 0, 1), false);
    color += ShadeLocalLights(gMaterial.color.rgb * textureColor.rgb * kFloorTint, N, float3(0, 0, 1), input.worldPosition, N, false);

    // The floor is matte: no environment reflection, specular highlight or rim glow.

    FloorPixelOutput output;
    output.color = float4(color, gMaterial.color.a * textureColor.a);
    output.encodedNormal = float4(N * 0.5f + 0.5f, input.position.z);
    return output;
}
