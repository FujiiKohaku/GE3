#include "SpritePixelCommon.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

float SpriteHash(float2 value)
{
    value = frac(value * float2(123.34f, 456.21f));
    value += dot(value, value + 45.32f);
    return frac(value.x * value.y);
}

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 sourceUv = input.texcoord;
    float2 uv = mul(float4(sourceUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 color = gTexture.Sample(gSampler, uv) * gMaterial.color;
    float time = gFrame.elapsedTime * gEffect.speed + gEffect.phase;

#if SPRITE_PIXEL_EFFECT == 1
    float3 sepia;
    sepia.r = dot(color.rgb, float3(0.393f, 0.769f, 0.189f));
    sepia.g = dot(color.rgb, float3(0.349f, 0.686f, 0.168f));
    sepia.b = dot(color.rgb, float3(0.272f, 0.534f, 0.131f));
    float sepiaAmount = saturate(0.5f + 0.5f * sin(time));
    color.rgb = lerp(color.rgb, sepia, sepiaAmount * gEffect.strength);
#elif SPRITE_PIXEL_EFFECT == 2
    float luminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float threshold = 0.5f + 0.25f * sin(time);
    color.rgb = step(threshold, luminance).xxx;
#elif SPRITE_PIXEL_EFFECT == 3
    float2 texel = 1.0f / max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float centerLum = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    float rightLum = dot(gTexture.Sample(gSampler, uv + float2(texel.x, 0.0f)).rgb, float3(0.299f, 0.587f, 0.114f));
    float downLum = dot(gTexture.Sample(gSampler, uv + float2(0.0f, texel.y)).rgb, float3(0.299f, 0.587f, 0.114f));
    float edge = saturate((abs(centerLum - rightLum) + abs(centerLum - downLum)) * gEffect.strength * 8.0f);
    color.rgb += edge * float3(0.1f, 0.8f, 1.0f);
#elif SPRITE_PIXEL_EFFECT == 4
    float blocks = max(gEffect.amplitude, 2.0f);
    float2 mosaicUv = (floor(sourceUv * blocks) + 0.5f) / blocks;
    mosaicUv = mul(float4(mosaicUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    color = gTexture.Sample(gSampler, mosaicUv) * gMaterial.color;
#elif SPRITE_PIXEL_EFFECT == 5
    float2 mirrorUv = sourceUv;
    mirrorUv.x = abs(mirrorUv.x * 2.0f - 1.0f);
    mirrorUv = mul(float4(mirrorUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    color = gTexture.Sample(gSampler, mirrorUv) * gMaterial.color;
#elif SPRITE_PIXEL_EFFECT == 6
    float2 kaleidoscopePosition = sourceUv - 0.5f;
    float radius = length(kaleidoscopePosition);
    float angle = atan2(kaleidoscopePosition.y, kaleidoscopePosition.x) + time * 0.15f;
    float segments = max(gEffect.amplitude, 2.0f);
    float sector = 6.28318530718f / segments;
    angle = abs(fmod(angle, sector) - sector * 0.5f);
    float2 kaleidoscopeUv = float2(cos(angle), sin(angle)) * radius + 0.5f;
    kaleidoscopeUv = mul(float4(kaleidoscopeUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    color = gTexture.Sample(gSampler, kaleidoscopeUv) * gMaterial.color;
#elif SPRITE_PIXEL_EFFECT == 7
    float2 crtPosition = sourceUv - 0.5f;
    crtPosition *= 1.0f + dot(crtPosition, crtPosition) * gEffect.amplitude;
    float2 crtUv = crtPosition + 0.5f;
    clip(crtUv.x);
    clip(crtUv.y);
    clip(1.0f - crtUv.x);
    clip(1.0f - crtUv.y);
    crtUv = mul(float4(crtUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    color = gTexture.Sample(gSampler, crtUv) * gMaterial.color;
    float crtScan = 0.5f + 0.5f * sin(sourceUv.y * gEffect.frequency + time);
    color.rgb *= 1.0f - crtScan * gEffect.strength;
#elif SPRITE_PIXEL_EFFECT == 8
    float row = floor(sourceUv.y * gEffect.frequency);
    float glitchGate = step(0.72f, SpriteHash(float2(row, floor(time))));
    float glitchOffset = (SpriteHash(float2(row + 3.1f, floor(time))) - 0.5f) * gEffect.amplitude * glitchGate;
    float2 glitchUv = sourceUv + float2(glitchOffset, 0.0f);
    glitchUv = mul(float4(glitchUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    color = gTexture.Sample(gSampler, glitchUv) * gMaterial.color;
#elif SPRITE_PIXEL_EFFECT == 9
    float noise = SpriteHash(sourceUv * gEffect.frequency + floor(time));
    color.rgb += (noise - 0.5f) * gEffect.strength;
#elif SPRITE_PIXEL_EFFECT == 10
    float fade = 0.15f + 0.85f * (0.5f + 0.5f * sin(time));
    color.a *= fade;
#elif SPRITE_PIXEL_EFFECT == 11
    float borderDistance = min(
        min(sourceUv.x, 1.0f - sourceUv.x),
        min(sourceUv.y, 1.0f - sourceUv.y));
    float border = 1.0f - smoothstep(gEffect.amplitude, gEffect.amplitude + 0.008f, borderDistance);
    float3 borderColor = 0.55f + 0.45f * sin(time + float3(0.0f, 2.1f, 4.2f));
    color.rgb = lerp(color.rgb, borderColor, border);
#elif SPRITE_PIXEL_EFFECT == 12
    float circleRadius = 0.34f + 0.14f * (0.5f + 0.5f * sin(time));
    float circleDistance = length(sourceUv - 0.5f);
    clip(circleRadius - circleDistance);
#elif SPRITE_PIXEL_EFFECT == 13
    float checkerSize = max(gEffect.amplitude, 2.0f);
    float2 checkerCell = floor(sourceUv * checkerSize);
    float checker = fmod(checkerCell.x + checkerCell.y, 2.0f);
    float checkerThreshold = 0.5f + 0.5f * sin(time);
    float checkerVisibility = abs(checker - checkerThreshold);
    clip(checkerVisibility - 0.18f);
#elif SPRITE_PIXEL_EFFECT == 14
    float2 hazeUv = sourceUv;
    hazeUv.x += sin(sourceUv.y * gEffect.frequency + time) * gEffect.amplitude;
    hazeUv.y += cos(sourceUv.x * gEffect.frequency * 0.73f + time * 1.21f) * gEffect.amplitude;
    hazeUv = mul(float4(hazeUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    color = gTexture.Sample(gSampler, hazeUv) * gMaterial.color;
#elif SPRITE_PIXEL_EFFECT == 15
    float rainbowPhase = sourceUv.x * gEffect.frequency + sourceUv.y * 3.0f + time;
    float3 rainbow = 0.5f + 0.5f * sin(rainbowPhase + float3(0.0f, 2.094395f, 4.188790f));
    color.rgb = lerp(color.rgb, color.rgb * rainbow * 1.6f, gEffect.strength);
#elif SPRITE_PIXEL_EFFECT == 16
    float hologramRow = floor(sourceUv.y * 80.0f);
    float hologramGate = step(0.9f, SpriteHash(float2(hologramRow, floor(time))));
    float hologramOffset = (SpriteHash(float2(hologramRow + 7.0f, floor(time))) - 0.5f) * gEffect.amplitude * hologramGate;
    float2 hologramUv = sourceUv + float2(hologramOffset, 0.0f);
    hologramUv = mul(float4(hologramUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    color = gTexture.Sample(gSampler, hologramUv) * gMaterial.color;
    float hologramScan = 0.65f + 0.35f * sin(sourceUv.y * gEffect.frequency + time);
    color.rgb = lerp(color.rgb, color.rgb * float3(0.15f, 1.25f, 1.5f) * hologramScan, gEffect.strength);
#elif SPRITE_PIXEL_EFFECT == 17
    float halftoneScale = max(gEffect.amplitude, 4.0f);
    float2 halftoneCell = frac(sourceUv * halftoneScale) - 0.5f;
    float halftoneLuminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float halftoneRadius = sqrt(saturate(halftoneLuminance)) * 0.62f;
    float halftoneDot = 1.0f - smoothstep(halftoneRadius - 0.04f, halftoneRadius, length(halftoneCell));
    color.rgb = lerp(float3(0.03f, 0.04f, 0.08f), color.rgb, halftoneDot);
#elif SPRITE_PIXEL_EFFECT == 18
    float2 embossTexel = max(gEffect.amplitude, 1.0f) / max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float3 embossA = gTexture.Sample(gSampler, uv - embossTexel).rgb;
    float3 embossB = gTexture.Sample(gSampler, uv + embossTexel).rgb;
    float embossValue = dot(embossA - embossB, float3(0.299f, 0.587f, 0.114f));
    color.rgb = saturate(0.5f + embossValue * gEffect.strength);
#elif SPRITE_PIXEL_EFFECT == 19
    float2 edgeTexel = max(gEffect.amplitude, 1.0f) / max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float3 edgeLeft = gTexture.Sample(gSampler, uv - float2(edgeTexel.x, 0.0f)).rgb;
    float3 edgeRight = gTexture.Sample(gSampler, uv + float2(edgeTexel.x, 0.0f)).rgb;
    float3 edgeUp = gTexture.Sample(gSampler, uv - float2(0.0f, edgeTexel.y)).rgb;
    float3 edgeDown = gTexture.Sample(gSampler, uv + float2(0.0f, edgeTexel.y)).rgb;
    float3 edgeGradient = abs(edgeRight - edgeLeft) + abs(edgeDown - edgeUp);
    float edgeAmount = saturate(dot(edgeGradient, float3(0.333333f, 0.333333f, 0.333333f)) * gEffect.strength);
    color.rgb = edgeAmount * float3(0.15f, 0.85f, 1.0f);
#elif SPRITE_PIXEL_EFFECT == 20
    float toonLevels = max(gEffect.amplitude, 2.0f);
    color.rgb = floor(color.rgb * toonLevels) / toonLevels;
    float toonLuminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    color.rgb = lerp(toonLuminance.xxx, color.rgb, 1.35f);
#elif SPRITE_PIXEL_EFFECT == 21
    float hueAngle = time;
    float3 hueAxis = normalize(float3(1.0f, 1.0f, 1.0f));
    float3 hueRotated = color.rgb * cos(hueAngle);
    hueRotated += cross(hueAxis, color.rgb) * sin(hueAngle);
    hueRotated += hueAxis * dot(hueAxis, color.rgb) * (1.0f - cos(hueAngle));
    color.rgb = saturate(hueRotated);
#elif SPRITE_PIXEL_EFFECT == 22
    float2 shadowUv = sourceUv - gEffect.direction * gEffect.amplitude;
    float2 shadowSampleUv = mul(float4(shadowUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float shadowAlpha = gTexture.Sample(gSampler, shadowSampleUv).a * gEffect.strength;
    float shadowBounds = step(0.0f, shadowUv.x) * step(0.0f, shadowUv.y);
    shadowBounds *= step(shadowUv.x, 1.0f) * step(shadowUv.y, 1.0f);
    shadowAlpha *= shadowBounds;
    float visibleShadow = shadowAlpha * (1.0f - color.a);
    color.rgb = lerp(color.rgb, float3(0.08f, 0.12f, 0.22f), visibleShadow);
    color.a = saturate(color.a + visibleShadow);
#elif SPRITE_PIXEL_EFFECT == 23
    float2 glowTexel = max(gEffect.amplitude, 1.0f) / max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float3 glow = gTexture.Sample(gSampler, uv + float2(glowTexel.x, 0.0f)).rgb;
    glow += gTexture.Sample(gSampler, uv - float2(glowTexel.x, 0.0f)).rgb;
    glow += gTexture.Sample(gSampler, uv + float2(0.0f, glowTexel.y)).rgb;
    glow += gTexture.Sample(gSampler, uv - float2(0.0f, glowTexel.y)).rgb;
    glow *= 0.25f;
    color.rgb = saturate(color.rgb + glow * gEffect.strength * 0.45f);
#elif SPRITE_PIXEL_EFFECT == 24
    float2 blurTexel = max(gEffect.amplitude, 1.0f) / max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float4 blurColor = color * 4.0f;
    blurColor += gTexture.Sample(gSampler, uv + float2(blurTexel.x, 0.0f)) * 2.0f;
    blurColor += gTexture.Sample(gSampler, uv - float2(blurTexel.x, 0.0f)) * 2.0f;
    blurColor += gTexture.Sample(gSampler, uv + float2(0.0f, blurTexel.y)) * 2.0f;
    blurColor += gTexture.Sample(gSampler, uv - float2(0.0f, blurTexel.y)) * 2.0f;
    blurColor += gTexture.Sample(gSampler, uv + blurTexel);
    blurColor += gTexture.Sample(gSampler, uv - blurTexel);
    blurColor += gTexture.Sample(gSampler, uv + float2(blurTexel.x, -blurTexel.y));
    blurColor += gTexture.Sample(gSampler, uv + float2(-blurTexel.x, blurTexel.y));
    color = blurColor * 0.0625f * gMaterial.color;
#elif SPRITE_PIXEL_EFFECT == 25
    float oldFilmNoise = SpriteHash(sourceUv * gEffect.frequency + floor(time));
    float oldFilmScratch = step(0.985f, SpriteHash(float2(floor(sourceUv.x * 120.0f), floor(time * 0.25f))));
    float oldFilmLuminance = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    float3 oldFilmColor = float3(oldFilmLuminance * 1.15f, oldFilmLuminance, oldFilmLuminance * 0.72f);
    oldFilmColor += (oldFilmNoise - 0.5f) * gEffect.strength;
    oldFilmColor += oldFilmScratch * 0.35f;
    float2 oldFilmPosition = sourceUv - 0.5f;
    oldFilmColor *= saturate(1.15f - dot(oldFilmPosition, oldFilmPosition) * 1.4f);
    color.rgb = saturate(oldFilmColor);
#endif

    output.color = color;
    return output;
}
