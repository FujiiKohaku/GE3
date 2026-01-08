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
    float32_t4 color;
    int32_t enableLighting;
    float32_t2 padding;
    float32_t shininess;
    float32_t4x4 uvTransform;
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
    float cosFalloffStart;
    float padding;
};
