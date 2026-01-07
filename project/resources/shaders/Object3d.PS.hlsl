#include "object3d.hlsli"
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
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
    
   
    
    if (gMaterial.enableLighting != 0)//Lightingする場合
    {
       
        //point light direction
       
// 法線
        float3 N = normalize(input.normal);

// ライト方向（ライト→物体）
        float3 L = normalize(-gDirectionalLight.direction);
        // または点光源の場合
        
        float3 PL = normalize(input.worldPosition - gPointLight.position);
        float3 pointColor = gPointLight.color.rgb * gPointLight.intensity;
// 視線方向（物体→カメラ）
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);

// Half Vector
        float3 H = normalize(L + V);

// 拡散反射
        float NdotL = saturate(dot(N, L));
        
        float NdotPL = saturate(dot(N, PL));
        float3 pointDiffuse =gMaterial.color.rgb *textureColor.rgb *pointColor *NdotPL;
        float3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;

// 鏡面反射（Blinn-Phong）
        float NdotH = saturate(dot(N, H));
        float specularPow = pow(NdotH, gMaterial.shininess);
        float3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow;
        float3 PH = normalize(PL + V);

        float NdotPH = saturate(dot(N, PH));
        float pointSpecPow = pow(NdotPH, gMaterial.shininess);

        float3 pointSpecular = pointColor * pointSpecPow;
// 合成
        output.color.rgb =diffuse + specular +pointDiffuse + pointSpecular;
        output.color.a = gMaterial.color.a * textureColor.a;

    }
    else
    { //Lightingしない場合前回までと同じ計算
        output.color = gMaterial.color * textureColor;
    }
    
    
    return output;
}
