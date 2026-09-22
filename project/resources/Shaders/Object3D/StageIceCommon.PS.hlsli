#include "Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b2);
Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);

struct StageIcePixelOutput
{
    float32_t4 color : SV_Target0;
    float32_t4 encodedNormal : SV_Target1;
};

struct StageIceLighting
{
    float3 normal;
    float3 view;
    float lightBand;
    float3 shade;
    float reflection;
};

float4 SampleStageIceTexture(VertexShaderOutput input)
{
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    return gTexture.Sample(gSampler, transformedUV.xy);
}

StageIcePixelOutput FinishStageIce(VertexShaderOutput input, float3 color, float textureAlpha)
{
    StageIcePixelOutput output;
    output.color = float4(color, gMaterial.color.a * textureAlpha);
    output.encodedNormal = float4(normalize(input.normal) * 0.5f + 0.5f, input.position.z);
    if (gMaterial.enableEnvironmentMap != 0)
    {
        float3 N = normalize(input.normal);
        float3 reflected = reflect(normalize(input.worldPosition - gCamera.worldPosition), N);
        output.color.rgb += gEnvironmentTexture.Sample(gSampler, reflected).rgb *
            gMaterial.environmentCoefficient;
    }
    return output;
}

float IceHash(float2 cell)
{
    return frac(sin(dot(cell, float2(127.1f, 311.7f))) * 43758.5453f);
}

float2 IcePlatePattern(float2 worldPosition, float plateSize)
{
    // Irregular, straight-edged plates provide detail without flowing bands.
    float2 p = worldPosition / plateSize;
    float2 cell = floor(p);
    float nearest = 100.0f;
    float secondNearest = 100.0f;
    float plateTone = 0.5f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 candidate = cell + float2(x, y);
            float seed = IceHash(candidate);
            float2 center = candidate + float2(0.20f, 0.20f) +
                float2(seed, frac(seed * 37.719f)) * 0.60f;
            float2 delta = p - center;
            float distanceSquared = dot(delta, delta);
            if (distanceSquared < nearest)
            {
                secondNearest = nearest;
                nearest = distanceSquared;
                plateTone = seed;
            }
            else
            {
                secondNearest = min(secondNearest, distanceSquared);
            }
        }
    }
    float edgeDistance = sqrt(secondNearest) - sqrt(nearest);
    float edgeAA = max(fwidth(edgeDistance), 0.002f);
    float edge = 1.0f - smoothstep(0.012f, 0.012f + edgeAA, edgeDistance);
    return float2(plateTone, edge);
}

float FloorCrack(float2 worldXZ)
{
    // Sparse, short fractures complement the plate boundaries.
    float2 p = worldXZ / 72.0f;
    float2 cell = floor(p);
    float2 local = frac(p);
    float seed = IceHash(cell);
    float2 start = float2(0.10f, 0.18f + 0.28f * IceHash(cell + 13.0f));
    float2 end = float2(0.78f, 0.70f + 0.20f * IceHash(cell + 29.0f));
    float2 segment = end - start;
    float t = saturate(dot(local - start, segment) / dot(segment, segment));
    float distanceToCrack = length(local - (start + segment * t));
    float aa = max(fwidth(distanceToCrack), 0.002f);
    return (1.0f - smoothstep(0.007f, 0.007f + aa, distanceToCrack)) *
        (seed < 0.55f ? 1.0f : 0.0f);
}

StageIceLighting GetStageIceLighting(VertexShaderOutput input)
{
    StageIceLighting lighting;
    lighting.normal = normalize(input.normal);
    lighting.view = normalize(gCamera.worldPosition - input.worldPosition);
    float faceLight = saturate(dot(lighting.normal,
        normalize(float3(-0.6f, 0.8f, -0.5f))) * 0.5f + 0.5f);
    float middleBand = smoothstep(0.35f, 0.37f, faceLight);
    lighting.lightBand = smoothstep(0.71f, 0.73f, faceLight);
    lighting.shade = lerp(float3(0.33f, 0.52f, 0.68f),
        float3(0.70f, 0.84f, 0.93f), middleBand);
    lighting.shade = lerp(lighting.shade,
        float3(0.91f, 0.96f, 1.0f), lighting.lightBand);
    float3 H = normalize(normalize(float3(0.6f, 0.8f, 0.5f)) + lighting.view);
    lighting.reflection = pow(saturate(dot(lighting.normal, H)), 48.0f);
    return lighting;
}

float3 ShadeStageIceFloor(VertexShaderOutput input, float3 textureColor)
{
    StageIceLighting lighting = GetStageIceLighting(input);
    float2 plates = IcePlatePattern(input.worldPosition.xz, 58.0f);
    float crack = FloorCrack(input.worldPosition.xz);
    float grazing = pow(1.0f - saturate(dot(lighting.normal, lighting.view)), 3.0f);
    float plateVariation = lerp(0.93f, 1.05f, plates.x);
    return gMaterial.color.rgb * textureColor *
        (lighting.shade * float3(0.84f, 0.94f, 1.0f) * plateVariation *
         (1.0f - 0.15f * plates.y - 0.25f * crack) +
         float3(0.045f, 0.075f, 0.09f) * grazing +
         float3(0.045f, 0.065f, 0.075f) * lighting.reflection);
}

float3 ShadeStageIceIsland(VertexShaderOutput input, float3 textureColor)
{
    StageIceLighting lighting = GetStageIceLighting(input);
    float snow = smoothstep(0.72f, 0.88f, lighting.normal.y);
    float2 patternPosition = lerp(input.worldPosition.xy, input.worldPosition.xz,
        abs(lighting.normal.y));
    float2 plates = IcePlatePattern(patternPosition, 8.0f);
    float3 blueIce = lighting.shade * float3(0.57f, 0.82f, 0.99f) *
        lerp(0.94f, 1.04f, plates.x) * (1.0f - 0.10f * plates.y);
    float3 snowTop = lerp(float3(0.78f, 0.86f, 0.94f),
        float3(0.94f, 0.97f, 1.0f), lighting.lightBand);
    return gMaterial.color.rgb * textureColor *
        (lerp(blueIce, snowTop, snow) +
         (1.0f - snow) * lighting.reflection * float3(0.035f, 0.05f, 0.06f));
}

float3 ShadeStageIceStructure(VertexShaderOutput input, float3 textureColor,
    float3 materialTint, float rimStrength)
{
    StageIceLighting lighting = GetStageIceLighting(input);
    float rim = pow(1.0f - saturate(dot(lighting.normal, lighting.view)), 4.0f) * rimStrength;
    float2 patternPosition = lerp(input.worldPosition.xy, input.worldPosition.xz,
        abs(lighting.normal.y));
    float2 plates = IcePlatePattern(patternPosition, 6.5f);
    float pattern = lerp(0.94f, 1.04f, plates.x) * (1.0f - 0.09f * plates.y);
    return gMaterial.color.rgb * textureColor *
        (lighting.shade * materialTint * pattern +
         rim * float3(0.65f, 0.90f, 1.0f) +
         lighting.reflection * float3(0.035f, 0.055f, 0.07f));
}
