cbuffer ReflectionParameters : register(b0) {
    row_major float4x4 inverseViewProjection;
    row_major float4x4 view;
    row_major float4x4 previousViewProjection;
    row_major float4x4 previousView;
    float4 cameraPosition;
    float4 controls; // distance, normal bias, ray bias, max roughness
    float4 temporal; // history valid, history weight, jitter delta UV
    float4 options; // samples, frame index, sun shadows, has motion
    float4 composition; // strength, debug view
    uint4 rtLocalLightMasks;
    float4 rtLocalShadowControls;
    float4 historyValidation; // changed geometry/lighting, temporal enabled, reserved
};
float3 ReflectionWorld(float2 uv, float depth) {
    float4 position = mul(float4(uv.x * 2 - 1, 1 - uv.y * 2, depth, 1), inverseViewProjection);
    return position.xyz / position.w;
}
