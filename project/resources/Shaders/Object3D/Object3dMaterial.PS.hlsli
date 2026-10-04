#include "Object3d.hlsli"
#if defined(KOHAKU_MATERIAL_SHADOWS)
#include "ShadowSampling.hlsli"
#endif

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);
Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);
#include "NormalMapping.PS.hlsli"
#include "LocalLighting.hlsli"
#include "EnvironmentLighting.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
    float32_t4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
};

float3 ShadeStandard(float3 baseColor, float3 normal, float3 worldPosition, float3 geometricNormal, out float3 indirectColor)
{
    float3 N = normalize(normal);
    float3 V = normalize(gCamera.worldPosition - worldPosition);
    float3 ambient = baseColor * HemisphereAmbient(gAmbientLight, N) + EnvironmentLighting(baseColor, N, V, true);
    indirectColor = ambient;
    float3 Ld = normalize(-gDirectionalLight.direction);
    float NdotLd = saturate(dot(N, Ld));
    float visibility = 1.0f;
#if defined(KOHAKU_MATERIAL_SHADOWS)
    visibility = ShadowDirectFactor(SampleShadowVisibility(worldPosition, geometricNormal));
#endif
    float3 result = ambient + baseColor * gDirectionalLight.color.rgb * NdotLd * gDirectionalLight.intensity * visibility * GetDirectLightingStrength(gAmbientLight);
    float3 Hd = normalize(Ld + V);
    if (gMaterial.shininess > 0.0f)
    {
        result += gDirectionalLight.color.rgb * gDirectionalLight.intensity *
            SurfaceSpecular(gMaterial, baseColor, N, V, Ld) * visibility * GetDirectLightingStrength(gAmbientLight);
    }

    result += ShadeLocalLights(baseColor, N, V, worldPosition, geometricNormal, gMaterial.shininess > 0.0f);
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
    PixelShaderOutput output;
    float3 indirectColor = 0.0f;
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
    float3 surfaceNormal = ApplyNormalMap(input.worldPosition, input.normal, transformedUV.xy);

#if OBJECT3D_MATERIAL_TYPE == 6
    output.color.rgb = ShadeToonSurface(baseColor, surfaceNormal,
        gCamera.worldPosition - input.worldPosition, -gDirectionalLight.direction,
        gDirectionalLight.color.rgb, gDirectionalLight.intensity) * GetDirectLightingStrength(gAmbientLight);
    float3 toonN = surfaceNormal;
    float3 toonV = normalize(gCamera.worldPosition - input.worldPosition);
    indirectColor = baseColor * HemisphereAmbient(gAmbientLight, toonN) * 0.30f;
    output.color.rgb += indirectColor;
    output.color.rgb += gDirectionalLight.color.rgb * gDirectionalLight.intensity *
        SurfaceSpecular(gMaterial, baseColor, toonN, toonV, normalize(-gDirectionalLight.direction)) * GetDirectLightingStrength(gAmbientLight);
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
    output.color.rgb = ShadeStandard(baseColor, surfaceNormal, input.worldPosition, input.normal, indirectColor);
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
        float3 reflected = reflect(normalize(input.worldPosition - gCamera.worldPosition), N);
        float3 environmentColor = gEnvironmentTexture.SampleLevel(gSampler, reflected, saturate(gMaterial.roughness) * 5.0f).rgb *
            gMaterial.environmentCoefficient * GetIndirectLightingStrength(gAmbientLight);
        output.color.rgb += environmentColor;
        indirectColor += environmentColor;
    }
    output.indirectColor = float4(indirectColor, output.color.a);
    return output;
}
