cbuffer ShadowWorld : register(b0) { float4x4 gWorld; };
cbuffer ShadowCamera : register(b1) { float4x4 gLightViewProjection; };
float4 main(float4 position : POSITION0) : SV_POSITION
{
    return mul(mul(position, gWorld), gLightViewProjection);
}
