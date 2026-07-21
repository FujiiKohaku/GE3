#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// プロシージャルハッシュノイズ関数
float hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

// コックピットガラス水滴（Rain Drops）の屈折オフセット計算
float2 N22(float2 p)
{
    float3 a = frac(p.xyx * float3(123.34f, 234.34f, 345.65f));
    a += dot(a, a.yzx + 34.45f);
    return frac((a.xx + a.yz) * a.zy);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;

    // 時間経過と自機スピードに応じた水滴の流れ速度
    float t = time * 0.8f;

    // 水滴グリッド（16:9アスペクト比補正）
    float2 aspectUV = uv * float2(16.0f, 9.0f) * 1.5f;

    // スーッと垂れる水滴の移動
    aspectUV.y += t * 0.6f;

    float2 id = floor(aspectUV);
    float2 gv = frac(aspectUV) - 0.5f;

    // セルごとのランダム値
    float2 n = N22(id);

    // 水滴の位置オフセット
    float2 dropPos = (n - 0.5f) * 0.6f;
    float d = length(gv - dropPos);

    // 水滴の丸みと大きさ
    float dropSize = lerp(0.08f, 0.22f, n.x);
    float dropMask = smoothstep(dropSize, dropSize - 0.05f, d);

    // 垂れる余波（水滴の尾）
    float trailMask = smoothstep(0.06f, 0.0f, abs(gv.x - dropPos.x)) * smoothstep(dropPos.y, dropPos.y - 0.45f, gv.y) * dropMask;

    // 水滴によるレンズ屈折ベクトル（水滴の形に沿った球面屈折）
    float2 dropNormal = (gv - dropPos) * dropMask * 2.5f;

    // 水滴屈折が影響するトータルオフセット
    float2 refractionOffset = dropNormal * 0.035f;

    if (length(refractionOffset) <= 0.0001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 水滴越しに背景世界をレンズ屈折描画！
    float2 distortedUV = uv + refractionOffset;
    float4 color = gTexture.Sample(gSampler, distortedUV);

    // 水滴のフチ（ハイライトハイライト光線）
    float highlight = pow(clamp(1.0f - d / dropSize, 0.0f, 1.0f), 3.0f) * dropMask;
    color.rgb += float3(0.30f, 0.35f, 0.40f) * highlight;

    return color;
}
