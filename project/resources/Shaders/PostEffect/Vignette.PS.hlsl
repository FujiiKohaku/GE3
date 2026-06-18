#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 outputColor = gTexture.Sample(gSampler, input.texcoord);
  //周囲を０に中心になるほど明るくなるように計算で調整
    float32_t2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    //correctだけで計算すると中心の最大値が暗すぎるのでスケールで調整
    float vignette = correct.x * correct.y * 15.0f;
    //とりあえず0.8乗で明るくしてみた
    vignette = saturate(pow(vignette, 0.8f));
    //係数として乗算
    outputColor.rgb *= lerp(1.0f, vignette, vignetteStrength);
    
    return outputColor;
}