#include "../Common/ParticleCommon.hlsli"

StructuredBuffer<TrailPoint> gTrailPoints : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);
ConstantBuffer<ParticleRenderParameter> gRenderParameter : register(b2);

float32_t3 CalculateTrailSide(float32_t3 tangent, float32_t3 center)
{
    float32_t3 referenceDirection = float32_t3(0.0f, 1.0f, 0.0f);
    if (gRenderParameter.faceCamera != 0)
    {
        referenceDirection = normalize(gPerView.cameraPosition - center);
    }

    float32_t3 side = cross(tangent, referenceDirection);
    if (dot(side, side) < 0.000001f)
    {
        side = cross(tangent, float32_t3(1.0f, 0.0f, 0.0f));
    }

    return normalize(side);
}

VertexShaderOutput main(uint32_t vertexId : SV_VertexID)
{
    VertexShaderOutput output = (VertexShaderOutput) 0;

    uint32_t segmentIndex = vertexId / 6;
    uint32_t cornerIndex = vertexId % 6;
    TrailPoint startPoint = gTrailPoints[segmentIndex];
    TrailPoint endPoint = gTrailPoints[segmentIndex + 1];

    if (startPoint.isActive == 0 || endPoint.isActive == 0)
    {
        output.position = float32_t4(0.0f, 0.0f, 0.0f, 1.0f);
        output.color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        return output;
    }

    bool useEndPoint = false;
    bool useRightSide = false;
    if (cornerIndex == 1 || cornerIndex == 4 || cornerIndex == 5)
    {
        useRightSide = true;
    }
    if (cornerIndex == 2 || cornerIndex == 3 || cornerIndex == 5)
    {
        useEndPoint = true;
    }

    TrailPoint selectedPoint = startPoint;
    if (useEndPoint)
    {
        selectedPoint = endPoint;
    }

    float32_t normalizedAge = saturate(selectedPoint.age / gRenderParameter.trailLifeTime);
    float32_t width = lerp(
        gRenderParameter.trailStartWidth,
        gRenderParameter.trailEndWidth,
        normalizedAge);
    float32_t4 color = lerp(
        gRenderParameter.trailStartColor,
        gRenderParameter.trailEndColor,
        normalizedAge);

    float32_t3 tangent = normalize(endPoint.position - startPoint.position);
    float32_t3 center = (startPoint.position + endPoint.position) * 0.5f;
    float32_t3 side = CalculateTrailSide(tangent, center);
    float32_t sideSign = -1.0f;
    float32_t widthCoordinate = 0.0f;
    if (useRightSide)
    {
        sideSign = 1.0f;
        widthCoordinate = 1.0f;
    }

    float32_t3 selectedPosition = selectedPoint.position;
    if (segmentIndex == 0 && !useEndPoint)
    {
        selectedPosition -= tangent * gRenderParameter.trailRootExtension;
    }

    float32_t3 worldPosition = selectedPosition + side * width * 0.5f * sideSign;
    output.position = mul(float32_t4(worldPosition, 1.0f), gPerView.viewProjection);
    output.texcoord = float32_t2(
        0.5f,
        widthCoordinate);
    output.normal = normalize(cross(side, tangent));
    output.color = color;
    output.worldPosition = worldPosition;
    output.viewDistance = length(worldPosition - gPerView.cameraPosition);
    return output;
}
