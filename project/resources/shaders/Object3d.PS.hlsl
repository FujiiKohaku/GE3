#include "Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        return output;
    }

    float3 N = normalize(input.normal);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    float3 resultColor = float3(0.0f, 0.0f, 0.0f);

    // ==================================================
    // Directional Light
    // ==================================================
    float3 Ld = normalize(-gDirectionalLight.direction);

    float NdotLd = saturate(dot(N, Ld));
    float3 dirDiffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotLd;

    float3 Hd = normalize(Ld + V);
    float NdotHd = saturate(dot(N, Hd));
    float3 dirSpec = gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(NdotHd, gMaterial.shininess);

    resultColor += dirDiffuse + dirSpec;

    // ==================================================
    // Point Light
    // ==================================================
    float3 Lp = normalize(gPointLight.position - input.worldPosition);

    float distP = length(gPointLight.position - input.worldPosition);
    float attenuationP =
        pow(saturate(-distP / gPointLight.radius + 1.0f), gPointLight.decay);

    float3 pointLightColor = gPointLight.color.rgb * gPointLight.intensity * attenuationP;

    float NdotLp = saturate(dot(N, Lp));
    float3 pointDiffuse = gMaterial.color.rgb * textureColor.rgb * pointLightColor * NdotLp;

    float3 Hp = normalize(Lp + V);
    float NdotHp = saturate(dot(N, Hp));
    float3 pointSpec = pointLightColor * pow(NdotHp, gMaterial.shininess);

    resultColor += pointDiffuse + pointSpec;

    // ==================================================
// Spot Light
// ==================================================

    float3 S = normalize(input.worldPosition - gSpotLight.position);


    float3 Ls = -S;

    float cosAngle = dot(-S, gSpotLight.direction);

    float falloffFactor =
    saturate((cosAngle - gSpotLight.cosAngle) / (1.0f - gSpotLight.cosAngle));

    float distS = length(gSpotLight.position - input.worldPosition);
    float attenuationS =
    pow(saturate(-distS / gSpotLight.distance + 1.0f), gSpotLight.decay);

    float3 spotLightColor =
    gSpotLight.color.rgb *
    gSpotLight.intensity *
    attenuationS *
    falloffFactor;


    float NdotLs = abs(dot(N, Ls));
    float3 spotDiffuse =
    gMaterial.color.rgb *
    textureColor.rgb *
    spotLightColor *
    NdotLs;

// SpecularÅi
    float3 Hs = normalize(Ls + V);
    float NdotHs = saturate(dot(N, Hs));
    float3 spotSpec =
    spotLightColor *
    pow(NdotHs, gMaterial.shininess);

    resultColor += spotDiffuse + spotSpec;

    
    
    
    //---------------------------------------------------------
    output.color.rgb = resultColor;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}