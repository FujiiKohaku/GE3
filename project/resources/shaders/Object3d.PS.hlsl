#include "object3d.hlsli"
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};


PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gMaterial.color;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);

        float3 Ld = normalize(-gDirectionalLight.direction);
        float NdotLd = saturate(dot(N, Ld));
        float3 dirDiffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * NdotLd * gDirectionalLight.intensity;

        float3 Hd = normalize(Ld + V);
        float NdotHd = saturate(dot(N, Hd));
        float3 dirSpec = gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(NdotHd, gMaterial.shininess);

        float3 Lp = normalize(gPointLight.position - input.worldPosition);
        float dist = length(gPointLight.position - input.worldPosition);
        float decayF = pow(saturate(-dist / gPointLight.radius + 1.0f), gPointLight.decay);
        float3 pointColor = gPointLight.color.rgb * gPointLight.intensity * decayF;

        float NdotLp = saturate(dot(N, Lp));
        float3 pointDiffuse = gMaterial.color.rgb * textureColor.rgb * pointColor * NdotLp;

        float3 Hp = normalize(Lp + V);
        float NdotHp = saturate(dot(N, Hp));
        float3 pointSpec = pointColor * pow(NdotHp, gMaterial.shininess);

        float3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
        float3 spotLightColor = gSpotLight.color.rgb * gSpotLight.intensity;

        float cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
        float falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (1.0f - gSpotLight.cosAngle));

        float distS = length(gSpotLight.position - input.worldPosition);
        float attenuationFactor = pow(saturate(-distS / gSpotLight.distance + 1.0f), gSpotLight.decay);

        spotLightColor *= attenuationFactor * falloffFactor;

        float NdotS = saturate(dot(N, -spotLightDirectionOnSurface));
        float3 spotDiffuse = gMaterial.color.rgb * textureColor.rgb * spotLightColor * NdotS;

        float3 Hs = normalize(-spotLightDirectionOnSurface + V);
        float NdotHs = saturate(dot(N, Hs));
        float3 spotSpec = spotLightColor * pow(NdotHs, gMaterial.shininess);

        output.color.rgb = dirDiffuse + dirSpec + pointDiffuse + pointSpec + spotDiffuse + spotSpec;
        output.color.a = gMaterial.color.a * textureColor.a;

        if (gMaterial.enableEnvironmentMap != 0)
        {
            float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
            float3 reflectedVector = reflect(cameraToPosition, N);
            float4 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);

            output.color.rgb += environmentColor.rgb * gMaterial.environmentCoefficient;
        }
    }
    else
    {
        output.color = gMaterial.color * textureColor;

        if (gMaterial.enableEnvironmentMap != 0)
        {
            float3 N = normalize(input.normal);
            float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
            float3 reflectedVector = reflect(cameraToPosition, N);
            float4 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);

            output.color.rgb += environmentColor.rgb * gMaterial.environmentCoefficient;
          
        }
    }

    return output;
}