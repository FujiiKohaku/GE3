cbuffer CurrentTransform : register(b0) { row_major float4x4 currentWorldViewProjection; };
cbuffer PreviousTransform : register(b1) { row_major float4x4 previousWorldViewProjection; };
cbuffer CurrentParameters : register(b2) { float4 currentParameters; };
cbuffer PreviousParameters : register(b3) { float4 previousParameters; };
cbuffer ProjectionJitter : register(b4) { float2 jitterNdc; };
struct MotionVectorInput {
    float4 position : POSITION0;
    float4 previousPosition : POSITION1;
};
struct MotionVectorOutput {
    float4 position : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 previousClip : TEXCOORD1;
};
MotionVectorOutput BuildMotionVector(float4 position, float4 previousPosition) {
    MotionVectorOutput output;
    output.currentClip = mul(position, currentWorldViewProjection);
    output.previousClip = mul(previousPosition, previousWorldViewProjection);
    output.position = output.currentClip;
    output.position.xy += jitterNdc * output.position.w;
    return output;
}
