#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 outputColor = gTexture.Sample(gSampler, input.texcoord);

    // 画面中心 (0.5, 0.5) からの距離を計算
    float2 centerDist = abs(input.texcoord - float2(0.5f, 0.5f));
    float dist = length(centerDist * float2(1.2f, 1.0f));

    // 画面中央を広く残し、四隅だけに小さくスタイリッシュに赤色が入るように調整 (0.42f 〜 0.88f)
    float mask = smoothstep(0.42f, 0.88f, dist);

    // 脈動・収縮強度 (vignetteStrength)
    float dangerAmount = mask * saturate(vignetteStrength);

    // スタイリッシュで小さめな赤色カラーを四隅だけに端的にブレンド
    float3 redDangerColor = float3(0.95f, 0.08f, 0.08f);
    outputColor.rgb = lerp(outputColor.rgb, redDangerColor, dangerAmount * 0.65f);

    // 四隅の端を少しだけ暗く調整
    outputColor.rgb *= (1.0f - dangerAmount * 0.25f);

    return outputColor;
}