struct PixelInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PixelOutput
{
    float4 color : SV_TARGET0;
};

struct TextAppearance
{
    float4 color;
    float4 outlineColor;
    float4 shadowColor;
    float2 atlasSize;
    float2 shadowOffset;
    float outlineWidth;
    float3 padding;
};

ConstantBuffer<TextAppearance> gAppearance : register(b1);
Texture2D<float4> gFontAtlas : register(t0);
SamplerState gSampler : register(s0);

PixelOutput main(PixelInput input)
{
    PixelOutput output;
    const float2 safeAtlasSize = max(gAppearance.atlasSize, float2(1.0f, 1.0f));
    const float2 outlineTexel = gAppearance.outlineWidth / safeAtlasSize;
    const float2 shadowUv = input.texcoord - gAppearance.shadowOffset / safeAtlasSize;

    const float glyphCoverage = gFontAtlas.Sample(gSampler, input.texcoord).a;
    float outlineCoverage = glyphCoverage;
    outlineCoverage = max(
        outlineCoverage,
        gFontAtlas.Sample(gSampler, input.texcoord + float2(outlineTexel.x, 0.0f)).a);
    outlineCoverage = max(
        outlineCoverage,
        gFontAtlas.Sample(gSampler, input.texcoord - float2(outlineTexel.x, 0.0f)).a);
    outlineCoverage = max(
        outlineCoverage,
        gFontAtlas.Sample(gSampler, input.texcoord + float2(0.0f, outlineTexel.y)).a);
    outlineCoverage = max(
        outlineCoverage,
        gFontAtlas.Sample(gSampler, input.texcoord - float2(0.0f, outlineTexel.y)).a);
    outlineCoverage = max(
        outlineCoverage,
        gFontAtlas.Sample(gSampler, input.texcoord + outlineTexel).a);
    outlineCoverage = max(
        outlineCoverage,
        gFontAtlas.Sample(gSampler, input.texcoord - outlineTexel).a);
    outlineCoverage = max(
        outlineCoverage,
        gFontAtlas.Sample(
            gSampler,
            input.texcoord + float2(outlineTexel.x, -outlineTexel.y)).a);
    outlineCoverage = max(
        outlineCoverage,
        gFontAtlas.Sample(
            gSampler,
            input.texcoord + float2(-outlineTexel.x, outlineTexel.y)).a);

    const float shadowCoverage = gFontAtlas.Sample(gSampler, shadowUv).a;
    const float shadowAlpha = shadowCoverage * gAppearance.shadowColor.a;
    const float outlineAlpha =
        saturate(outlineCoverage - glyphCoverage) * gAppearance.outlineColor.a;
    const float glyphAlpha = glyphCoverage * gAppearance.color.a;

    float accumulatedAlpha = shadowAlpha;
    float3 premultipliedColor = gAppearance.shadowColor.rgb * shadowAlpha;
    premultipliedColor =
        gAppearance.outlineColor.rgb * outlineAlpha +
        premultipliedColor * (1.0f - outlineAlpha);
    accumulatedAlpha = outlineAlpha + accumulatedAlpha * (1.0f - outlineAlpha);
    premultipliedColor =
        gAppearance.color.rgb * glyphAlpha +
        premultipliedColor * (1.0f - glyphAlpha);
    accumulatedAlpha = glyphAlpha + accumulatedAlpha * (1.0f - glyphAlpha);

    clip(accumulatedAlpha - 0.001f);
    output.color.rgb = premultipliedColor / max(accumulatedAlpha, 0.001f);
    output.color.a = accumulatedAlpha;
    return output;
}
