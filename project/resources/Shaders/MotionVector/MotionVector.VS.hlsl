#include "Common.hlsli"
MotionVectorOutput main(MotionVectorInput input) {
    return BuildMotionVector(input.position, input.previousPosition);
}
