#include "Particle.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);//グローバル変数にする
SamplerState gSampler : register(s0);
Texture2D<float32_t4> gTexture : register(t1);
PixelShaderOutput main(VertexShaderOutput input)
{
    
    PixelShaderOutput output;

    // UV変換は Sprite と同じ（使わないなら input.texcoord を直接使う）
    float32_t4 uv = mul(float32_t4(input.texcoord, 0, 1), gMaterial.uvTransform);

    // テクスチャ読み込み
    float32_t4 texColor = gTexture.Sample(gSampler, uv.xy);

    // 色を掛ける（gMaterial.color は頂点カラー）
    output.color = gMaterial.color * texColor * input.color;

    // αが0なら描画しない（透明な部分を捨てる）
    if (output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}
