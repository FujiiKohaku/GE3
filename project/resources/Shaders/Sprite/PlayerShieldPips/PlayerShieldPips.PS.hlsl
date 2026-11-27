#include "../Common/SpritePixelCommon.hlsli"

float4 main(SpriteVertexOutput input) : SV_Target0
{
    const float pipCount = 4.0f;
    const float pipSpacing = 25.0f;
    const float bottomPipY = 37.5f;
    const float2 position = (input.texcoord - 0.5f) * gEffect.spriteSize;
    const float index = round((bottomPipY - position.y) / pipSpacing);
    if (index < 0.0f || index >= pipCount) { discard; }

    // 下から数える。中央2つだけ少し左へ寄せて緩い弧を作る。
    const float centerX = (index == 0.0f || index == 3.0f) ? -49.0f : -57.0f;
    const float2 local = position - float2(centerX, bottomPipY - index * pipSpacing);
    const float2 p = abs(local);
    const float hexDistance = max(p.y, dot(p, float2(0.8660254f, 0.5f)));
    const float edge = max(fwidth(hexDistance), 0.5f);
    const float coverage = 1.0f - smoothstep(8.5f - edge, 8.5f + edge, hexDistance);

    const float fill = saturate(gEffect.threshold * pipCount - index);
    const float interior = 1.0f - smoothstep(6.5f - edge, 6.5f + edge, hexDistance);
    const float fillY = lerp(7.0f, -7.0f, fill);
    const float waterline = smoothstep(fillY - 0.7f, fillY + 0.7f, local.y);
    const float filled = fill >= 1.0f ? 1.0f : waterline * step(0.0001f, fill);
    const float3 cyan = gMaterial.color.rgb;
    const float3 fillColor = fill >= 1.0f ? cyan : cyan * 0.60f;
    float3 color = lerp(float3(0.035f, 0.075f, 0.13f), fillColor, interior * filled);
    const float pipMidpoint = (index + 0.5f) / pipCount;
    if (pipMidpoint >= gEffect.direction.x && pipMidpoint < gEffect.direction.y) {
        color = lerp(color, float3(0.88f, 0.98f, 1.0f), interior * saturate(gEffect.strength));
    }
    return float4(color, coverage * gMaterial.color.a);
}
