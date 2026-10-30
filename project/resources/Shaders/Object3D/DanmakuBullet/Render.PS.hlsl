#include "../Object3d.hlsli"

ConstantBuffer<Material> materialParameters : register(b0);
ConstantBuffer<Camera> gCamera : register(b2);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
    float32_t4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output = (PixelShaderOutput)0;

    // 視線ベクトルと法線の内積 (中心ほど 1.0、輪郭ほど 0.0)
    float3 N = normalize(input.normal);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);
    float NdotV = saturate(dot(N, V));

    // 東方風弾幕の光学グラデーション
    // 1. 中心のホワイトコア (急峻に立ち上がる高輝度白発光)
    float core = pow(NdotV, 3.2f);

    // 2. 外縁のアルファ減衰 (球体の硬い輪郭を消し、ふんわり溶け込ませる)
    float edgeFade = smoothstep(0.02f, 0.65f, NdotV);

    // 3. 透明感のあるアルファ設計 (コアはしっかり存在し、オーラ部分は透き通る)
    float alpha = edgeFade * lerp(0.55f, 1.0f, core) * saturate(materialParameters.color.a);

    // 4. カラー合成 (マテリアルカラー -> ホワイトコア)
    float3 bulletColor = materialParameters.color.rgb;
    // テクスチャがある場合は乗算
    float4 texColor = gTexture.Sample(gSampler, input.texcoord);
    bulletColor *= texColor.rgb;

    // コアに近づくにつれて白く
#if defined(KOHAKU_PLAYER_BULLET)
    // A small bright core leaves the heat color readable across most of the sphere.
    float playerCore = pow(NdotV, 18.0f);
    float3 coreColor = lerp(bulletColor, float3(1.0f, 1.0f, 1.0f), 0.22f);
    float3 finalRgb = lerp(bulletColor, coreColor, playerCore);
    finalRgb *= 1.25f + playerCore * 1.8f;

    // Keep the silhouette opaque and unlit so it survives bright ocean backgrounds.
    const float3 kPlayerOutlineColor = float3(0.008f, 0.018f, 0.045f);
    float outlineSoftness = clamp(fwidth(NdotV) * 0.5f, 0.015f, 0.08f);
    float outlineMask = 1.0f - smoothstep(0.28f - outlineSoftness, 0.42f + outlineSoftness, NdotV);
    finalRgb = lerp(finalRgb, kPlayerOutlineColor, outlineMask);
    alpha = saturate(materialParameters.color.a);
#else
    const float3 kEnemyOutlineColor = float3(0.0f, 0.0f, 0.0f);
    const float3 kEnemyBodyTint = float3(1.0f, 0.16f, 0.07f);
    const float3 kEnemyCoreColor = float3(1.0f, 0.55f, 0.60f);
    const float kEnemyBodyOpacity = 0.70f;
    const float kEnemyOutlineOpacity = 1.0f;
    const float kEnemyCoreOpacity = 0.88f;

    // 黒い外周で形を保ち、朱色の胴体は海面が少し透けるようにする。
    // 球の投影半径に対する外周幅を、従来の境界値から1.3倍へ広げる。
    float enemyOutlineMask = 1.0f - smoothstep(0.284353f, 0.563915f, NdotV);
    float enemyEdgeFade = smoothstep(0.02f, 0.25f, NdotV);
    float enemyCore = pow(NdotV, 24.0f);
    const float kEnemyBodyColorDepth = 1.2f;
    float3 enemyBodyColor = lerp(bulletColor, kEnemyBodyTint, 0.35f) / kEnemyBodyColorDepth;
    float3 finalRgb = lerp(enemyBodyColor, kEnemyOutlineColor, enemyOutlineMask);
    // 小さな淡いピンクのコアだけを控えめに発光させる。
    finalRgb = lerp(finalRgb, kEnemyCoreColor * 1.15f, enemyCore);
    float enemyOpacity = lerp(kEnemyBodyOpacity, kEnemyOutlineOpacity, enemyOutlineMask);
    enemyOpacity = lerp(enemyOpacity, kEnemyCoreOpacity, enemyCore);
    alpha = enemyEdgeFade * enemyOpacity * saturate(materialParameters.color.a);
#endif

    output.color = float32_t4(finalRgb, alpha);
    output.encodedNormal = float32_t4(N * 0.5f + 0.5f, input.position.z);
    output.indirectColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.surfaceMaterial = float4(0.0f, 0.0f, 0.0f, 0.0f);

    return output;
}
