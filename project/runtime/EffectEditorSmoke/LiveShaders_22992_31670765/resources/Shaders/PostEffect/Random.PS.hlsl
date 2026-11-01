#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float rand2dTo1d(float2 value)
{
    float random =
        frac(
            sin(
                dot(
                    value,
                    float2(12.9898f, 78.233f))) *
            43758.5453f);

    return random;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 mainColor = gTexture.Sample(gSampler, input.texcoord);

    // 時間経過でジジジッと高速変化するランダムノイズ値 (0.0 〜 1.0)
    float noise = rand2dTo1d(input.texcoord + float2(time * 17.0f, time * 29.0f));

    // vignetteStrength パラメータをノイズ強度 (0.0 〜 1.0) として受け取り滑らかにブレンド
    float factor = saturate(vignetteStrength);
    float noiseAmount = factor * 0.50f;
    float3 glitchColor = lerp(mainColor.rgb, float3(noise, noise, noise), noiseAmount);

    return float4(glitchColor, mainColor.a);
}
