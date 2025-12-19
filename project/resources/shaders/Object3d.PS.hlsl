#include "object3d.hlsli"
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
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
        
    
    if (gMaterial.enableLighting != 0)//Lightingする場合
    {
        //float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        //output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
        //half lambert
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        float32_t3 reflectDir = normalize(reflect(gDirectionalLight.direction, normalize(input.normal)));
        float RdotE = dot(reflectDir, toEye);
        float specularPow = pow(saturate(RdotE), gMaterial.shininess);
       //拡散反射
        float32_t3 diffuse =gMaterial.color.rgb * textureColor.rgb *gDirectionalLight.color.rgb * cos *gDirectionalLight.intensity;

        //鏡面反射
        float32_t3 specular =gDirectionalLight.color.rgb *gDirectionalLight.intensity *specularPow *float32_t3(1.0f, 1.0f, 1.0f);

        
        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
        
        
    }
    else
    { //Lightingしない場合前回までと同じ計算
        output.color = gMaterial.color * textureColor;
    }
    
    
    return output;
}
