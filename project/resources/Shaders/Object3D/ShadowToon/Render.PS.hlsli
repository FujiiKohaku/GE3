#include "../Object3d.hlsli"
#include "../../Common/AlphaMask.hlsli"
#include "../ShadowSampling.hlsli"

ConstantBuffer<Material> materialParameters : register(b0);
static Material gMaterial;
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
#include "../MaterialMaps.PS.hlsli"
#include "../NormalMapping.PS.hlsli"
#include "../LocalLighting.hlsli"
#include "../SurfaceLighting.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
#if defined(KOHAKU_RT_CAPTURE) || defined(KOHAKU_RT_REFLECTION_CAPTURE)
    float4 directionalLight : SV_Target4;
#endif
#if defined(KOHAKU_RT_REFLECTION_CAPTURE)
    float4 reflectionSurface : SV_Target5;
    float4 reflectionEnvironment : SV_Target6;
#endif
#if defined(KOHAKU_RT_LOCAL_SHADOW_CAPTURE)
    float4 localLight : SV_Target7;
#endif
};

PixelShaderOutput main(VertexShaderOutput input)
{
    gMaterial = materialParameters;
#if defined(KOHAKU_RT_LOCAL_SHADOW_CAPTURE)
    capturedRtLocalLight = 0;
#endif
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    gMaterial = ResolveRasterMaterial(gMaterial, uv);
    float4 textureColor = gTexture.Sample(gSampler, uv);
    if (gMaterial.alphaCutoff > 0) {
        float maskAlpha = gTexture.SampleLevel(gSampler, uv, 0).a;
        if (ShouldRejectAlpha(maskAlpha, gMaterial.color.a, gMaterial.alphaCutoff)) { discard; }
    }
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
    float3 N = ApplyNormalMap(input.worldPosition, input.normal, uv);
    float3 L = normalize(-gDirectionalLight.direction);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);
    float visibility = ShadowDirectFactor(SampleShadowVisibility(input.worldPosition, normalize(input.normal)));
#if defined(KOHAKU_RT_CAPTURE)
    visibility = 1.0f;
#endif
    float3 direct = max(gDirectionalLight.color.rgb, 0.0f) *
        max(gDirectionalLight.intensity, 0.0f) * visibility * GetDirectLightingStrength(gAmbientLight);
    float3 indirectColor = ShadowToonSurfaceIndirect(baseColor, N, V);
    float3 directionalColor = ShadowToonSurfaceDirect(baseColor, N, V, L) * direct;
    float3 color = indirectColor + directionalColor;


    color += ShadeLocalLights(baseColor, N, V, input.worldPosition, normalize(input.normal), true);

    PixelShaderOutput output;
    output.indirectColor = float4(indirectColor, gMaterial.color.a * textureColor.a);
    output.color = float4(color, gMaterial.color.a * textureColor.a);
    output.surfaceMaterial = float4(saturate(baseColor), (1 + saturate(gMaterial.metallic) * 254) / 255);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
#if defined(KOHAKU_RT_CAPTURE)
    output.directionalLight = float4(directionalColor, input.position.z);
#elif defined(KOHAKU_RT_REFLECTION_CAPTURE)
    output.directionalLight = float4(0, 0, 0, -1);
#endif
#if defined(KOHAKU_RT_REFLECTION_CAPTURE)
    output.reflectionSurface = float4(N * (1 + clamp(gMaterial.specularStrength, 0, 2)) * 0.5f + 0.5f, saturate(gMaterial.roughness));
    output.reflectionEnvironment = float4(0, 0, 0, input.position.z);
#endif
#if defined(KOHAKU_RT_LOCAL_SHADOW_CAPTURE)
    float hasLocalSpecular = 1; if (shouldReceiveRtLocalShadow == 0) { hasLocalSpecular = -1; }
    output.localLight = float4(capturedRtLocalLight, hasLocalSpecular);
#endif
    return output;
}
