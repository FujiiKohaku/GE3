#include "../Common/SpritePixelCommon.hlsli"
#include "../Common/ReticleStyle.hlsli"

float SegmentDistance(float2 position, float2 startPosition, float2 endPosition)
{
    float2 segment = endPosition - startPosition;
    float ratio = saturate(dot(position - startPosition, segment) / dot(segment, segment));
    return length(position - startPosition - segment * ratio);
}

float BulletDistance(float2 position)
{
    float distancePixels = SegmentDistance(position, float2(-3.0f, -7.0f), float2(0.0f, -13.0f));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(0.0f, -13.0f), float2(3.0f, -7.0f)));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(-3.0f, -7.0f), float2(-3.0f, 11.0f)));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(3.0f, -7.0f), float2(3.0f, 11.0f)));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(-3.0f, 11.0f), float2(3.0f, 11.0f)));
    return min(distancePixels, SegmentDistance(position, float2(-3.0f, 6.0f), float2(3.0f, 6.0f)));
}

float MissileDistance(float2 position)
{
    float distancePixels = SegmentDistance(position, float2(-4.0f, -13.0f), float2(0.0f, -22.0f));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(0.0f, -22.0f), float2(4.0f, -13.0f)));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(-4.0f, -13.0f), float2(-4.0f, 13.0f)));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(4.0f, -13.0f), float2(4.0f, 13.0f)));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(-4.0f, 13.0f), float2(4.0f, 13.0f)));
    distancePixels = min(distancePixels, SegmentDistance(position, float2(-4.0f, -9.0f), float2(4.0f, -9.0f)));
    float2 mirroredPosition = float2(abs(position.x), position.y);
    distancePixels = min(distancePixels, SegmentDistance(mirroredPosition, float2(4.0f, 3.0f), float2(12.0f, 14.0f)));
    distancePixels = min(distancePixels, SegmentDistance(mirroredPosition, float2(12.0f, 14.0f), float2(4.0f, 11.0f)));
    return min(distancePixels, SegmentDistance(position, float2(0.0f, 17.0f), float2(0.0f, 22.0f)));
}

float4 main(SpriteVertexOutput input) : SV_Target0
{
    // A fixed design space lets the same symbol scale cleanly for both displays.
    float2 position = (input.texcoord - 0.5f) * 64.0f;
    float distancePixels = 0.0f;
    float3 color = kReticleColor;
    if (gEffect.threshold < 0.5f) {
        distancePixels = BulletDistance(position - float2(-12.0f, 5.0f));
        distancePixels = min(distancePixels, BulletDistance(position - float2(0.0f, -4.0f)));
        distancePixels = min(distancePixels, BulletDistance(position - float2(12.0f, 5.0f)));
    } else if (gEffect.threshold < 1.5f) {
        distancePixels = MissileDistance(position);
    } else {
        distancePixels = MissileDistance((position - float2(6.0f, -3.0f)) / 0.75f) * 0.75f;
        float2 arcPosition = position - float2(0.0f, 2.0f);
        float arcDistance = max(abs(length(arcPosition) - 21.0f), arcPosition.x - 1.0f);
        arcDistance = max(arcDistance, -arcPosition.y - 12.0f);
        arcDistance = min(arcDistance, SegmentDistance(position, float2(-17.0f, -10.0f), float2(-24.0f, -6.0f)));
        arcDistance = min(arcDistance, SegmentDistance(position, float2(-17.0f, -10.0f), float2(-15.0f, -2.0f)));
        if (arcDistance < distancePixels) {
            color = kReticleReadyColor;
        }
        distancePixels = min(distancePixels, arcDistance);
    }
    float scale = gEffect.spriteSize.x / 64.0f;
    float4 symbolColor = ComposeReticleColor(color,
        ReticleStrokeMask(distancePixels * scale), ReticleOutlineMask(distancePixels * scale));
    // One sprite includes the dark backing and cyan border for bright scenery.
    float panelDistance = max(abs(position.x), abs(position.y)) - 28.0f;
    float panelAlpha = (1.0f - smoothstep(0.0f, 1.0f, panelDistance)) * 0.88f;
    float borderMask = 1.0f - smoothstep(0.5f, 1.3f, abs(panelDistance));
    float3 panelColor = lerp(float3(0.025f, 0.055f, 0.085f), kReticleReadyColor, borderMask * 0.65f);
    float alpha = symbolColor.a + panelAlpha * (1.0f - symbolColor.a);
    float3 combinedColor = symbolColor.rgb * symbolColor.a + panelColor * panelAlpha * (1.0f - symbolColor.a);
    if (alpha > 0.0f) {
        combinedColor /= alpha;
    }
    return float4(combinedColor * gMaterial.color.rgb, alpha * gMaterial.color.a);
}
