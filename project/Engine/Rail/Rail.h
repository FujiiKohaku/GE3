#pragma once

#include "Engine/Math/MathStruct.h"
#include <cstdint>
#include <vector>

class Rail {
public:
    void Initialize();
    void Update();
    void DrawDebug();

    void AddPoint(const Vector3& point);
    Vector3 GetPosition(float progress) const;
    Vector3 GetPositionByDistance(float distance) const;
    float GetTotalLength() const;
    const std::vector<Vector3>& GetControlPoints() const;

private:
    Vector3 EvaluateSegment(uint32_t segmentIndex, float t) const;

    Vector3 CatmullRom(
        const Vector3& p0,
        const Vector3& p1,
        const Vector3& p2,
        const Vector3& p3,
        float t) const;

    void GetSegmentIndices(
        uint32_t segmentIndex,
        uint32_t pointCount,
        uint32_t& p0Index,
        uint32_t& p1Index,
        uint32_t& p2Index,
        uint32_t& p3Index) const;

    void GetSampleParameter(
        uint32_t sampleIndex,
        uint32_t& segmentIndex,
        float& t) const;

    void RebuildDistanceTable();

private:
    // Points that define the rail shape.
    // DrawDebug() samples a Catmull-Rom curve from these points.
    std::vector<Vector3> controlPoints_;
    std::vector<float> cumulativeDistances_;
    float totalLength_ = 0.0f;
};
