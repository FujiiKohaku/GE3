#include "Object3d.hlsli"
#ifndef KOHAKU_NO_SHADOWS
#include "ShadowSampling.hlsli"
#else
float SampleShadowVisibility(float3 worldPosition, float3 normal) { return 1.0f; }
float ShadowDirectFactor(float visibility) { return 1.0f; }
#endif

ConstantBuffer<Material> gMaterial : register(b0);
#include "StageIceLighting.hlsli"
ConstantBuffer<Camera> gCamera : register(b2);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
TextureCube<float4> gEnvironmentTexture : register(t1);
#include "EnvironmentLighting.hlsli"
#include "NormalMapping.PS.hlsli"
#include "LocalLighting.hlsli"

struct StageIcePixelOutput
{
    float32_t4 color : SV_Target0;
    float32_t4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
};

static float3 indirectLightingColor = 0.0f;

struct StageIceLighting
{
    float3 normal;
    float3 view;
    float directVisibility;
    float3 shade;
    float3 indirectShade;
};

float4 SampleStageIceTexture(VertexShaderOutput input)
{
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    return gTexture.Sample(gSampler, transformedUV.xy);
}

StageIcePixelOutput FinishStageIce(VertexShaderOutput input, float3 color, float textureAlpha)
{
    StageIcePixelOutput output;
    float3 normal = normalize(input.normal);
    float3 view = normalize(gCamera.worldPosition - input.worldPosition);
    float3 environmentColor = EnvironmentLighting(gMaterial.color.rgb, normal, view, true);
    color += environmentColor;
    indirectLightingColor += environmentColor;
    color += ShadeLocalLights(gMaterial.color.rgb, normal, view, input.worldPosition, normal, true);
    output.color = float4(color, gMaterial.color.a * textureAlpha);
    output.indirectColor = float4(indirectLightingColor, output.color.a);
    float3 baseColor = gMaterial.color.rgb * SampleStageIceTexture(input).rgb;
    output.surfaceMaterial = float4(saturate(baseColor), (1 + saturate(gMaterial.metallic) * 254) / 255);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
    return output;
}

StageIceLighting GetStageIceLighting(VertexShaderOutput input)
{
    StageIceLighting lighting;
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    lighting.normal = ApplyNormalMap(input.worldPosition, input.normal, uv);
    lighting.view = normalize(gCamera.worldPosition - input.worldPosition);
    lighting.directVisibility = ShadowDirectFactor(SampleShadowVisibility(input.worldPosition, normalize(input.normal)));
    lighting.indirectShade = HemisphereAmbient(gAmbientLight, lighting.normal) * gAmbientLight.componentSettings.z;
    lighting.shade = GetStageIceDiffuseLighting(lighting.normal, lighting.directVisibility);
    return lighting;
}

float3 ShadeStageIceSurface(StageIceLighting lighting, float3 iceColor,
    float frostCoverage, float rimStrength, float3 indirectIceColor)
{
    // A smooth frost layer keeps the surface free of cracks and plate boundaries.
    float frost = saturate(frostCoverage * 0.22f);
    float facing = saturate(dot(lighting.normal, lighting.view));
    float grazing = 1.0f - facing;
    float grazingSquared = grazing * grazing;
    // A broad blue glaze reads across flat faces, without sampling scenery.
    float glaze = 0.06f + 0.94f * grazingSquared;
    float fresnel = 0.04f + 0.96f * grazingSquared * grazingSquared * grazing;
    float3 L = normalize(-gDirectionalLight.direction);
    float3 halfVector = L + lighting.view;
    halfVector *= rsqrt(max(dot(halfVector, halfVector), 0.000001f));
    float normalHalf = saturate(dot(lighting.normal, halfVector));
    float lightVisibility = smoothstep(0.0f, 0.15f, saturate(dot(lighting.normal, L)));
    float sharpHighlight = pow(normalHalf, max(2.0f / max(gMaterial.roughness * gMaterial.roughness, 0.01f) - 2.0f, 1.0f)) * lightVisibility * gMaterial.specularStrength;
    float broadHighlight = pow(normalHalf, 12.0f) * lightVisibility;
    float frostHighlight = pow(normalHalf, 18.0f);
    // Keep frost blue and retain the material tint instead of replacing it with white.
    float3 frostColor = lerp(iceColor, lighting.shade * float3(0.52f, 0.72f, 0.84f), 0.45f);
    float3 surface = lerp(iceColor * float3(0.82f, 0.88f, 0.92f), frostColor, frost);
    float3 indirectFrostColor = lerp(indirectIceColor, lighting.indirectShade * float3(0.52f, 0.72f, 0.84f), 0.45f);
    indirectLightingColor = lerp(indirectIceColor * float3(0.82f, 0.88f, 0.92f), indirectFrostColor, frost);
    float clearSurface = 1.0f - frost;
    // Highlights stay visible even on darker material/texture colors.
    float3 ambientRadiance = GetStageIceAmbientRadiance();
    surface += ambientRadiance * float3(0.75f, 0.90f, 1.0f) * glaze * 0.55f * clearSurface;
    surface += ambientRadiance * fresnel *
        (0.50f + rimStrength * 3.0f) * clearSurface;
    indirectLightingColor += ambientRadiance * float3(0.75f, 0.90f, 1.0f) * glaze * 0.55f * clearSurface;
    indirectLightingColor += ambientRadiance * fresnel * (0.50f + rimStrength * 3.0f) * clearSurface;
    float3 lightColor = GetStageIceDirectRadiance(lighting.directVisibility);
    surface += lightColor * float3(0.82f, 0.96f, 1.0f) *
        ((sharpHighlight * 0.85f + broadHighlight * 0.38f) * clearSurface +
         frostHighlight * 0.025f * frost);
    return surface;
}

float3 ShadeStageIceIsland(VertexShaderOutput input, float3 textureColor)
{
    StageIceLighting lighting = GetStageIceLighting(input);
    float snow = smoothstep(0.72f, 0.88f, lighting.normal.y);
    float3 blueIce = lighting.shade * float3(0.57f, 0.82f, 0.99f);
    return ShadeStageIceSurface(lighting,
        gMaterial.color.rgb * textureColor * blueIce, snow, 0.045f,
        gMaterial.color.rgb * textureColor * lighting.indirectShade * float3(0.57f, 0.82f, 0.99f));
}

float3 ShadeStageIceStructure(VertexShaderOutput input, float3 textureColor,
    float3 materialTint, float rimStrength)
{
    StageIceLighting lighting = GetStageIceLighting(input);
    return ShadeStageIceSurface(lighting,
        gMaterial.color.rgb * textureColor * lighting.shade * materialTint,
        0.0f, rimStrength, gMaterial.color.rgb * textureColor * lighting.indirectShade * materialTint);
}
