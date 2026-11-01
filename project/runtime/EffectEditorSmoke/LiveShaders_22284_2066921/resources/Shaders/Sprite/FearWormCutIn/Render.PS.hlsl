#include "../Common/SpritePixelCommon.hlsli"

float DistanceToSegment(float2 samplePosition, float2 start, float2 end)
{
    float2 direction = end - start;
    float rate = saturate(dot(samplePosition - start, direction) / max(dot(direction, direction), 0.0001f));
    return length(samplePosition - start - direction * rate);
}

float3 ShadeOrb(float2 samplePosition, float2 center, float radius, float3 baseColor, out float coverage)
{
    float2 local = (samplePosition - center) / radius;
    float distanceSquared = dot(local, local);
    coverage = 1.0f - smoothstep(0.96f, 1.02f, distanceSquared);
    float3 normal = float3(local, sqrt(saturate(1.0f - distanceSquared)));
    float illumination = 0.22f + saturate(dot(normal, normalize(float3(-0.6f, -0.7f, 1.0f)))) * 0.78f;
    float highlight = pow(saturate(dot(normal, normalize(float3(-0.4f, -0.5f, 1.0f)))), 24.0f);
    float rim = pow(saturate(1.0f - normal.z), 3.0f);
    return baseColor * illumination + float3(0.38f, 0.85f, 1.0f) * (highlight * 0.7f + rim * 0.5f);
}

float4 main(SpriteVertexOutput input) : SV_Target0
{
    float elapsedSeconds = gEffect.phase;
    float entryRate = smoothstep(0.0f, 0.08f, elapsedSeconds);
    float exitRate = smoothstep(0.43f, 0.65f, elapsedSeconds);
    float visibility = entryRate * (1.0f - exitRate);
    float2 uv = input.texcoord;
    float bandCenter = 0.295f - (uv.x - 0.5f) * 0.11f;
    float bandDistance = abs(uv.y - bandCenter);
    float bandMask = 1.0f - smoothstep(0.14f * visibility, 0.14f * visibility + 0.0025f, bandDistance);
    float dimAlpha = visibility * 0.16f;
    if (bandMask < 0.001f) { return float4(0.015f, 0.01f, 0.04f, dimAlpha); }

    float2 samplePosition = float2((uv.x - 0.5f) * gEffect.spriteSize.x / max(gEffect.spriteSize.y, 1.0f), uv.y - bandCenter);
    samplePosition.x += (1.0f - entryRate) * 1.4f - exitRate * 1.8f;
    float streak = pow(saturate(sin(uv.y * 650.0f + uv.x * 35.0f - elapsedSeconds * 90.0f)), 18.0f);
    float3 color = lerp(float3(0.025f, 0.012f, 0.08f), float3(0.18f, 0.035f, 0.32f), uv.x);
    color += float3(0.08f, 0.13f, 0.28f) * streak;
    // 紫の節を奥へ連ね、実モデルの青い球状頭部を大きく見せる。
    [unroll]
    for (int segmentIndex = 5; segmentIndex >= 0; --segmentIndex) {
        float bodyRate = float(segmentIndex);
        float2 center = float2(0.28f + bodyRate * 0.105f, 0.04f + sin(bodyRate * 0.9f) * 0.05f);
        float coverage;
        float3 bodyColor = ShadeOrb(samplePosition, center, 0.10f - bodyRate * 0.008f,
            float3(0.65f, 0.08f, 0.90f), coverage);
        color = lerp(color, bodyColor, coverage);
    }
    float headCoverage;
    float2 headCenter = float2(0.10f - elapsedSeconds * 0.08f, 0.02f);
    float3 headColor = ShadeOrb(samplePosition, headCenter, 0.205f, float3(0.12f, 0.72f, 0.95f), headCoverage);
    color = lerp(color, headColor, headCoverage);
    // 目に見立てたエネルギーの切れ込みと横方向のフレア。
    float2 eyeOffset = samplePosition - headCenter - float2(-0.04f, -0.022f);
    float eyeDistance = length(eyeOffset * float2(0.5f, 3.0f));
    float eye = exp(-eyeDistance * 95.0f) * headCoverage;
    float flare = exp(-abs(eyeOffset.y) * 600.0f) * exp(-abs(eyeOffset.x) * 5.0f);
    color += float3(0.30f, 0.95f, 1.0f) * (eye * 2.3f + flare * 0.45f);
    float lightning = 0.0f;
    [unroll]
    for (int boltIndex = 0; boltIndex < 2; ++boltIndex) {
        float previousY = -0.09f + float(boltIndex) * 0.19f;
        float2 previousPoint = float2(-0.95f, previousY);
        [unroll]
        for (int jointIndex = 1; jointIndex <= 9; ++jointIndex) {
            float joint = float(jointIndex);
            float2 nextPoint = float2(-0.95f + joint * 0.22f,
                previousY + sin(joint * 19.3f + boltIndex * 8.7f + floor(elapsedSeconds * 24.0f)) * 0.035f);
            float distanceToBolt = DistanceToSegment(samplePosition, previousPoint, nextPoint);
            lightning = max(lightning, exp(-distanceToBolt * 270.0f) + exp(-distanceToBolt * 65.0f) * 0.25f);
            previousPoint = nextPoint;
        }
    }
    color += float3(0.39f, 0.21f, 1.0f) * lightning * 1.2f;
    float edge = 1.0f - smoothstep(0.001f, 0.005f, abs(bandDistance - 0.14f * visibility));
    float flash = exp(-elapsedSeconds * 40.0f);
    color += float3(0.22f, 0.62f, 1.0f) * (edge + flash * 0.7f);
    return float4(color, max(dimAlpha, bandMask * visibility));
}
