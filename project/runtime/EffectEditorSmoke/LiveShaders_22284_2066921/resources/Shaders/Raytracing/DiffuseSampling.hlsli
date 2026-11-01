#ifndef KOHAKU_DIFFUSE_SAMPLING_HLSLI
#define KOHAKU_DIFFUSE_SAMPLING_HLSLI
// First two Sobol dimensions, with a per-pixel digital shift.
float2 SampleDiffuseSequence(uint sequenceIndex, uint2 scramble) {
    uint first = reversebits(sequenceIndex);
    uint second = 0;
    uint direction = 0x80000000u;
    for (uint remaining = sequenceIndex; remaining != 0; remaining >>= 1) {
        if ((remaining & 1u) != 0) { second ^= direction; }
        direction ^= direction >> 1;
    }
    uint2 shifted = uint2(first, second) ^ scramble;
    // 23 bits leave an exactly representable midpoint strictly below one.
    return (float2(shifted >> 9) + 0.5f) / 8388608.0f;
}
float GetDiffuseDistanceWeight(float hitDistance, float maxDistance, float fadeRatio) {
    if (fadeRatio <= 0) { return 1; }
    float fadeStart = maxDistance * (1 - fadeRatio);
    return 1 - smoothstep(fadeStart, maxDistance, hitDistance);
}
#endif
