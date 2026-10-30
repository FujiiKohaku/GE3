#ifndef KOHAKU_HDR_COLOR
#define KOHAKU_HDR_COLOR
float3 SanitizeHdr(float3 color) {
    for (uint index = 0; index < 3; ++index) {
        if (!isfinite(color[index])) { color[index] = 0; }
    }
    return clamp(color, 0.0f, 65504.0f);
}
#endif
