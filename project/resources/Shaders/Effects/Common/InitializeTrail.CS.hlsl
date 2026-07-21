#include "../../../Effects/Common/ParticleCommon.hlsli"

RWStructuredBuffer<TrailPoint> gTrailPoints : register(u0);

static const uint32_t kMaxTrailPoints = 64;

[numthreads(64, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t pointIndex = dispatchThreadId.x;
    if (pointIndex >= kMaxTrailPoints)
    {
        return;
    }

    gTrailPoints[pointIndex] = (TrailPoint) 0;
}
