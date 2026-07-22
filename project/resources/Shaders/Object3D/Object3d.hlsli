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

static const uint32_t kMaxPointLights = 16;

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
    float padding[2];
};
