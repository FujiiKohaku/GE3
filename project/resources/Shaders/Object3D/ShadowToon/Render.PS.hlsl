#include "../Object3d.hlsli"
#include "../ShadowSampling.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
#include "../NormalMapping.PS.hlsli"
#include "../LocalLighting.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 textureColor = gTexture.Sample(gSampler, uv);
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
    float3 N = ApplyNormalMap(input.worldPosition, input.normal, uv);
    float3 L = normalize(-gDirectionalLight.direction);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);
    float faceLight = saturate(dot(N, L) * 0.5f + 0.5f);
    float middleBand = smoothstep(0.34f, 0.38f, faceLight);
    float lightBand = smoothstep(0.68f, 0.72f, faceLight);
    float diffuse = lerp(0.10f, 0.65f, middleBand);
    diffuse = lerp(diffuse, 1.0f, lightBand);
    float visibility = ShadowDirectFactor(SampleShadowVisibility(input.worldPosition, normalize(input.normal)));
    float3 ambient = HemisphereAmbient(gAmbientLight, N) * 1.2f;
    float3 direct = max(gDirectionalLight.color.rgb, 0.0f) *
        max(gDirectionalLight.intensity, 0.0f) * visibility;
    float3 color = baseColor * (ambient + direct * diffuse);

    // Keep the title's graphic highlight, using the same light and shadow as the surface.
    float3 H = L + V;
    H *= rsqrt(max(dot(H, H), 0.000001f));
    float highlight = step(0.955f, saturate(dot(N, H))) * saturate(dot(N, L));
    float rim = smoothstep(0.72f, 0.88f, 1.0f - saturate(dot(N, V)));
    color += direct * SurfaceSpecular(gMaterial, baseColor, N, V, L);
    color += baseColor * ambient * rim * 0.12f;

    color += ShadeLocalLights(baseColor, N, V, input.worldPosition, normalize(input.normal), true);

    PixelShaderOutput output;
    output.color = float4(color, gMaterial.color.a * textureColor.a);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
    return output;
}
