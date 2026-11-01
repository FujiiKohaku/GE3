#include "../Object3d.hlsli"
#include "../ShadowSampling.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);
Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);
#include "../NormalMapping.PS.hlsli"
#include "../LocalLighting.hlsli"
#include "../EnvironmentLighting.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
};

struct JellyfishPixelInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float3 surface : TEXCOORD1;
};

PixelShaderOutput main(JellyfishPixelInput input)
{
    const float kFaceOpacityScale = 0.86f;
    const float kSpecularStrength = 0.65f;
    const float kRimStrength = 0.10f;

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 textureColor = gTexture.Sample(gSampler, uv);
    float3 N = ApplyNormalMap(input.worldPosition, input.normal, uv);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);
    float3 L = normalize(-gDirectionalLight.direction);
    float3 halfVector = L + V;
    float3 H = halfVector * rsqrt(max(dot(halfVector, halfVector), 0.000001f));

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float edge = pow(1.0f - NdotV, 3.0f);
    float fresnel = 0.04f + 0.96f * pow(1.0f - NdotV, 5.0f);
    float visibility = ShadowDirectFactor(SampleShadowVisibility(input.worldPosition, normalize(input.normal)));
    float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity * visibility * GetDirectLightingStrength(gAmbientLight);

    // Retain the ice texture softly so the body does not look like opaque stone.
    float3 textureTint = lerp(float3(0.80f, 0.90f, 1.0f), textureColor.rgb, 0.35f);
    float3 bodyTint = gMaterial.color.rgb * textureTint;
    float3 ambient = HemisphereAmbient(gAmbientLight, N);
    float3 illumination = float3(0.18f, 0.23f, 0.30f) * GetIndirectLightingStrength(gAmbientLight) + ambient + lightColor * NdotL * 0.50f;
    float3 indirectColor = bodyTint * (float3(0.18f, 0.23f, 0.30f) * GetIndirectLightingStrength(gAmbientLight) + ambient);
    float3 color = bodyTint * illumination;

    // A small backlight term suggests light passing through the icy surface.
    float backlight = pow(saturate(dot(-N, L)), 2.0f);
    color += bodyTint * lightColor * backlight * 0.14f;

    float specular = pow(saturate(dot(N, H)), max(gMaterial.shininess, 1.0f));
    specular *= smoothstep(0.0f, 0.15f, NdotL);
    color += lightColor * float3(0.82f, 0.96f, 1.0f) *
        SurfaceSpecular(gMaterial, bodyTint, N, V, L);
    if (input.surface.z > 0.5f) {
        float broadGlaze = pow(saturate(dot(N, H)), 24.0f);
        float flowingGlaze = 0.85f + 0.15f * sin(input.texcoord.y * 5.0f - input.surface.x * 1.2f + input.surface.y);
        color += lightColor * float3(0.68f, 0.88f, 1.0f) * broadGlaze * flowingGlaze * 0.38f;
    }
    float3 rimColor = float3(0.65f, 0.90f, 1.0f) * edge * kRimStrength * GetIndirectLightingStrength(gAmbientLight);
    color += rimColor;
    indirectColor += rimColor;

    if (gMaterial.enableEnvironmentMap != 0)
    {
        float3 reflected = reflect(-V, N);
        float3 environment = gEnvironmentTexture.SampleLevel(gSampler, reflected, saturate(gMaterial.roughness) * 5.0f).rgb;
        float3 environmentColor = environment * gMaterial.environmentCoefficient * (0.25f + 0.75f * fresnel) * GetIndirectLightingStrength(gAmbientLight);
        color += environmentColor;
        indirectColor += environmentColor;
    }

    float3 environmentLightingColor = EnvironmentLighting(bodyTint, N, V, true);
    color += environmentLightingColor;
    indirectColor += environmentLightingColor;
    color += ShadeLocalLights(bodyTint, N, V, input.worldPosition, normalize(input.normal), true);

    PixelShaderOutput output;
    // This material uses the existing sorted, depth-writing transparency mode.
    float opacity = gMaterial.color.a * lerp(kFaceOpacityScale, 1.0f, edge);
    output.color = float4(color, saturate(opacity * textureColor.a));
    output.indirectColor = float4(indirectColor, output.color.a);
    output.surfaceMaterial = float4(saturate(bodyTint * output.color.a), (1 + saturate(gMaterial.metallic) * 254) / 255);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
    if (input.surface.z > 0.5f) {
        // Tag tentacle normals so outline preserves the silhouette but softens internal joints.
        output.encodedNormal.a = -2.0f - input.position.z;
    }
    return output;
}
