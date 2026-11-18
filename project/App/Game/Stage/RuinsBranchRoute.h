#pragma once
#include "Engine/Rail/Rail.h"
#include <algorithm>
#include <cmath>

struct RuinsBranchSettings {
    bool isEnabled = false;
    bool isForced = false;
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    float previewDistance = 240.0f;
    float selectionOffsetX = -3.0f;
    std::vector<Vector3> controlPoints;

    bool IsValid(float railLength) const
    {
        if (!std::isfinite(startDistance) || !std::isfinite(endDistance) ||
            !std::isfinite(previewDistance) || !std::isfinite(selectionOffsetX) ||
            startDistance <= 0 || endDistance <= startDistance || endDistance > railLength ||
            previewDistance <= 0 || previewDistance > startDistance || selectionOffsetX >= 0 ||
            controlPoints.size() < 4) { return false; }
        float length = 0;
        for (size_t i = 0; i < controlPoints.size(); ++i) {
            const auto& p = controlPoints[i];
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) { return false; }
            if (i > 0) {
                const auto d = p - controlPoints[i - 1];
                length += std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
            }
        }
        return length > 1.0f;
    }
};

// 下ルートと独立した、左への進入・遺跡内部・海上への合流経路。
class RuinsBranchRoute {
public:
    void Initialize(const RuinsBranchSettings& settings)
    {
        settings_ = settings;
        selected_ = decided_ = false;
        route_.Initialize();
        if (settings_.isEnabled) {
            for (const auto& p : settings_.controlPoints) { route_.AddPoint(p); }
        }
    }

    void Update(float previousDistance, float nextDistance, float offsetX, bool canSelect)
    {
        if (!settings_.isEnabled) { return; }
        if (nextDistance < settings_.startDistance) { selected_ = decided_ = false; return; }
        if (decided_) { return; }
        if (nextDistance >= settings_.endDistance) { decided_ = true; return; }
        if (previousDistance < settings_.startDistance) {
            decided_ = true;
            selected_ = canSelect && offsetX < settings_.selectionOffsetX;
        }
    }

    bool IsPreviewVisible(float distance) const
    {
        return settings_.isEnabled && distance >= settings_.startDistance - settings_.previewDistance &&
            distance < settings_.startDistance;
    }
    bool IsActive(float distance) const
    {
        return settings_.isEnabled && (settings_.isForced || selected_) && distance >= settings_.startDistance &&
            distance <= settings_.endDistance;
    }
    Vector3 GetPosition(float distance, const Vector3& mainPosition) const
    {
        if (!IsActive(distance)) { return mainPosition; }
        const float progress = std::clamp((distance - settings_.startDistance) /
            (settings_.endDistance - settings_.startDistance), 0.0f, 1.0f);
        return route_.GetPositionByDistance(progress * route_.GetTotalLength());
    }
private:
    RuinsBranchSettings settings_;
    Rail route_;
    bool selected_ = false;
    bool decided_ = false;
};
