#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;

    // 画面中央（照準・レティクルエリア）は色収差を完全にゼロ(0.0)に保護し、画面外周に向かって放射状に色ズレを拡大！
    float2 center = float2(0.5f, 0.5f);
    float distFromCenter = length((uv - center) * float2(1.25f, 1.0f));

    // 中央から離れるほど色収差が強まる滑らかなイージングマスク（元のバランスの取れた数値 0.015f に復元）
    float aberrationAmount = smoothstep(0.15f, 0.65f, distFromCenter) * 0.015f;

    if (aberrationAmount <= 0.0001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 放射状のレンズ収差方向ベクトル
    float2 dir = normalize(uv - center);

    // R (Red) チャンネルを外側、B (Blue) チャンネルを内側にわずかにシフト
    float r = gTexture.Sample(gSampler, uv + dir * aberrationAmount).r;
    float g = gTexture.Sample(gSampler, uv).g;
    float b = gTexture.Sample(gSampler, uv - dir * aberrationAmount).b;
    float a = gTexture.Sample(gSampler, uv).a;

    return float4(r, g, b, a);
}
