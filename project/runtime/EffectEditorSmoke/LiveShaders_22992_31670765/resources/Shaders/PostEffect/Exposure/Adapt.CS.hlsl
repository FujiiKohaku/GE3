#include "Common.hlsli"
Texture2D<float> previousExposure : register(t1);
RWTexture2D<float> outputExposure : register(u1);
[numthreads(1, 1, 1)]
void main() {
    uint total = 0;
    for (uint index = 1; index < 256; ++index) { total += histogram[index]; }
    float lowCount = float(total) * percentiles.x;
    float highCount = float(total) * percentiles.y;
    float sum = 0; float weight = 0; float cumulative = 0;
    for (uint index = 1; index < 256; ++index) {
        float nextCount = cumulative + float(histogram[index]);
        float count = max(min(nextCount, highCount) - max(cumulative, lowCount), 0);
        float logLuminance = luminanceRange.x + (float(index - 1) + 0.5f) / 254.0f * (luminanceRange.y - luminanceRange.x);
        sum += logLuminance * count; weight += count; cumulative = nextCount;
    }
    float target = 1;
    if (weight > 0) { target = clamp(exposureRange.z / exp2(sum / weight), exposureRange.x, exposureRange.y); }
    target = clamp(target, exposureRange.x, exposureRange.y);
    float result = target;
    if (adaptation.w > 0.5f) {
        float previous = previousExposure.Load(int3(0, 0, 0));
        float speed = adaptation.y;
        if (target < previous) { speed = adaptation.z; }
        float blend = 1 - exp(-speed * adaptation.x);
        result = exp2(lerp(log2(max(previous, exposureRange.x)), log2(target), blend));
    }
    outputExposure[uint2(0, 0)] = clamp(result, exposureRange.x, exposureRange.y);
}
