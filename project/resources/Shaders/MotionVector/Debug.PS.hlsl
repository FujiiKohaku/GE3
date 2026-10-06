Texture2D<float2> motionVectors : register(t0);
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0) : SV_TARGET0 {
    float2 velocity = motionVectors.Load(int3(int2(position.xy), 0));
    float speed = saturate(length(velocity) * 40.0f);
    float2 direction = float2(0, 0);
    if (speed > 0.00001f) { direction = normalize(velocity); }
    return float4(float3(direction * 0.5f + 0.5f, 0.5f) * speed, 1);
}
