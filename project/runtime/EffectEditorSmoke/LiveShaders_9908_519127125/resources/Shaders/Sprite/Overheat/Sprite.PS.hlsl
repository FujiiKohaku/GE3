#include "../Common/SpritePixelCommon.hlsli"

static const float kPi = 3.14159265359f;
static const float kDotCount = 24.0f;
static const float kOutlineWidthPixels = 1.0f;

float4 main(SpriteVertexOutput input) : SV_Target0
{
    float2 position = (input.texcoord - 0.5f) * gEffect.spriteSize;
    float radius = length(position);
    float angle = atan2(position.x, -position.y);
    float progress = frac(angle / (2.0f * kPi) + 1.0f);
    float heatRatio = saturate(gEffect.threshold);
    float dotIndex = floor(progress * kDotCount + 0.5f);
    float dotAngle = dotIndex * (2.0f * kPi / kDotCount);
    float2 dotPosition = float2(sin(dotAngle), -cos(dotAngle)) * 40.0f;
    float dotDistance = length(position - dotPosition);
    float lineDistance = abs(radius - 34.0f);
    float dotMask = 1.0f - smoothstep(1.5f, 2.3f, dotDistance);
    float lineMask = 1.0f - smoothstep(0.7f, 1.7f, lineDistance);
    float dotOutlineMask = 1.0f - smoothstep(
        1.5f + kOutlineWidthPixels, 2.3f + kOutlineWidthPixels, dotDistance);
    float lineOutlineMask = 1.0f - smoothstep(
        0.7f + kOutlineWidthPixels, 1.7f + kOutlineWidthPixels, lineDistance);
    float activeDotCount = ceil(heatRatio * kDotCount);
    float isDotActive = 0.0f;
    if (heatRatio > 0.0f && fmod(dotIndex, kDotCount) < activeDotCount) {
        isDotActive = 1.0f;
    }
    float isLineActive = 0.0f;
    if (heatRatio > 0.0f && progress <= heatRatio) {
        isLineActive = 1.0f;
    }
    float3 heatColor = lerp(float3(0.95f, 0.97f, 1.0f),
        float3(1.0f, 0.78f, 0.12f), saturate(heatRatio * 2.0f));
    if (heatRatio > 0.5f) {
        heatColor = lerp(float3(1.0f, 0.78f, 0.12f),
            float3(1.0f, 0.08f, 0.04f), (heatRatio - 0.5f) * 2.0f);
    }
    float pulse = 1.0f;
    if (gEffect.strength > 0.5f) {
        heatColor = float3(1.0f, 0.08f, 0.04f);
        pulse = 0.65f + 0.35f * sin(gFrame.elapsedTime * 12.0f);
    }
    float activeMask = max(dotMask * isDotActive, lineMask * isLineActive);
    float trackMask = max(dotMask * 0.80f, lineMask * 0.65f);
    float alpha = max(trackMask, activeMask * pulse);
    float3 color = float3(0.90f, 0.94f, 1.0f);
    if (activeMask > 0.0f) {
        color = heatColor;
    }
    float centerMask = 1.0f - smoothstep(1.2f, 2.2f, radius);
    float2 absolutePosition = abs(position);
    float horizontalMask = (1.0f - smoothstep(0.5f, 1.3f, absolutePosition.y)) *
        smoothstep(6.0f, 7.0f, absolutePosition.x) *
        (1.0f - smoothstep(14.0f, 15.0f, absolutePosition.x));
    float verticalMask = (1.0f - smoothstep(0.5f, 1.3f, absolutePosition.x)) *
        smoothstep(6.0f, 7.0f, absolutePosition.y) *
        (1.0f - smoothstep(14.0f, 15.0f, absolutePosition.y));
    float aimMask = max(centerMask, max(horizontalMask, verticalMask));
    if (aimMask > 0.0f) {
        color = float3(0.95f, 0.97f, 1.0f);
        alpha = max(alpha, aimMask);
    }
    float centerOutlineMask = 1.0f - smoothstep(
        1.2f + kOutlineWidthPixels, 2.2f + kOutlineWidthPixels, radius);
    float horizontalOutlineMask = (1.0f - smoothstep(
        0.5f + kOutlineWidthPixels, 1.3f + kOutlineWidthPixels, absolutePosition.y)) *
        smoothstep(6.0f - kOutlineWidthPixels, 7.0f - kOutlineWidthPixels, absolutePosition.x) *
        (1.0f - smoothstep(14.0f + kOutlineWidthPixels, 15.0f + kOutlineWidthPixels, absolutePosition.x));
    float verticalOutlineMask = (1.0f - smoothstep(
        0.5f + kOutlineWidthPixels, 1.3f + kOutlineWidthPixels, absolutePosition.x)) *
        smoothstep(6.0f - kOutlineWidthPixels, 7.0f - kOutlineWidthPixels, absolutePosition.y) *
        (1.0f - smoothstep(14.0f + kOutlineWidthPixels, 15.0f + kOutlineWidthPixels, absolutePosition.y));
    float outlineMask = max(max(dotOutlineMask, lineOutlineMask),
        max(centerOutlineMask, max(horizontalOutlineMask, verticalOutlineMask)));
    // Composite the visible strokes over a black outline using straight alpha.
    float combinedAlpha = alpha + outlineMask * (1.0f - alpha);
    if (combinedAlpha > 0.0f) {
        color *= alpha / combinedAlpha;
    }
    alpha = combinedAlpha;
    return float4(color, alpha);
}
