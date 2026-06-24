#include "Rail.h"

#include "Engine/Debug/DebugRenderer.h"
#include <cmath>

namespace {
constexpr uint32_t kDistanceSamplesPerSegment = 100;
}

void Rail::Initialize()
{
    controlPoints_.clear();
    cumulativeDistances_.clear();
    totalLength_ = 0.0f;
}

void Rail::Update()
{
}

void Rail::DrawDebug()
{
    if (controlPoints_.size() < 4) {
        return;
    }

    const float step = 0.02f;
    const uint32_t pointCount = static_cast<uint32_t>(controlPoints_.size());

    for (uint32_t segmentIndex = 0; segmentIndex < pointCount - 1; segmentIndex++) {
        for (float t = 0.0f; t < 1.0f; t += step) {
            float nextT = t + step;
            if (nextT > 1.0f) {
                nextT = 1.0f;
            }

            Vector3 currentPosition = EvaluateSegment(segmentIndex, t);
            Vector3 nextPosition = EvaluateSegment(segmentIndex, nextT);

            DebugRenderer::GetInstance()->AddLine(
                currentPosition,
                nextPosition,
                { 1.0f, 0.0f, 0.0f, 1.0f },
                5.0f);
        }
    }
}

void Rail::AddPoint(const Vector3& point)
{
    controlPoints_.push_back(point);
    RebuildDistanceTable();
}

Vector3 Rail::GetPosition(float progress) const
{
    if (controlPoints_.size() < 4) {
        return { 0.0f, 0.0f, 0.0f };
    }

    float clampedProgress = progress;
    if (clampedProgress < 0.0f) {
        clampedProgress = 0.0f;
    }
    if (clampedProgress > 1.0f) {
        clampedProgress = 1.0f;
    }

    const uint32_t pointCount = static_cast<uint32_t>(controlPoints_.size());
    const uint32_t segmentCount = pointCount - 1;
    const float scaledProgress = clampedProgress * static_cast<float>(segmentCount);

    uint32_t segmentIndex = static_cast<uint32_t>(scaledProgress);
    if (segmentIndex >= segmentCount) {
        segmentIndex = segmentCount - 1;
    }

    float localT = scaledProgress - static_cast<float>(segmentIndex);
    if (localT > 1.0f) {
        localT = 1.0f;
    }

    return EvaluateSegment(segmentIndex, localT);
}

Vector3 Rail::GetPositionByDistance(float distance) const
{
    if (controlPoints_.size() < 4) {
        return { 0.0f, 0.0f, 0.0f };
    }

    if (cumulativeDistances_.size() < 2 || totalLength_ <= 0.0f) {
        return GetPosition(0.0f);
    }

    float clampedDistance = distance;
    if (clampedDistance < 0.0f) {
        clampedDistance = 0.0f;
    }
    if (clampedDistance > totalLength_) {
        clampedDistance = totalLength_;
    }

    if (clampedDistance <= 0.0f) {
        return EvaluateSegment(0, 0.0f);
    }

    if (clampedDistance >= totalLength_) {
        uint32_t lastSegmentIndex = static_cast<uint32_t>(controlPoints_.size()) - 2;
        return EvaluateSegment(lastSegmentIndex, 1.0f);
    }

    uint32_t endSampleIndex = 1;
    while (endSampleIndex < cumulativeDistances_.size() && cumulativeDistances_[endSampleIndex] < clampedDistance) {
        ++endSampleIndex;
    }

    if (endSampleIndex >= cumulativeDistances_.size()) {
        uint32_t lastSegmentIndex = static_cast<uint32_t>(controlPoints_.size()) - 2;
        return EvaluateSegment(lastSegmentIndex, 1.0f);
    }

    uint32_t startSampleIndex = endSampleIndex - 1;
    float startDistance = cumulativeDistances_[startSampleIndex];
    float endDistance = cumulativeDistances_[endSampleIndex];
    float distanceRange = endDistance - startDistance;

    float distanceRate = 0.0f;
    if (distanceRange > 0.0f) {
        distanceRate = (clampedDistance - startDistance) / distanceRange;
    }

    uint32_t startSegmentIndex = 0;
    uint32_t endSegmentIndex = 0;
    float startT = 0.0f;
    float endT = 0.0f;
    GetSampleParameter(startSampleIndex, startSegmentIndex, startT);
    GetSampleParameter(endSampleIndex, endSegmentIndex, endT);

    uint32_t segmentIndex = startSegmentIndex;
    float localT = startT + (endT - startT) * distanceRate;

    if (startSegmentIndex != endSegmentIndex) {
        segmentIndex = endSegmentIndex;
        localT = endT * distanceRate;
    }

    return EvaluateSegment(segmentIndex, localT);
}

float Rail::GetTotalLength() const
{
    return totalLength_;
}

const std::vector<Vector3>& Rail::GetControlPoints() const
{
    return controlPoints_;
}

Vector3 Rail::EvaluateSegment(uint32_t segmentIndex, float t) const
{
    uint32_t pointCount = static_cast<uint32_t>(controlPoints_.size());
    uint32_t p0Index = 0;
    uint32_t p1Index = 0;
    uint32_t p2Index = 0;
    uint32_t p3Index = 0;
    GetSegmentIndices(segmentIndex, pointCount, p0Index, p1Index, p2Index, p3Index);

    return CatmullRom(
        controlPoints_[p0Index],
        controlPoints_[p1Index],
        controlPoints_[p2Index],
        controlPoints_[p3Index],
        t);
}

Vector3 Rail::CatmullRom(
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& p2,
    const Vector3& p3,
    float t) const
{
    float t2 = t * t;
    float t3 = t2 * t;

    Vector3 result {};

    result.x = 0.5f * ((2.0f * p1.x)
        + (-p0.x + p2.x) * t
        + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2
        + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);

    result.y = 0.5f * ((2.0f * p1.y)
        + (-p0.y + p2.y) * t
        + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2
        + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);

    result.z = 0.5f * ((2.0f * p1.z)
        + (-p0.z + p2.z) * t
        + (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2
        + (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);

    return result;
}

void Rail::GetSegmentIndices(
    uint32_t segmentIndex,
    uint32_t pointCount,
    uint32_t& p0Index,
    uint32_t& p1Index,
    uint32_t& p2Index,
    uint32_t& p3Index) const
{
    p0Index = segmentIndex;
    if (segmentIndex > 0) {
        p0Index = segmentIndex - 1;
    }

    p1Index = segmentIndex;
    p2Index = segmentIndex + 1;
    p3Index = segmentIndex + 2;
    if (p3Index >= pointCount) {
        p3Index = pointCount - 1;
    }
}

void Rail::GetSampleParameter(
    uint32_t sampleIndex,
    uint32_t& segmentIndex,
    float& t) const
{
    segmentIndex = 0;
    t = 0.0f;

    if (sampleIndex == 0) {
        return;
    }

    uint32_t sampleNumber = sampleIndex - 1;
    segmentIndex = sampleNumber / kDistanceSamplesPerSegment;

    uint32_t segmentCount = static_cast<uint32_t>(controlPoints_.size()) - 1;
    if (segmentIndex >= segmentCount) {
        segmentIndex = segmentCount - 1;
        t = 1.0f;
        return;
    }

    uint32_t stepIndex = sampleNumber % kDistanceSamplesPerSegment + 1;
    t = static_cast<float>(stepIndex) / static_cast<float>(kDistanceSamplesPerSegment);
}

void Rail::RebuildDistanceTable()
{
    cumulativeDistances_.clear();
    totalLength_ = 0.0f;

    if (controlPoints_.size() < 4) {
        return;
    }

    cumulativeDistances_.push_back(0.0f);

    uint32_t pointCount = static_cast<uint32_t>(controlPoints_.size());
    uint32_t segmentCount = pointCount - 1;
    Vector3 previousPosition = EvaluateSegment(0, 0.0f);

    for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
        for (uint32_t sampleIndex = 1; sampleIndex <= kDistanceSamplesPerSegment; ++sampleIndex) {
            float t = static_cast<float>(sampleIndex) / static_cast<float>(kDistanceSamplesPerSegment);
            Vector3 currentPosition = EvaluateSegment(segmentIndex, t);
            Vector3 difference = currentPosition - previousPosition;
            float distance = std::sqrt(
                difference.x * difference.x
                + difference.y * difference.y
                + difference.z * difference.z);

            totalLength_ += distance;
            cumulativeDistances_.push_back(totalLength_);
            previousPosition = currentPosition;
        }
    }
}
