#include "Object3d.hlsli"
#include "../Common/AlphaMask.hlsli"
#if defined(KOHAKU_MATERIAL_SHADOWS)
#include "ShadowSampling.hlsli"
#endif

ConstantBuffer<Material> materialParameters : register(b0);
static Material gMaterial;
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);
Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);
#include "MaterialMaps.PS.hlsli"
#include "NormalMapping.PS.hlsli"
#include "LocalLighting.hlsli"
#if defined(KOHAKU_RT_REFLECTION_CAPTURE)
static float3 capturedReflectionEnvironment = 0;
#endif
#include "EnvironmentLighting.hlsli"
#include "SurfaceLighting.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
    float32_t4 encodedNormal : SV_Target1;
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

float3 ShadeStandard(float3 baseColor, float3 normal, float3 worldPosition, float3 geometricNormal, out float3 indirectColor, out float3 directionalColor)
{
    float3 N = normalize(normal);
    float3 V = normalize(gCamera.worldPosition - worldPosition);
    float3 ambient = StandardSurfaceIndirect(baseColor, N, V);
    indirectColor = ambient;
    float3 Ld = normalize(-gDirectionalLight.direction);
    float visibility = 1.0f;
#if defined(KOHAKU_MATERIAL_SHADOWS) && !defined(KOHAKU_RT_CAPTURE)
    visibility = ShadowDirectFactor(SampleShadowVisibility(worldPosition, geometricNormal));
#endif
    directionalColor = StandardSurfaceDirect(baseColor, N, V, Ld) * gDirectionalLight.color.rgb
        * gDirectionalLight.intensity * GetDirectLightingStrength(gAmbientLight);
    float3 result = ambient;

    result += directionalColor * visibility + ShadeLocalLights(baseColor, N, V, worldPosition, geometricNormal, gMaterial.shininess > 0.0f);
    return result;
}

float3 ShadeArchive(float3 baseColor, float3 normal, float3 worldPosition, float2 uv)
{
    float3 N = normalize(normal);
    float3 V = normalize(gCamera.worldPosition - worldPosition);
    float3 L = normalize(float3(-3.5f, 6.0f, -6.0f) - worldPosition);
    float3 H = normalize(L + V);
    float pool = exp(-dot(worldPosition.xy * float2(0.095f, 0.10f),
                          worldPosition.xy * float2(0.095f, 0.10f)));
    float diffuse = saturate(dot(N, L));
    float3 illumination = float3(0.32f, 0.34f, 0.37f) +
        float3(0.85f, 0.72f, 0.52f) * (0.25f + diffuse * 0.65f) * pool;
    float occlusion = 1.0f;
    float specular = 0.0f;

#if OBJECT3D_MATERIAL_TYPE == 3
    float pageU = saturate(uv.x);
    float edgeDistance = min(pageU, 1.0f - pageU);
    float edgeShade = exp(-edgeDistance * 36.0f) * 0.08f;
    float contact = saturate(gMaterial.environmentCoefficient);
    float band = exp(-pow((edgeDistance - (0.12f + contact * 0.28f)) / 0.18f, 2.0f));
    occlusion = 1.0f - edgeShade - contact * (0.08f + band * 0.18f);
    illumination += float3(0.12f, 0.10f, 0.07f) * saturate(dot(-N, L));
#else
    float grain = 0.85f + 0.15f * sin(uv.x * 950.0f) * sin(uv.y * 1130.0f);
#if OBJECT3D_MATERIAL_TYPE == 5
    specular = pow(saturate(dot(N, H)), 72.0f) * 0.38f * pool * grain;
#else
    specular = pow(saturate(dot(N, H)), 30.0f) * 0.07f * pool * grain;
#endif
#endif
    return baseColor * illumination * saturate(occlusion) +
        float3(1.0f, 0.78f, 0.42f) * specular;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    gMaterial = materialParameters;
#if defined(KOHAKU_RT_LOCAL_SHADOW_CAPTURE)
    capturedRtLocalLight = 0;
#endif
    PixelShaderOutput output;
#if defined(KOHAKU_RT_REFLECTION_CAPTURE)
    capturedReflectionEnvironment = 0;
#endif
    float3 indirectColor = 0.0f;
    float3 directionalColor = 0.0f;
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    gMaterial = ResolveRasterMaterial(gMaterial, transformedUV.xy);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    if (gMaterial.alphaCutoff > 0) {
        float maskAlpha = gTexture.SampleLevel(gSampler, transformedUV.xy, 0).a;
        if (ShouldRejectAlpha(maskAlpha, gMaterial.color.a, gMaterial.alphaCutoff)) { discard; }
    }
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
    float3 surfaceNormal = ApplyNormalMap(input.worldPosition, input.normal, transformedUV.xy);

#if OBJECT3D_MATERIAL_TYPE == 6
    float3 toonN = surfaceNormal;
    float3 toonV = normalize(gCamera.worldPosition - input.worldPosition);
    indirectColor = ToonSurfaceIndirect(baseColor, toonN, toonV);
    directionalColor = ToonSurfaceDirect(baseColor, toonN, toonV, normalize(-gDirectionalLight.direction));
    output.color.rgb = directionalColor + indirectColor;
    output.color.rgb += ShadeLocalLights(baseColor, toonN, toonV, input.worldPosition, input.normal, true);
#elif OBJECT3D_MATERIAL_TYPE == 2
    float3 N = surfaceNormal;
    float faceLight = saturate(dot(N, normalize(float3(-0.6f, 0.8f, -0.5f))) * 0.5f + 0.5f);
    float middleBand = smoothstep(0.34f, 0.38f, faceLight);
    float lightBand = smoothstep(0.70f, 0.74f, faceLight);
    float3 faceTint = lerp(float3(0.28f, 0.52f, 0.76f), float3(0.60f, 0.77f, 0.90f), middleBand);
    faceTint = lerp(faceTint, float3(0.84f, 0.90f, 0.96f), lightBand);
    float3 iceTexture = lerp(float3(0.80f, 0.88f, 0.95f), textureColor.rgb, 0.52f);
    float horizontal = smoothstep(0.82f, 0.96f, abs(N.y));
    faceTint *= lerp(1.0f.xxx, float3(0.62f, 0.73f, 0.81f), horizontal * 0.72f);
    output.color.rgb = gMaterial.color.rgb * iceTexture * faceTint;
#elif OBJECT3D_MATERIAL_TYPE >= 3 && OBJECT3D_MATERIAL_TYPE <= 5
    output.color.rgb = ShadeArchive(baseColor, surfaceNormal, input.worldPosition, transformedUV.xy);
#elif OBJECT3D_MATERIAL_TYPE == 1
    output.color.rgb = ShadeStandard(baseColor, surfaceNormal, input.worldPosition, input.normal, indirectColor, directionalColor);
#else
    output.color.rgb = baseColor;
#endif

    output.color.a = gMaterial.color.a * textureColor.a;
    // Store hardware depth in alpha so the outline pass can reject a stale
    // normal when a later non-MRT renderer covers this pixel.
    output.encodedNormal = float4(
        normalize(input.normal) * 0.5f + 0.5f,
        input.position.z);
    if (gMaterial.enableEnvironmentMap != 0)
    {
        float3 N = surfaceNormal;
        float3 environmentColor = LegacySurfaceEnvironment(N, normalize(gCamera.worldPosition - input.worldPosition));
        output.color.rgb += environmentColor;
        indirectColor += environmentColor;
#if defined(KOHAKU_RT_REFLECTION_CAPTURE)
        capturedReflectionEnvironment += environmentColor;
#endif
    }
    output.indirectColor = float4(indirectColor, output.color.a);
    output.surfaceMaterial = 0;
#if defined(KOHAKU_RT_CAPTURE)
    output.directionalLight = float4(directionalColor, input.position.z);
#elif defined(KOHAKU_RT_REFLECTION_CAPTURE)
    output.directionalLight = float4(0, 0, 0, -1);
#endif
#if defined(KOHAKU_RT_REFLECTION_CAPTURE)
    output.reflectionSurface = float4(normalize(surfaceNormal) * (1 + clamp(gMaterial.specularStrength, 0, 2)) * 0.5f + 0.5f,
        saturate(gMaterial.roughness));
    output.reflectionEnvironment = float4(capturedReflectionEnvironment, input.position.z);
#endif
#if OBJECT3D_MATERIAL_TYPE == 1 || OBJECT3D_MATERIAL_TYPE == 6
    output.surfaceMaterial = float4(saturate(baseColor), (1 + saturate(gMaterial.metallic) * 254) / 255);
#endif
#if defined(KOHAKU_RT_LOCAL_SHADOW_CAPTURE)
    float hasLocalSpecular = 1;
#if OBJECT3D_MATERIAL_TYPE == 1
    hasLocalSpecular = 0; if (gMaterial.shininess > 0) { hasLocalSpecular = 1; }
#endif
    if (shouldReceiveRtLocalShadow == 0) { hasLocalSpecular = -1; }
    output.localLight = float4(capturedRtLocalLight, hasLocalSpecular);
#endif
    return output;
}
