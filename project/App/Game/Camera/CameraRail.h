#pragma once

#include "Engine/Rail/Rail.h"
#include <algorithm>
#include <cmath>

// distance はプレイヤーレールの進行距離、offset はレール基準の右・上・前。
struct CameraRailPoint {
    float distance = 0.0f;
    Vector3 offset {};
};

class CameraRail : public Rail {
public:
    static bool IsValid(const std::vector<CameraRailPoint>& points)
    {
        if (points.empty()) { return true; }
        if (points.size() < 4) { return false; }
        float previous = -1.0f;
        for (const auto& point : points) {
            if (!std::isfinite(point.distance) || point.distance < 0.0f ||
                point.distance <= previous || !std::isfinite(point.offset.x) ||
                !std::isfinite(point.offset.y) || !std::isfinite(point.offset.z)) {
                return false;
            }
            previous = point.distance;
        }
        // 演出区間の外側は元のレールに重ねる。
        const auto isZero = [](const Vector3& v) {
            return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
        };
        return isZero(points.front().offset) && isZero(points.back().offset);
    }

    bool Configure(const std::vector<CameraRailPoint>& points)
    {
        Rail::Initialize();
        distances_.clear();
        if (!IsValid(points)) { return false; }
        for (const auto& point : points) {
            AddPoint(point.offset);
            distances_.push_back(point.distance);
        }
        return true;
    }

    Vector3 GetOffsetAtPlayerDistance(float distance) const
    {
        if (distances_.empty() || distance <= distances_.front() ||
            distance >= distances_.back()) { return {}; }
        const auto next = std::upper_bound(distances_.begin(), distances_.end(), distance);
        const size_t index = static_cast<size_t>(next - distances_.begin() - 1);
        const auto& points = GetControlPoints();
        const auto isZero = [](const Vector3& v) {
            return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
        };
        if (isZero(points[index]) && isZero(points[index + 1])) { return {}; }
        float t = (distance - distances_[index]) / (distances_[index + 1] - distances_[index]);
        // 各対応点で速度を緩め、離脱・合流を滑らかにする。
        t = t * t * (3.0f - 2.0f * t);
        return GetPosition((static_cast<float>(index) + t) /
            static_cast<float>(distances_.size() - 1));
    }

    Vector3 GetBasePosition(float playerDistance, const Vector3& mainPosition,
        const Vector3& right, const Vector3& up, const Vector3& forward) const
    {
        const Vector3 offset = GetOffsetAtPlayerDistance(playerDistance);
        return mainPosition + right * offset.x + up * offset.y + forward * offset.z;
    }

private:
    std::vector<float> distances_;
};
