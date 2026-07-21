#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 outputColor = gTexture.Sample(gSampler, input.texcoord);

    // 画面中心 (0.5, 0.5) からの距離を計算 (アスペクト比考慮)
    float2 centerDist = abs(input.texcoord - float2(0.5f, 0.5f));
    float dist = length(centerDist * float2(1.25f, 1.0f));
    
    // 中心はクリア(0.0)、四隅に向かって鮮やかに広がるグラデーション (0.0 〜 1.0)
    float mask = smoothstep(0.18f, 0.72f, dist);
    
    // 脈動・収縮強度 (vignetteStrength)
    float dangerAmount = mask * saturate(vignetteStrength);
    
    // 鮮やかな赤色危険カラー (Red Danger Color) を四隅に強烈ブレンド
    float3 redDangerColor = float3(1.0f, 0.05f, 0.05f);
    outputColor.rgb = lerp(outputColor.rgb, redDangerColor, dangerAmount * 0.88f);
    
    // 四隅の端ほど暗く落として緊迫感を強調
    outputColor.rgb *= (1.0f - dangerAmount * 0.45f);
    
    return outputColor;
}