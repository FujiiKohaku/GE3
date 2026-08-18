#include "WaterPillar.hlsli"

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float Noise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(Hash21(cell), Hash21(cell + float2(1, 0)), f.x),
        lerp(Hash21(cell + float2(0, 1)), Hash21(cell + 1.0f), f.x), f.y);
}

float Fbm(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    [unroll]
    for (int octave = 0; octave < 4; ++octave) {
        value += Noise(p) * amplitude;
        p = mul(p, float2x2(1.62f, -1.17f, 1.17f, 1.62f)) + 7.3f;
        amplitude *= 0.5f;
    }
    return value;
}

float4 main(WaterPillarVertexOutput input) : SV_TARGET
{
    const float time = gPillar.cameraPositionAndTime.w;

    // 内部の球形バブルは円として見えるため描画せず、表面ノイズへ統一する。
    if (input.kind >= 0.5f) {
        discard;
    }

    const float radius = max(gPillar.pillarPositionAndRadius.w, 0.001f);
    const float height = max(gPillar.heightAndShape.x, 0.001f);
    const float3 localPosition = input.worldPosition - gPillar.pillarPositionAndRadius.xyz;
    const float2 horizontalPosition = localPosition.xz / radius;
    const float normalizedHeight = localPosition.y / height;
    const float verticalFlow = normalizedHeight * 7.0f - time * 2.8f;

    // メッシュのUV段ではなく、連続したワールド位置から流れを生成する。
    float2 flowUv = horizontalPosition * 2.35f + float2(verticalFlow * 0.71f, verticalFlow * 1.13f);
    float broadFlow = Fbm(flowUv);
    float2 fineUv = horizontalPosition * 5.4f + float2(verticalFlow * -1.37f, verticalFlow * 0.83f);
    float fineFlow = Fbm(fineUv + float2(time * 0.45f, -time * 1.6f));
    float stream = smoothstep(0.48f, 0.78f, broadFlow * 0.65f + fineFlow * 0.55f);
    float foam = smoothstep(0.68f, 0.88f, fineFlow + broadFlow * 0.32f);

    float3 viewDirection = normalize(gPillar.cameraPositionAndTime.xyz - input.worldPosition);
    float fresnel = pow(1.0f - saturate(abs(dot(normalize(input.normal), viewDirection))), 2.2f);

    float verticalShade = lerp(0.72f, 1.08f, saturate(1.0f - normalizedHeight));

    float3 deepWater = float3(0.004f, 0.075f, 0.22f);
    float3 clearWater = gPillar.color.rgb * float3(0.48f, 0.62f, 0.72f);
    float3 waterColor = lerp(deepWater, clearWater, broadFlow * 0.72f + fresnel * 0.28f);
    waterColor *= verticalShade;
    waterColor += stream * float3(0.08f, 0.28f, 0.34f);
    waterColor = lerp(waterColor, float3(0.82f, 0.97f, 1.0f), foam * 0.72f + fresnel * 0.22f);

    float alpha = gPillar.color.a * saturate(0.76f + broadFlow * 0.14f + foam * 0.16f + fresnel * 0.12f);
    return float4(waterColor, alpha);
}
