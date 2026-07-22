#include "object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLightCollection> gPointLights : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);

Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);

SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gMaterial.color;

    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.enableLighting != 0)
    {
        float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
        float3 N = normalize(input.normal);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float3 ambient = baseColor * gAmbientLight.color.rgb * gAmbientLight.color.a;

        float3 Ld = normalize(-gDirectionalLight.direction);

        float NdotLd = saturate(dot(N, Ld));
        float3 dirDiffuse = baseColor * gDirectionalLight.color.rgb * NdotLd * gDirectionalLight.intensity;

        float3 Hd = normalize(Ld + V);
        float NdotHd = saturate(dot(N, Hd));
        float3 dirSpec = gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(NdotHd, gMaterial.shininess);

        float3 pointDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float3 pointSpec = float32_t3(0.0f, 0.0f, 0.0f);
        for (uint32_t lightIndex = 0; lightIndex < kMaxPointLights; ++lightIndex)
        {
            PointLight pointLight = gPointLights.lights[lightIndex];
            if (pointLight.isActive == 0)
            {
                continue;
            }

            float3 Lp = normalize(input.worldPosition - pointLight.position);
            float dist = length(pointLight.position - input.worldPosition);
            float decayF = pow(saturate(-dist / pointLight.radius + 1.0f), pointLight.decay);
            float3 pointColor = pointLight.color.rgb * pointLight.intensity * decayF;

            float NdotLp = saturate(dot(N, Lp));
            pointDiffuse += baseColor * pointColor * NdotLp;

            float3 Hp = normalize(Lp + V);
            float NdotHp = saturate(dot(N, Hp));
            pointSpec += pointColor * pow(NdotHp, gMaterial.shininess);
        }

        float3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
        float3 spotLightColor = gSpotLight.color.rgb * gSpotLight.intensity;

        float32_t cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
        float32_t falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (1.0f - gSpotLight.cosAngle));

        float distS = length(gSpotLight.position - input.worldPosition);
        float attenuationFactor = pow(saturate(-distS / gSpotLight.distance + 1.0f), gSpotLight.decay);

        spotLightColor *= attenuationFactor * falloffFactor;

        float NdotS = saturate(dot(N, spotLightDirectionOnSurface));
        float3 spotDiffuse = baseColor * spotLightColor * NdotS;

        float3 Hs = normalize(spotLightDirectionOnSurface + V);
        float NdotHs = saturate(dot(N, Hs));
        float3 spotSpec = spotLightColor * pow(NdotHs, gMaterial.shininess);

        output.color.rgb = ambient + dirDiffuse + dirSpec + pointDiffuse + pointSpec + spotDiffuse + spotSpec;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    if (gMaterial.enableEnvironmentMap != 0)
    {
        float3 N = normalize(input.normal);
        float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraToPosition, N);

        float4 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);

        output.color.rgb += environmentColor.rgb * gMaterial.environmentCoefficient;
    }

    return output;
}
