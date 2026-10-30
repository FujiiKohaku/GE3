#include "../PostEffect/HdrColor.hlsli"
cbuffer TemporalConstants : register(b0) {
    row_major float4x4 inverseViewProjection;
    row_major float4x4 view;
    row_major float4x4 previousViewProjection;
    row_major float4x4 previousView;
    float4 cameraPosition;
    float4 inputSize;
    float4 outputSize;
    float4 controls;
    float4 jitter;
};
Texture2D<float4> currentColor : register(t0);
Texture2D<float> currentDepth : register(t1);
Texture2D<float2> currentMotion : register(t2);
Texture2D<float4> currentReprojection : register(t3);
Texture2D<float4> historyColor : register(t4);
Texture2D<float2> historyGuide : register(t5);
RWTexture2D<float4> outputColor : register(u0);
RWTexture2D<float2> outputGuide : register(u1);
SamplerState linearClamp : register(s0);
int2 SourcePixel(float2 uv) { return clamp(int2(uv * inputSize.xy), int2(0, 0), int2(inputSize.xy) - 1); }
float3 CompressColor(float3 color) { return color / (1 + max(color.r, max(color.g, color.b))); }
float3 ExpandColor(float3 color) { return SanitizeHdr(max(color, 0) / max(1 - max(color.r, max(color.g, color.b)), 1.0f / 65504.0f)); }
float3 ToYCoCg(float3 color) { return float3(dot(color, float3(0.25f, 0.5f, 0.25f)), color.r * 0.5f - color.b * 0.5f, -color.r * 0.25f + color.g * 0.5f - color.b * 0.25f); }
float3 FromYCoCg(float3 color) { return float3(color.x + color.y - color.z, color.x + color.z, color.x - color.y - color.z); }
float4 CubicWeights(float fraction) {
    float square = fraction * fraction; float cube = square * fraction;
    return float4(-0.5f * fraction + square - 0.5f * cube, 1 - 2.5f * square + 1.5f * cube,
        0.5f * fraction + 2 * square - 1.5f * cube, -0.5f * square + 0.5f * cube);
}
float4 ReconstructColor(float2 uv) {
    if (jitter.z < 0.5f) { return currentColor.SampleLevel(linearClamp, uv, 0); }
    float2 pixel = uv * inputSize.xy - 0.5f; int2 cell = int2(floor(pixel));
    float4 weightsX = CubicWeights(frac(pixel.x)); float4 weightsY = CubicWeights(frac(pixel.y));
    float4 color = 0;
    for (int offsetY = 0; offsetY < 4; ++offsetY) {
        for (int offsetX = 0; offsetX < 4; ++offsetX) {
            int2 samplePixel = clamp(cell + int2(offsetX - 1, offsetY - 1), int2(0, 0), int2(inputSize.xy) - 1);
            color += currentColor.Load(int3(samplePixel, 0)) * weightsX[offsetX] * weightsY[offsetY];
        }
    }
    return float4(SanitizeHdr(color.rgb), saturate(color.a));
}
float3 WorldPosition(float2 uv, float depth) {
    float4 position = mul(float4(uv * float2(2, -2) + float2(-1, 1), depth, 1), inverseViewProjection);
    return position.xyz / max(position.w, 0.000001f);
}
[numthreads(8, 8, 1)]
void main(uint3 thread : SV_DispatchThreadID) {
    uint2 pixel = thread.xy; if (any(pixel >= uint2(outputSize.xy))) { return; }
    float2 uv = (float2(pixel) + 0.5f) * outputSize.zw;
    float2 sampleUv = saturate(uv + jitter.xy);
    int2 sourcePixel = SourcePixel(sampleUv);
    float depth = currentDepth.Load(int3(sourcePixel, 0));
    float4 current = ReconstructColor(sampleUv);
    float3 minimum = 1e10; float3 maximum = -1e10; float3 mean = 0; float3 square = 0;
    float3 sourceMinimum = 1e10; float3 sourceMaximum = 0;
    int2 closestPixel = sourcePixel; float closestDepth = depth;
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            int2 neighbor = clamp(sourcePixel + int2(offsetX, offsetY), int2(0, 0), int2(inputSize.xy) - 1);
            float3 color = SanitizeHdr(currentColor.Load(int3(neighbor, 0)).rgb);
            sourceMinimum = min(sourceMinimum, color); sourceMaximum = max(sourceMaximum, color);
            float3 compressed = ToYCoCg(CompressColor(color));
            minimum = min(minimum, compressed); maximum = max(maximum, compressed);
            mean += compressed; square += compressed * compressed;
            float neighborDepth = currentDepth.Load(int3(neighbor, 0));
            if (neighborDepth < closestDepth) { closestDepth = neighborDepth; closestPixel = neighbor; }
        }
    }
    // Suppress negative lobes/ringing without removing HDR highlights.
    current.rgb = clamp(SanitizeHdr(current.rgb), sourceMinimum, sourceMaximum);
    bool isSky = depth >= 0.999999f;
    float3 position = WorldPosition(sampleUv, depth);
    float viewDepth = mul(float4(position, 1), view).z;
    float expectedDepth = mul(float4(position, 1), previousView).z;
    float surfaceId = 0; bool hasValidMetadata = true;
    if (controls.w > 0.5f && !isSky) {
        float4 metadata = currentReprojection.Load(int3(sourcePixel, 0));
        surfaceId = abs(metadata.w); hasValidMetadata = metadata.w > 0 && abs(metadata.z) > 0 && all(isfinite(metadata));
        expectedDepth = abs(metadata.z);
    }
    outputGuide[pixel] = float2(viewDepth, surfaceId);
    if (isSky) { outputGuide[pixel] = float2(-1, 0); }
    outputColor[pixel] = current;
    if (controls.x < 0.5f || !hasValidMetadata) { return; }
    float2 previousUv = uv - currentMotion.Load(int3(closestPixel, 0));
    if (isSky) {
        float3 direction = normalize(WorldPosition(sampleUv, 1) - cameraPosition.xyz);
        float4 clip = mul(float4(direction, 0), previousViewProjection);
        if (clip.w <= 0) { return; }
        previousUv = clip.xy / clip.w * float2(0.5f, -0.5f) + 0.5f;
    }
    if (!all(isfinite(previousUv)) || any(previousUv < 0) || any(previousUv >= 1)) { return; }
    // Test each tap before interpolation; never average across disoccluded surfaces.
    float2 historyPixel = previousUv * outputSize.xy - 0.5f;
    int2 historyBase = int2(floor(historyPixel)); float2 fraction = frac(historyPixel);
    float4 previous = 0; float total = 0;
    for (int offsetY = 0; offsetY < 2; ++offsetY) {
        for (int offsetX = 0; offsetX < 2; ++offsetX) {
            int2 tap = historyBase + int2(offsetX, offsetY);
            if (any(tap < 0) || any(tap >= int2(outputSize.xy))) { continue; }
            float2 guide = historyGuide.Load(int3(tap, 0));
            if (!all(isfinite(guide))) { continue; }
            if (isSky) { if (guide.x >= 0) { continue; } }
            else {
                if (guide.x <= 0 || abs(guide.x - expectedDepth) > max(0.01f, abs(expectedDepth) * 0.02f)
                    || guide.y != surfaceId) { continue; }
            }
            float weightX = 1 - fraction.x; if (offsetX != 0) { weightX = fraction.x; }
            float weightY = 1 - fraction.y; if (offsetY != 0) { weightY = fraction.y; }
            float weight = weightX * weightY;
            float4 signal = historyColor.Load(int3(tap, 0)); if (!all(isfinite(signal))) { continue; }
            previous += signal * weight; total += weight;
        }
    }
    if (total <= 0.01f) { return; }
    previous /= total;
    mean /= 9; square /= 9;
    float3 sigma = sqrt(max(square - mean * mean, 0));
    float3 low = max(minimum, mean - sigma * controls.z);
    float3 high = min(maximum, mean + sigma * controls.z);
    float3 oldColor = ToYCoCg(CompressColor(SanitizeHdr(previous.rgb)));
    oldColor = ExpandColor(FromYCoCg(clamp(oldColor, low, high)));
    float currentLuminance = dot(current.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float oldLuminance = dot(oldColor, float3(0.2126f, 0.7152f, 0.0722f));
    float reactive = abs(currentLuminance - oldLuminance) / max(1 + max(currentLuminance, oldLuminance), 1);
    float weight = controls.y * (1 - saturate(reactive));
    outputColor[pixel] = float4(SanitizeHdr(lerp(current.rgb, oldColor, weight)), current.a);
}
