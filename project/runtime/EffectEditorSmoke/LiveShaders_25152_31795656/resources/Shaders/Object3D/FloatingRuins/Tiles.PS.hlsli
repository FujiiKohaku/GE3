// UVs measure masonry courses, independently of the wall's silhouette.
void ResolveRuinsTiles(VertexShaderOutput input, float2 uv, inout float3 baseColor, inout float3 normal)
{
    const float kJointWidth = 0.018f;
    const float kBevelWidth = 0.09f;
    float row = floor(uv.y);
    float2 tileUv = float2(uv.x + frac(row * 0.5f) , uv.y);
    float2 tileId = floor(tileUv);
    float2 localUv = frac(tileUv);
    float2 edgeDistance = min(localUv, 1.0f - localUv);
    float2 pixelWidth = max(fwidth(uv), 0.0001f);
    float2 stoneCoverage = smoothstep(kJointWidth - pixelWidth, kJointWidth + pixelWidth, edgeDistance);
    float tileCoverage = stoneCoverage.x * stoneCoverage.y;
    // Fade subpixel joints to their average coverage instead of shimmering.
    float detailFade = 1.0f - smoothstep(0.15f, 0.6f, max(pixelWidth.x, pixelWidth.y));
    tileCoverage = lerp(0.9293f, tileCoverage, detailFade);
    float variation = frac(sin(dot(tileId, float2(127.1f, 311.7f))) * 43758.5453f);
    float3 stoneColor = baseColor * lerp(float3(0.88f, 0.90f, 0.92f), float3(1.12f, 1.08f, 1.02f), variation);
    baseColor = lerp(baseColor * float3(0.38f, 0.36f, 0.31f), stoneColor, tileCoverage);
    float2 bevelRate = saturate((edgeDistance - kJointWidth) / kBevelWidth);
    float2 bevelHeight = bevelRate * bevelRate * (3.0f - 2.0f * bevelRate);
    float2 bevelSlope = 6.0f * bevelRate * (1.0f - bevelRate) / kBevelWidth;
    bevelSlope *= sign(0.5f - localUv);
    float2 heightGradient = float2(bevelSlope.x * bevelHeight.y, bevelSlope.y * bevelHeight.x);
    float3 detailNormal = normalize(float3(-heightGradient * 0.06f * detailFade, 1.0f));
    normal = ApplyNormalDetail(normal, ddx(input.worldPosition), ddy(input.worldPosition),
        ddx(uv), ddy(uv), detailNormal, 1.0f, 0.0f);
    gMaterial.roughness = lerp(0.96f, 0.78f, tileCoverage);
    gMaterial.specularStrength = lerp(0.04f, 0.25f, tileCoverage);
}
