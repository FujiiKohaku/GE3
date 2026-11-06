#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"
#include "../Explosion/Explosion.hlsli"

float FlameHash(float2 position)
{
    float3 seed = frac(float3(position.xyx) * 0.1031f);
    seed += dot(seed, seed.yzx + 33.33f);
    return frac((seed.x + seed.y) * seed.z);
}

float FlameNoise(float2 position)
{
    float2 cell = floor(position);
    float2 fraction = frac(position);
    fraction = fraction * fraction * (3.0f - 2.0f * fraction);
    return lerp(lerp(FlameHash(cell), FlameHash(cell + float2(1.0f, 0.0f)), fraction.x),
        lerp(FlameHash(cell + float2(0.0f, 1.0f)), FlameHash(cell + 1.0f), fraction.x), fraction.y);
}

PixelShaderOutput main(ExplosionVertexOutput input)
{
    float2 position = input.texcoord * 2.0f - 1.0f;
    float lifeRatio = input.lifeRatio;
    float2 flow = position * float2(3.0f, 4.5f)
        + float2(input.shapeSeed * 23.0f - lifeRatio * 3.0f, -lifeRatio * 4.0f);
    float turbulence = FlameNoise(flow) * 0.7f + FlameNoise(flow * 2.7f + 5.1f) * 0.3f;
    float curve = sin(position.x * 3.2f + input.shapeSeed * 6.28f + lifeRatio * 7.0f) * 0.18f;
    curve += sin(position.x * 8.0f - lifeRatio * 5.0f + input.shapeSeed * 12.0f) * 0.055f;
    float bandDistance = abs(position.y - curve);
    float bandWidth = lerp(0.10f, 0.34f, turbulence);
    float envelope = 1.0f - smoothstep(0.60f, 0.98f, abs(position.x));
    float ribbon = 1.0f - smoothstep(bandWidth * 0.25f, bandWidth, bandDistance);
    float flameTips = (1.0f - smoothstep(0.0f, 0.85f, abs(position.y - curve)))
        * smoothstep(0.55f, 0.80f, turbulence);
    float erosion = smoothstep(0.15f, 1.0f, lifeRatio);
    float breakup = smoothstep(erosion * 0.65f, erosion * 0.65f + 0.18f, turbulence);
    float coverage = saturate(ribbon + flameTips * 0.75f) * envelope * breakup;
    float hotCore = exp(-bandDistance * bandDistance * 150.0f)
        * smoothstep(0.35f, 0.75f, turbulence) * (1.0f - erosion);
    const float3 kOuterFlameColor = float3(0.95f, 0.18f, 0.015f);
    const float3 kInnerFlameColor = float3(1.45f, 0.72f, 0.12f);
    const float3 kWhiteFlameColor = float3(1.6f, 1.25f, 0.55f);
    float3 flameColor = lerp(kOuterFlameColor, kInnerFlameColor, hotCore);
    flameColor = lerp(flameColor, kWhiteFlameColor, smoothstep(0.80f, 0.98f, hotCore) * 0.35f);
    float alpha = coverage * input.color.a * gMaterial.color.a;
    if (alpha <= 0.012f) { discard; }
    float4 color = float4(flameColor * gMaterial.color.rgb, alpha);
    color = ApplyParticleFog(color, input.worldPosition, input.viewDistance);
    return MakeParticlePixelOutput(color);
}

