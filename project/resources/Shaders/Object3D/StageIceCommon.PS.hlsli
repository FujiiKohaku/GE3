#include "Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct StageIcePixelOutput
{
    float32_t4 color : SV_Target0;
    float32_t4 encodedNormal : SV_Target1;
};

struct StageIceLighting
{
    float3 normal;
    float3 view;
    float lightBand;
    float3 shade;
};

float4 SampleStageIceTexture(VertexShaderOutput input)
{
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    return gTexture.Sample(gSampler, transformedUV.xy);
}

StageIcePixelOutput FinishStageIce(VertexShaderOutput input, float3 color, float textureAlpha)
{
    StageIcePixelOutput output;
    output.color = float4(color, gMaterial.color.a * textureAlpha);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
    return output;
}

StageIceLighting GetStageIceLighting(VertexShaderOutput input)
{
    StageIceLighting lighting;
    lighting.normal = normalize(input.normal);
    lighting.view = normalize(gCamera.worldPosition - input.worldPosition);
    float faceLight = saturate(dot(lighting.normal,
        normalize(float3(-0.6f, 0.8f, -0.5f))) * 0.5f + 0.5f);
    float middleBand = smoothstep(0.35f, 0.37f, faceLight);
    lighting.lightBand = smoothstep(0.71f, 0.73f, faceLight);
    lighting.shade = lerp(float3(0.33f, 0.52f, 0.68f),
        float3(0.70f, 0.84f, 0.93f), middleBand);
    lighting.shade = lerp(lighting.shade,
        float3(0.91f, 0.96f, 1.0f), lighting.lightBand);
    return lighting;
}

float3 ShadeStageIceSurface(StageIceLighting lighting, float3 iceColor,
    float frostCoverage, float rimStrength)
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
    float sharpHighlight = pow(normalHalf, max(gMaterial.shininess, 1.0f)) * lightVisibility;
    float broadHighlight = pow(normalHalf, 12.0f) * lightVisibility;
    float frostHighlight = pow(normalHalf, 18.0f);
    // Keep frost blue and retain the material tint instead of replacing it with white.
    float3 frostColor = lerp(iceColor, lighting.shade * float3(0.52f, 0.72f, 0.84f), 0.45f);
    float3 surface = lerp(iceColor * float3(0.82f, 0.88f, 0.92f), frostColor, frost);
    float clearSurface = 1.0f - frost;
    // Highlights stay visible even on darker material/texture colors.
    surface += float3(0.44f, 0.76f, 1.0f) * glaze * 0.18f * clearSurface;
    surface += float3(0.65f, 0.88f, 1.0f) * fresnel *
        (0.14f + rimStrength * 0.90f) * clearSurface;
    float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
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
        gMaterial.color.rgb * textureColor * blueIce, snow, 0.045f);
}

float3 ShadeStageIceStructure(VertexShaderOutput input, float3 textureColor,
    float3 materialTint, float rimStrength)
{
    StageIceLighting lighting = GetStageIceLighting(input);
    return ShadeStageIceSurface(lighting,
        gMaterial.color.rgb * textureColor * lighting.shade * materialTint,
        0.0f, rimStrength);
}
