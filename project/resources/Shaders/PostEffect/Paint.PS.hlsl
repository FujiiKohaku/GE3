#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 2D 勾配ノイズ用ハッシュ
float2 PaintHash2(float2 p)
{
    p = float2(dot(p, float2(127.1f, 311.7f)), dot(p, float2(269.5f, 183.3f)));
    return -1.0f + 2.0f * frac(sin(p) * 43758.5453123f);
}

// 2D Perlin Gradient Noise
float PaintGradientNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    float2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    float2 g00 = PaintHash2(i + float2(0.0f, 0.0f));
    float2 g10 = PaintHash2(i + float2(1.0f, 0.0f));
    float2 g01 = PaintHash2(i + float2(0.0f, 1.0f));
    float2 g11 = PaintHash2(i + float2(1.0f, 1.0f));

    float n00 = dot(g00, f - float2(0.0f, 0.0f));
    float n10 = dot(g10, f - float2(1.0f, 0.0f));
    float n01 = dot(g01, f - float2(0.0f, 1.0f));
    float n11 = dot(g11, f - float2(1.0f, 1.0f));

    float n = lerp(lerp(n00, n10, u.x), lerp(n01, n11, u.x), u.y);
    return 0.5f + 0.5f * n;
}

// 幾何学的回転を加えた FBM ノイズ
float PaintFBM(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float2x2 rot = float2x2(0.80f, 0.60f, -0.60f, 0.80f);

    for (int i = 0; i < 4; i++)
    {
        value += amplitude * PaintGradientNoise(p);
        p = mul(rot, p) * 2.02f;
        amplitude *= 0.5f;
    }
    return value;
}

// キュートなハート型シルエット判定
float HeartShape(float2 p)
{
    p.y = -p.y + 0.10f; // 正しいハート向きに調整
    float x = p.x * 2.4f;
    float y = p.y * 2.4f;
    float a = x * x + y * y - 0.42f;
    float d = a * a * a - x * x * y * y * y;
    return smoothstep(0.08f, -0.08f, d);
}

// 星型（5角星）シルエット判定
float StarShape(float2 p)
{
    float r = length(p);
    float angle = atan2(p.y, p.x);
    float starMod = 0.35f + 0.20f * cos(angle * 5.0f);
    return smoothstep(starMod + 0.05f, starMod - 0.15f, r);
}

// リング（ドーナツ）型シルエット判定
float RingShape(float2 p)
{
    float r = length(p);
    float outer = smoothstep(0.48f, 0.35f, r);
    float inner = smoothstep(0.12f, 0.22f, r);
    return outer * inner;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float4 baseColor = gTexture.Sample(gSampler, uv);

    if (paintIntensity <= 0.001f)
    {
        return baseColor;
    }

    // 重力によるドロドロ垂れ落ち
    float dripTime = max(0.0f, paintProgress - 0.20f) / 0.80f;
    float dripCurve = dripTime * dripTime;

    float2 drippedUv = uv;
    float dripNoise = PaintGradientNoise(float2(uv.x * 4.0f + paintSeed, 2.0f + paintSeed * 0.5f));
    float dripAmount = dripCurve * (0.50f + 0.50f * dripNoise);
    drippedUv.y -= dripAmount;

    // paintSeed によるランダム位置オフセット
    float seedX = (PaintGradientNoise(float2(paintSeed, 1.1f)) - 0.5f) * 0.30f;
    float seedY = (PaintGradientNoise(float2(paintSeed, 2.2f)) - 0.5f) * 0.20f;
    float seedSize = 0.85f + PaintGradientNoise(float2(paintSeed, 3.3f)) * 0.30f;

    float2 mainCenter = drippedUv - float2(0.5f + seedX, 0.40f + seedY);
    float mainDist = length(mainCenter);

    float fbmNoise = PaintFBM(drippedUv * 5.0f + float2(paintSeed, -paintSeed)) * 0.30f;

    float mainBlob = 0.0f;

    // パターンタイプに応じた形状生成
    if (paintPatternType == 1)
    {
        // 稀に出現する【ハート型インク】 (ノイズでインクフチを自然に歪ませる)
        float2 heartCenter = mainCenter;
        heartCenter.y /= seedSize;
        heartCenter.x /= seedSize;
        float heartBase = HeartShape(heartCenter + fbmNoise * 0.15f);
        mainBlob = saturate(heartBase + (1.0f - mainDist) * fbmNoise * 0.4f);
    }
    else if (paintPatternType == 2)
    {
        // 【星型 / クロススプラッター】
        float2 starCenter = mainCenter / seedSize;
        float starBase = StarShape(starCenter + fbmNoise * 0.20f);
        mainBlob = saturate(starBase + fbmNoise * 0.3f);
    }
    else if (paintPatternType == 3)
    {
        // 【リング / ドーナツスプラッター】
        float2 ringCenter = mainCenter / seedSize;
        float ringBase = RingShape(ringCenter + fbmNoise * 0.15f);
        mainBlob = saturate(ringBase + fbmNoise * 0.25f);
    }
    else
    {
        // 【通常スプラッター (Default)】
        float2 defaultCenter = mainCenter;
        defaultCenter.x *= 1.3f / seedSize;
        defaultCenter.y /= seedSize;
        mainBlob = smoothstep((0.45f + fbmNoise) * seedSize, 0.08f, length(defaultCenter));
    }

    // 周囲に飛び散るサブ水沫
    float subX1 = (PaintGradientNoise(float2(paintSeed, 4.4f)) - 0.5f) * 0.42f;
    float subY1 = (PaintGradientNoise(float2(paintSeed, 5.5f)) - 0.5f) * 0.35f;
    float2 subCenter1 = drippedUv - float2(0.32f + subX1, 0.50f + subY1);
    float subBlob1 = smoothstep(0.25f + fbmNoise * 0.6f, 0.04f, length(subCenter1 * 1.2f));

    float subX2 = (PaintGradientNoise(float2(paintSeed, 6.6f)) - 0.5f) * 0.42f;
    float subY2 = (PaintGradientNoise(float2(paintSeed, 7.7f)) - 0.5f) * 0.35f;
    float2 subCenter2 = drippedUv - float2(0.68f + subX2, 0.38f + subY2);
    float subBlob2 = smoothstep(0.28f + fbmNoise * 0.6f, 0.05f, length(subCenter2 * 1.2f));

    // 下への垂れ筋
    float streakNoise = PaintFBM(float2(uv.x * 6.0f + paintSeed, drippedUv.y * 2.0f + paintSeed));
    float streaks = step(0.58f, streakNoise) * smoothstep(0.80f, 0.15f, mainDist);

    // インク全パターンの合成
    float paintPattern = saturate(mainBlob + subBlob1 * 0.85f + subBlob2 * 0.9f + streaks * 0.5f);

    // 画面外枠のスムーズエッジマスク
    float edgeFadeX = smoothstep(0.0f, 0.03f, uv.x) * (1.0f - smoothstep(0.97f, 1.0f, uv.x));
    float edgeFadeY = smoothstep(0.0f, 0.03f, uv.y) * (1.0f - smoothstep(0.97f, 1.0f, uv.y));
    float screenEdgeMask = edgeFadeX * edgeFadeY;

    // 消失フェードアウト
    float fadeOut = 1.0f - smoothstep(0.65f, 1.0f, paintProgress);
    float finalAlpha = saturate(paintPattern * fadeOut * paintIntensity * screenEdgeMask * 1.5f);

    // カラー適用
    float3 inkColor = paintColor;
    float3 darkInkColor = paintColor * 0.45f;
    float3 finalInkColor = lerp(inkColor, darkInkColor, saturate(drippedUv.y * 1.2f));

    float3 outputColor = lerp(baseColor.rgb, finalInkColor, finalAlpha);

    return float4(outputColor, baseColor.a);
}
