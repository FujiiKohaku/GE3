#include "object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLightCollection> gPointLights : register(b3);
ConstantBuffer<SpotLightCollection> gSpotLights : register(b4);
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

    if (gMaterial.enableLighting == 2)
    {
        // Fixed diffuse shading keeps ice silhouettes readable as the camera moves.
        float3 N = normalize(input.normal);
        float faceLight = saturate(dot(N, normalize(float3(-0.6f, 0.8f, -0.5f))) * 0.5f + 0.5f);
        float3 faceTint = lerp(float3(0.30f, 0.55f, 0.78f), float3(1.0f, 1.0f, 1.0f), faceLight);
        float3 iceTexture = lerp(float3(1.0f, 1.0f, 1.0f), textureColor.rgb, 0.25f);
        output.color = float4(gMaterial.color.rgb * iceTexture * faceTint, gMaterial.color.a * textureColor.a);
    }
    else if (gMaterial.enableLighting >= 3 && gMaterial.enableLighting <= 5)
    {
        // Archive-only materials: paper=3, leather=4, brass=5. Other scenes are unchanged.
        float3 N = normalize(input.normal);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float3 L = normalize(float3(-3.5f, 6.0f, -6.0f) - input.worldPosition);
        float3 H = normalize(L + V);
        float pool = exp(-dot(input.worldPosition.xy * float2(0.095f, 0.10f),
                              input.worldPosition.xy * float2(0.095f, 0.10f)));
        float diffuse = saturate(dot(N, L));
        float3 base = gMaterial.color.rgb * textureColor.rgb;
        float3 illumination = float3(0.32f, 0.34f, 0.37f) +
            float3(0.85f, 0.72f, 0.52f) * (0.25f + diffuse * 0.65f) * pool;
        float occlusion = 1.0f;
        float specular = 0.0f;
        if (gMaterial.enableLighting == 3)
        {
            // Each page now uses one full texture. Shade only the real outer edges;
            // subdividing the UV here would create dark seams across the page.
            // environmentCoefficient carries contact strength only in archive mode.
            float pageU = saturate(transformedUV.x);
            float edgeDistance = min(pageU, 1.0f - pageU);
            float edgeShade = exp(-edgeDistance * 36.0f) * 0.08f;
            float contact = saturate(gMaterial.environmentCoefficient);
            float band = exp(-pow((edgeDistance - (0.12f + contact * 0.28f)) / 0.18f, 2.0f));
            occlusion = 1.0f - edgeShade - contact * (0.08f + band * 0.18f);
            // A little diffuse transmission, without a plastic highlight on paper.
            illumination += float3(0.12f, 0.10f, 0.07f) * saturate(dot(-N, L));
        }
        else
        {
            bool brass = gMaterial.enableLighting == 5;
            float grain = 0.85f + 0.15f * sin(transformedUV.x * 950.0f) * sin(transformedUV.y * 1130.0f);
            specular = pow(saturate(dot(N, H)), brass ? 72.0f : 30.0f) *
                (brass ? 0.38f : 0.07f) * pool * grain;
        }
        output.color = float4(base * illumination * saturate(occlusion) +
            float3(1.0f, 0.78f, 0.42f) * specular, gMaterial.color.a * textureColor.a);
    }
    else if (gMaterial.enableLighting != 0)
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
        // A zero shininess value disables highlights while keeping diffuse lighting.
        float3 dirSpec = gMaterial.shininess > 0.0f
            ? gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(NdotHd, gMaterial.shininess) : float3(0, 0, 0);

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
            if (gMaterial.shininess > 0.0f) {
                pointSpec += pointColor * pow(NdotHp, gMaterial.shininess);
            }
        }

        float3 spotDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float3 spotSpec = float32_t3(0.0f, 0.0f, 0.0f);
        for (uint32_t lightIndex = 0; lightIndex < kMaxSpotLights; ++lightIndex)
        {
            SpotLight spotLight = gSpotLights.lights[lightIndex];
            if (spotLight.isActive == 0)
            {
                continue;
            }

            float3 spotLightDirectionOnSurface = normalize(input.worldPosition - spotLight.position);
            float3 spotLightColor = spotLight.color.rgb * spotLight.intensity;
            float32_t cosAngle = dot(spotLightDirectionOnSurface, spotLight.direction);
            float32_t falloffFactor = saturate((cosAngle - spotLight.cosAngle) / (1.0f - spotLight.cosAngle));
            float distS = length(spotLight.position - input.worldPosition);
            float attenuationFactor = pow(saturate(-distS / spotLight.distance + 1.0f), spotLight.decay);

            spotLightColor *= attenuationFactor * falloffFactor;

            float NdotS = saturate(dot(N, spotLightDirectionOnSurface));
            spotDiffuse += baseColor * spotLightColor * NdotS;

            float3 Hs = normalize(spotLightDirectionOnSurface + V);
            float NdotHs = saturate(dot(N, Hs));
            if (gMaterial.shininess > 0.0f) {
                spotSpec += spotLightColor * pow(NdotHs, gMaterial.shininess);
            }
        }

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
