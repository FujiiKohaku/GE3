#include "object3d.hlsli"
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
    output.color = gMaterial.color;

    //これは不要同じスコープで二回宣言するとエラーになるからねー06_01
    //float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // UV座標を同次座標系に拡張して（x, y, 1.0）、アフィン変換を適用する
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    // 変換後のUV座標を使ってテクスチャから色をサンプリングする
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
        
    float32_t3 pointLightDirection = normalize(input.worldPosition - gPointLight.position);
    
    float32_t distance = length(gPointLight.position - input.worldPosition);
    
    float32_t factor = pow(saturate(-distance / gPointLight.radius + 1.0), gPointLight.decay);

   
    

    if (gMaterial.enableLighting != 0)//Lightingする場合
    {
       
       
       
// 法線
        float3 N = normalize(input.normal);

// 視線方向（物体→カメラ）
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);

//
// ---- Directional Light ----
//
        float3 Ld = normalize(-gDirectionalLight.direction);

// Diffuse
        float NdotLd = saturate(dot(N, Ld));
        float3 dirDiffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * NdotLd * gDirectionalLight.intensity;

// Specular (Blinn-Phong)
        float3 Hd = normalize(Ld + V);
        float NdotHd = saturate(dot(N, Hd));
        float3 dirSpec = gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(NdotHd, gMaterial.shininess);

//
// ---- point Light ----
//

        float3 Lp = normalize(gPointLight.position - input.worldPosition);

        float dist = length(gPointLight.position - input.worldPosition);
        float decayF = pow(saturate(-dist / gPointLight.radius + 1.0), gPointLight.decay);
        float3 pointColor = gPointLight.color.rgb * gPointLight.intensity * decayF;

        float NdotLp = saturate(dot(N, Lp));
        float3 pointDiffuse = gMaterial.color.rgb * textureColor.rgb * pointColor * NdotLp;

        float3 Hp = normalize(Lp + V);
        float NdotHp = saturate(dot(N, Hp));
        float3 pointSpec = pointColor * pow(NdotHp, gMaterial.shininess);

        
//
//--------spotLight----
//
        
        float3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);

        float3 spotLightColor = gSpotLight.color.rgb * gSpotLight.intensity;

        float32_t cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);

        float32_t falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (1.0 - gSpotLight.cosAngle));

        float distS = length(gSpotLight.position - input.worldPosition);
        float attenuationFactor = pow(saturate(-distS / gSpotLight.distance + 1.0), gSpotLight.decay);

        spotLightColor *= attenuationFactor * falloffFactor;

        float NdotS = saturate(dot(N, -spotLightDirectionOnSurface));
        float3 spotDiffuse = gMaterial.color.rgb * textureColor.rgb * spotLightColor * NdotS;

        float3 Hs = normalize(-spotLightDirectionOnSurface + V);
        float NdotHs = saturate(dot(N, Hs));
        float3 spotSpec = spotLightColor * pow(NdotHs, gMaterial.shininess);


// ---- 合成 ----
//
        output.color.rgb = dirDiffuse + dirSpec + pointDiffuse + pointSpec + spotDiffuse + spotSpec;
        output.color.a = gMaterial.color.a * textureColor.a;

    }
    else
    { //Lightingしない場合前回までと同じ計算
        output.color = gMaterial.color * textureColor;
    }
    
    
    return output;
}