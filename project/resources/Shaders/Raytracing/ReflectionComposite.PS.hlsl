#include "ReflectionFilter.hlsli"
float4 main(PixelInput input) : SV_Target0 {
    float4 scene = sceneTexture.SampleLevel(linearSampler, input.texcoord, 0);
    uint fullWidth, fullHeight; depthTexture.GetDimensions(fullWidth, fullHeight);
    int2 fullPixel = clamp(int2(input.position.xy), int2(0, 0), int2(fullWidth, fullHeight) - 1);
    float depth = depthTexture.Load(int3(fullPixel, 0));
    float4 environment = environmentTexture.Load(int3(fullPixel, 0));
    float4 surface = surfaceTexture.Load(int3(fullPixel, 0));
    bool isUnsupportedSurface = depth >= 1 || environment.a < 0 || abs(environment.a - depth) > 0.000001f;
    if (composition.z < 0.5f && surface.a > controls.w) { isUnsupportedSurface = true; }
    if (composition.z > 1.5f && localLightTexture.Load(int3(fullPixel, 0)).a < 0) { isUnsupportedSurface = true; }
    if (isUnsupportedSurface) {
        if (composition.y > 0.5f) { return float4(0, 0, 0, 1); }
        return scene;
    }
    float viewDepth = mul(float4(ReflectionWorld(input.texcoord, depth), 1), view).z;
    float3 normal = normalize(surface.xyz * 2 - 1);
    uint width, height; signalTexture.GetDimensions(width, height);
    float2 position = input.texcoord * float2(width, height) - 0.5f;
    int2 basePixel = int2(floor(position)); float2 fraction = frac(position);
    float4 sum = 0; float weightSum = 0;
    for (int offsetY = 0; offsetY < 2; ++offsetY) {
        for (int offsetX = 0; offsetX < 2; ++offsetX) {
            int2 pixel = clamp(basePixel + int2(offsetX, offsetY), int2(0, 0), int2(width, height) - 1);
            float4 guide = guideTexture.Load(int3(pixel, 0));
            if (composition.z > 1.5f && !HasMatchingLocalMaterial(fullPixel, FullReflectionPixel(pixel))) { continue; }
            if (guide.w <= 0 || abs(guide.w - viewDepth) > max(0.03f, viewDepth * 0.01f)
                || dot(normal, GuideNormal(guide)) < 0.98f) { continue; }
            float2 weight = 1 - fraction;
            if (offsetX == 1) { weight.x = fraction.x; }
            if (offsetY == 1) { weight.y = fraction.y; }
            sum += signalTexture.Load(int3(pixel, 0)) * weight.x * weight.y; weightSum += weight.x * weight.y;
        }
    }
    float4 reflection = 0;
    if (composition.z > 1.5f && weightSum <= 0) {
        if (composition.y > 0.5f) { return float4(0, 0, 0, 1); }
        return scene;
    }
    if (weightSum > 0) { reflection = sum / weightSum; }
    float3 contribution = reflection.rgb - environment.rgb * saturate(reflection.a);
    if (composition.z > 0.5f && composition.z < 1.5f) {
        float4 material = materialTexture.Load(int3(fullPixel, 0));
        float metallicValue = saturate((material.a * 255 - 1) / 254);
        contribution = reflection.rgb * max(material.rgb, 0) * (1 - metallicValue);
    }
    if (composition.z > 1.5f) { contribution = reflection.rgb - localLightTexture.Load(int3(fullPixel, 0)).rgb; }
    if (composition.y > 0.5f) {
        if (composition.z < 0.5f || composition.z > 1.5f) { contribution = reflection.rgb; }
        return float4(contribution, 1);
    }
    scene.rgb += composition.x * contribution;
    return float4(max(scene.rgb, 0), scene.a);
}
