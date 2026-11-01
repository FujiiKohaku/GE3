#ifndef KOHAKU_RAY_CONE_HLSLI
#define KOHAKU_RAY_CONE_HLSLI

float2 BuildCameraRayCone(float2 uv, float depth, float3 position, float3 normal) {
    if (historyValidation.w < 0.5f) { return 0; }
    uint width, height; sceneDepth.GetDimensions(width, height);
    // One RT pixel covers two full-resolution pixels in each direction.
    float3 horizontal = ReflectionWorld(uv + float2(2.0f / width, 0), depth) - position;
    float3 vertical = ReflectionWorld(uv + float2(0, 2.0f / height), depth) - position;
    float cameraDistance = max(length(position - cameraPosition.xyz), 0.0001f);
    float diameter = max(length(horizontal), length(vertical));
    float incidence = max(abs(dot(normal, normalize(cameraPosition.xyz - position))), 0.05f);
    return float2(diameter / incidence, diameter / cameraDistance);
}

float2 GetRayConeUvFootprint(float3 worldVertices[3], float2 transformedUvs[3], float coneDiameter) {
    float3 firstEdge = worldVertices[1] - worldVertices[0];
    float3 secondEdge = worldVertices[2] - worldVertices[0];
    float3 areaVector = cross(firstEdge, secondEdge);
    float area = length(areaVector);
    if (area <= 0.00000001f || !isfinite(area)) { return 0; }
    float3 planeNormal = areaVector / area;
    float3 firstDual = cross(secondEdge, planeNormal) / area;
    float3 secondDual = cross(planeNormal, firstEdge) / area;
    float2 firstUvEdge = transformedUvs[1] - transformedUvs[0];
    float2 secondUvEdge = transformedUvs[2] - transformedUvs[0];
    float3 gradientU = firstDual * firstUvEdge.x + secondDual * secondUvEdge.x;
    float3 gradientV = firstDual * firstUvEdge.y + secondDual * secondUvEdge.y;
    float incidence = max(abs(dot(WorldRayDirection(), planeNormal)), 0.05f);
    float2 footprint = float2(length(gradientU), length(gradientV)) * coneDiameter / incidence;
    if (!all(isfinite(footprint))) { return 0; }
    return footprint;
}

float GetRayConeMip(Texture2D<float4> texture, float2 uvFootprint) {
    if (max(uvFootprint.x, uvFootprint.y) <= 0) { return 0; }
    uint width, height, mipCount; texture.GetDimensions(0, width, height, mipCount);
    float texelDiameter = max(uvFootprint.x * width, uvFootprint.y * height);
    return clamp(log2(max(texelDiameter, 1.0f)), 0.0f, float(mipCount - 1));
}
#endif
