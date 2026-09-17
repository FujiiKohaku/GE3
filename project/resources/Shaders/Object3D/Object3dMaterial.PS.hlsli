#include "Object3d.hlsli"

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
    float32_t4 encodedNormal : SV_Target1;
};

float3 ShadeStandard(float3 baseColor, float3 normal, float3 worldPosition)
{
    float3 N = normalize(normal);
    float3 V = normalize(gCamera.worldPosition - worldPosition);
    float3 ambient = baseColor * gAmbientLight.color.rgb * gAmbientLight.color.a;
    float3 Ld = normalize(-gDirectionalLight.direction);
    float NdotLd = saturate(dot(N, Ld));
    float3 result = ambient + baseColor * gDirectionalLight.color.rgb * NdotLd * gDirectionalLight.intensity;
    float3 Hd = normalize(Ld + V);
    if (gMaterial.shininess > 0.0f)
    {
        result += gDirectionalLight.color.rgb * gDirectionalLight.intensity *
            pow(saturate(dot(N, Hd)), gMaterial.shininess);
    }

    for (uint32_t i = 0; i < kMaxPointLights; ++i)
    {
        PointLight light = gPointLights.lights[i];
        if (light.isActive == 0) continue;
        float3 L = normalize(worldPosition - light.position);
        float attenuation = pow(saturate(-length(light.position - worldPosition) / light.radius + 1.0f), light.decay);
        float3 lightColor = light.color.rgb * light.intensity * attenuation;
        result += baseColor * lightColor * saturate(dot(N, L));
        if (gMaterial.shininess > 0.0f)
        {
            result += lightColor * pow(saturate(dot(N, normalize(L + V))), gMaterial.shininess);
        }
    }

    for (uint32_t i = 0; i < kMaxSpotLights; ++i)
    {
        SpotLight light = gSpotLights.lights[i];
        if (light.isActive == 0) continue;
        float3 L = normalize(worldPosition - light.position);
        float cosAngle = dot(L, light.direction);
        float falloff = saturate((cosAngle - light.cosAngle) / (1.0f - light.cosAngle));
        float attenuation = pow(saturate(-length(light.position - worldPosition) / light.distance + 1.0f), light.decay);
        float3 lightColor = light.color.rgb * light.intensity * attenuation * falloff;
        result += baseColor * lightColor * saturate(dot(N, L));
        if (gMaterial.shininess > 0.0f)
        {
            result += lightColor * pow(saturate(dot(N, normalize(L + V))), gMaterial.shininess);
        }
    }
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
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;

#if OBJECT3D_MATERIAL_TYPE == 6
    output.color.rgb = ShadeToonSurface(baseColor, input.normal,
        gCamera.worldPosition - input.worldPosition, -gDirectionalLight.direction,
        gDirectionalLight.color.rgb, gDirectionalLight.intensity);
#elif OBJECT3D_MATERIAL_TYPE == 2
    float3 N = normalize(input.normal);
    float faceLight = saturate(dot(N, normalize(float3(-0.6f, 0.8f, -0.5f))) * 0.5f + 0.5f);
    float middleBand = smoothstep(0.34f, 0.38f, faceLight);
    float lightBand = smoothstep(0.70f, 0.74f, faceLight);
    float3 faceTint = lerp(float3(0.28f, 0.52f, 0.76f), float3(0.65f, 0.86f, 1.0f), middleBand);
    faceTint = lerp(faceTint, float3(1.0f, 1.0f, 1.0f), lightBand);
    float3 iceTexture = lerp(float3(0.88f, 0.94f, 1.0f), textureColor.rgb, 0.52f);
    float horizontal = smoothstep(0.82f, 0.96f, abs(N.y));
    float wave = sin(input.worldPosition.x * 0.037f + sin(input.worldPosition.z * 0.021f) * 2.2f);
    float glacierBand = wave < -0.28f ? 0.68f : (wave > 0.34f ? 0.94f : 0.81f);
    faceTint *= lerp(1.0f.xxx, float3(0.76f, 0.88f, 1.0f) * glacierBand, horizontal * 0.72f);
    output.color.rgb = gMaterial.color.rgb * iceTexture * faceTint;
#elif OBJECT3D_MATERIAL_TYPE >= 3 && OBJECT3D_MATERIAL_TYPE <= 5
    output.color.rgb = ShadeArchive(baseColor, input.normal, input.worldPosition, transformedUV.xy);
#elif OBJECT3D_MATERIAL_TYPE == 1
    output.color.rgb = ShadeStandard(baseColor, input.normal, input.worldPosition);
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
        float3 N = normalize(input.normal);
        float3 reflected = reflect(normalize(input.worldPosition - gCamera.worldPosition), N);
        output.color.rgb += gEnvironmentTexture.Sample(gSampler, reflected).rgb *
            gMaterial.environmentCoefficient;
    }
    return output;
}
