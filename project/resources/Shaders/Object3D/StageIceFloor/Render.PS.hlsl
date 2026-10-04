#include "../Object3d.hlsli"
#include "../ShadowSampling.hlsli"
#include "../StageIceLighting.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b2);
#include "../LocalLighting.hlsli"
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
TextureCube<float4> gEnvironmentTexture : register(t1);
#include "../EnvironmentLighting.hlsli"

struct FloorPixelOutput
{
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
};

FloorPixelOutput main(VertexShaderOutput input)
{
    // Floor appearance is independent of the ice object shaders.
    const float3 kFloorTint = float3(0.84f, 0.94f, 1.0f);

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 textureColor = gTexture.Sample(gSampler, uv);
    float3 normal = normalize(input.normal);
    float3 viewDirection = normalize(gCamera.worldPosition - input.worldPosition);
    if (dot(normal, viewDirection) < 0) { normal = -normal; }
    float direct = ShadowDirectFactor(SampleShadowVisibility(input.worldPosition, normal));
    float3 shade = GetStageIceDiffuseLighting(normal, direct);
    float3 indirectColor = gMaterial.color.rgb * textureColor.rgb * kFloorTint *
        float3(0.45f, 0.58f, 0.70f) * HemisphereAmbient(gAmbientLight, normal) * gAmbientLight.componentSettings.z;
    float3 color = gMaterial.color.rgb * textureColor.rgb * shade * kFloorTint *
        float3(0.45f, 0.58f, 0.70f);

    float3 environmentColor = 0.65f * EnvironmentLighting(gMaterial.color.rgb * textureColor.rgb * kFloorTint, normal, float3(0, 0, 1), false);
    color += environmentColor;
    indirectColor += environmentColor;
    color += ShadeLocalLights(gMaterial.color.rgb * textureColor.rgb * kFloorTint, normal, viewDirection, input.worldPosition, normal, true);
    float3 lightDirection = normalize(-gDirectionalLight.direction);
    color += GetStageIceDirectRadiance(direct) *
        SurfaceSpecular(gMaterial, gMaterial.color.rgb * textureColor.rgb, normal, viewDirection, lightDirection);
    // A restrained blue glaze gives unreflected areas a view-dependent ice sheen.
    float facing = saturate(dot(normal, viewDirection));
    float grazing = pow(1 - facing, 5);
    float3 glazeColor = GetStageIceAmbientRadiance() * float3(0.65f, 0.85f, 1.0f) * grazing * 0.22f;
    color += glazeColor;
    indirectColor += glazeColor;

    // SSR supplies the floor reflection after the forward pass.

    FloorPixelOutput output;
    output.color = float4(color, gMaterial.color.a * textureColor.a);
    output.indirectColor = float4(indirectColor, output.color.a);
    // Preserve normal direction and depth while carrying material roughness in its length.
    output.surfaceMaterial = float4(saturate(gMaterial.color.rgb * textureColor.rgb * kFloorTint), (1 + saturate(gMaterial.metallic) * 254) / 255);
    output.encodedNormal = float4(normal * (1 + saturate(gMaterial.roughness)) * 0.5f + 0.5f, 2.0f + input.position.z);
    return output;
}
