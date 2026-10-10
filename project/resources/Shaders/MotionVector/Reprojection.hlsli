#ifndef KOHAKU_MOTION_REPROJECTION
#define KOHAKU_MOTION_REPROJECTION
float2 EncodeReprojectionNormal(float3 normal) {
    normal /= max(abs(normal.x) + abs(normal.y) + abs(normal.z), 0.000001f);
    float2 encoded = normal.xy;
    if (normal.z < 0) { float2 signs = 1; if (encoded.x < 0) { signs.x = -1; } if (encoded.y < 0) { signs.y = -1; } encoded = (1 - abs(encoded.yx)) * signs; }
    return encoded;
}
float3 DecodeReprojectionNormal(float2 encoded) {
    float3 normal = float3(encoded, 1 - abs(encoded.x) - abs(encoded.y));
    float fold = saturate(-normal.z);
    if (normal.x >= 0) { normal.x -= fold; } else { normal.x += fold; }
    if (normal.y >= 0) { normal.y -= fold; } else { normal.y += fold; }
    return normalize(normal);
}
#endif
