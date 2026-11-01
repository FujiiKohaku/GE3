#include "../../MotionVector/Common.hlsli"
#include "Deformation.hlsli"
float4 DeformMotionPosition(float4 position, float4 parameters) {
    float3 normal = float3(0, 1, 0);
    if (parameters.w < 0.5f) { DeformBell(position.xyz, normal, parameters); }
    else { DeformTentacle(position.xyz, normal, parameters); }
    return position;
}
MotionVectorOutput main(MotionVectorInput input) {
    return BuildMotionVector(DeformMotionPosition(input.position, currentParameters),
        DeformMotionPosition(input.previousPosition, previousParameters));
}
