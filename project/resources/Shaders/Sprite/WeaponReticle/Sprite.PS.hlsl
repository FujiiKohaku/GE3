#include "../Common/SpritePixelCommon.hlsli"
#include "../Common/ReticleStyle.hlsli"

static const float kMissileModeBoundary = 0.5f;
static const float kLockMarkerModeBoundary = 1.5f;

float BoxDistance(float2 position, float2 halfSize)
{
    float2 offset = abs(position) - halfSize;
    return length(max(offset, 0.0f)) + min(max(offset.x, offset.y), 0.0f);
}

// Project onto the square, then measure equal distances along its four edges.
// Zero starts at the top midpoint and increases clockwise.
float SquarePerimeterProgress(float2 position, float halfExtent)
{
    float2 edgePosition = position * halfExtent / max(max(abs(position.x), abs(position.y)), 0.001f);
    float perimeterDistance = 0.0f;
    if (abs(edgePosition.y) >= abs(edgePosition.x)) {
        if (edgePosition.y < 0.0f) {
            perimeterDistance = edgePosition.x + halfExtent;
        } else {
            perimeterDistance = 5.0f * halfExtent - edgePosition.x;
        }
    } else if (edgePosition.x > 0.0f) {
        perimeterDistance = 3.0f * halfExtent + edgePosition.y;
    } else {
        perimeterDistance = 7.0f * halfExtent - edgePosition.y;
    }
    return frac((perimeterDistance - halfExtent) / (8.0f * halfExtent) + 1.0f);
}

float4 ComposeCooldownReticle(float frameDistance, float aimDistance,
    float progress, float reloadRatio, float readyFlash)
{
    float3 frameColor = kReticleTrackColor;
    if (reloadRatio >= 1.0f) {
        frameColor = lerp(kReticleColor, kReticleReadyColor, readyFlash);
    } else if (reloadRatio > 0.0f && progress <= reloadRatio) {
        frameColor = kReticleReadyColor;
    }
    float frameMask = ReticleStrokeMask(frameDistance);
    float aimMask = ReticleStrokeMask(aimDistance);
    float strokeAlpha = max(frameMask, aimMask);
    float3 strokeColor = frameColor * frameMask;
    if (aimMask >= frameMask) {
        strokeColor = kReticleColor * aimMask;
    }
    if (strokeAlpha > 0.0f) {
        strokeColor /= strokeAlpha;
    }
    return ComposeReticleColor(strokeColor, strokeAlpha,
        ReticleOutlineMask(min(frameDistance, aimDistance)));
}

float4 main(SpriteVertexOutput input) : SV_Target0
{
    float2 position = (input.texcoord - 0.5f) * gEffect.spriteSize;
    float2 absolutePosition = abs(position);
    float radius = length(position);
    float strokeDistance = 0.0f;
    float3 color = kReticleColor;
    float readyFlash = saturate(gEffect.phase);

    // strength: 0 = missile, 1 = homing aim, 2 = confirmed target frame.
    if (gEffect.strength < kMissileModeBoundary) {
        float reloadRatio = saturate(gEffect.threshold);
        const float kFrameHalfExtentPixels = 28.0f;
        float frameDistance = abs(BoxDistance(position,
            float2(kFrameHalfExtentPixels, kFrameHalfExtentPixels)));
        float horizontalTick = BoxDistance(
            float2(absolutePosition.x - 36.0f, position.y), float2(3.5f, 0.0f));
        float verticalTick = BoxDistance(
            float2(position.x, absolutePosition.y - 36.0f), float2(0.0f, 3.5f));
        float progress = SquarePerimeterProgress(position, kFrameHalfExtentPixels);
        float aimDistance = min(radius - 0.5f, min(horizontalTick, verticalTick));
        return ComposeCooldownReticle(frameDistance, aimDistance,
            progress, reloadRatio, readyFlash);
    } else if (gEffect.strength < kLockMarkerModeBoundary) {
        // Match the assault ring's outer size while retaining the homing diamond.
        const float kDiamondExtentPixels = 40.0f;
        float frameDistance = abs(absolutePosition.x + absolutePosition.y - kDiamondExtentPixels) * 0.70710678f;
        float horizontalTick = BoxDistance(
            float2(absolutePosition.x - 10.5f, position.y), float2(3.5f, 0.0f));
        float verticalTick = BoxDistance(
            float2(position.x, absolutePosition.y - 10.5f), float2(0.0f, 3.5f));
        float aimDistance = min(radius - 0.5f, min(horizontalTick, verticalTick));
        // Rotate the diamond into a square and shift the start to its top vertex.
        float2 squarePosition = float2(position.x + position.y, position.y - position.x);
        float progress = frac(SquarePerimeterProgress(squarePosition, kDiamondExtentPixels) + 0.125f);
        return ComposeCooldownReticle(frameDistance, aimDistance,
            progress, saturate(gEffect.threshold), readyFlash);
    } else {
        // Fold into one quadrant to draw all four corner brackets.
        float horizontalDistance = BoxDistance(
            absolutePosition - float2(23.0f, 30.0f), float2(7.0f, 0.0f));
        float verticalDistance = BoxDistance(
            absolutePosition - float2(30.0f, 23.0f), float2(0.0f, 7.0f));
        strokeDistance = min(horizontalDistance, verticalDistance);
        float pulse = 0.88f + 0.12f * sin(gFrame.elapsedTime * 6.0f);
        color = kReticleReadyColor * pulse;
    }

    return ComposeReticleColor(color, ReticleStrokeMask(strokeDistance),
        ReticleOutlineMask(strokeDistance));
}
