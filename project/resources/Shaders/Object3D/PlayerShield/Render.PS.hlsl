#include "Surface.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b2);

float2 CalculateHexStroke(float2 position, out float2 cellCenter)
{
    const float2 period = float2(1.0f, 1.7320508f);
    const float2 halfPeriod = period * 0.5f;
    float2 firstCell = frac(position / period) * period - halfPeriod;
    float2 secondCell = frac((position - halfPeriod) / period) * period - halfPeriod;
    float2 nearestCell = firstCell;
    if (dot(secondCell, secondCell) < dot(firstCell, firstCell)) { nearestCell = secondCell; }
    cellCenter = position - nearestCell;
    float2 cellOffset = abs(nearestCell);
    const float edgeDistance = max(cellOffset.x, dot(cellOffset, float2(0.5f, 0.8660254f)));
    return float2(smoothstep(0.34f, 0.40f, edgeDistance), smoothstep(0.42f, 0.47f, edgeDistance));
}

float4 main(ShieldVertexOutput input, bool isFrontFace : SV_IsFrontFace) : SV_Target0
{
    const float kHexGridScale = 6.5f;
    const float3 localDirection = normalize(input.localPosition);
    const float breakCode = gMaterial.uvTransform[0].w;
    if (breakCode < 0.0f)
    {
        // 球面にヒビを一瞬表示する。飛散する破片は別のメッシュで描く。
        const float progress = saturate(-breakCode - 1.0f);
        const float2 sphereUV = float2(atan2(localDirection.z, localDirection.x) / 6.2831853f + 0.5f,
            asin(clamp(localDirection.y, -1.0f, 1.0f)) / 3.1415927f + 0.5f);
        const float2 grid = sphereUV * float2(16.0f, 8.0f);
        const float2 tile = floor(grid);
        float2 cell = frac(grid);
        cell.x = lerp(cell.x, 1.0f - cell.x, fmod(tile.x + tile.y, 2.0f));
        const float triangleSide = step(1.0f, cell.x + cell.y);
        cell = lerp(cell, 1.0f - cell, triangleSide);
        const float crackDistance = min(min(cell.x, cell.y), (1.0f - cell.x - cell.y) * 0.7071f);
        const float crack = (1.0f - smoothstep(0.012f, 0.039f, crackDistance)) *
            (1.0f - smoothstep(0.0f, 1.0f, progress));
        const float opacity = crack * (isFrontFace ? 1.0f : 0.45f);
        clip(opacity - 0.001f);
        return float4(2.0f, 2.8f, 3.4f, opacity);
    }

    float impactLight = 0.0f;
    float centerLight = 0.0f;
    float2 hexStroke = 0.0f;
    [unroll]
    for (int impactIndex = 0; impactIndex < 4; ++impactIndex)
    {
        const float4 impact = gMaterial.uvTransform[impactIndex];
        const float3 offset = localDirection - impact.xyz;
        const float distanceSquared = dot(offset, offset);
        if (impact.w > 0.0f && distanceSquared < 0.17f)
        {
            // Center a hex grid on the contact point using its local tangent plane.
            float3 axis = float3(0.0f, 1.0f, 0.0f);
            if (abs(impact.y) > 0.9f) { axis = float3(1.0f, 0.0f, 0.0f); }
            const float3 tangent = normalize(cross(axis, impact.xyz));
            const float3 bitangent = cross(impact.xyz, tangent);
            const float2 hexPosition = float2(dot(offset, tangent), dot(offset, bitangent)) * kHexGridScale;
            const float fade = smoothstep(0.0f, 0.55f, impact.w);
            const float flash = (1.0f - smoothstep(0.035f, 0.17f, distanceSquared)) * fade;
            float2 cellCenter;
            const float2 stroke = CalculateHexStroke(hexPosition, cellCenter);
            const float cellDistance = length(cellCenter);
            const float age = 1.0f - impact.w;
            const float delay = cellDistance * 0.12f;
            const float cellLight = smoothstep(delay - 0.03f, delay + 0.05f, age);
            const float centerPulse = (1.0f - step(0.5f, cellDistance)) * (1.0f - smoothstep(0.04f, 0.20f, age));
            impactLight = max(impactLight, flash);
            centerLight = max(centerLight, centerPulse * flash);
            hexStroke = max(hexStroke, stroke * flash * cellLight);
        }
    }

    const float impactOpacity = max(0.86f * centerLight,
        max(0.11f * impactLight, max(0.76f * hexStroke.x, 0.95f * hexStroke.y)));
    const float coreWeight = max(centerLight, saturate(hexStroke.y / max(hexStroke.x, 0.001f)));
    const float3 impactColor = lerp(float3(0.025f, 0.16f, 0.65f), float3(1.7f, 2.5f, 3.0f), coreWeight);
    float3 weightedColor = impactColor * impactOpacity;
    float opacity = impactOpacity;
    if (isFrontFace && gMaterial.color.a > 0.0f)
    {
        const float3 normal = normalize(input.normal);
        const float3 viewDirection = normalize(gCamera.worldPosition - input.worldPosition);
        const float facing = saturate(dot(normal, viewDirection));
        const float rim = pow(1.0f - facing, 4.0f);
        const float skyFacing = saturate(normal.y * 0.5f + 0.5f);
        const float3 lightDirection = float3(-0.48f, 0.80f, -0.36f);
        const float highlight = pow(saturate(dot(normal, normalize(lightDirection + viewDirection))), 18.0f);
        const float skyReflection = smoothstep(0.43f, 0.60f, normal.y) *
            (1.0f - smoothstep(0.64f, 0.84f, normal.y)) * smoothstep(0.12f, 0.45f, facing);
        const float3 surfaceTint = lerp(float3(0.036f, 0.12f, 0.48f), float3(0.32f, 0.74f, 1.05f), skyFacing);
        const float3 shellColor = surfaceTint + gMaterial.color.rgb * rim * 0.46f +
            float3(0.65f, 0.83f, 1.0f) * highlight + float3(0.30f, 0.62f, 0.95f) * skyReflection;
        const float shellOpacity = (0.045f + 0.19f * (1.0f - skyFacing) + 0.25f * rim +
            0.19f * highlight + 0.13f * skyReflection) * gMaterial.color.a * (1.0f - impactOpacity);
        weightedColor += shellColor * shellOpacity;
        opacity += shellOpacity;
    }
    clip(opacity - 0.001f);
    return float4(weightedColor / max(opacity, 0.001f), saturate(opacity));
}
