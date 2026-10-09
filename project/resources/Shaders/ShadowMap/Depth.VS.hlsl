cbuffer ShadowWorld : register(b0) { float4x4 gWorld; };
cbuffer ShadowCamera : register(b1) { float4x4 gLightViewProjection; };
struct DepthOutput { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; };
DepthOutput main(float4 position : POSITION0, float2 texcoord : TEXCOORD0)
{
    DepthOutput output;
    output.position = mul(mul(position, gWorld), gLightViewProjection);
    output.texcoord = texcoord;
    return output;
}
