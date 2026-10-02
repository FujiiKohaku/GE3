#include "Fullscreen.hlsli"
#include "../Atmosphere/Scattering.hlsli"
Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
Texture2D<float4> gNormalTexture : register(t2);
SamplerState gSampler : register(s0);

float3 ReconstructPosition(float2 uv, float depth) {
    float4 position = mul(float4(uv * float2(2, -2) + float2(-1, 1), depth, 1), screenInverseProjection);
    position.xyz /= max(position.w, 0.000001f);
    return mul(float4(position.xyz, 0), screenCameraRotation).xyz;
}

float4 main(VertexShaderOutput input) : SV_TARGET {
    float4 scene = gTexture.Sample(gSampler, input.texcoord);
    uint width, height;
    gDepthTexture.GetDimensions(width, height);
    int2 pixel = clamp(int2(input.texcoord * float2(width, height)), int2(0, 0), int2(width, height) - 1);
    float depth = gDepthTexture.Load(int3(pixel, 0));
    if (depth >= 0.999999f || screenCameraSettings.z < 0.5f) { return scene; }
    float3 position = ReconstructPosition(input.texcoord, depth);
    if (ssaoSettings.w > 0.5f && ssaoSettings.x > 0.0f && screenCameraSettings.w > 0.5f) {
        float4 encoded = gNormalTexture.Load(int3(pixel, 0));
        if (encoded.a <= -2.0f) { encoded.a = -encoded.a - 2.0f; }
        if (encoded.a >= 0.0f && abs(encoded.a - depth) < 0.002f) {
            float3 normal = normalize(encoded.xyz * 2.0f - 1.0f);
            float viewDepth = screenCameraSettings.x * screenCameraSettings.y /
                max(screenCameraSettings.y - depth * (screenCameraSettings.y - screenCameraSettings.x), 0.000001f);
            float radiusPixels = clamp(ssaoSettings.y * float(height) / max(2.0f * abs(screenInverseProjection[1][1]) * viewDepth, 0.001f), 1.0f, 96.0f);
            float occlusion = 0.0f;
            const uint kSsaoSamples = 16;
            for (uint index = 0; index < kSsaoSamples; ++index) {
                float angle = float(index) * 2.39996323f;
                float radius = sqrt((float(index) + 0.5f) / float(kSsaoSamples)) * radiusPixels;
                int2 samplePixel = pixel + int2(round(float2(cos(angle), sin(angle)) * radius));
                if (any(samplePixel < 0) || any(samplePixel >= int2(width, height))) { continue; }
                float sampleDepth = gDepthTexture.Load(int3(samplePixel, 0));
                if (sampleDepth >= 0.999999f) { continue; }
                float2 sampleUv = (float2(samplePixel) + 0.5f) / float2(width, height);
                float3 delta = ReconstructPosition(sampleUv, sampleDepth) - position;
                float distance = length(delta);
                if (distance <= ssaoSettings.z || distance >= ssaoSettings.y) { continue; }
                float elevation = max(dot(normal, delta / distance) - ssaoSettings.z / distance, 0.0f);
                occlusion += elevation * (1.0f - smoothstep(ssaoSettings.y * 0.5f, ssaoSettings.y, distance));
            }
            // 合成済みForward色への適用なので直接光も弱く減衰する。強度上限で黒つぶれを防ぐ。
            scene.rgb *= 1.0f - saturate(occlusion * 4.0f / float(kSsaoSamples)) * ssaoSettings.x;
        }
    }
    if (atmosphereSettings.x > 0.5f) {
        float distance = length(position);
        float3 direction = position / max(distance, 0.000001f);
        float transmission = exp(-max(atmosphereSettings.y, 0.0f) * distance);
        float3 scattering = AtmosphereRadiance(direction, normalize(screenSunDirection.xyz), screenSunColor.rgb,
            screenSunDirection.w, atmosphereSettings.w) * atmosphereSettings.z;
        scene.rgb = scene.rgb * transmission + scattering * (1.0f - transmission);
    }
    return scene;
}
