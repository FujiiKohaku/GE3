#include "Fullscreen.hlsli"
#include "ColorFinish.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float Luma(float3 color)
{
    // Intermediate textures are sampled in linear space; edge tests use perceptual luma.
    return sqrt(max(dot(color, float3(0.299f, 0.587f, 0.114f)), 0.0f));
}

float SampleLuma(float2 uv)
{
    return Luma(FinishColor(gTexture.SampleLevel(gSampler, saturate(uv), 0.0f).rgb));
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width;
    uint height;
    gTexture.GetDimensions(width, height);
    float2 pixel = 1.0f / float2(width, height);
    float2 uv = input.texcoord;
    float4 center = gTexture.SampleLevel(gSampler, uv, 0.0f);
    center.rgb = FinishColor(center.rgb);
    if (fxaaStrength <= 0.0f) { return center; }
    float m = Luma(center.rgb);
    float n = SampleLuma(uv - float2(0.0f, pixel.y));
    float s = SampleLuma(uv + float2(0.0f, pixel.y));
    float w = SampleLuma(uv - float2(pixel.x, 0.0f));
    float e = SampleLuma(uv + float2(pixel.x, 0.0f));
    float low = min(m, min(min(n, s), min(w, e)));
    float high = max(m, max(max(n, s), max(w, e)));
    float range = high - low;
    if (range < max(fxaaEdgeThresholdMin, high * fxaaEdgeThreshold)) { return center; }

    float nw = SampleLuma(uv - pixel);
    float ne = SampleLuma(uv + float2(pixel.x, -pixel.y));
    float sw = SampleLuma(uv + float2(-pixel.x, pixel.y));
    float se = SampleLuma(uv + pixel);
    float horizontal = abs(n + s - 2.0f * m) * 2.0f +
        abs(nw + sw - 2.0f * w) + abs(ne + se - 2.0f * e);
    float vertical = abs(w + e - 2.0f * m) * 2.0f +
        abs(nw + ne - 2.0f * n) + abs(sw + se - 2.0f * s);
    bool horizontalEdge = horizontal >= vertical;
    float sideA = w;
    float sideB = e;
    float2 normalStep = float2(pixel.x, 0.0f);
    float2 edgeStep = float2(0.0f, pixel.y);
    if (horizontalEdge) {
        sideA = n;
        sideB = s;
        normalStep = float2(0.0f, pixel.y);
        edgeStep = float2(pixel.x, 0.0f);
    }
    float gradientA = abs(sideA - m);
    float gradientB = abs(sideB - m);
    float edgeLuma = (sideB + m) * 0.5f;
    float gradient = gradientB;
    if (gradientA >= gradientB) {
        normalStep = -normalStep;
        edgeLuma = (sideA + m) * 0.5f;
        gradient = gradientA;
    }

    float2 edgeOrigin = uv + normalStep * 0.5f;
    float2 endA = edgeOrigin - edgeStep;
    float2 endB = edgeOrigin + edgeStep;
    float deltaA = SampleLuma(endA) - edgeLuma;
    float deltaB = SampleLuma(endB) - edgeLuma;
    float limit = gradient * 0.25f;
    bool doneA = abs(deltaA) >= limit;
    bool doneB = abs(deltaB) >= limit;
    [loop]
    for (int search = 0; search < 7; ++search) {
        if (doneA && doneB) { break; }
        float stride = 1.0f + float(search) * 0.5f;
        if (!doneA) {
            endA -= edgeStep * stride;
            deltaA = SampleLuma(endA) - edgeLuma;
            doneA = abs(deltaA) >= limit;
        }
        if (!doneB) {
            endB += edgeStep * stride;
            deltaB = SampleLuma(endB) - edgeLuma;
            doneB = abs(deltaB) >= limit;
        }
    }
    float distanceA = length(uv - endA + normalStep * 0.5f);
    float distanceB = length(uv - endB + normalStep * 0.5f);
    float nearest = distanceB;
    float nearestDelta = deltaB;
    bool endpointFound = doneB;
    if (distanceA <= distanceB) {
        nearest = distanceA;
        nearestDelta = deltaA;
        endpointFound = doneA;
    }
    float edgeBlend = 0.0f;
    if (endpointFound && ((nearestDelta < 0.0f) != (m < edgeLuma))) {
        edgeBlend = 0.5f - nearest / max(distanceA + distanceB, 0.00001f);
    }
    float average = (2.0f * (n + s + w + e) + nw + ne + sw + se) / 12.0f;
    float subpixel = saturate(abs(average - m) / max(range, 0.00001f));
    subpixel = subpixel * subpixel * (3.0f - 2.0f * subpixel);
    subpixel = subpixel * subpixel * saturate(fxaaSubpixel);
    float offset = max(edgeBlend, subpixel) * saturate(fxaaStrength);
    float3 filtered = gTexture.SampleLevel(gSampler, saturate(uv + normalStep * offset), 0.0f).rgb;
    return float4(FinishColor(filtered), center.a);
}
