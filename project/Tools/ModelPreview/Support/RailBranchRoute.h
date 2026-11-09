#pragma once
#include "Engine/math/MathStruct.h"
#include <algorithm>
#include <cmath>

struct RailBranchSettings {
    bool isEnabled = false;
    float startDistance = 900.0f;
    float endDistance = 2100.0f;
    float previewDistance = 260.0f;
    float selectionOffsetY = 0.0f;
    Vector3 offset = { 0.0f, -65.0f, 0.0f };

    bool IsValid(float railLength) const
    {
        return std::isfinite(railLength) && std::isfinite(startDistance) && std::isfinite(endDistance) &&
            std::isfinite(previewDistance) && std::isfinite(selectionOffsetY) &&
            std::isfinite(offset.x) && std::isfinite(offset.y) && std::isfinite(offset.z) &&
            startDistance > 0.0f && endDistance > startDistance && endDistance <= railLength &&
            previewDistance > 0.0f && previewDistance <= startDistance && offset.y < 0.0f;
    }
};

// 主ルートの進行距離を保ち、下ルートの選択と滑らかな下降・合流を管理する。
class RailBranchRoute {
public:
    void Initialize(const RailBranchSettings& settings)
    {
        settings_ = settings;
        hasSelectedRoute_ = false;
        isLowerRouteSelected_ = false;
    }

    void Update(float previousDistance, float nextDistance, float playerOffsetY, bool isSelectionEnabled)
    {
        if (!settings_.isEnabled) { return; }
        if (nextDistance < settings_.startDistance) {
            hasSelectedRoute_ = false;
            isLowerRouteSelected_ = false;
            return;
        }
        if (hasSelectedRoute_) { return; }
        // デバッグ移動などで区間全体を飛び越えた場合は、主ルートを維持する。
        if (nextDistance >= settings_.endDistance) {
            hasSelectedRoute_ = true;
            return;
        }
        if (previousDistance < settings_.startDistance) {
            hasSelectedRoute_ = true;
            isLowerRouteSelected_ = isSelectionEnabled && playerOffsetY < settings_.selectionOffsetY;
        }
    }

    bool IsPreviewVisible(float distance) const
    {
        return settings_.isEnabled && distance >= settings_.startDistance - settings_.previewDistance &&
            distance < settings_.startDistance;
    }

    bool IsLowerRouteActive(float distance) const
    {
        return settings_.isEnabled && isLowerRouteSelected_ &&
            distance >= settings_.startDistance && distance < settings_.endDistance;
    }

    bool IsLowerRouteSelected() const { return isLowerRouteSelected_; }

    Vector3 GetOffset(float distance) const
    {
        if (!IsLowerRouteActive(distance)) { return {}; }
        const float progress = (distance - settings_.startDistance) /
            (settings_.endDistance - settings_.startDistance);
        constexpr float kTransitionRatio = 0.3f;
        float transition = 1.0f;
        if (progress < kTransitionRatio) {
            transition = progress / kTransitionRatio;
        } else if (progress > 1.0f - kTransitionRatio) {
            transition = (1.0f - progress) / kTransitionRatio;
        }
        transition = std::clamp(transition, 0.0f, 1.0f);
        // 両端の速度・加速度が0となる補間で、分岐と合流の向きを揃える。
        const float weight = transition * transition * transition *
            (transition * (transition * 6.0f - 15.0f) + 10.0f);
        return { settings_.offset.x * weight, settings_.offset.y * weight, settings_.offset.z * weight };
    }

private:
    RailBranchSettings settings_;
    bool hasSelectedRoute_ = false;
    bool isLowerRouteSelected_ = false;
};
