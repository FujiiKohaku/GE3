#include "../Common/SpritePixelCommon.hlsli"
#include "../Common/ReticleStyle.hlsli"

static const float kPi = 3.14159265359f;
static const float kDotCount = 24.0f;
static const float kOutlineWidthPixels = kReticleOutlineWidthPixels;
static const float3 kLowHeatColor = float3(1.0f, 0.62f, 0.02f);
static const float3 kMidHeatColor = float3(1.0f, 0.32f, 0.02f);
static const float3 kOverheatColor = float3(1.0f, 0.08f, 0.04f);
static const float kHeatWarningThreshold = 0.8f;

float4 main(SpriteVertexOutput input) : SV_Target0
{
    float2 position = (input.texcoord - 0.5f) * gEffect.spriteSize;
    float radius = length(position);
    float angle = atan2(position.x, -position.y);
    float progress = frac(angle / (2.0f * kPi) + 1.0f);
    float heatRatio = saturate(gEffect.threshold);
    bool isOverheated = gEffect.strength > 0.5f;
    float readyFlash = saturate(gEffect.phase);
    float dotIndex = floor(progress * kDotCount + 0.5f);
    float dotAngle = dotIndex * (2.0f * kPi / kDotCount);
    float2 dotPosition = float2(sin(dotAngle), -cos(dotAngle)) * 40.0f;
    float dotDistance = length(position - dotPosition);
    float lineDistance = abs(radius - 34.0f);
    float dotMask = 1.0f - smoothstep(1.5f, 2.3f, dotDistance);
    float lineMask = ReticleStrokeMask(lineDistance);
    float dotOutlineMask = 1.0f - smoothstep(
        1.5f + kOutlineWidthPixels, 2.3f + kOutlineWidthPixels, dotDistance);
    float lineOutlineMask = ReticleOutlineMask(lineDistance);
    float activeDotCount = ceil(heatRatio * kDotCount);
    float isDotActive = 0.0f;
    if (heatRatio > 0.0f && fmod(dotIndex, kDotCount) < activeDotCount) {
        isDotActive = 1.0f;
    }
    float isLineActive = 0.0f;
    if (heatRatio > 0.0f && progress <= heatRatio) {
        isLineActive = 1.0f;
    }
    float3 heatColor = lerp(kLowHeatColor,
        kMidHeatColor, saturate(heatRatio * 2.0f));
    if (heatRatio > 0.5f) {
        heatColor = lerp(kMidHeatColor,
            kOverheatColor, (heatRatio - 0.5f) * 2.0f);
    }
    float pulse = 1.0f;
    if (isOverheated) {
        heatColor = kOverheatColor;
        pulse = 0.65f + 0.35f * sin(gFrame.elapsedTime * 12.0f);
    }
    float activeMask = max(dotMask * isDotActive, lineMask * isLineActive);
    float trackMask = max(dotMask * 0.80f, lineMask * 0.65f);
    float alpha = max(trackMask, activeMask * pulse);
    alpha = max(alpha, max(dotMask, lineMask) * readyFlash);
    float3 color = kReticleColor;
    if (activeMask > 0.0f) {
        color = heatColor;
    }
    float centerMask = ReticleStrokeMask(radius - 0.5f);
    float2 aimPosition = position;
    if (isOverheated) {
        // Rotate only the center sight; the heat gauge keeps its position.
        aimPosition = float2(position.x + position.y, position.y - position.x) * 0.70710678f;
    }
    float2 absolutePosition = abs(aimPosition);
    float2 innerMask = smoothstep(6.0f, 7.0f, absolutePosition);
    float2 innerOutlineMask = smoothstep(
        6.0f - kOutlineWidthPixels, 7.0f - kOutlineWidthPixels, absolutePosition);
    float3 aimColor = kReticleColor;
    if (isOverheated) {
        innerMask = float2(1.0f, 1.0f);
        innerOutlineMask = float2(1.0f, 1.0f);
        aimColor = kOverheatColor * (0.85f + 0.15f * sin(gFrame.elapsedTime * 12.0f));
    } else if (heatRatio >= kHeatWarningThreshold) {
        aimColor = kLowHeatColor * (0.80f + 0.20f * sin(gFrame.elapsedTime * 6.0f));
    }
    float horizontalMask = ReticleStrokeMask(absolutePosition.y) *
        innerMask.x *
        (1.0f - smoothstep(14.0f, 15.0f, absolutePosition.x));
    float verticalMask = ReticleStrokeMask(absolutePosition.x) *
        innerMask.y *
        (1.0f - smoothstep(14.0f, 15.0f, absolutePosition.y));
    float aimMask = max(centerMask, max(horizontalMask, verticalMask));
    if (aimMask > 0.0f) {
        color = aimColor;
        alpha = max(alpha, aimMask);
    }
    float centerOutlineMask = ReticleOutlineMask(radius - 0.5f);
    float horizontalOutlineMask = ReticleOutlineMask(absolutePosition.y) *
        innerOutlineMask.x *
        (1.0f - smoothstep(14.0f + kOutlineWidthPixels, 15.0f + kOutlineWidthPixels, absolutePosition.x));
    float verticalOutlineMask = ReticleOutlineMask(absolutePosition.x) *
        innerOutlineMask.y *
        (1.0f - smoothstep(14.0f + kOutlineWidthPixels, 15.0f + kOutlineWidthPixels, absolutePosition.y));
    float outlineMask = max(max(dotOutlineMask, lineOutlineMask),
        max(centerOutlineMask, max(horizontalOutlineMask, verticalOutlineMask)));
    // Composite the visible strokes over a black outline using straight alpha.
    return ComposeReticleColor(color, alpha, outlineMask);
}
