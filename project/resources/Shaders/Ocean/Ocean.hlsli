struct OceanWakePoint
{
    float4 positionTimeStrength; // world XZ, birth time, strength
    float4 directionSize; // forward XZ, boost width, unused
};

struct OceanConstants
{
    float4x4 viewProjection;
    float4x4 world;
    float4 cameraPositionAndTime;
    float4 waveParameters;
    float4 deepColor;
    float4 crestColor;
    float4 wakeMeta; // count, mesh focus XZ, reserved
    OceanWakePoint wakes[48];
};

struct OceanVertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0;
    float2 waveData : TEXCOORD0;
    float2 wakeData : TEXCOORD1; // signed interaction height, slope magnitude
};

ConstantBuffer<OceanConstants> gOcean : register(b0);
