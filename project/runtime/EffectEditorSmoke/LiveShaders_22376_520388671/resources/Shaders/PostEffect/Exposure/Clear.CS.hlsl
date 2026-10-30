#include "Common.hlsli"
[numthreads(256, 1, 1)]
void main(uint index : SV_GroupIndex) { histogram[index] = 0; }
