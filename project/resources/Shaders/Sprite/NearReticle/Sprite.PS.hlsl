#include "../Common/SpritePixelCommon.hlsli"
#include "../Common/ReticleStyle.hlsli"

static const float kNearFrameHalfExtentPixels = 52.0f;
static const float kNearFrameCornerLengthPixels = 14.0f;
static const float3 kNearFrameColor = float3(1.0f, 1.0f, 1.0f);

float SegmentDistance(float2 position, float2 start, float2 end)
{
    float2 direction = end - start;
    float progress = saturate(dot(position - start, direction) / dot(direction, direction));
    return length(position - start - direction * progress);
}

float4 main(SpriteVertexOutput input) : SV_Target0
{
    float2 position = abs((input.texcoord - 0.5f) * gEffect.spriteSize);
    float innerExtent = kNearFrameHalfExtentPixels - kNearFrameCornerLengthPixels;
    float horizontalDistance = SegmentDistance(position,
        float2(innerExtent, kNearFrameHalfExtentPixels),
        float2(kNearFrameHalfExtentPixels, kNearFrameHalfExtentPixels));
    float verticalDistance = SegmentDistance(position,
        float2(kNearFrameHalfExtentPixels, innerExtent),
        float2(kNearFrameHalfExtentPixels, kNearFrameHalfExtentPixels));
    float strokeDistance = min(horizontalDistance, verticalDistance);
    float4 color = ComposeReticleColor(kNearFrameColor,
        ReticleStrokeMask(strokeDistance), ReticleOutlineMask(strokeDistance));
    return color;
}
