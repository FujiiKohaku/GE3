Texture2D<float4> sceneColor : register(t0);
Texture2D<float4> directionalLight : register(t1);
Texture2D<float> shadowVisibility : register(t2);
cbuffer CompositeParameters : register(b0) { uint isDebugVisible; };
float4 main(float4 position : SV_Position) : SV_Target0 {
    int2 pixel = int2(position.xy);
    float visibility = saturate(shadowVisibility.Load(int3(pixel, 0)));
    if (isDebugVisible != 0) { return float4(visibility.xxx, 1); }
    float4 color = sceneColor.Load(int3(pixel, 0));
    float3 sun = directionalLight.Load(int3(pixel, 0)).rgb;
    color.rgb = max(color.rgb - sun * (1.0f - visibility), 0.0f);
    return color;
}
