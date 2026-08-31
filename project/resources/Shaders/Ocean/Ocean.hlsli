struct OceanConstants
{
    float4x4 viewProjection;
    float4x4 world;
    float4 cameraPositionAndTime;
    float4 waveParameters;
    float4 deepColor;
    float4 crestColor;
};

struct OceanVertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0;
    float2 waveData : TEXCOORD0;
};

ConstantBuffer<OceanConstants> gOcean : register(b0);
