#include "WaterPillar.hlsli"

struct VertexInput
{
    float4 position : POSITION0;
    float3 normal : NORMAL0;
    float3 attributes : TEXCOORD0;
};

WaterPillarVertexOutput main(VertexInput input)
{
    WaterPillarVertexOutput output;
    float radius = gPillar.pillarPositionAndRadius.w;
    float height = gPillar.heightAndShape.x;
    float kind = input.attributes.z;
    float3 localPosition;
    if (kind < 0.5f) {
        localPosition = float3(input.position.x * radius, input.position.y * height, input.position.z * radius);
    } else {
        float bubbleId = kind - 1.0f;
        float bob = sin(gPillar.cameraPositionAndTime.w * (4.2f + bubbleId * 0.13f) + bubbleId * 1.73f) * 0.08f;
        localPosition = float3(input.position.x * radius, height + (input.position.y + bob) * radius, input.position.z * radius);
    }
    float3 worldPosition = gPillar.pillarPositionAndRadius.xyz + localPosition;
    output.position = mul(float4(worldPosition, 1.0f), gPillar.viewProjection);
    output.worldPosition = worldPosition;
    output.normal = normalize(input.normal);
    output.texcoord = input.attributes.xy;
    output.kind = kind;
    return output;
}
