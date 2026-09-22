//03_00
struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : POSITION0;
};


//05_03
struct Material
{
    float4 color;
    int enableLighting;
    int enableEnvironmentMap;
    float shininess;
    float environmentCoefficient;
    float4x4 uvTransform;
};

static const int kShadingUnlit = 0;
static const int kShadingStandard = 1;
static const int kShadingIce = 2;
static const int kShadingArchivePaper = 3;
static const int kShadingArchiveLeather = 4;
static const int kShadingArchiveBrass = 5;
static const int kShadingToon = 6;

float3 ShadeToonSurface(
    float3 baseColor,
    float3 normal,
    float3 viewDirection,
    float3 lightDirection,
    float3 lightColor,
    float lightIntensity)
{
    float3 N = normalize(normal);
    float3 L = normalize(lightDirection);
    float3 V = normalize(viewDirection);

    // Half-Lambert keeps the dark side readable, then two narrow transitions
    // turn the smooth light into three animation-style color bands.
    float halfLambert = dot(N, L) * 0.5f + 0.5f;
    float middleBand = smoothstep(0.34f, 0.38f, halfLambert);
    float lightBand = smoothstep(0.68f, 0.72f, halfLambert);

    float3 shadowColor = baseColor * float3(0.34f, 0.46f, 0.66f);
    float3 middleColor = baseColor * float3(0.72f, 0.82f, 0.96f);
    float3 litColor = baseColor * 1.06f;
    float3 toonColor = lerp(shadowColor, middleColor, middleBand);
    toonColor = lerp(toonColor, litColor, lightBand);

    float3 H = normalize(L + V);
    float graphicHighlight = step(0.955f, saturate(dot(N, H)));
    float rim = smoothstep(0.72f, 0.88f, 1.0f - saturate(dot(N, V)));
    float3 highlightColor = float3(0.82f, 0.96f, 1.0f);

    float effectiveIntensity = clamp(lightIntensity, 0.65f, 1.25f);
    toonColor *= lerp(float3(1.0f, 1.0f, 1.0f), lightColor, 0.32f);
    toonColor *= effectiveIntensity;
    toonColor += highlightColor * graphicHighlight * 0.20f;
    toonColor += highlightColor * rim * 0.08f;
    return toonColor;
}
//05_03
struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
struct AmbientLight
{
    float4 color;
};
struct Camera
{
    float32_t3 worldPosition;
};
struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius; // ライトの届く最大距離
    float decay; // 減衰率
    int32_t isActive;
    float padding;
};

static const uint32_t kMaxPointLights = 32;

struct PointLightCollection
{
    PointLight lights[kMaxPointLights];
    uint32_t activeCount;
    float32_t3 padding;
};
//スポットライト
struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    int32_t isActive;
    float cosFalloffStart;
};

static const uint32_t kMaxSpotLights = 8;

struct SpotLightCollection
{
    SpotLight lights[kMaxSpotLights];
    uint32_t activeCount;
    float32_t3 padding;
};
