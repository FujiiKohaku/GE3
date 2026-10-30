float MountainHash(float3 position)
{
    position = frac(position * 0.1031f);
    position += dot(position, position.yzx + 33.33f);
    return frac((position.x + position.y) * position.z);
}

float MountainNoise(float3 position)
{
    float3 cell = floor(position);
    float3 fraction = frac(position);
    float3 blend = fraction * fraction * (3.0f - 2.0f * fraction);
    float lower = lerp(
        lerp(MountainHash(cell), MountainHash(cell + float3(1, 0, 0)), blend.x),
        lerp(MountainHash(cell + float3(0, 1, 0)), MountainHash(cell + float3(1, 1, 0)), blend.x), blend.y);
    float upper = lerp(
        lerp(MountainHash(cell + float3(0, 0, 1)), MountainHash(cell + float3(1, 0, 1)), blend.x),
        lerp(MountainHash(cell + float3(0, 1, 1)), MountainHash(cell + float3(1, 1, 1)), blend.x), blend.y);
    return lerp(lower, upper, blend.z);
}

float3 MountainBump(float3 position, float3 normal, float height)
{
    float3 positionDx = ddx(position);
    float3 positionDy = ddy(position);
    float3 tangentX = cross(positionDy, normal);
    float3 tangentY = cross(normal, positionDx);
    float determinant = dot(positionDx, tangentX);
    float3 gradient = ddx(height) * tangentX + ddy(height) * tangentY;
    float3 bumped = abs(determinant) * normal - sign(determinant) * gradient;
    return normalize(bumped + normal * 0.0000001f);
}

void ResolveMountainSurface(VertexShaderOutput input, float3 sourceColor,
    inout float3 baseColor, inout float3 normal)
{
    // These predicates distinguish the supplied flat Kd textures without changing UVs.
    bool isGrass = sourceColor.g > sourceColor.r * 1.08f && sourceColor.g > sourceColor.b * 1.3f;
    bool isLimestone = !isGrass && sourceColor.r > 0.55f &&
        sourceColor.r > sourceColor.g * 1.01f && sourceColor.r < sourceColor.g * 1.16f &&
        sourceColor.g > sourceColor.b * 1.2f;
    float3 position = input.worldPosition;
    float broadNoise = MountainNoise(position * float3(0.065f, 0.11f, 0.065f));
    float fineNoise = MountainNoise(position * 0.85f + float3(12.7f, 4.3f, 8.1f));
    float pixelWidth = max(length(ddx(position)), length(ddy(position))) * 0.85f;
    float detailFade = 1.0f - smoothstep(0.3f, 1.1f, pixelWidth);
    float grain = (fineNoise - 0.5f) * detailFade;
    float sourceLuma = dot(sourceColor, float3(0.2126f, 0.7152f, 0.0722f));
    float facetVariation = lerp(0.90f, 1.08f, saturate((sourceLuma - 0.24f) / 0.45f));
    float reliefHeight = broadNoise * 0.20f + grain * 0.12f;
    float upward = smoothstep(0.25f, 0.85f, normal.y);
    float moss = upward * smoothstep(0.42f, 0.75f, broadNoise);
    gMaterial.metallic = 0.0f;
    gMaterial.roughness = 0.91f;
    gMaterial.specularStrength = 0.18f;

    if (isGrass)
    {
        baseColor = lerp(float3(0.075f, 0.13f, 0.027f), float3(0.23f, 0.32f, 0.075f), broadNoise);
        baseColor *= facetVariation * (1.0f + grain * 0.30f);
        gMaterial.roughness = 0.97f;
        gMaterial.specularStrength = 0.08f;
        reliefHeight = broadNoise * 0.10f + grain * 0.10f;
    }
    else if (isLimestone)
    {
        float weathering = smoothstep(0.25f, 0.8f, broadNoise);
        baseColor = lerp(float3(0.42f, 0.34f, 0.20f), float3(0.68f, 0.58f, 0.37f), weathering);
        baseColor *= facetVariation * (1.0f + grain * 0.14f);
        baseColor = lerp(baseColor, float3(0.17f, 0.22f, 0.055f), moss * 0.30f);
        gMaterial.roughness = lerp(0.94f, 0.79f, weathering);
        gMaterial.specularStrength = 0.22f;
        reliefHeight = broadNoise * 0.08f + grain * 0.065f;
    }
    else
    {
        float strata = 0.5f + 0.5f * sin(position.y * 0.31f + broadNoise * 7.0f);
        float fracture = 1.0f - smoothstep(0.015f, 0.10f, abs(broadNoise - 0.48f));
        baseColor = lerp(float3(0.24f, 0.17f, 0.095f), float3(0.48f, 0.36f, 0.21f),
            broadNoise * 0.70f + strata * 0.30f);
        baseColor *= facetVariation * (1.0f + grain * 0.24f) * (1.0f - fracture * 0.18f);
        baseColor = lerp(baseColor, float3(0.12f, 0.19f, 0.045f), moss * 0.42f);
        // Stage01 sea level is -30: damp rock stays darker and slightly smoother.
        float wetness = 1.0f - smoothstep(-29.0f, -18.0f, position.y + broadNoise * 2.0f);
        baseColor *= lerp(1.0f, 0.65f, wetness);
        gMaterial.roughness = lerp(0.92f, 0.48f, wetness);
        gMaterial.specularStrength = lerp(0.18f, 0.42f, wetness);
        reliefHeight -= fracture * 0.06f;
    }
    normal = MountainBump(position, normal, reliefHeight);
    baseColor *= gMaterial.color.rgb;
}
