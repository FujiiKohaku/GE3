#include "../../../Effects/Common/ParticleCommon.hlsli"
#include "ParticleField.hlsli"

RWStructuredBuffer<TrailPoint> gTrailPoints : register(u0);
ConstantBuffer<ParticleFieldCollection> gFieldCollection : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

static const uint32_t kMaxTrailFieldPoints = 64;

[numthreads(64, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t pointIndex = dispatchThreadId.x;
    if (pointIndex >= kMaxTrailFieldPoints)
    {
        return;
    }

    TrailPoint trailPoint = gTrailPoints[pointIndex];
    if (trailPoint.isActive == 0)
    {
        return;
    }

    float32_t3 fieldForce = CalculateParticleFieldForce(
        trailPoint.position,
        gFieldCollection);
    trailPoint.velocity += fieldForce * gPerFrame.deltaTime;
    float32_t damping = pow(0.96f, gPerFrame.deltaTime * 60.0f);
    trailPoint.velocity *= damping;
    trailPoint.position += trailPoint.velocity * gPerFrame.deltaTime;
    gTrailPoints[pointIndex] = trailPoint;
}
