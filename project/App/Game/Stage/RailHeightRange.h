#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// 高度はワールド座標のY。移動量はその地点のレール中心から指定する。
struct RailHeightRange {
    float minimumHeight = 0.0f;
    float maximumHeight = 0.0f;

    float ClampOffset(float offsetY, float baseHeight, float upY) const
    {
        if (std::abs(upY) < 0.0001f) { return offsetY; }
        float minimumOffset = (minimumHeight - baseHeight) / upY;
        float maximumOffset = (maximumHeight - baseHeight) / upY;
        if (minimumOffset > maximumOffset) {
            std::swap(minimumOffset, maximumOffset);
        }
        return std::clamp(offsetY, minimumOffset, maximumOffset);
    }
};

struct RailHeightRangeSettings {
    float minimumOffsetY = -15.0f;
    float maximumOffsetY = 15.0f;
    bool shouldUseFloorMinimum = false;
    float floorClearance = 3.0f;

    bool IsValid() const
    {
        return std::isfinite(minimumOffsetY) && std::isfinite(maximumOffsetY) &&
            std::isfinite(floorClearance) && minimumOffsetY < maximumOffsetY && floorClearance >= 0.0f;
    }
};

struct RailHeightZoneSettings {
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    bool isLowerRoute = false;
    RailHeightRangeSettings range;
};

// 区間は[startDistance, endDistance)。重なる場合は後の設定を優先する。
inline RailHeightRange CalculateRailHeightRange(
    const RailHeightRangeSettings& defaultRange, const RailHeightRangeSettings& lowerRange,
    const std::vector<RailHeightZoneSettings>& zones, bool isLowerRoute, float distance,
    float railHeight, float floorHeight, float branchHeightOffset)
{
    const RailHeightRangeSettings* selectedRange = &defaultRange;
    if (isLowerRoute) { selectedRange = &lowerRange; }
    for (const RailHeightZoneSettings& zone : zones) {
        if (zone.isLowerRoute == isLowerRoute && distance >= zone.startDistance && distance < zone.endDistance) {
            selectedRange = &zone.range;
        }
    }
    RailHeightRange range { railHeight + selectedRange->minimumOffsetY, railHeight + selectedRange->maximumOffsetY };
    if (selectedRange->shouldUseFloorMinimum) {
        range.minimumHeight = (std::max)(range.minimumHeight,
            floorHeight + branchHeightOffset + selectedRange->floorClearance);
    }
    // 床が設定範囲全体より高い場合も、有効な上下限を返す。
    range.minimumHeight = (std::min)(range.minimumHeight, range.maximumHeight);
    return range;
}
