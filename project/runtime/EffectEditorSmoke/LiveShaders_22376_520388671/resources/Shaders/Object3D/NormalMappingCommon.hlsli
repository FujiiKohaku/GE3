#ifndef KOHAKU_NORMAL_MAPPING_COMMON
#define KOHAKU_NORMAL_MAPPING_COMMON
float3 ApplyNormalDetail(float3 geometricNormal, float3 positionEdge1, float3 positionEdge2,
    float2 uvEdge1, float2 uvEdge2, float3 detail, float strength, float flipY) {
    float3 normal = geometricNormal * rsqrt(max(dot(geometricNormal, geometricNormal), 0.000001f));
    float3 perpendicular2 = cross(positionEdge2, normal);
    float3 perpendicular1 = cross(normal, positionEdge1);
    float3 tangent = perpendicular2 * uvEdge1.x + perpendicular1 * uvEdge2.x;
    float3 bitangent = perpendicular2 * uvEdge1.y + perpendicular1 * uvEdge2.y;
    float frameLength = max(dot(tangent, tangent), dot(bitangent, bitangent));
    if (!isfinite(frameLength) || frameLength < 0.000000000001f) { return normal; }
    detail.xy *= strength;
    if (flipY > 0.5f) { detail.y = -detail.y; }
    float3 result = (tangent * detail.x + bitangent * detail.y) * rsqrt(frameLength) + normal * detail.z;
    if (!all(isfinite(result)) || dot(result, result) < 0.000001f) { return normal; }
    return normalize(result);
}
#endif
