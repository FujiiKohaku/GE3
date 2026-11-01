cbuffer ExposureConstants : register(b0) {
    float4 luminanceRange;
    float4 exposureRange;
    float4 adaptation;
    float4 percentiles;
};
RWStructuredBuffer<uint> histogram : register(u0);
