#include "ParticlePixelCommon.hlsli"
#include "ParticleFogCommon.hlsli"
#include "../Explosion/Explosion.hlsli"

float ExplosionHash(float2 position)
{
    float3 seed = frac(float3(position.xyx) * 0.1031f);
    seed += dot(seed, seed.yzx + 33.33f);
    return frac((seed.x + seed.y) * seed.z);
}

float ExplosionNoise(float2 position)
{
    float2 cell = floor(position);
    float2 fraction = frac(position);
    fraction = fraction * fraction * (3.0f - 2.0f * fraction);
    return lerp(lerp(ExplosionHash(cell), ExplosionHash(cell + float2(1.0f, 0.0f)), fraction.x),
        lerp(ExplosionHash(cell + float2(0.0f, 1.0f)), ExplosionHash(cell + 1.0f), fraction.x), fraction.y);
}

float ExplosionTurbulence(float2 position)
{
    float density = ExplosionNoise(position) * 0.57f;
    position = mul(position, float2x2(1.62f, -1.17f, 1.17f, 1.62f)) + 7.3f;
    density += ExplosionNoise(position) * 0.28f;
    position = mul(position, float2x2(1.62f, -1.17f, 1.17f, 1.62f)) + 4.1f;
    density += ExplosionNoise(position) * 0.15f;
    return density;
}

PixelShaderOutput main(ExplosionVertexOutput input)
{
    float2 position = input.texcoord * 2.0f - 1.0f;
    float radius = length(position);
    float lifeRatio = input.lifeRatio;
    float coverage = 0.0f;
    float3 particleColor = input.color.rgb;
#if EXPLOSION_PART_KIND == 0
    // 星形ではなく、ごく短い小さな発光として見せる。
    coverage = exp(-radius * radius * 9.0f) * (1.0f - smoothstep(0.65f, 0.95f, radius));
    particleColor *= 1.2f;
#elif EXPLOSION_PART_KIND == 1
    // 一つの炎を、乱流の濃淡と細かな侵食で不規則に燃え尽きさせる。
    float2 flow = position * 4.6f + float2(lifeRatio * 0.7f, -lifeRatio * 2.2f);
    flow += float2(input.shapeSeed * 31.0f, input.shapeSeed * 17.0f);
    float turbulence = ExplosionTurbulence(flow);
    float detail = ExplosionNoise(flow * 3.7f + turbulence * 2.0f);
    float envelope = 1.0f - smoothstep(0.20f, 0.94f, radius);
    float erosion = smoothstep(0.30f, 1.0f, lifeRatio);
    float density = envelope * 0.70f + turbulence * 0.62f + detail * 0.12f;
    float threshold = 0.38f + erosion * 0.65f;
    coverage = smoothstep(threshold, threshold + 0.18f, density) * envelope;
    float heat = saturate((density - threshold) * 1.8f);
    float hotPockets = smoothstep(0.56f, 0.82f, turbulence + detail * 0.16f);
    const float3 kCoolFlameColor = float3(0.18f, 0.065f, 0.025f);
    const float3 kWarmFlameColor = float3(0.95f, 0.29f, 0.055f);
    const float3 kHotFlameColor = float3(1.15f, 0.77f, 0.30f);
    particleColor = lerp(kCoolFlameColor, kWarmFlameColor, smoothstep(0.0f, 0.65f, heat));
    particleColor = lerp(particleColor, kHotFlameColor, hotPockets * heat * (1.0f - erosion));
#elif EXPLOSION_PART_KIND == 2
    // 菱形の粒をやめ、細く先細りする短い火花にする。
    float halfWidth = lerp(0.22f, 0.80f, position.y * 0.5f + 0.5f);
    float sideMask = 1.0f - smoothstep(halfWidth * 0.30f, halfWidth, abs(position.x));
    float lengthMask = 1.0f - smoothstep(0.70f, 0.98f, abs(position.y));
    coverage = sideMask * lengthMask;
    float coreMask = exp(-position.x * position.x * 18.0f);
    particleColor = lerp(particleColor, float3(1.1f, 0.85f, 0.40f), coreMask * 0.60f);
#elif EXPLOSION_PART_KIND == 3
    // 輪郭の閉じた丸い煙ではなく、隙間の多い短い煙片を作る。
    float2 flow = position * float2(4.0f, 2.8f) + float2(lifeRatio * 0.8f, -lifeRatio * 1.5f);
    float turbulence = ExplosionTurbulence(flow);
    float envelope = 1.0f - smoothstep(0.12f, 0.94f, radius);
    float erosion = lerp(0.34f, 0.65f, lifeRatio);
    coverage = smoothstep(erosion, erosion + 0.22f, turbulence) * envelope;
    particleColor *= lerp(0.65f, 1.15f, turbulence);
#endif
    // 使用画像は白一色。形状と質感はすべて上の計算から生成する。
    coverage *= SampleParticleTexture(float2(input.texcoord.x, 1.0f - input.texcoord.y)).a;
    float4 color = float4(particleColor * gMaterial.color.rgb, coverage * input.color.a * gMaterial.color.a);
    const float kExplosionCoverageCutoff = 0.012f;
    if (color.a <= kExplosionCoverageCutoff) { discard; }
    color = ApplyParticleFog(color, input.worldPosition, input.viewDistance);
    return MakeParticlePixelOutput(color);
}
