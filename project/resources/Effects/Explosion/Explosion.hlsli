#ifndef EFFECT_EXPLOSION_HLSLI
#define EFFECT_EXPLOSION_HLSLI

struct ExplosionVertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    float3 worldPosition : WORLDPOSITION0;
    float viewDistance : TEXCOORD1;
    nointerpolation float lifeRatio : TEXCOORD2;
    nointerpolation float shapeSeed : TEXCOORD3;
};

#endif
