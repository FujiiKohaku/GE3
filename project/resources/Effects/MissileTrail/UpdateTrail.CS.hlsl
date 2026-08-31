#include "../Common/ParticleCommon.hlsli"

RWStructuredBuffer<TrailPoint> gTrailPoints : register(u0);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<ParticleRenderParameter> gRenderParameter : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

static const uint32_t kMaxTrailPoints = 64;

void ClearTrail()
{
    for (uint32_t pointIndex = 0; pointIndex < kMaxTrailPoints; ++pointIndex)
    {
        gTrailPoints[pointIndex] = (TrailPoint) 0;
    }
}

void InsertTrailPoint(float32_t3 position)
{
    uint32_t pointCount = min(gRenderParameter.maxTrailPoints, kMaxTrailPoints);
    for (uint32_t pointIndex = pointCount - 1; pointIndex > 0; --pointIndex)
    {
        gTrailPoints[pointIndex] = gTrailPoints[pointIndex - 1];
    }

    TrailPoint newTrailPoint = (TrailPoint) 0;
    newTrailPoint.position = position;
    newTrailPoint.age = 0.0f;
    newTrailPoint.isActive = 1;
    gTrailPoints[0] = newTrailPoint;
}

[numthreads(1, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    for (uint32_t pointIndex = 0; pointIndex < kMaxTrailPoints; ++pointIndex)
    {
        if (gTrailPoints[pointIndex].isActive == 0)
        {
            continue;
        }

        gTrailPoints[pointIndex].age += gPerFrame.deltaTime;
        if (gTrailPoints[pointIndex].age >= gRenderParameter.trailLifeTime)
        {
            gTrailPoints[pointIndex].isActive = 0;
        }
    }

    if (gEmitter.emit == 0)
    {
        return;
    }

    if (gTrailPoints[0].isActive == 0)
    {
        InsertTrailPoint(gEmitter.translate);
        return;
    }

    float32_t distanceFromLatestPoint = distance(
        gTrailPoints[0].position,
        gEmitter.translate);

    if (distanceFromLatestPoint >= gRenderParameter.trailBreakDistance)
    {
        ClearTrail();
        InsertTrailPoint(gEmitter.translate);
        return;
    }

    if (distanceFromLatestPoint >= gRenderParameter.trailMinVertexDistance)
    {
        InsertTrailPoint(gEmitter.translate);
    }
}
