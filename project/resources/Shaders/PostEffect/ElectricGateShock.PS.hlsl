#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float Hash(float value)
{
    return frac(sin(value * 127.1f) * 43758.5453f);
}

float ArcOffset(float position, float seed)
{
    // 区間ごとに折れ曲がる稲妻の輪郭を作る。
    float segment = floor(position);
    return lerp(Hash(segment + seed), Hash(segment + seed + 1.0f), frac(position)) - 0.5f;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float4 color = gTexture.Sample(gSampler, uv);
    float edgeDistance = min(uv.x, 1.0f - uv.x);
    if (edgeDistance >= 0.20f) {
        return color;
    }

    float progress = saturate(customParameter0);
    float side = step(0.5f, uv.x);
    float seed = 13.0f + side * 31.0f;
    float signedHeight = uv.y - lerp(0.37f, 0.64f, side);
    float reach = lerp(0.035f, 0.19f, smoothstep(0.0f, 0.22f, progress));
    float fade = 1.0f - smoothstep(0.10f, 1.0f, progress);
    float extent = 1.0f - smoothstep(reach * 0.55f, reach, edgeDistance);

    float mainArcY = ArcOffset(edgeDistance * 112.0f, seed) * 0.13f;
    float branchY = ArcOffset(edgeDistance * 163.0f, seed + 47.0f) * 0.09f;
    branchY += edgeDistance * 0.65f;
    float mainDistance = abs(signedHeight - mainArcY);
    float branchDistance = abs(signedHeight - branchY);
    float core = 1.0f - smoothstep(0.0015f, 0.0045f, mainDistance);
    float branch = 1.0f - smoothstep(0.0010f, 0.0035f, branchDistance);
    float glow = exp2(-mainDistance * 100.0f) + exp2(-branchDistance * 135.0f) * 0.6f;
    float edgeGlow = exp2(-edgeDistance * 34.0f - abs(signedHeight) * 10.0f) * 0.22f;

    color.rgb += fade * (extent * (float3(0.66f, 1.35f, 2.0f) * (core + branch * 0.65f) +
        float3(0.08f, 0.43f, 1.05f) * glow) + float3(0.04f, 0.30f, 0.75f) * edgeGlow);
    return color;
}
