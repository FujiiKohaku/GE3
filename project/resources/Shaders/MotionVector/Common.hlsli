cbuffer CurrentTransform : register(b0) { row_major float4x4 currentWorldViewProjection; };
cbuffer PreviousTransform : register(b1) { row_major float4x4 previousWorldViewProjection; };
cbuffer CurrentParameters : register(b2) { float4 currentParameters; };
cbuffer PreviousParameters : register(b3) { float4 previousParameters; };
cbuffer ProjectionJitter : register(b4) { float2 jitterNdc; };
cbuffer ReprojectionParameters : register(b6) {
    row_major float4x4 previousWorldView;
    row_major float4x4 previousNormalTransform;
    float4 reprojectionControls;
};
#include "Reprojection.hlsli"
struct MotionVectorInput {
    float4 position : POSITION0;
    float4 previousPosition : POSITION1;
    float2 texcoord : TEXCOORD0;
    float3 previousNormal : NORMAL0;
};
struct MotionVectorOutput {
    float4 position : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 previousClip : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float3 previousNormal : TEXCOORD3;
    float previousDepth : TEXCOORD4;
};
MotionVectorOutput BuildMotionVector(float4 position, float4 previousPosition, float2 texcoord = float2(0, 0), float3 previousNormal = float3(0, 0, 0)) {
    MotionVectorOutput output;
    output.currentClip = mul(position, currentWorldViewProjection);
    output.previousClip = mul(previousPosition, previousWorldViewProjection);
    output.position = output.currentClip;
    output.position.xy += jitterNdc * output.position.w;
    output.texcoord = texcoord;
    output.previousDepth = mul(previousPosition, previousWorldView).z;
    output.previousNormal = mul(float4(previousNormal, 0), previousNormalTransform).xyz;
    float normalLength = length(output.previousNormal);
    if (normalLength > 0.000001f) { output.previousNormal /= normalLength; }
    return output;
}
struct MotionPixelOutput { float2 motion : SV_Target0; float4 reprojection : SV_Target1; };
MotionPixelOutput BuildMotionPixel(MotionVectorOutput input) {
    MotionPixelOutput output; output.motion = 0; output.reprojection = float4(0, 0, 0, -reprojectionControls.x);
    if (input.currentClip.w <= 0.00001f || input.previousClip.w <= 0.00001f) { return output; }
    float2 currentUv = input.currentClip.xy / input.currentClip.w * float2(0.5f, -0.5f) + 0.5f;
    float2 previousUv = input.previousClip.xy / input.previousClip.w * float2(0.5f, -0.5f) + 0.5f;
    output.motion = currentUv - previousUv;
    if (reprojectionControls.y > 0 && input.previousDepth > 0 && all(isfinite(input.previousNormal)) && dot(input.previousNormal, input.previousNormal) > 0.000001f) {
        float previousDepth = input.previousDepth;
        if (reprojectionControls.z > 0) { previousDepth = -previousDepth; }
        output.reprojection = float4(EncodeReprojectionNormal(normalize(input.previousNormal)), previousDepth, reprojectionControls.x);
    }
    return output;
}
