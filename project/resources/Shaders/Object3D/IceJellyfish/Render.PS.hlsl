#include "../Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);
Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    const float kFaceOpacityScale = 0.86f;
    const float kSpecularStrength = 0.65f;
    const float kRimStrength = 0.10f;

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 textureColor = gTexture.Sample(gSampler, uv);
    float3 N = normalize(input.normal);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);
    float3 L = normalize(-gDirectionalLight.direction);
    float3 halfVector = L + V;
    float3 H = halfVector * rsqrt(max(dot(halfVector, halfVector), 0.000001f));

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float edge = pow(1.0f - NdotV, 3.0f);
    float fresnel = 0.04f + 0.96f * pow(1.0f - NdotV, 5.0f);
    float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity;

    // Retain the ice texture softly so the body does not look like opaque stone.
    float3 textureTint = lerp(float3(0.80f, 0.90f, 1.0f), textureColor.rgb, 0.35f);
    float3 bodyTint = gMaterial.color.rgb * textureTint;
    float3 ambient = gAmbientLight.color.rgb * gAmbientLight.color.a;
    float3 illumination = float3(0.18f, 0.23f, 0.30f) + ambient + lightColor * NdotL * 0.50f;
    float3 color = bodyTint * illumination;

    // A small backlight term suggests light passing through the icy surface.
    float backlight = pow(saturate(dot(-N, L)), 2.0f);
    color += bodyTint * lightColor * backlight * 0.14f;

    float specular = pow(saturate(dot(N, H)), max(gMaterial.shininess, 1.0f));
    specular *= smoothstep(0.0f, 0.15f, NdotL);
    color += lightColor * float3(0.82f, 0.96f, 1.0f) * specular * kSpecularStrength;
    color += float3(0.65f, 0.90f, 1.0f) * edge * kRimStrength;

    if (gMaterial.enableEnvironmentMap != 0)
    {
        float3 reflected = reflect(-V, N);
        float3 environment = gEnvironmentTexture.Sample(gSampler, reflected).rgb;
        color += environment * gMaterial.environmentCoefficient * (0.25f + 0.75f * fresnel);
    }

    PixelShaderOutput output;
    // This material uses the existing sorted, depth-writing transparency mode.
    float opacity = gMaterial.color.a * lerp(kFaceOpacityScale, 1.0f, edge);
    output.color = float4(color, saturate(opacity * textureColor.a));
    output.encodedNormal = float4(N * 0.5f + 0.5f, input.position.z);
    return output;
}
