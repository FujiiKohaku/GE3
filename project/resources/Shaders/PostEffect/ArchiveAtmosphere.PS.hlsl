#include "Fullscreen.hlsli"

// Reuse the three reserved floats without changing the shared shader layout.
#define archiveFocusDistance padding0
#define archiveFocusRange padding1
#define archiveApproach padding2

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);

float ViewDepth(float2 uv)
{
    uint w, h;
    gDepthTexture.GetDimensions(w, h);
    int2 pixel = clamp(int2(uv * float2(w, h)), int2(0, 0), int2(w - 1, h - 1));
    float depth = gDepthTexture.Load(int3(pixel, 0));
    return outlineNearClip * outlineFarClip /
        max(outlineFarClip - depth * (outlineFarClip - outlineNearClip), 0.0001f);
}

float Luma(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 BrightPass(float3 color)
{
    // LDR scene: high threshold + small gain keep parchment from glowing white.
    float peak = max(color.r, max(color.g, color.b));
    return color * smoothstep(0.72f, 0.96f, peak);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    float2 uv = input.texcoord;
    float approach = saturate(archiveApproach);
    float4 source = gTexture.Sample(gSampler, uv);
    float3 color = source.rgb;

    // Far-only depth of field: book, printed leaves and foreground stay sharp.
    float focusLimit = archiveFocusDistance + archiveFocusRange;
    float depth = ViewDepth(uv);
    float blur = smoothstep(focusLimit, focusLimit + 4.0f, depth) * approach;
    if (blur > 0.001f)
    {
        float3 sum = color;
        float weight = 1.0f;
        [unroll]
        for (int i = 0; i < 16; ++i)
        {
            float angle = float(i) * 2.39996323f;
            float radius = sqrt((float(i) + 0.5f) / 16.0f) * 5.0f * blur;
            float2 sampleUV = saturate(uv + float2(cos(angle), sin(angle)) * texel * radius);
            float sampleDepth = ViewDepth(sampleUV);
            // Do not spread the crisp foreground book into the blurred shelves.
            float sampleWeight = smoothstep(focusLimit, focusLimit + 1.0f, sampleDepth) *
                exp(-abs(sampleDepth - depth) * 0.15f);
            sum += gTexture.Sample(gSampler, sampleUV).rgb * sampleWeight;
            weight += sampleWeight;
        }
        color = sum / weight;
    }

    // Subtle wide highlight halo, with the original sharp image retained.
    float3 bloom = BrightPass(source.rgb);
    [unroll]
    for (int j = 0; j < 12; ++j)
    {
        float angle = float(j) * 2.39996323f;
        float radius = sqrt((float(j) + 0.5f) / 12.0f) * 9.0f;
        float2 sampleUV = saturate(uv + float2(cos(angle), sin(angle)) * texel * radius);
        bloom += BrightPass(gTexture.Sample(gSampler, sampleUV).rgb);
    }
    color += bloom / 13.0f * 0.075f;

    // Split toning: restrained teal shadows, amber highlights, neutral readable paper.
    float luminance = Luma(color);
    float shadows = 1.0f - smoothstep(0.04f, 0.34f, luminance);
    float highlights = smoothstep(0.35f, 0.88f, luminance);
    color *= lerp(float3(1, 1, 1), float3(0.91f, 1.035f, 1.04f), shadows * 0.45f);
    color *= lerp(float3(1, 1, 1), float3(1.045f, 1.012f, 0.94f), highlights * 0.55f);

    float2 centered = (uv - 0.5f) * 2.0f;
    // Keep the central book clear, with a more visible falloff around the shelves.
    float vignette = smoothstep(0.50f, 1.25f, length(centered));
    color *= 1.0f - vignette * lerp(0.18f, 0.48f, approach);

    // Slightly larger monochrome grain stays visible at normal viewing size.
    // The later 2D text/UI pass remains unaffected.
    float2 pixel = floor(uv * float2(width, height) / 1.5f);
    float noise = frac(sin(dot(pixel, float2(12.9898f, 78.233f)) + floor(time * 24.0f) * 37.719f) * 43758.5453f);
    color += (noise - 0.5f) * 0.015f * smoothstep(0.0f, 0.12f, luminance);
    return float4(saturate(color), source.a);
}
