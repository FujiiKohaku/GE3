#include "Common.hlsli"
float2 main(MotionVectorOutput input) : SV_TARGET0 {
    if (input.currentClip.w <= 0.00001f || input.previousClip.w <= 0.00001f) { return float2(0, 0); }
    float2 currentUv = input.currentClip.xy / input.currentClip.w * float2(0.5f, -0.5f) + 0.5f;
    float2 previousUv = input.previousClip.xy / input.previousClip.w * float2(0.5f, -0.5f) + 0.5f;
    return currentUv - previousUv;
}
